#!/usr/bin/env python3
"""Cluster open stops into reviewable mechanism candidates; never writes game data.

The map and machine-code resolver are evidence, not proof of ownership. Review every
suggestion before opening an issue. Install capstone and pefile for --game.
"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import json
import re
import shlex
import struct
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

from citation_check import Inventory, find_inventory

ROOT = Path(__file__).resolve().parents[2]
ID = re.compile(r'\bS-\d+\b')
HEX = re.compile(r'0x[0-9a-fA-F]{5,8}\b')
TASK = re.compile(r'\b(?:PORT[- ]\d+|Local[- ]\d+|issue\s*#\d+)\b', re.I)
# These are diagnostic/provenance links, not mechanism dependencies.
IGNORE_REFS = {'S-001'}
# W3D draw state selection is one large dispatcher: cited spans cover Lua,
# animation timing and missing-asset recovery, which are separate mechanisms
# (manual review in PORT268_GROUP_CHECK.md). Preserve its citations, not edges.
BROAD_DISPATCHERS = {0x004BE587}
MAX_GROUP = 40


@dataclass
class Stop:
    id: str
    kind: str
    cells: list[str]
    addresses: set[int] = field(default_factory=set)
    functions: set[int] = field(default_factory=set)
    link_functions: set[int] = field(default_factory=set)
    classes: set[str] = field(default_factory=set)
    port_classes: set[str] = field(default_factory=set)
    rotwk_classes: set[str] = field(default_factory=set)
    primary_classes: set[str] = field(default_factory=set)
    refs: set[str] = field(default_factory=set)
    all_refs: set[str] = field(default_factory=set)
    tasks: set[str] = field(default_factory=set)


def read_stops(paths):
    stops = {}
    for path in paths:
        if not path.exists():
            continue
        lines = path.read_text(encoding='utf-8').splitlines()
        # Only line one of a fragment is its stop row; later markdown is notes.
        if path.parent.name == 'stops.d':
            lines = lines[:1]
        for line in lines:
            if not re.match(r'^\|\s*S-\d+\s*\|', line):
                continue
            cells = [x.strip() for x in line.strip().strip('|').split('|')]
            if len(cells) > 8:
                cells = cells[:3] + [' | '.join(cells[3:-4])] + cells[-4:]
            cells += [''] * (8 - len(cells))
            sid, kind = cells[:2]
            if sid in stops:
                raise ValueError(f'duplicate stop {sid} in {path}')
            if kind in ('code', 'project'):
                stops[sid] = Stop(sid, kind, cells)
    return stops


def table(path):
    with path.open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def read_map(path):
    funcs = {}
    classes = set()
    for row in table(path):
        a = int(row['function'], 16)
        c = row['module']
        classes.add(c)
        item = funcs.setdefault(a, {'classes': set(), 'status': row['status'], 'names': set()})
        item['classes'].add(c)
        if row['decomp']:
            item['names'].add(row['decomp'])
    return funcs, classes


def rw_addresses(text):
    # Explicit RW citations and immediately following address/range lists; never
    # interpret arbitrary BFME2 VAs, offsets, RVAs or test data as RotWK VAs.
    result = set()
    for m in re.finditer(r'\bRW\s+(0x[0-9a-fA-F]{5,8}(?:(?:\s*(?:/|,|\.\.|–|->)\s*)(?:RW\s*)?0x[0-9a-fA-F]{5,8})*)', text):
        result.update(int(x, 16) for x in HEX.findall(m.group(1)))
    return result


def seh_after_return(data, offset):
    """Recognize a candidate MSVC SEH entry immediately after ret/ret imm16."""
    return (offset >= 3 and offset + 5 < len(data) and data[offset] == 0xb8
            and data[offset + 5] == 0xe8
            and (data[offset - 1] == 0xc3 or data[offset - 3] == 0xc2))


class BinaryResolver:
    """Bounded reachable basic-block search from independently found entry seeds.

    PE call targets, map starts and post-padding prologues are candidates, not
    a symbol table. We require an instruction span to actually reach a citation.
    For authoritative blocks use citation_check.Inventory instead.
    """
    def __init__(self, path, map_starts):
        import pefile
        import capstone
        pe = pefile.PE(str(path), fast_load=True)
        self.data = pe.get_memory_mapped_image()
        self.base = pe.OPTIONAL_HEADER.ImageBase
        sec = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
        self.lo = self.base + sec.VirtualAddress
        self.hi = self.lo + sec.Misc_VirtualSize
        data = self.data
        t = data[sec.VirtualAddress:sec.VirtualAddress + sec.Misc_VirtualSize]
        seeds = {a for a in map_starts if self.lo <= a < self.hi}
        for i in range(len(t) - 5):
            if t[i] in (0xe8, 0xe9):
                dest = self.lo + i + 5 + struct.unpack_from('<i', t, i + 1)[0]
                if self.lo <= dest < self.hi:
                    seeds.add(dest)
            if t[i] == 0xcc and t[i + 1] in (0x55, 0x56, 0x57, 0x53, 0x8b, 0x83, 0xb8):
                seeds.add(self.lo + i + 1)
            # MSVC SEH prologues may immediately follow the previous ret,
            # without padding or a direct call to this particular entry.
            if seh_after_return(t, i):
                seeds.add(self.lo + i)
        rdata = next((s for s in pe.sections if s.Name.rstrip(b'\0') == b'.rdata'), None)
        if rdata:
            r = data[rdata.VirtualAddress:rdata.VirtualAddress + rdata.Misc_VirtualSize]
            for i in range(0, len(r) - 4, 4):
                a = struct.unpack_from('<I', r, i)[0]
                if self.lo <= a < self.hi:
                    seeds.add(a)
        self.seeds = sorted(seeds)
        self.map_starts = sorted(a for a in map_starts if self.lo <= a < self.hi)
        self.map_start_set = set(self.map_starts)
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.cache = {}

    def reaches(self, start, target):
        # Stop at a later known entry, but do not trust indirect-jump-table seeds
        # as boundaries; reachable instructions are a stronger criterion.
        todo, visited, steps = [start], set(), 0
        while todo and steps < 3500:
            pc = todo.pop()
            while self.lo <= pc < self.hi and pc not in visited and steps < 3500:
                # A tail-jump or fall-through into another known entry is not
                # evidence that the target instruction belongs to this function.
                if pc != start and pc in self.map_start_set:
                    break
                visited.add(pc)
                off = pc - self.base
                ins = next(self.md.disasm(self.data[off:off + 16], pc, count=1), None)
                if ins is None:
                    break
                steps += 1
                if pc <= target < pc + ins.size:
                    return True
                if ins.mnemonic.startswith('ret') or ins.mnemonic in ('int3', 'hlt', 'ud2'):
                    break
                end = pc + ins.size
                if ins.mnemonic.startswith('j'):
                    try:
                        dest = int(ins.op_str, 16)
                    except ValueError:
                        dest = None
                    if dest is not None and self.lo <= dest < self.hi and abs(dest-start) < 0x10000:
                        todo.append(dest)
                    if ins.mnemonic == 'jmp':
                        break
                pc = end
        return False

    def owner(self, a):
        if a in self.cache:
            return self.cache[a]
        if not self.lo <= a < self.hi:
            return None
        # Prefer known map starts over speculative call-target / data pointers.
        # A misread immediate or switch-table entry can point into a function.
        i = bisect.bisect_right(self.map_starts, a)
        for candidate in reversed(self.map_starts[max(0, i - 4):i]):
            if a - candidate > 0x4000:
                break
            if self.reaches(candidate, a):
                self.cache[a] = candidate
                return candidate
        i = bisect.bisect_right(self.seeds, a)
        # Bound the search: do not claim an owner merely by address proximity.
        for candidate in reversed(self.seeds[max(0, i - 24):i]):
            if a - candidate > 0x4000:
                break
            if self.reaches(candidate, a):
                self.cache[a] = candidate
                return candidate
        self.cache[a] = None
        return None


def enrich(stops, funcs, classes, resolver, port_classes=frozenset()):
    # The decomp column supplies RotWK counterpart names, which need not
    # coincide with module names or any port class. They are descriptive only.
    rotwk_classes = {owner for info in funcs.values() for name in info.get('names', ())
                     for owner in re.findall(r'\b([A-Za-z_]\w*)::', name)
                     if len(owner) >= 4 and owner[0].isupper()}
    # Keep broad literal names in metadata, but do not let generic names such
    # as Object or Player make an otherwise unplaced stop a mechanism.
    specific_port = {c for c in port_classes if len(c) >= 7 and any(ch.isupper() for ch in c[1:])} - {'GameLogic'}
    specific_rw = {c for c in rotwk_classes if len(c) >= 7 and any(ch.isupper() for ch in c[1:])} - {'GameLogic'}
    mechanism_classes = classes | specific_port | specific_rw
    known_classes = classes | port_classes | rotwk_classes
    pattern = re.compile(r'(?<![\w])(?:' + '|'.join(map(re.escape, sorted(known_classes, key=lambda x: (-len(x), x)))) + r')(?![\w])') if known_classes else None
    for s in stops.values():
        # Pinned-by contains test-title ranges, not dependencies.
        evidence = ' '.join(s.cells[2:5] + s.cells[6:])
        s.addresses = rw_addresses(evidence)
        s.functions = {f for a in s.addresses if (f := resolver.owner(a)) is not None}
        # Preserve all citations for audit, but do not use explicitly historical
        # fixes as mechanism edges. Such citations often connect unrelated gaps.
        live = []
        for sentence in re.split(r'(?<=[.;])\s+', evidence):
            historical = re.search(r'\b(?:since|earlier|already ported|is ported|are ported|now ported|no longer|passes now|stored verbatim|land(?:s|ed) its hit)\b', sentence, re.I)
            unresolved = re.search(r'\b(?:not ported|unported|not traced|remains? unported)\b', sentence, re.I)
            if not historical or unresolved:
                live.append(sentence)
        s.link_functions = {f for a in rw_addresses(' '.join(live)) if (f := resolver.owner(a)) is not None}
        named = set(pattern.findall(' '.join(s.cells[2:4] + s.cells[6:]))) if pattern else set()
        s.classes = named & mechanism_classes
        s.port_classes = named & port_classes
        s.rotwk_classes = named & rotwk_classes
        # Named subjects are stronger than classes mentioned only as callers,
        # reports or historical context in a long gap description.
        s.primary_classes = set(pattern.findall(s.cells[2])) & classes if pattern else set()
        if not s.primary_classes and pattern:
            prefix = s.cells[3][:100]
            s.primary_classes = {c for c in pattern.findall(prefix)
                                 if c in classes and re.match(r'^\s*`?' + re.escape(c) + r'\b', prefix)}
        s.all_refs = set(ID.findall(evidence)) - {s.id}
        s.refs = (s.all_refs - IGNORE_REFS) & stops.keys()
        s.tasks = set(TASK.findall(' '.join(s.cells)))


def cluster(stops, funcs, limit=MAX_GROUP, stoplist=frozenset()):
    parent = {x: x for x in stops}
    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    def union(a, b):
        a, b = find(a), find(b)
        if a != b:
            parent[max(a, b)] = min(a, b)
    seen = {}
    for sid, s in stops.items():
        for ref in s.refs:
            union(sid, ref)
        keys = [('class', c) for c in s.primary_classes]
        # Common library helpers create giant, meaningless components.
        keys += [('function', a) for a in s.link_functions if a not in stoplist and len(funcs.get(a, {}).get('classes', ())) <= 3]
        for key in keys:
            if key in seen:
                union(sid, seen[key])
            else:
                seen[key] = sid
    components = collections.defaultdict(list)
    unplaced = []
    for sid, s in stops.items():
        if not (s.functions or s.classes or s.refs) and find(sid) == sid and not any(sid in x.refs for x in stops.values()):
            unplaced.append(sid)
        else:
            components[find(sid)].append(sid)
    groups = []
    for component in components.values():
        if len(component) <= limit:
            groups.append(sorted(component))
            continue
        # Split by a shared function first, then by class or actual references.
        # Never stuff unrelated leftovers into one arbitrary 40-stop task.
        remaining = set(component)
        while remaining:
            counts = collections.Counter(a for sid in remaining for a in stops[sid].link_functions
                                         if a not in stoplist and len(funcs.get(a, {}).get('classes', ())) <= 3)
            common = [(count, a) for a, count in counts.items() if count >= 2]
            if common:
                _, pivot = min(common, key=lambda x: (-x[0], x[1]))
                batch = sorted(sid for sid in remaining if pivot in stops[sid].link_functions)[:limit]
            else:
                counts = collections.Counter(c for sid in remaining for c in stops[sid].primary_classes)
                common = [(count, c) for c, count in counts.items() if count >= 2]
                if common:
                    _, pivot = min(common, key=lambda x: (-x[0], x[1]))
                    batch = sorted(sid for sid in remaining if pivot in stops[sid].primary_classes)[:limit]
                else:
                    first = min(remaining)
                    # A generic cross reference can describe a related but separate
                    # mechanism (e.g. numerical facade used by wall replacement).
                    # Keep dependency-only records with a referenced task, but do
                    # not collapse two independently placed mechanisms here.
                    linked = {sid for sid in remaining if
                              (sid in stops[first].refs or first in stops[sid].refs)
                              and (not stops[first].functions and not stops[first].primary_classes
                                   or not stops[sid].functions and not stops[sid].primary_classes)}
                    batch = [first] + sorted(linked - {first})[:limit - 1]
            remaining.difference_update(batch)
            groups.append(batch)
    return groups, sorted(unplaced)


def open_issues(path):
    if path:
        return json.loads(path.read_text(encoding='utf-8'))
    try:
        p = subprocess.run(['gh', 'issue', 'list', '-R', 'Gleb302596555/openbfme-godot', '--state', 'open', '--limit', '1000', '--json', 'number,title'], capture_output=True, text=True, check=True, timeout=20)
        return json.loads(p.stdout)
    except (OSError, subprocess.SubprocessError, ValueError):
        return None


def output(stops, groups, unplaced, funcs, matrix, fields, issues, out, command, stoplist=frozenset()):
    map_classes = set().union(*(v['classes'] for v in funcs.values())) if funcs else set()
    details = []
    for ids in groups:
        fs = set().union(*(stops[x].functions for x in ids))
        cs = set().union(*(stops[x].classes for x in ids))
        cc = collections.Counter(c for x in ids for c in sorted(stops[x].primary_classes))
        fc = collections.Counter(f for x in ids for f in sorted(stops[x].link_functions))
        name = (cc.most_common(1)[0][0] if cc and (len(ids) == 1 or cc.most_common(1)[0][1] > 1)
                else f'RW 0x{fc.most_common(1)[0][0]:08X}' if fc and (len(ids) == 1 or fc.most_common(1)[0][1] > 1)
                else 'linked stops ' + ', '.join(ids[:2]))
        missing = sum(funcs.get(f, {}).get('status') in ('none', 'partial') for f in fs)
        buttons = sorted({r['button'] for r in matrix if r['module class'] in cs and r['button']})
        fl = sorted({f"{r['owner']}.{r['field']}" for r in fields if r['owner'] in cs and r['status'] in ('parsed-unused', 'not-read')})
        covers = None if issues is None else [f"#{i['number']} {i['title']}" for i in issues if re.search(r'\b(?:Port|Local)[ -]+\d+', i['title'], re.I) and (any(re.search(r'(?<![\w-])'+re.escape(x)+r'(?![\w-])', i['title'], re.I) for x in ids) or any(re.search(r'(?<!\w)'+re.escape(x)+r'(?!\w)', i['title'], re.I) for x in cs & map_classes))]
        details.append((ids, fs, cs, name, missing, buttons, fl, covers))
    # Unknown functions are not map 'none', but they are unverified work:
    # count them in the denominator to avoid promoting huge ambiguous groups.
    details.sort(key=lambda x: (-(len(x[0]) / (x[4] + sum(a not in funcs for a in x[1]))
                                  if x[4] + sum(a not in funcs for a in x[1]) else 0),
                                -len(x[5]), -len(x[6]), x[0]))
    out.parent.mkdir(parents=True, exist_ok=True)
    assigned = {sid: f'G{n:03d}' for n, g in enumerate(details, 1) for sid in g[0]}
    incoming = collections.defaultdict(set)
    for source in stops.values():
        for target in source.all_refs & stops.keys():
            incoming[target].add(source.id)
    with out.with_suffix('.tsv').open('w', encoding='utf-8', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['id', 'kind', 'group', 'functions', 'classes', 'port classes', 'RotWK classes', 'references', 'referenced by', 'reported tasks', 'why'])
        for sid, s in sorted(stops.items()):
            peers = [stops[x] for x in next((g[0] for g in details if sid in g[0]), []) if x != sid]
            shared_f = sorted(a for a in set().union(*(s.link_functions & p.link_functions for p in peers))
                              if a not in stoplist and len(funcs.get(a, {}).get('classes', ())) <= 3)
            shared_c = sorted(set().union(*(s.primary_classes & p.primary_classes for p in peers)))
            shared_r = sorted(p.id for p in peers if p.id in s.refs or sid in p.refs)
            cross_refs = sorted(other for other in (s.refs | incoming[sid])
                                if other in assigned and assigned[other] != assigned.get(sid)
                                and (other in s.refs or sid in stops[other].refs))
            reasons = ([f'shared RW 0x{x:08X}' for x in shared_f] +
                       [f'shared class {x}' for x in shared_c] + [f'reference {x}' for x in shared_r] +
                       [f'cross-group reference {x} ({assigned[x]})' for x in cross_refs])
            why = '; '.join(reasons) or ('singleton' if sid in assigned else 'no resolvable mechanism')
            # Keep the non-empty explanation last, so empty trailing TSV fields
            # do not become trailing whitespace in every generated line.
            w.writerow([sid, s.kind, assigned.get(sid, 'unplaced'),
                        ', '.join(f'0x{x:08X}' for x in sorted(s.functions)), ', '.join(sorted(s.classes)),
                        ', '.join(sorted(s.port_classes)), ', '.join(sorted(s.rotwk_classes)),
                        ', '.join(sorted(s.all_refs)), ', '.join(sorted(incoming[sid])), ', '.join(sorted(s.tasks)), why])
    lines = ['# Open stop mechanism candidates', '', f'Regenerate: `{command}`', '', 'For --game install Python packages `capstone` and `pefile`; issue matching uses `gh` or an explicit --issues JSON snapshot. A discovered or explicitly supplied whole-function inventory takes precedence over the game heuristic; without either, only exact map starts are resolved.', '', 'Candidates are heuristic, not proof. Class merges use the area or the first named class of the gap; literal port and map-decomp name matches remain listed separately in TSV; generic names do not place a stop. Cross-group references remain visible after splitting. Common utility functions, the manually reviewed broad draw dispatcher RW 0x004BE587, and citations in explicitly historical-fix sentences are not merge pivots; every citation is retained. Check ownership, dependencies and issue overlap before implementation. Large transitive components are split by function; links can cross groups. Unknown function status is reported separately and is not counted as none, but is included as unverified work in the ranking denominator. Unavailable issue lookup is not evidence of no coverage.', '', f'{len(stops)} open stops; {len(details)} groups; {len(unplaced)} unplaced.', '', '## Ranked groups', '', '| Rank | Mechanism | Stops | none/partial functions | Red buttons | Unused fields | Open task |', '|---:|---|---:|---:|---:|---:|---|']
    for n, (ids, fs, cs, name, missing, buttons, fl, covers) in enumerate(details, 1):
        coverage = 'lookup unavailable' if covers is None else ', '.join(covers) if covers else 'none matched'
        lines.append(f'| {n} | {name} | {len(ids)} | {missing} | {len(buttons)} | {len(fl)} | {coverage.replace("|", "/")} |')
    lines += ['', '## Group details', '']
    for n, (ids, fs, cs, name, missing, buttons, fl, covers) in enumerate(details, 1):
        functions = ', '.join(f"RW 0x{a:08X} ({funcs.get(a, {}).get('status', 'unknown')})" for a in sorted(fs)) or 'none resolved'
        lines += [f'### G{n:03d}: {name}', '',
                  f'Stops: {", ".join(ids)}. Kinds: {", ".join(sorted({stops[x].kind for x in ids}))}.',
                  f'Functions: {functions}. Classes: {", ".join(sorted(cs)) or "none"}.',
                  f'Candidates: {missing} none/partial functions; {sum(a not in funcs for a in fs)} unknown status.',
                  f'Red buttons: {", ".join(buttons) or "none linked"}.',
                  f'Unused fields: {", ".join(fl) or "none linked"}.',
                  f'Existing task matches: {"lookup unavailable" if covers is None else "; ".join(covers) or "none"}.', '']
    lines += ['', '## Unplaced', '', ', '.join(unplaced) or 'None', '', '## Top 30 task drafts', '']
    for n, (ids, fs, cs, name, missing, buttons, fl, covers) in enumerate(details[:30], 1):
        functions = ', '.join(f"RW 0x{a:08X} ({funcs.get(a, {}).get('status', 'unknown')})" for a in sorted(fs)) or 'none resolved'
        known = '; '.join(f"{sid}: " + ', '.join(f'RW 0x{a:08X}' for a in sorted(stops[sid].addresses)) for sid in ids)
        problem_rows = [f"- {sid} ({stops[sid].kind}): {stops[sid].cells[2]} — {stops[sid].cells[3][:450].replace(chr(124), chr(47))}." for sid in ids]
        lines += [f'### Port N: close mechanism {name}: stops {", ".join(ids)}', '', '#### Problem', f'{len(ids)} open stops may share a mechanism. Verify each link; do not assume the grouping proves ownership.', *problem_rows, '', '#### Known', f'Kinds: {", ".join(sorted({stops[x].kind for x in ids}))}. Classes: {", ".join(sorted(cs)) or "none"}.', f'Functions: {functions}.', f'Addresses: {known}.', f'Red buttons: {", ".join(buttons) or "none linked"}.', f'Unused fields: {", ".join(fl) or "none linked"}.', f'Existing task matches: {"lookup unavailable" if covers is None else "; ".join(covers) or "none"}.', '', '#### Task', 'Trace the RotWK mechanism and callers, implement only verified missing behavior, and close or narrow the relevant stops.', '', '#### Tests', 'Add synthetic regression tests for the verified behavior and adjacent branches; run the full test target.', '', '#### In game', 'Choose a verified affected Ennorath object and document reproduction and expected retail behavior.', '', '#### Do not touch', 'Unrelated stops, other open task areas, game files and generated source maps.', '', '#### Done', 'RW instructions and tests justify each change; report any remaining stops and inference.', '']
    out.with_suffix('.md').write_text('\n'.join(lines), encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game', type=Path)
    ap.add_argument('--inventory', type=Path)
    ap.add_argument('--issues', type=Path, help='JSON from gh issue list; otherwise query gh')
    ap.add_argument('--out', type=Path, default=ROOT/'docs/ennorath/STOP_GROUPS')
    args = ap.parse_args()
    funcs, classes = read_map(ROOT/'docs/ennorath/RW_FUNCTION_MAP.tsv')
    stops = read_stops([ROOT/'docs/STOPS.md', *sorted((ROOT/'docs/stops.d').glob('*.md'))])
    inv = args.inventory or find_inventory(ROOT)
    if inv:
        resolver = Inventory.load(inv)
        mode = 'inventory'
    elif args.game:
        resolver = BinaryResolver(args.game, funcs)
        mode = 'game heuristic'
    else:
        class Exact:
            def owner(self, a):
                return a if a in funcs else None
        resolver = Exact()
        mode = 'exact map starts only'
    # Port-only class names are descriptive metadata, never map-class merge keys.
    port_classes = set()
    for path in (ROOT/'engine/src').rglob('*'):
        if path.suffix in ('.h', '.hpp', '.cpp'):
            text = path.read_text(encoding='utf-8', errors='replace')
            port_classes.update(re.findall(r'\b(?:class|struct)\s+([A-Z][A-Za-z0-9_]{3,})\b', text))
    port_classes = {c for c in port_classes if len(c) >= 4 and c[0].isupper()}
    enrich(stops, funcs, classes, resolver, port_classes)
    map_report = (ROOT/'docs/ennorath/RW_FUNCTION_MAP.md').read_text(encoding='utf-8')
    stop_section = map_report.split('## Stop list', 1)[1].split('## ', 1)[0]
    stoplist = {int(a, 16) for a in re.findall(r'(?m)^- (0x[0-9A-Fa-f]+):', stop_section)}
    groups, unplaced = cluster(stops, funcs, stoplist=stoplist | BROAD_DISPATCHERS)
    issues = open_issues(args.issues)
    command_parts = ['python', 'tools/mod/stop_groups.py']
    if args.game:
        command_parts += ['--game', str(args.game)]
    if args.inventory:
        command_parts += ['--inventory', str(args.inventory)]
    if args.issues:
        command_parts += ['--issues', str(args.issues)]
    # Use a repository-relative default so the command is portable between users.
    try:
        out_arg = str(args.out.relative_to(ROOT)) if args.out.is_absolute() else str(args.out)
    except ValueError:
        out_arg = str(args.out)
    command_parts += ['--out', out_arg]
    command = shlex.join(command_parts)
    output(stops, groups, unplaced, funcs, table(ROOT/'docs/ennorath/MATRIX_GAPS.tsv'), table(ROOT/'docs/ennorath/FIELD_GAPS.tsv'), issues, args.out, command, stoplist | BROAD_DISPATCHERS)
    print(f'{len(stops)} stops, {len(groups)} groups, {len(unplaced)} unplaced; resolver: {mode}; issues: {"available" if issues is not None else "unavailable"}')


if __name__ == '__main__':
    main()

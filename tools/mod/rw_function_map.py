#!/usr/bin/env python3
"""Repeatable RotWK module call-graph audit (Port 219).

The CLI resolves create -> constructor -> module vtable, compares each interface
with the direct base's tables, and walks only overridden slots. Functions and
their basic blocks come from a cache produced by an independently seeded CFG;
the tool decodes direct calls/tail jumps and virtual displacements without
following virtual or other indirect targets. It matches exact RW citations in
engine/src and writes TSV/Markdown reports of addresses, names and counts only.
No bytes from retail images are emitted.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from pathlib import Path
import re

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM


@dataclass(frozen=True)
class Function:
    calls: frozenset[int]
    vcalls: frozenset[int]
    instructions: int
    callsites: tuple[tuple[int, int], ...] = ()


def decode_function(data: bytes, base: int, start: int, end: int, entries: set[int]) -> Function:
    """Walk control flow in [start, end); only known entries become tail calls.

    Branches into the current function are followed, not classified as calls.
    Indirect memory calls record their displacement only: they are not resolved.
    The caller supplies independently determined function boundaries and entries.
    """
    if not (base <= start < end <= base + len(data)):
        raise ValueError("function bounds outside image")
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    pending = [start]
    visited: set[int] = set()
    calls: set[int] = set()
    vcalls: set[int] = set()
    callsites: set[tuple[int, int]] = set()
    while pending:
        address = pending.pop()
        while start <= address < end and address not in visited:
            offset = address - base
            ins = next(decoder.disasm(data[offset: min(end - base, offset + 15)], address, count=1), None)
            if ins is None:
                raise ValueError(f"undecodable instruction at 0x{address:X}")
            visited.add(address)
            nxt = address + ins.size
            if ins.mnemonic.startswith("ret") or ins.mnemonic in ("int3", "ud2", "hlt"):
                break
            if ins.mnemonic == "call":
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    calls.add(op.imm)
                    callsites.add((ins.address, op.imm))
                elif op.type == X86_OP_MEM and op.mem.base:
                    vcalls.add(op.mem.disp)
            elif ins.mnemonic == "jmp":
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    if op.imm in entries and op.imm != start:
                        calls.add(op.imm)
                        callsites.add((ins.address, op.imm))
                    elif start <= op.imm < end:
                        pending.append(op.imm)
                break
            elif ins.mnemonic.startswith("j") and ins.operands[0].type == X86_OP_IMM:
                target = ins.operands[0].imm
                if start <= target < end:
                    pending.append(target)
            address = nxt
    return Function(frozenset(calls), frozenset(vcalls), len(visited), tuple(sorted(callsites)))


def walk_graph(roots: set[int], functions: dict[int, Function], stops: set[int]):
    """Return minimum depths and all incoming edges; stopped nodes are included."""
    depth: dict[int, int] = {}
    incoming: dict[int, set[int]] = {}
    queue = deque((root, 0) for root in sorted(roots))
    while queue:
        address, d = queue.popleft()
        if address in depth:
            continue
        depth[address] = d
        if address in stops or address not in functions:
            continue
        for child in sorted(functions[address].calls):
            incoming.setdefault(child, set()).add(address)
            queue.append((child, d + 1))
    return depth, incoming


CITE = re.compile(r"\bRW\s+0x([0-9a-fA-F]{6,8})\b")
PARTIAL = re.compile(r"\bS-\d+\b|\bNOT PORTED\b|\bINFERENCE\b", re.I)


def scan_citations(root: Path) -> dict[int, str]:
    """Classify explicit cites; any unmarked citation takes precedence."""
    found: dict[int, str] = {}
    for path in sorted(root.rglob('*')):
        if not path.is_file() or path.suffix.lower() not in {'.c', '.cc', '.cpp', '.h', '.hpp', '.inl', '.inc'}:
            continue
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        for i, line in enumerate(lines):
            # A marker on the citation line or the next line qualifies the citation.
            context = line + '\n' + (lines[i + 1] if i + 1 < len(lines) else '')
            status = 'partial' if PARTIAL.search(context) else 'ported'
            for match in CITE.finditer(line):
                address = int(match.group(1), 16)
                if address not in found or status == 'ported':
                    found[address] = status
    return found


def importance(objects: int, depth: int, instructions: int, module: str, address: int):
    return (-objects, depth, -instructions, module, address)


def cited_neighbours(address: int, functions: dict[int, Function], citations: dict[int, str]):
    """Cited immediate callers and callees, sorted by address, with direction.

    This is evidence for manual investigation, not proof that the uncited
    function itself was ported. Only citations supplied by the caller count.
    """
    result: set[tuple[str, int, str]] = set()
    for callee in functions.get(address, Function(frozenset(), frozenset(), 0)).calls:
        if callee in citations:
            result.add(('callee', callee, citations[callee]))
    for caller, function in functions.items():
        if address in function.calls and caller in citations:
            result.add(('caller', caller, citations[caller]))
    return sorted(result)


class DecompLookup:
    """Optional counterpart map using the same bounds and fields as rw2decomp."""

    def __init__(self, path: Path):
        import sys
        from bisect import bisect_right

        self._bisect_right = bisect_right
        self.rows = []
        self.starts = []
        if not path.exists():
            return
        # rw2decomp owns CSV parsing and RotWK RVA/size interpretation.
        re_dir = str(Path(__file__).resolve().parents[1] / 're')
        if re_dir not in sys.path:
            sys.path.insert(0, re_dir)
        import rw2decomp
        self.rows = rw2decomp.load(path)
        self.starts = [r['_rva'] for r in self.rows]

    def get(self, address: int) -> str:
        rva = address - 0x400000
        i = self._bisect_right(self.starts, rva) - 1
        if i < 0 or rva >= self.starts[i] + self.rows[i]['_size']:
            return ''
        row = self.rows[i]
        # No BFME2 counterpart: do not present the RotWK name as decomp.
        if row['tier'] == '-':
            return ''
        name = row.get('decomp_qualname') or row.get('decomp_name') or ''
        return f"{name} (tier {row['tier']})" if name else ''


@dataclass(frozen=True)
class MapRow:
    module: str
    depth: int
    function: int
    status: str
    called_from: tuple[int, ...] = ()
    vcalls: tuple[int, ...] = ()
    neighbours: tuple[tuple[str, int, str], ...] = ()
    decomp: str = ''
    objects: int = 0
    instructions: int = 0


TSV_COLUMNS = ('module', 'depth', 'function', 'status', 'called from', 'vcalls', 'neighbour cited', 'decomp')


def write_reports(rows: list[MapRow], tsv: Path, markdown: Path, command: str,
                  hand_stops: dict[int, str], frequency_stops: dict[int, int], threshold: int):
    """Write reports from already validated graph rows, printing improvements.

    Counts are per (module, function) row, not distinct binary functions.
    No module coverage, function boundaries or stop frequencies are inferred here.
    The inventory/graph producer is responsible for supplying those facts.
    """
    import csv
    from collections import Counter

    keys = [(r.module, r.function) for r in rows]
    if len(keys) != len(set(keys)):
        raise ValueError('duplicate module/function row')
    if any(r.status not in {'none', 'partial', 'ported', 'library'} or r.depth < 0 or r.objects < 0 or r.instructions < 0 for r in rows):
        raise ValueError('invalid map row')
    previous = {}
    if tsv.exists():
        with tsv.open(newline='', encoding='utf-8') as f:
            reader = csv.DictReader(f, delimiter='\t')
            if tuple(reader.fieldnames or ()) != TSV_COLUMNS:
                raise ValueError(f'invalid existing report columns: {reader.fieldnames}')
            for row in reader:
                key = (row['module'], int(row['function'], 16))
                if key in previous:
                    raise ValueError(f'duplicate existing report row: {key}')
                previous[key] = row['status']
    ordered = sorted(rows, key=lambda r: (r.module.casefold(), r.module, r.depth, r.function))
    changed = [(r.module, r.function, previous[(r.module, r.function)]) for r in ordered
               if r.status == 'ported' and previous.get((r.module, r.function)) in {'none', 'partial'}]
    tsv.parent.mkdir(parents=True, exist_ok=True)
    markdown.parent.mkdir(parents=True, exist_ok=True)
    with tsv.open('w', newline='', encoding='utf-8') as f:
        out = csv.writer(f, delimiter='\t', lineterminator='\n')
        out.writerow(TSV_COLUMNS)
        for r in ordered:
            out.writerow((r.module, r.depth, f'0x{r.function:08X}', r.status,
                          ', '.join(f'0x{x:08X}' for x in sorted(r.called_from)),
                          ', '.join(f'vcall {x:+#x}' for x in sorted(r.vcalls)),
                          ', '.join(f'{direction} 0x{address:08X} ({status})'
                                    for direction, address, status in sorted(r.neighbours)), r.decomp))
    counts = Counter(r.status for r in rows)
    lines = ['# RotWK module function map', '', f'Command: `{command}`', '',
             f"Rows: {len(rows)}; ported {counts['ported']}, partial {counts['partial']}, none {counts['none']}, library {counts['library']}.",
             'library: code the port replaces wholesale (STL, strings, the C runtime, the Lua VM, compiler-generated destructors, the INI / Xfer',
             'infrastructure): tools/mod/rw_library_functions.tsv, made by tools/mod/rw_library_functions.py; a citation still wins (ported).',
             'Counts are per module/function pair, not unique functions.', '',
             '## Stop list', '', f'Frequency threshold: more than {threshold} call sites.', '',
             'Hand list:']
    lines.extend(f'- 0x{a:08X}: {name}' for a, name in sorted(hand_stops.items()))
    if not hand_stops:
        lines.append('- (empty)')
    lines.extend(['', 'Frequency list:'])
    lines.extend(f'- 0x{a:08X}: {count} call sites' for a, count in sorted(frequency_stops.items()))
    if not frequency_stops:
        lines.append('- (empty)')
    lines.extend(['', '## How to read', '',
                  'A citation is evidence, not a proof of complete equivalence. An uncited address can be',
                  'a compiler thunk, an inlined helper, or logic implemented under another address.',
                  'Conversely, an unmarked target-facts citation can make a function with a stop marker',
                  'look ported: check the module\'s stop lines before treating `ported` as complete.',
                  'Cited neighbours are leads, not proof. Indirect memory calls are candidates for virtual',
                  'calls; their targets are not followed.', '',
                  '## None / partial, by importance', '',
                  '| Module | Objects | Depth | Function | Instructions | Status | Decomp |',
                  '|---|---:|---:|---|---:|---|---|'])
    pending = sorted((r for r in rows if r.status != 'ported'),
                     key=lambda r: importance(r.objects, r.depth, r.instructions, r.module, r.function))
    for r in pending:
        clean = lambda s: s.replace('|', '\\|').replace('\n', ' ')
        lines.append(f'| {clean(r.module)} | {r.objects} | {r.depth} | 0x{r.function:08X} | {r.instructions} | {r.status} | {clean(r.decomp)} |')
    markdown.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    for module, address, old in changed:
        print(f'{module} 0x{address:08X}: {old} -> ported')
    print(f'{len(changed)} rows moved from none/partial to ported')
    return changed


@dataclass(frozen=True)
class ModuleTables:
    name: str
    create: int
    constructor: int
    data_vtable: int
    tables: tuple[tuple[int, int], ...]


def discover_module_tables(image, registry: list[dict], constructor_finder, table_finder) -> list[ModuleTables]:
    """Resolve constructor-installed tables, never treating ModuleData as Module.

    The finders operate on the PE image. Each result needs a primary table at
    offset zero; an unresolved factory is an error, not an empty module graph.
    The offset table can contain secondary interfaces and still requires an
    independently determined base class and slot extent before use as roots.
    """
    result = []
    seen = set()
    for record in registry:
        name = record['name']
        if name in seen:
            raise ValueError(f'duplicate module: {name}')
        seen.add(name)
        create = int(record['create'], 0)
        ctor = constructor_finder(image, create)
        if ctor is None:
            raise ValueError(f'{name}: constructor not found for 0x{create:X}')
        stores = table_finder(image, ctor)
        if 0 not in stores:
            raise ValueError(f'{name}: primary vtable not found for 0x{ctor:X}')
        result.append(ModuleTables(name, create, ctor, int(record['vtable'], 0), tuple(sorted(stores.items()))))
    return result


@dataclass(frozen=True)
class ModuleEntry:
    name: str
    tag: str
    category: str
    mask: int
    ai: bool
    slot7: bool
    inheritable: bool = False
    copied: bool = False


def copy_module_state(entries: tuple[ModuleEntry, ...]) -> tuple[ModuleEntry, ...]:
    """Mark all copied modules, as ThingTemplate::setCopiedFromDefault does."""
    from dataclasses import replace
    return tuple(replace(entry, copied=True) for entry in entries)


def remove_module(entries: tuple[ModuleEntry, ...], tag: str) -> tuple[tuple[ModuleEntry, ...], ModuleEntry]:
    """Remove the tag from all categories (RW 0x73E782 / 0x73E2B3)."""
    removed = [entry for entry in entries if entry.tag == tag]
    if len(removed) != 1:
        raise ValueError(f'module tag {tag!r}: expected one entry, found {len(removed)}')
    return tuple(entry for entry in entries if entry.tag != tag), removed[0]


def add_module(entries: tuple[ModuleEntry, ...], entry: ModuleEntry, *, child: bool,
               replacing: ModuleEntry | None = None) -> tuple[ModuleEntry, ...]:
    """Replay the module-list effects, given actual registry mask/predicate facts.

    This does not parse INI, classify unknown classes, or relax invalid tags.
    Non-child declarations clear copied, non-inheritable entries with matching
    interface masks across all lists (RW 0x73F33D..0x73F375). Data predicates
    clear AI or slot-7 entries within the same list (RW 0x73F496..0x73F4B9).
    A child can replace an existing tag, except a different Draw class.
    """
    from dataclasses import replace
    if not entry.name or not entry.tag or entry.category not in {'behavior', 'draw', 'clientupdate', 'clientbehavior'}:
        raise ValueError('invalid module name, tag or category')
    if replacing and (replacing.name != entry.name or replacing.tag == entry.tag):
        raise ValueError('ReplaceModule requires the same class and a new tag')
    current = list(entries)
    if not child:
        current = [old for old in current if not (old.copied and not old.inheritable and old.mask & entry.mask)]
    if entry.ai:
        current = [old for old in current if not (old.category == entry.category and old.ai)]
    if child and entry.slot7:
        current = [old for old in current if not (old.category == entry.category and old.slot7)]
    matching = [old for old in current if old.tag == entry.tag]
    if matching:
        if not child or any(old.category == 'draw' and old.name != entry.name for old in matching):
            raise ValueError(f'duplicate tag {entry.tag!r}')
        current = [old for old in current if old.tag != entry.tag]
    current.append(replace(entry, copied=False))
    return tuple(current)


@dataclass(frozen=True)
class ModuleEvent:
    action: str
    tag: str = ''
    name: str = ''
    category: str = ''
    inheritable: bool = False
    source: str = ''
    body: bool = False


def parse_module_events(lines: list[tuple[str, str]], start: int, grammar, *, limit: int | None = None):
    """Extract object-level module edits using the retail field-table grammar.

    `start` is the first line inside an Object/ChildObject/ObjectReskin. Other
    object sub-blocks and module data are skipped using field_audit's grammar;
    arbitrary tags are supported. Return (events, index after the closing End).
    Unknown module classes or unclosed blocks are errors, not empty modules.
    """
    import sys
    local = str(Path(__file__).resolve().parent)
    if local not in sys.path:
        sys.path.insert(0, local)
    import field_audit as fa

    classes = {name.lower(): (name, node) for name, node in grammar.classes.items()}

    def module_end(i, node, source):
        while i < len(lines):
            key, value = fa.field_key(lines[i][0])
            if key.lower() == 'end':
                return i
            kind = fa.row_kind(node, key) if node is not None else None
            if kind and kind[0] == 'script':
                i += 1
                while i < len(lines) and fa.field_key(lines[i][0])[0].lower() != 'endscript':
                    i += 1
                if i == len(lines):
                    raise ValueError(f'{source}: unclosed module script')
            elif (kind and kind[0] == 'block' and
                  (key.lower() != 'addemotion' or value.split()[:1] and value.split()[0].lower() == 'override')):
                end = fa.block_end(lines, i + 1, kind[1], grammar.classes)
                if end >= 0:
                    i = end
            i += 1
        raise ValueError(f'{source}: unclosed module')

    def walk(i, mode):
        events = []
        while i < len(lines):
            text, source = lines[i]
            key, value = fa.field_key(text)
            low = key.lower()
            if low == 'end':
                return events, i + 1
            if low in {'addmodule', 'inheritablemodule', 'replacemodule'}:
                args = value.replace('=', ' ').split()
                if mode != 'normal':
                    raise ValueError(f'{source}: nested module editing mode')
                if low == 'replacemodule':
                    if len(args) != 1:
                        raise ValueError(f'{source}: ReplaceModule requires a tag')
                    events.append(ModuleEvent('replace_begin', tag=args[0], source=source))
                child, i = walk(i + 1, low)
                events.extend(child)
                if low == 'replacemodule':
                    events.append(ModuleEvent('replace_end', tag=args[0], source=source))
                continue
            if low == 'removemodule':
                args = value.replace('=', ' ').split()
                if mode != 'normal' or len(args) != 1:
                    raise ValueError(f'{source}: invalid RemoveModule')
                events.append(ModuleEvent('remove', tag=args[0], source=source))
            elif low in fa.MODULE_KEYS:
                args = value.replace('=', ' ').split()
                if len(args) < 2:
                    raise ValueError(f'{source}: {key} requires class and tag')
                found = classes.get(args[0].lower())
                if not found:
                    raise ValueError(f'{source}: unknown module class {args[0]}')
                end = module_end(i + 1, found[1], source)
                category = 'behavior' if low == 'body' else low
                events.append(ModuleEvent('add', tag=args[1], name=found[0], category=category,
                                          inheritable=mode == 'inheritablemodule', source=source, body=low == 'body'))
                i = end + 1
                continue
            else:
                kind = fa.row_kind(grammar.object, low)
                if kind and kind[0] == 'script':
                    i += 1
                    while i < len(lines) and fa.field_key(lines[i][0])[0].lower() != 'endscript':
                        i += 1
                    if i == len(lines):
                        raise ValueError(f'{source}: unclosed script')
                elif kind and kind[0] == 'block' and fa.opens_block(lines, i + 1, kind[1], grammar.object, grammar.classes):
                    end = fa.block_end(lines, i + 1, kind[1], grammar.classes)
                    if end < 0:
                        raise ValueError(f'{source}: unclosed object block {key}')
                    i = end
            i += 1
        raise ValueError('object or module edit block is not closed')

    events, end = walk(start, 'normal')
    if limit is not None and end != limit:
        raise ValueError(f'object boundary mismatch: parsed to line index {end}, expected {limit}')
    return events, end


def replay_module_events(entries: tuple[ModuleEntry, ...], events: list[ModuleEvent], registry: list[dict], *, child: bool):
    """Apply extracted edits to a copied object using registry masks/predicates.

    Caller selects the source object and whether it is a ChildObject; copying
    and default-template creation are deliberately explicit. Errors are fatal:
    silently omitting a module would make the object counts misleading.
    """
    records = {record['name']: record for record in registry}
    types = {'behavior': 0, 'draw': 1, 'clientupdate': 2, 'clientbehavior': 3}
    replacing = None
    in_replace = False
    for event in events:
        try:
            if event.action == 'remove':
                entries, _ = remove_module(entries, event.tag)
            elif event.action == 'replace_begin':
                if in_replace:
                    raise ValueError('nested replacement')
                entries, replacing = remove_module(entries, event.tag)
                in_replace = True
            elif event.action == 'replace_end':
                if not in_replace or replacing is None or replacing.tag != event.tag:
                    raise ValueError('unmatched replacement')
                replacing = None
                in_replace = False
            elif event.action == 'add':
                record = records.get(event.name)
                if record is None or record['type'] != types[event.category]:
                    raise ValueError(f'unknown module/type {event.name}/{event.category}')
                if event.category == 'behavior' and bool(record['mask'] & 32) != event.body:
                    raise ValueError(f'body category mismatch for {event.name}')
                entry = ModuleEntry(event.name, event.tag, event.category, record['mask'], record['isAiModuleData'],
                                    record['slot7'], event.inheritable)
                entries = add_module(entries, entry, child=child, replacing=replacing)
            else:
                raise ValueError(f'unknown module event {event.action!r}')
        except (ValueError, KeyError) as exc:
            raise ValueError(f'{event.source}: {exc}') from exc
    if in_replace:
        raise ValueError('unclosed replacement')
    return entries


@dataclass(frozen=True)
class ObjectDefinition:
    name: str
    kind: str
    parent: str
    events: tuple[ModuleEvent, ...]
    source: str = ''


def resolve_objects(definitions: list[ObjectDefinition], registry: list[dict]):
    """Resolve definitions in supplied *load order*, rejecting missing parents.

    Counting is per distinct object, not number of declarations or instances of
    a module within an object. This function does not infer the archive's load
    order or a map's override load type. Duplicate Object definitions are
    replayed into the existing template for normal load (type 1), as in
    ThingFactory::parseObjectDefinition. All keys are case-sensitive like the port.
    """
    from collections import Counter
    objects: dict[str, tuple[ModuleEntry, ...]] = {}
    for obj in definitions:
        if not obj.name:
            raise ValueError(f'{obj.source}: empty object name')
        if obj.kind not in {'object', 'childobject', 'objectreskin'}:
            raise ValueError(f'{obj.source}: unsupported object kind {obj.kind!r}')
        if obj.kind == 'object':
            if obj.parent:
                raise ValueError(f'{obj.source}: Object has a parent')
            entries = objects.get(obj.name)
            if entries is None:
                entries = copy_module_state(objects.get('DefaultThingTemplate', ()))
        else:
            if obj.parent not in objects:
                raise ValueError(f'{obj.source}: parent {obj.parent!r} not yet loaded')
            entries = copy_module_state(objects[obj.parent])
        objects[obj.name] = replay_module_events(entries, list(obj.events), registry, child=obj.kind == 'childobject')
    counts = Counter()
    for entries in objects.values():
        counts.update({entry.name for entry in entries})
    return objects, counts


def decode_inventory(data: bytes, base: int, bounds: dict[int, int], threshold: int = 200):
    """Decode an externally established inventory and count distinct call sites.

    `bounds` maps entry VA to exclusive end VA. It must contain nonoverlapping,
    independently determined function extents; do not pass guessed linear-sweep
    boundaries. Return decoded functions and targets exceeding the threshold.
    An unknown direct target remains in the graph as an unresolved leaf.
    """
    from collections import Counter
    if threshold < 0:
        raise ValueError('negative stop threshold')
    previous_end = base
    for start, end in sorted(bounds.items()):
        if start < previous_end or start >= end or end > base + len(data):
            raise ValueError(f'invalid or overlapping function bounds at 0x{start:X}')
        previous_end = end
    entries = set(bounds)
    functions = {start: decode_function(data, base, start, end, entries) for start, end in sorted(bounds.items())}
    counts = Counter(target for function in functions.values() for _, target in function.callsites)
    return functions, {target: count for target, count in counts.items() if count > threshold}


def overridden_slots(child: dict[int, tuple[int, ...]], parent: dict[int, tuple[int, ...]]):
    """Compare independently bounded, corresponding interface tables by offset.

    Result: (interface offset, slot index, target). Never infer table lengths,
    parent types or match an unrelated interface by position. A child may add
    slots or interfaces; shared slots are retained only when their target differs.
    """
    result = []
    for offset, slots in sorted(child.items()):
        if offset < 0 or offset % 4 or not slots or any(target <= 0 for target in slots):
            raise ValueError('invalid child vtable')
        base_slots = parent.get(offset, ())
        for index, target in enumerate(slots):
            if index >= len(base_slots) or target != base_slots[index]:
                result.append((offset, index, target))
    return result


class IniZip:
    """Case-insensitive, read-only VFS for an INI archive (including includes)."""
    def __init__(self, path: Path):
        import zipfile
        self.archive = zipfile.ZipFile(path)
        self.files = {}
        for info in self.archive.infolist():
            name = info.filename.replace('\\', '/').lower()
            if info.is_dir() or not name.startswith('data/ini/'):
                continue
            if name in self.files:
                raise ValueError(f'duplicate case-insensitive INI path: {name}')
            self.files[name] = info

    def read(self, name: str):
        info = self.files.get(name.replace('\\', '/').lower())
        if info is None:
            return None
        # splitlines on latin1 would split at 0x85, a printable ANSI byte.
        return self.archive.read(info).decode('cp1252', errors='replace')

    def close(self):
        self.archive.close()


def inventory_from_ini(path: Path, grammar, registry: list[dict]):
    """Read default and object INIs in case-insensitive retail file order.

    Loads includes via ennorath_graph.expand and replays effective modules.
    Only Object headers in the object/default files are considered; any missing
    include, unclosed block or unresolved parent/tag is fatal. #defines use the
    existing audit's per-file semantics; map-specific overrides are not loaded.
    """
    import sys
    local = str(Path(__file__).resolve().parent)
    if local not in sys.path:
        sys.path.insert(0, local)
    import ennorath_graph as eg
    vfs = IniZip(path)
    definitions = []
    previous_missing = len(eg.MISSING)
    try:
        default = 'data/ini/default/object.ini'
        names = ([default] if default in vfs.files else []) + sorted(
            n for n in vfs.files if n.startswith('data/ini/object/') and n.endswith('.ini'))
        if not names:
            raise ValueError('archive contains no default or object INI files')
        for name in names:
            lines = eg.expand(vfs, name, {})
            i = 0
            while i < len(lines):
                text, source = lines[i]
                tokens = text.split()
                kind = tokens[0].lower() if tokens else ''
                if '=' not in text and kind in {'object', 'childobject', 'objectreskin'} and len(tokens) >= 2:
                    if kind != 'object' and len(tokens) < 3:
                        raise ValueError(f'{source}: {kind} has no parent')
                    events, end = parse_module_events(lines, i + 1, grammar)
                    definitions.append(ObjectDefinition(tokens[1], kind, tokens[2] if kind != 'object' else '', tuple(events), source))
                    i = end
                else:
                    i += 1
        missing = eg.MISSING[previous_missing:]
        if missing:
            raise ValueError(f'{len(missing)} missing INI includes, first: {missing[0]}')
        objects, counts = resolve_objects(definitions, registry)
        return objects, counts, definitions
    finally:
        vfs.close()
        del eg.MISSING[previous_missing:]


def decode_block_inventory(data: bytes, base: int,
                           blocks: dict[int, tuple[tuple[int, int], ...]], threshold: int = 200):
    """Decode externally recovered basic blocks, without filling address gaps.

    Each pair is (block VA, size). Shared tail blocks are allowed between
    functions, but frequency counts deduplicate physical call sites. An indirect
    jump is not followed. Direct jumps to another entry are tail calls; jump
    tables and other gaps are not decoded as instructions. A block that ends in
    the middle of an instruction is rejected, rather than silently shortened.
    """
    from collections import Counter
    if threshold < 0:
        raise ValueError('negative frequency threshold')
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    functions = {}
    all_sites = set()
    for start, spans in sorted(blocks.items()):
        if not spans or start not in {a for a, _ in spans}:
            raise ValueError(f'entry has no basic block: 0x{start:X}')
        calls, vcalls, sites, visited = set(), set(), set(), set()
        end_previous = base
        for address, size in sorted(spans):
            end = address + size
            if size <= 0 or address < end_previous or not base <= address < end <= base + len(data):
                raise ValueError(f'invalid basic block: 0x{address:X}')
            end_previous = end
            offset = address - base
            instructions = list(decoder.disasm(data[offset:offset + size], address))
            if not instructions or instructions[-1].address + instructions[-1].size != end:
                raise ValueError(f'truncated basic block: 0x{address:X}')
            for ins in instructions:
                visited.add(ins.address)
                if ins.mnemonic not in {'call', 'jmp'}:
                    continue
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    if ins.mnemonic == 'call' or (op.imm in blocks and op.imm != start):
                        calls.add(op.imm)
                        sites.add((ins.address, op.imm))
                elif ins.mnemonic == 'call' and op.type == X86_OP_MEM and op.mem.base:
                    vcalls.add(op.mem.disp)
        functions[start] = Function(frozenset(calls), frozenset(vcalls), len(visited), tuple(sorted(sites)))
        all_sites.update(sites)
    counts = Counter(target for _, target in all_sites)
    return functions, {target: count for target, count in counts.items() if count > threshold}


def recover_module_layouts(game: Path, registry: list[dict]):
    """Recover direct base calls with this==ecx before the derived table store.

    This is static evidence, not a C++ type proof. Every ambiguity is fatal.
    Table extents end at a known table start or first non-code pointer. These
    machine-recovered extents must be included in the report's INFERENCE caveat.
    """
    import sys
    from capstone.x86 import X86_OP_REG
    directory = str(Path(__file__).resolve().parents[1] / 'rw_object_model')
    if directory not in sys.path:
        sys.path.insert(0, directory)
    from rwimage import Image
    import update_phases as up
    image = Image(str(game))
    stores, parents = {}, {}

    def resolve(f):
        if f in stores:
            return stores[f]
        tables = up.vtable_stores(image, f)
        stores[f] = tables
        seen, _, _ = image.reach(f)
        aliases, candidates = {'ecx': 0}, []
        wrote = False
        for _, ins in sorted(seen.items()):
            ops = ins.operands
            if ins.mnemonic == 'call':
                if ops[0].type == X86_OP_IMM:
                    target = ops[0].imm
                    if aliases.get('ecx') == 0 and target != f and target != up.mr.SEH_PROLOG and not wrote:
                        if resolve(target).get(0):
                            candidates.append(target)
                    if target == up.mr.SEH_PROLOG:
                        continue
                for r in ('eax', 'ecx', 'edx'):
                    aliases.pop(r, None)
            elif ops and ins.mnemonic == 'mov' and ops[0].type == X86_OP_MEM:
                mem = ops[0].mem
                if (mem.base and aliases.get(ins.reg_name(mem.base)) == 0 and mem.disp == 0
                        and len(ops) == 2 and ops[1].type == X86_OP_IMM and ops[1].imm == tables.get(0)):
                    wrote = True
            elif ops and ops[0].type == X86_OP_REG:
                dst = ins.reg_name(ops[0].reg)
                if (ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == X86_OP_REG
                        and ins.reg_name(ops[1].reg) in aliases):
                    aliases[dst] = aliases[ins.reg_name(ops[1].reg)]
                elif (ins.mnemonic == 'lea' and len(ops) == 2 and ops[1].type == X86_OP_MEM
                      and ops[1].mem.base and not ops[1].mem.index and ins.reg_name(ops[1].mem.base) in aliases):
                    aliases[dst] = aliases[ins.reg_name(ops[1].mem.base)] + ops[1].mem.disp
                elif ins.mnemonic not in ('cmp', 'test', 'push'):
                    aliases.pop(dst, None)
        parents[f] = candidates
        return tables

    modules = discover_module_tables(image, registry, up.constructor, lambda _, f: resolve(f))
    # Gate has a secondary UpdateModule base at +4, not a primary base call.
    # RW 0x89C068 installs 0xC57388; RW 0x89C06E calls 0x653114 with ecx=this+4.
    # Verify the actual instructions before accepting this exceptional layout.
    gate = next((m for m in modules if m.name == 'GateOpenAndCloseBehavior'), None)
    if gate:
        seen, _, _ = image.reach(gate.constructor)
        if (gate.constructor != 0x89C04B or 0x89C068 not in seen or 0x89C06E not in seen
                or seen[0x89C068].op_str != 'dword ptr [esi], 0xc57388'
                or seen[0x89C06E].op_str != '0x653114'):
            raise ValueError('GateOpenAndCloseBehavior exceptional base evidence changed')
        resolve(0x653114)
    starts = {v for tables in stores.values() for v in tables.values()}
    starts.add(0xC57388)
    # All constructor-installed tables, not just those used by mod objects,
    # delimit adjacent interfaces. Non-code data is a second terminating guard.
    def slots(address):
        result = []
        for i in range(1024):
            slot = address + 4 * i
            if i and slot in starts:
                break
            if not image.in_data(slot):
                raise ValueError(f'vtable escapes data at 0x{slot:X}')
            value = image.u32(slot)
            if not image.in_text(value):
                break
            result.append(value)
        else:
            raise ValueError(f'unterminated vtable at 0x{address:X}')
        if not result:
            raise ValueError(f'empty vtable at 0x{address:X}')
        return tuple(result)

    result, evidence = {}, []
    for module in modules:
        if gate and module.name == gate.name:
            baseline = {offset + 4: value for offset, value in stores[0x653114].items()}
            baseline[0] = 0xC57388
            parent = 0x653114
        else:
            candidates = parents.get(module.constructor, [])
            if len(candidates) != 1:
                raise ValueError(f'{module.name}: ambiguous base constructors {candidates}')
            parent = candidates[0]
            baseline = stores[parent]
        child = {offset: slots(v) for offset, v in module.tables}
        base_tables = {offset: slots(v) for offset, v in baseline.items()}
        result[module.name] = {target for _, _, target in overridden_slots(child, base_tables)}
        evidence.append((module, parent, child, base_tables))
    return result, evidence


HAND_STOPS = {
    0x42F6E0: 'operator new (RW allocation wrapper)',
    0x42F6A0: 'operator delete (RW deallocation wrapper)',
    0xA3CEF0: 'SEH prolog (compiler helper)',
    0xA3CE04: '_CxxThrowException (noreturn compiler helper)',
    0x42F3C1: 'AsciiString helper',
    0x435D50: 'AsciiString destructor / release',
    0x4374E0: 'AsciiString(const char *) constructor',
    0x437B90: 'AsciiString::translate',
    0x42DC9F: 'INI::getNextToken',
    0x42EAAD: 'INI::scanReal',
    0x42CE52: 'INI boolean-token helper',
    0x42B710: 'MultiIniFieldParse constructor',
    0x42B8D7: 'MultiIniFieldParse::add',
    0x42D4B0: 'INI::initFromINIMulti',
    0xA2D770: 'CRC-32 chained-byte helper',
    0xA211DF: 'XferCRC transfer step',
    0xA20DD0: 'XferSave::Open',
    0xA2111A: 'XferSave::beginBlock',
    0xA206EF: 'XferSave::endBlock',
    0xA203D6: 'Xfer::skipBlock',
}


ANALYZER = ('angr CFGFast(normalize, tail-call detection, no symbols, no prologue scan, '
            'no indirect-jump resolution, no data references)')


def load_function_blocks(game: Path, inventory: Path, registry: list[dict],
                         extra_starts: set[int] | None = None, refresh=False):
    """Cache independently recovered blocks, bound to the exact input SHA256.

    angr is needed only on a cache miss. The cache contains addresses and sizes,
    never instructions or bytes. Keep it under ignored workspace/.
    """
    import hashlib
    import json
    import logging
    digest = hashlib.sha256(game.read_bytes()).hexdigest()
    seeds = {int(r[k], 0) for r in registry for k in ('create', 'createData')}
    seeds.update(extra_starts or ())
    if inventory.exists() and not refresh:
        record = json.loads(inventory.read_text(encoding='utf-8'))
        if record.get('sha256') != digest:
            raise ValueError('function inventory SHA256 does not match game.dat')
        if record.get('missing_seeds'):
            raise ValueError('function inventory has unresolved seeded entries')
        if record.get('seed_count') is not None and record['seed_count'] != len(seeds):
            raise ValueError('function inventory was built from different vtable roots')
    else:
        try:
            import angr
        except ImportError as error:
            raise ValueError('cache missing: install angr==9.2.213 bitstring==4.4.0 pycparser==3.0 to recover it') from error
        logging.getLogger('angr').setLevel(logging.ERROR)
        logging.getLogger('cle').setLevel(logging.ERROR)
        project = angr.Project(str(game), auto_load_libs=False)
        cfg = project.analyses.CFGFast(
            normalize=True, function_starts=sorted(seeds), start_at_entry=False,
            symbols=False, function_prologues=False, resolve_indirect_jumps=False,
            data_references=False, cross_references=False, force_complete_scan=False,
            force_smart_scan=False, detect_tail_calls=True, eh_frame=False,
            exceptions=False, show_progressbar=False)
        functions = []
        for address, function in sorted(cfg.kb.functions.items()):
            if not project.loader.main_object.contains_addr(address):
                continue
            spans = sorted({(block.addr, block.size) for block in function.blocks})
            if spans:
                functions.append(dict(address=address, blocks=spans))
        missing = sorted(seeds - {function['address'] for function in functions})
        if missing:
            raise ValueError(f'CFG did not recover {len(missing)} seeded functions (first 0x{missing[0]:X})')
        record = dict(sha256=digest, analyzer='angr ' + angr.__version__ + ' ' + ANALYZER,
                      seed_count=len(seeds), missing_seeds=missing, functions=functions)
        inventory.parent.mkdir(parents=True, exist_ok=True)
        inventory.write_text(json.dumps(record, sort_keys=True) + '\n', encoding='utf-8')
    result = {}
    for function in record['functions']:
        address = function['address']
        if address in result:
            raise ValueError(f'duplicate cached function 0x{address:X}')
        result[address] = tuple(tuple(span) for span in function['blocks'])
    if not result or seeds - result.keys():
        raise ValueError('incomplete function inventory: a seeded root has no function')
    return result, record['analyzer'], digest


def display_command(argv: list[str]) -> str:
    """A reproducible command without absolute developer-machine paths."""
    import shlex
    import sys
    try:
        executable = str(Path(sys.executable).resolve().relative_to(Path.cwd().resolve()))
    except ValueError:
        executable = 'python'
    parts = [executable, 'tools/mod/rw_function_map.py', *argv]
    shown = []
    for item in parts:
        path = Path(item)
        if path.is_absolute():
            try:
                shown.append(str(path.resolve().relative_to(Path.cwd().resolve())))
                continue
            except ValueError:
                pass
        shown.append(item)
    return shlex.join(shown)


def verify_direct_calls(functions: dict[int, Function], text_start: int, text_end: int):
    """Every direct call/tail-jump target in .text must have its own CFG entry."""
    missing = {target for function in functions.values() for target in function.calls
               if text_start <= target < text_end and target not in functions}
    if missing:
        first = min(missing)
        raise ValueError(f'{len(missing)} direct call targets have no CFG function (first 0x{first:08X})')


def build_rows(roots, counts, functions, citations, stops, decomp, library=frozenset()):
    """Index cited reverse neighbours once, rather than once per report row."""
    neighbours = {}
    for caller, function in functions.items():
        for callee in function.calls:
            if callee in citations:
                neighbours.setdefault(caller, set()).add(('callee', callee, citations[callee]))
            if caller in citations:
                neighbours.setdefault(callee, set()).add(('caller', caller, citations[caller]))
    rows = []
    for module in sorted(counts):
        if module not in roots:
            raise ValueError(f'mod module absent from registry: {module}')
        missing = roots[module] - functions.keys()
        if missing:
            raise ValueError(f'{module}: vtable roots absent from CFG: {sorted(hex(x) for x in missing)}')
        depths, incoming = walk_graph(roots[module], functions, stops)
        for address, depth in sorted(depths.items()):
            function = functions.get(address, Function(frozenset(), frozenset(), 0))
            status = citations.get(address, 'library' if address in library else 'none')  # owner 2026-11-21: library code is not open work
            rows.append(MapRow(module, depth, address, status, tuple(sorted(incoming.get(address, ()))),
                               tuple(sorted(function.vcalls)), tuple(sorted(neighbours.get(address, ()))) if status == 'none' else (),
                               decomp.get(address), counts[module], function.instructions))
    return rows


def main(argv=None):
    import argparse
    import csv
    import json
    import sys
    import time
    import pefile
    parser = argparse.ArgumentParser(description='Audit RotWK module call graphs; no game bytes are written.')
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--registry', type=Path, default=Path('engine/data/rotwk-201/module-registry.json'))
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--ini', type=Path)
    source.add_argument('--coverage', type=Path, help='module-coverage.tsv; object counts unknown (rank=0)')
    parser.add_argument('--src', type=Path, default=Path('engine/src'))
    parser.add_argument('--inventory', type=Path, default=Path('workspace/rebuild/rw_function_blocks.json'))
    parser.add_argument('--refresh-inventory', action='store_true')
    parser.add_argument('--decomp', type=Path, default=Path('workspace/rebuild/xmap/rw_map.csv'))
    parser.add_argument('--threshold', type=int, default=200)
    parser.add_argument('--out', type=Path, default=Path('docs/ennorath/RW_FUNCTION_MAP'))
    args = parser.parse_args(argv)
    if args.threshold < 0:
        parser.error('--threshold must be nonnegative')
    if not args.src.is_dir():
        parser.error('--src must be an existing source directory')
    start = time.monotonic()
    registry = json.loads(args.registry.read_text(encoding='utf-8'))['classes']
    if args.ini:
        import field_audit
        objects, counts, definitions = inventory_from_ini(args.ini, field_audit.Grammar(args.registry.parent), registry)
        source_note = f'INI replay: {len(definitions)} declarations; {len(objects)} unique templates; {len(counts)} module classes.'
    else:
        with args.coverage.open(encoding='utf-8', newline='') as stream:
            reader = csv.DictReader(stream, delimiter='\t')
            if 'class' not in (reader.fieldnames or ()):
                raise ValueError('coverage TSV lacks class column')
            counts = {r['class']: 0 for r in reader}
        source_note = 'Coverage TSV: object counts unavailable; importance object rank is zero, not an observed count.'
    roots, evidence = recover_module_layouts(args.game, registry)
    root_starts = set().union(*roots.values())
    blocks, analyzer, digest = load_function_blocks(args.game, args.inventory, registry,
                                                    root_starts, args.refresh_inventory)
    pe = pefile.PE(str(args.game), fast_load=True)
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    functions, frequent = decode_block_inventory(image, base, blocks, args.threshold)
    text = next(section for section in pe.sections if section.Name.rstrip(b'\0') == b'.text')
    text_start = base + text.VirtualAddress
    text_end = text_start + max(text.Misc_VirtualSize, text.SizeOfRawData)
    verify_direct_calls(functions, text_start, text_end)
    citations = scan_citations(args.src)
    from rw_library_functions import load as load_library
    rows = build_rows(roots, counts, functions, citations, set(HAND_STOPS) | set(frequent), DecompLookup(args.decomp),
                      frozenset(load_library()))
    invoked = sys.argv[1:] if argv is None else list(argv)
    command = display_command(invoked)
    tsv, markdown = args.out.with_suffix('.tsv'), args.out.with_suffix('.md')
    write_reports(rows, tsv, markdown, command, HAND_STOPS, frequent, args.threshold)
    seconds = time.monotonic() - start
    with markdown.open('a', encoding='utf-8') as stream:
        stream.write(f'\n## Provenance and limitations\n\nSHA256: `{digest}`. Analyzer: {analyzer}. '
                     f'Functions: {len(functions)}.\n\n{source_note}\n\n'
                     'INFERENCE: CFGFast and constructor register tracking are static recovery, not a proof of completeness. '
                     'Table extent ends at a constructor-installed table start or the first non-code pointer; '
                     'unreferenced adjacent tables can be merged. Base calls require ecx=this before the derived store; '
                     'GateOpenAndCloseBehavior has a separately checked secondary UpdateModule base at +4. '
                     'All table corrections are local to this report; the registry remains unchanged.\n\n'
                     'INI paths use case-insensitive lexical order after default/object.ini; '
                     'retail directory enumeration order and top-level parser completeness are not independently proven. '
                     'Citation matching is exact-address only, not containing-function equivalence; '
                     'an interior citation or inlined implementation can produce false none. '
                     'No virtual targets or register-indirect calls are resolved by this audit.\n\n'
                     '## Constructor and table evidence\n\n'
                     '| Module | Create | Constructor | Parent constructor | Module tables (offset:address, slots) | Registry ModuleData table |\n'
                     '|---|---|---|---|---|---|\n')
        for module, parent, child, _ in evidence:
            if module.name not in counts:
                continue
            table_text = ', '.join(f'+0x{o:X}:0x{a:08X} ({len(child[o])})' for o, a in module.tables)
            stream.write(f'| {module.name} | 0x{module.create:08X} | 0x{module.constructor:08X} | '
                         f'0x{parent:08X} | {table_text} | 0x{module.data_vtable:08X} |\n')
    print(f'{len(rows)} rows, {len(functions)} functions, {seconds:.2f} seconds')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Start addresses for the mod's unused fields: the field audit's parsed-unused / not-read rows with their owner class's uncited RotWK
functions from the function map (owner 2026-11-16, the cheap link between audit task 1 and task 2).

    field_gaps.py --audit <field-audit-vN/field-audit.tsv> [--map docs/ennorath/RW_FUNCTION_MAP.tsv]
                  [--inventory <rw_function_blocks.json> --game <game.dat>] [--out docs/ennorath/FIELD_GAPS.tsv]

A row's functions are the owner class's `specific none` functions (tools/mod/matrix_gaps.py: reached from at most three classes, not
trivial, not getModuleNameKey / xfer / a deleting destructor). They are candidates, not the field's reader: the reader can be one of them,
a function ported without a citation, or a function of another class that reads the module's data. A row with `how` = manual or a
`stop` may already carry a verdict ("RotWK does not read it", TOP20 / docs/STOPS.md): read it first. The task still proves the reader
(parser -> offset -> every reader, PROCESS 5 step 4). Only block, class and field names, counts and RW addresses are written: no values.
With --game and --inventory, configured singleton blocks get intra-function global-pointer read candidates. Other blocks remain unimplemented.
With --xmap, named template getters add direct-caller candidates (entry-block reads or address-taking only).
Array-singleton receiver chains are followed for up to two direct calls; --extra-inventory can add verified CFG roots.
These are heuristic leads, not no-reader verdicts: stack spills and unknown indirect calls are not followed.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_gaps as mg  # noqa: E402
import block_readers as br  # noqa: E402
import rw_function_map as rm  # noqa: E402

STATUSES = ("parsed-unused", "not-read")
HEADER = ["status", "block", "owner", "field", "defs", "how", "stop", "owner in map", "map none", "specific none", "map partial",
          "candidate functions (address@depth, smallest depth first) / note", "route"]


def defs_of(row) -> int:
    try:
        return int(row.get("defs") or 0)
    except ValueError:
        return 0


def join(audit_rows, gaps, classes, reach=None, sizes=None, skip=None, block_candidates=None):
    include_route = block_candidates is not None
    block_candidates = block_candidates or {}
    rows = []
    for r in audit_rows:
        if r.get("status") not in STATUSES:
            continue
        owner = r.get("owner") or ""
        base = [r["status"], r.get("block", ""), owner, r.get("field", ""), str(defs_of(r)), r.get("how", ""), r.get("stop", "")]
        if owner not in classes:
            candidates = block_candidates.get((r.get("block", "").lower(), r.get("field", "").lower())) if not owner else None
            if candidates is not None:
                route, note = candidates
                rows.append(base + ["no", "", "", "", note, route])
            else:
                note = "the owner is not a module class of the map (an INI block, or a class the mod's objects do not use)"
                if include_route and owner.startswith("/"):
                    note = "no reader route implemented for this nested top-level subtable"
                elif include_route and not owner:
                    block = r.get("block", "").lower()
                    if block in TEMPLATES:
                        note = "template getter route unavailable or field absent from the recovered table"
                    elif block in SINGLETONS or block in STATIC_SINGLETONS or block in SINGLETON_ARRAYS:
                        note = "field absent from the recovered singleton table"
                    else:
                        note = "no reader route implemented for this top-level block"
                rows.append(base + ["no", "", "", "", note] + ([""] if include_route else []))
            continue
        none, specific, partial = mg.class_gaps(gaps.get(owner, {}), reach, sizes, skip)
        note = mg.format_functions(specific) or "no specific none function: the reader is ported without a citation, shared, or in another class"
        rows.append(base + ["yes", str(none), str(len(specific)), str(partial), note] + (["module"] if include_route else []))
    rows.sort(key=lambda x: (-int(x[4]), x[1], x[2], x[3]))
    return rows


def write(rows, path: Path):
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        w = csv.writer(stream, delimiter="\t", lineterminator="\n")
        w.writerow(HEADER)
        w.writerows(rows)


# RW 0x644148 parses into TheGlobalData [0xDE4364] using 0xBFF580
# (0x6441A5..AD). Names and offsets come from the
# original mapped image because those tables are absent from field-tables.json.
SINGLETONS = {
    "gamedata": ("TheGlobalData", 0xDE4364, (), 0xBFF580, None),
    # RW 0x7017AA: TheAI [0xDE4B40], data [ecx+0x18], table 0xC1CB20.
    "aidata": ("TheAI->data", 0xDE4B40, (0x18,), 0xC1CB20, None),
    # RW 0x44107A: TheAudio [0xDE42FC], settings [eax+0x10], table 0xBD7698.
    "audiosettings": ("TheAudio->settings", 0xDE42FC, (0x10,), 0xBD7698, None),
    # RW 0x5ED789: TheMouse at 0xDE36E0, table 0xBF6158. Last
    # valid entry ends at 0xBF62A8, where its name string begins. There is no
    # sentinel in the static image; bounded rows are leads, not a full verdict.
    # DONOR BFME2 parse 0x5EE050 (Code/GameEngine/Source/GameClient/Input/Mouse.cpp).
    "mouse": ("TheMouse", 0xDE36E0, (), 0xBF6158, 0xBF62A8),
    # RW 0x5ECE27 calls TheAudio vtable slot +0x13C and parses into
    # the returned MiscAudio using table 0xBF5450.
    "miscaudio": ("TheAudio->friend_getMiscAudio", 0xDE42FC, ('v13C',), 0xBF5450, None),
    # RW 0x5EEAB3: TheMultiplayerSettings, table pushed at 0x5EEAF4.
    # DONOR BFME2: INI::parseMultiplayerSettingsDefinition (INIMultiplayer.cpp).
    "multiplayersettings": ("TheMultiplayerSettings", 0xDE7D3C, (), 0xC2F708, None),
    # RW 0x6860E0: TheFireLogicSystem, table pushed at 0x6860ED.
    # DONOR BFME2 0x6859A8 (INIBfmeBlockParsers_Rva00210F64Parse.cpp).
    "firelogicsystem": ("TheFireLogicSystem", 0xDE46A8, (), 0xC11750, None),
    # RW 0x5DC3F3: TheDrawGroupInfo, table pushed at 0x5DC425.
    # DONOR BFME2 0x5DCC9F (INIDrawGroupInfo.cpp).
    "drawgroupinfo": ("TheDrawGroupInfo", 0xDE7720, (), 0xBF1F88, None),
    # RW 0x62017F: TheCreateAHeroManager, table pushed at 0x62019C.
    # DONOR BFME2 0x62044C (CreateAHero_FindBlingByUpgradeName.cpp).
    "createaherosystem": ("TheCreateAHeroManager", 0xDE3D84, (), 0xBFCBD8, None),
    # RW 0x5DC743: TheEva + 0x80, table pushed at 0x5DC777.
    # DONOR BFME2 0x5DCFE5 (INI_parseMiscEvaData.cpp).
    "miscevadata": ("TheEva", 0xDE3670, (), 0xBF25E0, None),
    # RW 0x6011F3: WeatherSetting pointer at 0xDE3B58, table at
    # 0x6012BE; an override may be used, so this catches only direct base reads.
    # DONOR BFME2 0x60187A (INIParseWeatherDefinition.cpp).
    "weather": ("TheWeatherSetting", 0xDE3B58, (), 0xBF8D70, None),
    # RW 0x6A9456 passes TheSkirmishAI + 0x10 with table 0xC13C18.
    # This table is bounded: the next record at 0xC13DE8 is not a terminator.
    # DONOR BFME2 INIBfmeBlockParsers_Rva00210F64Parse.cpp.
    "skirmishaidata": ("TheSkirmishAI", 0xDE4938, (), 0xC13C18, 0xC13DE8),
}

# Offsets in these field tables are relative to a subobject of the singleton.
# The parser RW 0x5DC743 passes TheEva + 0x80 to initFromINI.
BASE_OFFSETS = {"miscevadata": 0x80, "skirmishaidata": 0x10}

# RW 0x614FC9 copies saved -> active (0x614FD5), parses active at
# 0x614FEB, and copies active -> saved (0x615003) when not an override.
# DONOR BFME2 0x61526C (INIBfmeSettingsParsers.cpp).
STATIC_SINGLETONS = {
    "cloudeffect": ("TheCloudEffectSettings", (0xDE3CC0,), 0xBFB6A8),
    # RW 0x600564: indexes by six TimeOfDayNames, stride 0x7C at
    # 0x6005C4..CD, base 0xDE4BE8, table 0xC1E4A0 at 0x600605.
    # DONOR BFME2 0x600BEB (INIWater.cpp; WaterSettingsArray.cpp).
    "waterset": ("WaterSettings", tuple(0xDE4BE8 + i * 0x7C for i in range(6)), 0xC1E4A0),
}


# RW 0x719DD3..E4 parses five WeatherData records at TheGlobalWeatherSystem
# +0x30 with stride 8. The receiver passed to methods (e.g. RW 0x719BF0 and
# RW 0x719DEE) needs an observed direct call or tail jump carrying the global.
SINGLETON_ARRAYS = {"weatherdata": ("TheGlobalWeatherSystem", 0xDE772C, 0xC22D0C, 0x30, 8, 5)}


def singleton_offset(block, table_offset):
    return table_offset + BASE_OFFSETS.get(block, 0)


# Table addresses are arguments to initFromINI: Object (RW 0x73BEC3),
# Weapon (RW 0x6CED65, 0x6CEDE9), Locomotor (RW 0x5E8276, 0x5E83DC),
# Upgrade (RW 0x66FCDA, 0x66FD7B); ExperienceLevel table RW 0xC11BF0.
TEMPLATES = {
    "object": ("ThingTemplate", 0xDA3DF8),
    "weapon": ("WeaponTemplate", 0xC16DD8),
    "locomotor": ("LocomotorTemplate", 0xBF4478),
    "upgrade": ("UpgradeTemplate", 0xC107F8),
    "experiencelevel": ("ExperienceLevel", 0xC11BF0),
}


def inferred_candidates(pe, data, base, blocks, audit_rows, citations, known):
    """Top-level INI blocks without a proven parse address (not SINGLETONS / TEMPLATES, no owner, no sub-block): the block's field table
    found in the image by its names (br.find_field_tables / match_block_tables), the global pointer or static object whose reads best
    explain the table's USED fields' offsets (br.match_instance), kept only when no other table claims the same instance
    (br.assign_instances). Leads, not proofs: the route says "inferred" and the task proves the parse address first."""
    fields, used, unused = {}, {}, {}
    for r in audit_rows:
        block, field = (r.get("block") or "").lower(), (r.get("field") or "").lower()
        if r.get("owner") or not block or "/" in block or block in known:
            continue
        fields.setdefault(block, set()).add(field)
        (unused if r.get("status") in STATUSES else used).setdefault(block, set()).add(field)
    sections = br.image_sections(pe)
    tables = br.find_field_tables(data, base, sections)
    by_table = br.match_block_tables({b: f for b, f in fields.items() if unused.get(b)}, tables)
    reads = br.all_pointer_reads(data, base, blocks, (sections[".data"][0], sections[".data"][1] + 0x200000))
    instances = br.instance_offsets(reads)
    absolute = {}
    for function, kind, root, path, offset, instruction in reads:
        if kind == "abs":
            absolute.setdefault(root, set()).add((function, instruction))
    statics = {root for (kind, root, path) in instances if kind == "static"}
    pointers = {i: o for i, o in instances.items() if i[0] == "ptr"}
    matches = {}
    for block, table in by_table.items():
        m = br.match_instance(tables[table], used.get(block, set()), pointers, statics, absolute)
        # a static object gathers every absolute read near it: below 0.9 it is other globals next to it (RW 0x4A8AAD reads 0xDCBBB4,
        # not a SpecialPower template; hand check 2026-11-17)
        if m and not (m[0][0] == "static" and m[2] < 0.9):
            matches[block] = (m, table)
    kept = br.assign_instances({b: m for b, (m, _) in matches.items()})
    output = {}
    for block, table in by_table.items():
        offsets = {row[0].lower(): row[3] for row in tables[table]}
        for field in unused.get(block, ()):
            if block not in kept:
                why = "no instance explains the used fields" if block not in matches else "its best instance also explains another table"
                output[(block, field)] = (f"table 0x{table:X}", why + "; not a no-reader verdict")
                continue
            (kind, root, path), hit, precision, reads_at = kept[block]
            name = f"{kind} 0x{root:X}" + "".join(f" ->+0x{step:X}" for step in path)
            off = offsets.get(field)
            route = f"inferred {name} +0x{off:X} (table 0x{table:X}, used fields hit {hit}, precision {precision:.2f})" if off is not None                 else f"inferred {name} (table 0x{table:X}: field not in the table)"
            hits = sorted(reads_at.get(off, ())) if off is not None else []
            note = ", ".join(f"0x{function:08X} ({citations.get(function, 'none')}; read 0x{instruction:08X})" for function, instruction in hits)
            output[(block, field)] = (route, note or "no read of this offset through the instance; not a no-reader verdict")
    return output


def named_template_getters(stream, base):
    """Keep every qualifying name: several template getters may share one body."""
    reader = csv.DictReader(stream)
    required = {"decomp_qualname", "tier", "rotwk_rva"}
    if not required.issubset(reader.fieldnames or ()):
        raise ValueError("invalid rw_map.csv: missing getter columns")
    named = {}
    for row in reader:
        name = row.get("decomp_qualname", "")
        if row.get("tier") not in {"A", "B"}:
            continue
        for block, (cls, _) in TEMPLATES.items():
            if name.startswith(cls + "::get"):
                address = int(row["rotwk_rva"], 16) + base
                named.setdefault(address, set()).add((block, name))
    return named


def merge_extra_inventory(blocks, digest, extra):
    """Validate supplemental CFGs and keep main blocks for duplicate entries."""
    if extra.get("sha256") != digest or extra.get("missing_seeds"):
        raise ValueError("extra inventory does not match game or has missing seeds")
    extra_blocks = {f["address"]: tuple(tuple(span) for span in f["blocks"]) for f in extra["functions"]}
    if not extra_blocks or len(extra_blocks) != len(extra["functions"]):
        raise ValueError("extra function inventory is empty or has duplicate entries")
    merged = dict(blocks)
    for address, spans in extra_blocks.items():
        merged.setdefault(address, spans)
    return merged


def linked_array_reads(data, base, blocks, calls, array_pointers, indexed_fields, indexed_bases):
    """Follow up to two direct receiver hops and retain every origin chain."""
    # Follow at most two observed direct calls/tail jumps carrying an array
    # singleton in ECX. Preserve each call chain as a hint, not proof of type.
    frontier = {}
    for caller, callsite, target, pointer, path in calls:
        if pointer in array_pointers and not path:
            frontier.setdefault((target, pointer), set()).add(((caller, callsite),))
    origins, linked_by_field = {}, {}
    for _ in range(2):
        if not frontier:
            break
        receivers = {}
        for (target, pointer), chains in frontier.items():
            origins.setdefault((target, pointer), set()).update(chains)
            receivers.setdefault(target, set()).add((pointer, ()))
        next_calls = set()
        linked = br.global_reads(data, base, {entry: blocks[entry] for entry in receivers}, set(),
                                 indexed_fields=indexed_fields, initial_receivers=receivers,
                                 calls_out=next_calls, call_targets=blocks, indexed_bases=indexed_bases)
        for function, pointer, path, offset, instruction in linked:
            linked_by_field.setdefault((pointer, path, offset), set()).add((function, instruction))
        next_frontier = {}
        for caller, callsite, target, pointer, path in next_calls:
            if path:
                continue
            for chain in frontier.get((caller, pointer), ()):
                next_frontier.setdefault((target, pointer), set()).add(chain + ((caller, callsite),))
        frontier = next_frontier
    return origins, linked_by_field


def top_level_candidates(game, inventory, xmap=None, src=Path("engine/src"), extra_inventory=None, audit_rows=None):
    import json
    import pefile
    import hashlib
    record = json.loads(inventory.read_text(encoding="utf-8"))
    if record.get("sha256") != hashlib.sha256(game.read_bytes()).hexdigest():
        raise ValueError("inventory does not match game")
    if record.get("missing_seeds"):
        print(f"warning: inventory misses {len(record['missing_seeds'])} seeds")
    blocks = {f["address"]: tuple(tuple(span) for span in f["blocks"]) for f in record["functions"]}
    if not blocks or len(blocks) != len(record["functions"]):
        raise ValueError("function inventory is empty or has duplicate entries")
    if extra_inventory:
        extra = json.loads(extra_inventory.read_text(encoding="utf-8"))
        blocks = merge_extra_inventory(blocks, record["sha256"], extra)
    if not src.is_dir():
        raise ValueError(f"source directory not found: {src}")
    pe = pefile.PE(str(game), fast_load=True)
    data, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    citations = rm.scan_citations(src)
    static_fields = {block: br.field_table(data, base, table) for block, (_, _, table) in STATIC_SINGLETONS.items()}
    static_bases = {address: set(static_fields[block].values())
                    for block, (_, addresses, _) in STATIC_SINGLETONS.items() for address in addresses}
    array_fields = {block: br.field_table(data, base, table)
                    for block, (_, _, table, _, _, _) in SINGLETON_ARRAYS.items()}
    indexed_fields, indexed_bases = {}, {}
    for block, (_, pointer, _, start, stride, _) in SINGLETON_ARRAYS.items():
        indexed_fields.setdefault(pointer, set()).update((start + offset, stride) for offset in array_fields[block].values())
        indexed_bases.setdefault(pointer, set()).add((start, stride))
    calls = set()
    skipped = [] if record.get("missing_seeds") else None
    matches = br.global_reads(data, base, blocks, {v[1] for v in SINGLETONS.values()} | {v[1] for v in SINGLETON_ARRAYS.values()},
                              {(v[1], v[2]) for v in SINGLETONS.values() if v[2] and isinstance(v[2][-1], int)},
                              {(0xDE42FC, (), 0x13C)}, skipped, static_bases=static_bases,
                              subobject_offsets={SINGLETONS[b][1]: {offset} for b, offset in BASE_OFFSETS.items()},
                              indexed_fields=indexed_fields, calls_out=calls, indexed_bases=indexed_bases)
    if skipped:
        print(f"warning: {len(skipped)} functions with mis-split blocks left out")
        blocks = {f: spans for f, spans in blocks.items() if f not in set(skipped)}
        calls = {c for c in calls if c[0] in blocks and c[2] in blocks}
    by_field = {}
    for function, pointer, path, offset, instruction in matches:
        by_field.setdefault((pointer, path, offset), set()).add((function, instruction))
    origins, linked_by_field = linked_array_reads(
        data, base, blocks, calls, {entry[1] for entry in SINGLETON_ARRAYS.values()}, indexed_fields, indexed_bases)
    output = {}
    if audit_rows is not None:
        output.update(inferred_candidates(pe, data, base, blocks, audit_rows, citations,
                                          set(SINGLETONS) | set(STATIC_SINGLETONS) | set(SINGLETON_ARRAYS) | set(TEMPLATES)))
    for block, (name, pointer, path, table, end) in SINGLETONS.items():
        for field, table_offset in br.field_table(data, base, table, end).items():
            offset = singleton_offset(block, table_offset)
            route = f"global {name} +0x{offset:X}"
            candidates = sorted(by_field.get((pointer, path, offset), ()))
            note = ", ".join(f"0x{function:08X} ({citations.get(function, 'none')}; read 0x{instruction:08X})"
                             for function, instruction in candidates)
            output[(block, field)] = (route, note or "no candidate in recovered basic blocks; not a no-reader verdict")
    for block, (name, pointer, table, start, stride, count) in SINGLETON_ARRAYS.items():
        for field, field_offset in array_fields[block].items():
            route = f"global {name}[0..{count-1}] +0x{start + field_offset:X} stride 0x{stride:X}"
            candidates = {(function, instruction, str(i)) for i in range(count)
                          for function, instruction in by_field.get((pointer, (), start + i * stride + field_offset), ())}
            candidates.update((function, instruction, 'dynamic, bounds unproven')
                              for function, instruction in by_field.get((pointer, ('indexed', stride), start + field_offset), ()))
            notes = [f"0x{function:08X} ({citations.get(function, 'none')}; read 0x{instruction:08X}; index {i})"
                     for function, instruction, i in sorted(candidates)]
            linked_candidates = {(function, instruction, str(i)) for i in range(count)
                                 for function, instruction in linked_by_field.get((pointer, (), start + i * stride + field_offset), ())}
            linked_candidates.update((function, instruction, 'dynamic, bounds unproven')
                                     for function, instruction in linked_by_field.get((pointer, ('indexed', stride), start + field_offset), ()))
            for function, instruction, i in sorted(linked_candidates):
                sites = ", ".join(" -> ".join(f"0x{caller:08X}/0x{site:08X}" for caller, site in chain)
                                  for chain in sorted(origins[(function, pointer)]))
                notes.append(f"0x{function:08X} ({citations.get(function, 'none')}; read 0x{instruction:08X}; index {i}; via {sites})")
            output[(block, field)] = (route, ", ".join(notes) or "no candidate; indirect calls and paths over two call hops not resolved")
    for block, (name, addresses, _) in STATIC_SINGLETONS.items():
        for field, offset in static_fields[block].items():
            suffix = f"[0..{len(addresses)-1}]" if len(addresses) > 1 else ""
            route = f"global {name}{suffix} +0x{offset:X}"
            candidates = sorted((function, instruction, i) for i, address in enumerate(addresses)
                                for function, instruction in by_field.get((address, ('static',), offset), ()))
            note = ", ".join(f"0x{function:08X} ({citations.get(function, 'none')}; read 0x{instruction:08X}"
                             + (f"; index {i}" if len(addresses) > 1 else "") + ")"
                             for function, instruction, i in candidates)
            output[(block, field)] = (route, note or "no candidate in recovered basic blocks; not a no-reader verdict")
    if xmap is not None:
        with xmap.open(encoding="utf-8", newline="") as stream:
            named = named_template_getters(stream, base)
        offsets = br.getter_offsets(data, base, blocks, named)
        graph, _ = rm.decode_block_inventory(data, base, blocks)
        callers = br.getter_callers(graph, set(named))
        for block, (_, table) in TEMPLATES.items():
            for field, offset in br.field_table(data, base, table).items():
                matching = sorted((addr, name) for addr, aliases in named.items()
                                  for owner, name in aliases
                                  if owner == block and offset in offsets.get(addr, ()))
                if matching:
                    route = "; ".join(f"getter 0x{addr:08X} {name}" for addr, name in matching)
                    candidates = sorted({(caller, addr) for addr, _ in matching for caller in callers[addr]})
                    note = ", ".join(f"0x{caller:08X} ({citations.get(caller, 'none')}; getter 0x{getter:08X})"
                                     for caller, getter in candidates)
                    output[(block, field)] = (route, note or "getter has no direct caller in recovered inventory")
                else:
                    output[(block, field)] = ("getter", "no named getter candidate in entry blocks; not a no-reader verdict")
    return output


def coverage_by_block(rows):
    """Count proven routes, inferred instances and missing concrete routes separately."""
    from collections import Counter, defaultdict
    counts = defaultdict(Counter)
    for row in rows:
        block, note, route = row[1], row[11], row[12]
        counts[block]['total'] += 1
        if route == 'module':
            counts[block]['module'] += 1
        elif route.startswith('global '):
            counts[block]['global'] += 1
        elif route.startswith('getter '):
            counts[block]['getter'] += 1
        elif route.startswith('inferred '):
            counts[block]['inferred'] += 1
        elif route.startswith('table '):
            counts[block]['table'] += 1
            counts[block]['unrouted'] += 1
        else:
            counts[block]['unrouted'] += 1
        if route.startswith(('global ', 'getter ', 'inferred ')) and note.startswith('0x'):
            counts[block]['candidates'] += 1
    return counts


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--audit", required=True, type=Path)
    ap.add_argument("--map", type=Path, default=Path("docs/ennorath/RW_FUNCTION_MAP.tsv"))
    ap.add_argument("--inventory", type=Path, default=Path("workspace/rebuild/rw_function_blocks.json"))
    ap.add_argument("--game", type=Path)
    ap.add_argument("--src", type=Path, default=Path("engine/src"), help="source tree for exact RW citation status")
    ap.add_argument("--xmap", type=Path, help="rw_map.csv from Port 240; enables named template getters")
    ap.add_argument("--extra-inventory", type=Path, help="additional SHA-verified CFG inventory from extra_inventory.py")
    ap.add_argument("--out", type=Path, default=Path("docs/ennorath/FIELD_GAPS.tsv"))
    a = ap.parse_args(argv)
    with a.audit.open(encoding="utf-8", newline="") as stream:
        audit_rows = list(csv.DictReader(stream, delimiter="\t"))
    gaps, classes, reach = mg.read_map(a.map)
    sizes, skip = mg.load_filters(a.inventory, a.game)
    if (a.xmap or a.extra_inventory) and not a.game:
        ap.error("--xmap and --extra-inventory require --game")
    candidates = top_level_candidates(a.game, a.inventory, a.xmap, a.src, a.extra_inventory, audit_rows) if a.game else {}
    rows = join(audit_rows, gaps, classes, reach, sizes, skip, candidates)
    write(rows, a.out)
    in_map = sum(1 for r in rows if r[7] == "yes")
    with_candidates = sum(1 for r in rows if r[7] == "yes" and r[9] not in ("", "0"))
    counts = coverage_by_block(rows)
    print(f"{len(rows)} unused fields, {in_map} of a module class in the map, {with_candidates} with module candidates -> {a.out}"
          + ("" if skip else " (without --game: boilerplate not filtered)"))
    for block, count in sorted(counts.items()):
        print(f"  {block}: {count['total']} total, global {count['global']}, getter {count['getter']}, "
              f"module {count['module']}, inferred {count['inferred']}, no route {count['unrouted']} "
              f"(table only {count['table']}), top-level candidates {count['candidates']}")


if __name__ == "__main__":
    main()

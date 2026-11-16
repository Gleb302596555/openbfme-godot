#!/usr/bin/env python3
"""Join the button matrix's failing rows with the RotWK function map (audit task 2 follow-up, owner 2026-11-16).

For every failing row of docs/ennorath/MATRIX.tsv (any result but PASS, STANCE, DISABLED, INVENTORY) the tool reads the button's
CommandButton (its SpecialPower and Upgrade names), finds the modules of the pressed object and of the objects its contain modules
name (a horde's members) whose fields name that power or upgrade, and lists, for each such module class, the RotWK functions the map
(docs/ennorath/RW_FUNCTION_MAP.tsv, tools/mod/rw_function_map.py) marks `none` or `partial`. The data come from the mounted archives
(tools/mod/ennorath_graph.py: the clean 2.01 base, the mod over it); only names, counts and RW addresses are written.

    matrix_gaps.py --base <ROTWK_INSTALL> --overlay <mod install> [--matrix docs/ennorath/MATRIX.tsv]
                   [--map docs/ennorath/RW_FUNCTION_MAP.tsv] [--out docs/ennorath/MATRIX_GAPS]

Limits (said in the report too): a button whose command names no power or upgrade (formation, weapon toggle, stance, plain FIRE_WEAPON)
links no module; a link is a name match in the module's fields, not a proof that this module fails; the map's status is exact-citation
(a port without an RW citation reads `none`). The join points at where to look first; the failure's cause is still traced per row.
"""
from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_graph as eg  # noqa: E402

GREEN = {"PASS", "STANCE", "DISABLED", "INVENTORY"}
MODULE_KEYS = {"behavior", "body", "draw", "clientupdate", "clientbehavior"}
TOP_FUNCTIONS = 8
# a function reached from at most this many module classes is specific to them; the rest are shared helpers (base-class slots,
# allocation, strings) that read `none` in every class for want of a citation and would bury the class's own gaps
SPECIFIC_CLASSES = 3
# with the map's function inventory (rw_function_map.py --inventory), a function of at most this many bytes is trivial: a class-name
# getter (mov eax, <string>; ret), a one-field setter. The port has them implicitly, without a citation; they are not listed as gaps
TRIVIAL_BYTES = 16  # 16: an interface cast thunk (lea eax, [ecx - k]; neg; sbb; and; ret) is 13 bytes
# with --game: per-class boilerplate every module has and the port writes without a citation, recognised by its code
NAMEKEY_GENERATOR = 0x5487EC   # NameKeyGenerator::nameToKey: a short function that calls it is getModuleNameKey (a static key of the class name)
NAMEKEY_MAX_BYTES = 80
XFER_VERSION_SLOT = 0x28       # Xfer vslot xferVersion: a function whose first indirect call is it is the module's xfer
OPERATOR_DELETE = 0x42F6A0     # a short function that calls operator delete is a scalar deleting destructor (call the destructor, free on flag 1)
DELETING_DTOR_MAX_BYTES = 40


def boilerplate(game: Path, sizes: dict[int, int], blocks: dict[int, list]) -> dict[int, str]:
    """function address -> 'namekey' / 'xfer' for the functions of the inventory that are such boilerplate."""
    import pefile
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM
    pe = pefile.PE(str(game), fast_load=True)
    image, base = pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase
    cs = Cs(CS_ARCH_X86, CS_MODE_32)
    cs.detail = True
    out = {}
    for address, spans in blocks.items():
        first_indirect = None
        calls_namekey = calls_delete = False
        first = last = None
        for start, size in sorted(spans):
            for ins in cs.disasm(image[start - base:start - base + size], start):
                first = first or ins
                last = ins
                if ins.mnemonic != "call":
                    continue
                op = ins.operands[0]
                if op.type == X86_OP_IMM and op.imm == NAMEKEY_GENERATOR:
                    calls_namekey = True
                elif op.type == X86_OP_IMM and op.imm == OPERATOR_DELETE:
                    calls_delete = True
                elif op.type == X86_OP_MEM and first_indirect is None:
                    first_indirect = op.mem.disp
        if calls_namekey and sizes.get(address, 0) <= NAMEKEY_MAX_BYTES:
            out[address] = "namekey"
        elif first_indirect == XFER_VERSION_SLOT:
            out[address] = "xfer"
        elif calls_delete and sizes.get(address, 0) <= DELETING_DTOR_MAX_BYTES:
            out[address] = "deleting destructor"
        elif first is not None and is_vtable_store(first) and last is not None and last.mnemonic == "jmp" \
                and last.operands and last.operands[0].type == X86_OP_IMM and sizes.get(address, 0) <= DTOR_MAX_BYTES:
            out[address] = "destructor"  # mov [ecx], <vtable> ... jmp <the base destructor>: the vtable resets of a class destructor
    return out


DTOR_MAX_BYTES = 64


def is_vtable_store(ins) -> bool:
    """`mov dword ptr [ecx], imm32`: a destructor's (or constructor's) first store of the class's vtable."""
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_ECX
    if ins.mnemonic != "mov" or len(ins.operands) != 2:
        return False
    dst, src = ins.operands
    return dst.type == X86_OP_MEM and dst.mem.base == X86_REG_ECX and dst.mem.disp == 0 and src.type == X86_OP_IMM


def read_matrix(path: Path):
    lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")]
    return [r for r in csv.DictReader(lines, delimiter="\t") if r["result"] not in GREEN]


def read_map(path: Path):
    """(module class -> {function address: (status, depth)} for the `none` / `partial` rows (the smallest depth wins),
    the classes of the map, function address -> the number of module classes that reach it)."""
    out: dict[str, dict[int, tuple[str, int]]] = defaultdict(dict)
    classes = set()
    reach: dict[int, set] = defaultdict(set)
    with path.open(encoding="utf-8", newline="") as stream:
        for r in csv.DictReader(stream, delimiter="\t"):
            classes.add(r["module"])
            reach[int(r["function"], 16)].add(r["module"])
            if r["status"] not in ("none", "partial"):
                continue
            address, depth = int(r["function"], 16), int(r["depth"])
            old = out[r["module"]].get(address)
            if old is None or depth < old[1]:
                out[r["module"]][address] = (r["status"], depth)
    return out, classes, {a: len(c) for a, c in reach.items()}


def object_modules(db: eg.Db, obj: str) -> dict[str, str]:
    """module tag -> class of an object's effective modules (`Behavior = Class ModuleTag_X` and the other module keys)."""
    out = {}
    for k, v, _src, m in eg.effective_fields(db, "object", obj):
        toks = v.split()
        if m and k.lower() in MODULE_KEYS and len(toks) >= 2 and toks[-1] == m:
            out[m] = toks[0]
    return out


def module_tokens(db: eg.Db, obj: str, tag: str) -> set[str]:
    return {t.lower() for k, v, _s, m in eg.effective_fields(db, "object", obj) if m == tag for t in eg.tokens_of(v)}


def members(db: eg.Db, obj: str) -> list[str]:
    """The objects a contain module of the object names (a horde's RankInfo / InitialPayload): its members."""
    mods = object_modules(db, obj)
    out = []
    for tag, cls in mods.items():
        if "contain" not in cls.lower():
            continue
        for t in sorted(module_tokens(db, obj, tag)):
            for (typ, name) in ((ty, n) for (ty, n) in db.defs if ty == "object" and n.lower() == t):
                if name != obj and name not in out:
                    out.append(name)
    return out


def button_refs(db: eg.Db, button: str):
    """(command, {lower names of the button's SpecialPower / Upgrade})."""
    lm = eg.leafmap(db, "commandbutton", button)
    command = eg.first(lm, "command").split()[0] if lm.get("command") else ""
    refs = set()
    for key in ("specialpower", "upgrade"):
        for v in lm.get(key, []):
            refs |= {t.lower() for t in eg.tokens_of(v)}
    return command, refs


def linked_modules(db: eg.Db, obj: str, refs: set[str]):
    """[(object, tag, class)] of the object and its members whose module fields name one of `refs`."""
    out = []
    if not refs:
        return out
    for o in [obj] + members(db, obj):
        for tag, cls in object_modules(db, o).items():
            if module_tokens(db, o, tag) & refs:
                out.append((o, tag, cls))
    return out


def class_gaps(fs: dict[int, tuple[str, int]], reach=None, sizes=None, skip=None):
    """(none count, [(depth, address)] of the specific none functions, smallest depth first, partial count) of one class's map rows."""
    reach = reach or {}
    sizes = sizes or {}
    skip = skip or {}
    none = [(d, a) for a, (s, d) in fs.items() if s == "none"]
    specific = sorted((d, a) for d, a in none if reach.get(a, 1) <= SPECIFIC_CLASSES and sizes.get(a, TRIVIAL_BYTES + 1) > TRIVIAL_BYTES
                      and a not in skip)
    return len(none), specific, sum(1 for s, _d in fs.values() if s == "partial")


def format_functions(specific) -> str:
    return " ".join(f"0x{a:08X}@{d}" for d, a in specific[:TOP_FUNCTIONS])


def load_filters(inventory: Path, game: Path | None):
    """(sizes, boilerplate) from rw_function_map.py's function inventory and, with game.dat, the recognised boilerplate."""
    sizes, skip = {}, {}
    if inventory and inventory.exists():
        import json
        functions = json.loads(inventory.read_text(encoding="utf-8"))["functions"]
        sizes = {f["address"]: sum(n for _s, n in f["blocks"]) for f in functions}
        if game:
            skip = boilerplate(game, sizes, {f["address"]: f["blocks"] for f in functions})
    return sizes, skip


def join(db: eg.Db, matrix_rows, gaps, map_classes, reach=None, sizes=None, skip=None):
    reach = reach or {}
    sizes = sizes or {}
    skip = skip or {}
    rows = []
    for r in matrix_rows:
        command, refs = button_refs(db, r["button"]) if r["button"] else ("", set())
        links = linked_modules(db, r["object"], refs)
        base = [r["faction"], r["object"], r["button"], r["result"], command, " ".join(sorted(refs))]
        if not links:
            reason = "no power or upgrade on the button" if not refs else "no module of the object or its members names it"
            rows.append(base + ["", "", "-", "", "", "", reason])
            continue
        for o, tag, cls in links:
            if cls not in map_classes:
                rows.append(base + [o, tag, cls, "", "", "", "class not in the function map"])
                continue
            none, specific, partial = class_gaps(gaps.get(cls, {}), reach, sizes, skip)
            rows.append(base + [o, tag, cls, str(none), str(len(specific)), str(partial), format_functions(specific)])
    return rows


HEADER = ["faction", "object", "button", "result", "command", "power / upgrade", "module object", "module tag", "module class",
          "map none", "specific none", "map partial", "specific none functions (address@depth, smallest depth first) / note", "main class"]


def mark_main(rows):
    """Append the `main class` column: of the classes a failing button links to, the most specific one says "yes".

    One ability runs through a chain of classes (SpecialPowerModule -> UnpauseSpecialPowerUpgrade -> the update that acts), so the shared
    links of the chain would head every ranking. The main class of a button is the class linked to the fewest failing buttons overall,
    then the one with more specific none functions, then by name: no hand list of generic classes."""
    buttons: dict[str, set] = defaultdict(set)
    for x in rows:
        if x[8] not in ("-", ""):
            buttons[x[8]].add((x[1], x[2]))
    choice = {}
    for x in rows:
        if x[8] in ("-", ""):
            continue
        key = (x[1], x[2])
        rank = (len(buttons[x[8]]), -int(x[10] or 0), x[8])
        if key not in choice or rank < choice[key]:
            choice[key] = rank
    return [x + ["yes" if x[8] not in ("-", "") and choice[(x[1], x[2])][2] == x[8] else ""] for x in rows]


def write(rows, out: Path, command_line: str, matrix_rows, trivial_dropped: bool = False, boilerplate_dropped: int = 0):
    tsv, md = out.with_suffix(".tsv"), out.with_suffix(".md")
    rows = sorted(rows, key=lambda x: (x[8] == "-", x[8], x[0], x[1], x[2]))
    with tsv.open("w", encoding="utf-8", newline="\n") as stream:
        w = csv.writer(stream, delimiter="\t", lineterminator="\n")
        w.writerow(HEADER)
        w.writerows(rows)
    by_class: dict[str, set] = defaultdict(set)
    first: dict[str, list[str]] = {}
    unlinked: dict[str, int] = defaultdict(int)
    for x in rows:
        if x[8] == "-":
            unlinked[x[3]] += 1
            continue
        by_class[x[8]].add((x[1], x[2]))
        first.setdefault(x[8], x[9:13])
    failing = len({(r["object"], r["button"], r["result"]) for r in matrix_rows})
    with md.open("w", encoding="utf-8", newline="\n") as s:
        s.write("# Failing matrix buttons and the RotWK functions their modules lack\n\n")
        s.write(f"Command: `{command_line}`\n\n")
        s.write(f"Failing matrix rows: {failing}; joined rows: {len(rows)}; module classes reached: {len(by_class)}.\n\n")
        s.write("A link is a name match: the module's fields name the button's SpecialPower or Upgrade. The map's status is exact RW citation "
                "in engine/src (a port without a citation reads `none`). The join says where to look first, not which function fails: "
                "each row's cause is still traced.\n\n")
        s.write(f"`specific none`: a `none` function reached from at most {SPECIFIC_CLASSES} module classes of the map (the class's own code); "
                "the other `none` rows are shared helpers that lack a citation everywhere. "
                + (f"Functions of at most {TRIVIAL_BYTES} bytes (class-name getters, one-field setters) are left out of it.\n\n" if trivial_dropped
                   else "Without the function inventory, trivial getters are counted too.\n\n"))
        if boilerplate_dropped:
            s.write(f"Recognised by their code with --game and left out of it too ({boilerplate_dropped} functions of the inventory): getModuleNameKey (a short call of "
                    f"NameKeyGenerator RW 0x{NAMEKEY_GENERATOR:X}) and xfer functions (first indirect call Xfer vslot 0x{XFER_VERSION_SLOT:X}, "
                    f"xferVersion), scalar deleting destructors (a short call of operator delete RW 0x{OPERATOR_DELETE:X}) and destructors "
                    f"(a first `mov [ecx], <vtable>` and a tail jump to the base destructor, at most {DTOR_MAX_BYTES} bytes).\n\n")
        main: dict[str, set] = defaultdict(set)
        results: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
        for x in rows:
            if x[13] == "yes" and (x[1], x[2]) not in main[x[8]]:
                main[x[8]].add((x[1], x[2]))
                results[x[8]][x[3]] += 1
        s.write("## Main classes: each failing button once, under its most specific class\n\n"
                "The main class of a button is the class it links to that the fewest failing buttons link to (then: more specific none functions). "
                "The shared links of an ability's chain (SpecialPowerModule, UnpauseSpecialPowerUpgrade, ...) stay in the next table; here a button "
                "counts where its own mechanism lives.\n\n"
                "| class | buttons | results | specific none | first specific none functions | example buttons |\n|---|---:|---|---:|---|---|\n")
        for cls, btns in sorted(main.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            _n, sp, _p, top = first[cls]
            res = ", ".join(f"{k} {v}" for k, v in sorted(results[cls].items(), key=lambda kv: (-kv[1], kv[0])))
            ex = ", ".join(f"{o} {b}" for o, b in sorted(btns)[:3])
            s.write(f"| {cls} | {len(btns)} | {res} | {sp or '-'} | {top or '-'} | {ex} |\n")
        s.write("\n## Module classes by all the failing buttons they touch (shared links included)\n\n"
                "| class | buttons | map none | specific none | map partial | first specific none functions |\n|---|---:|---:|---:|---:|---|\n")
        for cls, btns in sorted(by_class.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            n, sp, p, top = first[cls]
            s.write(f"| {cls} | {len(btns)} | {n or '-'} | {sp or '-'} | {p or '-'} | {top or '-'} |\n")
        s.write("\n## Failing rows without a module link, by result\n\n| result | rows |\n|---|---:|\n")
        for res, n in sorted(unlinked.items(), key=lambda kv: (-kv[1], kv[0])):
            s.write(f"| {res} | {n} |\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", required=True, type=Path)
    ap.add_argument("--overlay", required=True, type=Path)
    ap.add_argument("--matrix", type=Path, default=Path("docs/ennorath/MATRIX.tsv"))
    ap.add_argument("--map", type=Path, default=Path("docs/ennorath/RW_FUNCTION_MAP.tsv"))
    ap.add_argument("--out", type=Path, default=Path("docs/ennorath/MATRIX_GAPS"))
    ap.add_argument("--inventory", type=Path, default=Path("workspace/rebuild/rw_function_blocks.json"),
                    help="rw_function_map.py's function inventory: functions of at most %d bytes are not listed as gaps" % TRIVIAL_BYTES)
    ap.add_argument("--game", type=Path, help="RotWK 2.01 game.dat: with the inventory, getModuleNameKey and xfer functions are not listed as gaps")
    a = ap.parse_args(argv)
    db = eg.load_db(eg.mount(a.base, a.overlay))
    if eg.MISSING:
        raise SystemExit(f"{len(set(eg.MISSING))} missing INI includes, first: {sorted(set(eg.MISSING))[0]}")
    matrix_rows = read_matrix(a.matrix)
    gaps, classes, reach = read_map(a.map)
    sizes, skip = load_filters(a.inventory, a.game)
    rows = mark_main(join(db, matrix_rows, gaps, classes, reach, sizes, skip))
    shown =["tools/mod/matrix_gaps.py", "--base", "<ROTWK_INSTALL>", "--overlay", "<mod install>",
             "--matrix", str(a.matrix).replace("\\", "/"), "--map", str(a.map).replace("\\", "/"), "--out", str(a.out).replace("\\", "/")]
    shown += ["--inventory", "<rw_function_blocks.json>"] if sizes else []
    shown += ["--game", "<game.dat>"] if skip else []
    write(rows, a.out, " ".join(shown), matrix_rows, bool(sizes), len(skip))
    print(f"{len(matrix_rows)} failing matrix rows, {len(rows)} joined rows -> {a.out.with_suffix('.tsv')}, {a.out.with_suffix('.md')}")


if __name__ == "__main__":
    main()

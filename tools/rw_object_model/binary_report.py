"""Reports of the whole-image map (binary_map.py + class_map.py): docs/ennorath/RW_BINARY_MAP.{tsv,md}, RW_CLASSES.tsv,
RW_VCALLS.tsv. Addresses, counts and statuses only: no game bytes."""
from __future__ import annotations

import sys
from collections import Counter, defaultdict
from pathlib import Path


def load_xmap(path):
    """tools/re/match_rotwk.py's rw_map.csv over this inventory: {va: (name, subsystem, tier)}; {} when absent."""
    import csv
    if not path or not Path(path).exists():
        return {}
    out = {}
    with open(path, encoding="utf-8", newline="") as fh:
        for r in csv.DictReader(fh):
            va = 0x400000 + int(r["rotwk_rva"], 16)
            out[va] = ((r.get("decomp_name") or r.get("rotwk_name") or "").strip(), (r.get("subsystem") or "").strip(),
                       (r.get("tier") or "").strip())
    return out


def write_reports(b, rec, m, ty, vc, out_dir: Path, src: Path = Path("engine/src"), xmap_path=None):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "mod"))
    from rw_function_map import scan_citations
    from rw_library_functions import load as load_library

    cites = scan_citations(src)
    library = set(load_library())
    tier = {int(k, 16): v for k, v in rec["tier"].items()}
    callers = Counter()
    for fn in rec["functions"]:
        for c in set(fn["calls"]) | set(fn["tails"]):
            callers[c] += 1
    resolved_in = Counter()
    for f, site, targets, why, obj, disp in vc:
        for x in targets or ():
            resolved_in[x] += 1
    table_owner = {}
    for c, lay in m.layout.items():
        for k, v in lay.items():
            if k != "conflicts":
                table_owner.setdefault(v, c)

    xmap = load_xmap(xmap_path)
    # the instruction right after `mov eax, handler; call _EH_prolog` was a function start in the old angr inventory: a citation of
    # it is a citation of the function (8 such in engine/src on 2026-11-23)
    seh_after = {}
    for fn in rec["functions"]:
        a = fn["address"]
        i = b.insn(a)
        j = b.insn(a + i.size) if i is not None and i.mnemonic == "mov" else None
        if j is not None and j.mnemonic == "call" and j.operands and j.operands[0].imm == 0xA3CEF0:
            seh_after[a] = a + i.size + j.size

    def status(a):
        if a in cites:
            return cites[a]
        if a in seh_after and seh_after[a] in cites:
            return cites[seh_after[a]]
        if a in library or xmap.get(a, ("", "", ""))[1] == "library":
            return "library"
        return "none"

    out_dir.mkdir(parents=True, exist_ok=True)
    counts = Counter()
    by_sub = defaultdict(Counter)
    rows = ["function\tsize\tevidence\tstatus\tclass\tslot\tdirect callers\tresolved virtual callers\tsubsystem\tBFME2 name\tmatch tier"]
    for fn in rec["functions"]:
        a = fn["address"]
        st = status(a)
        counts[st] += 1
        cls = slot = ""
        slots = sorted(m.slot_of.get(a, ()))
        if slots:
            v, n = slots[0]
            owner = table_owner.get(v)
            cls, slot = (f"0x{owner:X}" if owner else ""), f"0x{v:X}[{n}]"
        elif a in m.class_of_fn:
            cls, slot = f"0x{m.class_of_fn[a]:X}", "ctor/dtor"
        name, sub, mt = xmap.get(a, ("", "", ""))
        by_sub[sub.rstrip("?").strip() or "unclassified"][st] += 1
        rows.append(f"0x{a:08X}\t{fn['size']}\t{tier.get(a, '')}\t{st}\t{cls}\t{slot}\t{callers.get(a, 0)}\t{resolved_in.get(a, 0)}\t{sub}\t{name}\t{mt}")
    (out_dir / "RW_BINARY_MAP.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    rows = ["class\ttables (offset:vtable)\tctors/dtors\tbases (offset:class)\tdescendants\tvirtual methods\tported\tnone"]
    for c in sorted(m.layout):
        lay = {k: v for k, v in m.layout[c].items() if k != "conflicts"}
        meth = set()
        for v in lay.values():
            meth.update(m.vtables.get(v, ()))
        st = [status(x) for x in meth]
        rows.append("\t".join([
            f"0x{c:X}",
            " ".join(f"{k:#x}:0x{v:X}" for k, v in sorted(lay.items())),
            " ".join(f"0x{x:X}" for x in sorted(m.cdtors[c])),
            " ".join(f"{k:#x}:0x{p:X}" for k, p in sorted(m.parents.get(c, ()))),
            str(len(m.descendants(c)) - 1), str(len(meth)),
            str(st.count("ported") + st.count("partial")), str(st.count("none"))]))
    (out_dir / "RW_CLASSES.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    vcount = Counter()
    rows = ["function\tsite\tslot offset\tstatus\ttargets\treceiver"]
    for f, site, targets, why, obj, disp in sorted(vc, key=lambda x: x[1]):
        kind = why.split(";")[0]
        vcount[kind if kind in ("resolved", "object type unproven", "receiver not a vtable load") else "table / slot mismatch"] += 1
        tg = " ".join(f"0x{x:X}" for x in targets) if targets else ""
        rows.append(f"0x{f:08X}\t0x{site:08X}\t{'' if disp is None else hex(disp)}\t{why}\t{tg}\t{expr(obj)}")
    (out_dir / "RW_VCALLS.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    total = len(rec["functions"])
    ev = Counter(tier.get(fn["address"], "") for fn in rec["functions"])
    md = [
        "# RotWK whole-image map (tools/rw_object_model/binary_map.py, class_map.py)",
        "",
        f"game.dat SHA256 `{rec['sha256'][:16]}…`. Addresses, counts and statuses only: no game bytes.",
        "",
        "## Functions",
        "",
        f"{total} functions (evidence: {', '.join(f'{k} {v}' for k, v in sorted(ev.items()))}). Status by `RW 0x…` citations in engine/src: "
        f"ported {counts['ported']}, partial {counts['partial']}, library {counts['library']}, none {counts['none']}.",
        "`library`: the proven list (tools/mod/rw_library_functions.tsv) and the functions match_rotwk.py labels `library` (see below).",
        "`none` means no `RW 0x...` citation: the client side (draw, audio, UI, network) is rebuilt on Godot rather than ported line by line, so there `none` is not a to-do count.",
        "",
        "Exact by construction: an entry has code evidence (the PE entry, a direct call, a slot of a table code installs) or is a data /",
        "code pointer that decodes cleanly and owns no other function's instruction; a switch is followed only when its `cmp` / `ja` guard",
        "proves the bound; every function is decoded again with the final entry set, so the result does not depend on discovery order.",
        f"Unresolved indirect jumps: {sum(len(f['unresolved']) for f in rec['functions'])}; rejected candidates: {len(rec['rejected_candidates'])}; "
        f"call targets that do not decode: {len(rec['call_target_errors'])}.",
        "",
        "## Classes",
        "",
        f"{len(m.vtables)} installed vtables, {len(m.layout)} classes (keyed by the table their constructors / destructors install on",
        f"this + 0), {len(m.parents)} with a proven base (a base constructor / destructor called on the same this, or on this + k for a",
        "secondary base whose table the class also installs).",
        "",
        "## Virtual calls",
        "",
        f"{len(vc)} `call [R + d]` sites: " + ", ".join(f"{k} {v}" for k, v in sorted(vcount.items())) + ".",
        "A call is resolved only when its receiver's type is proven: a method's this (the class and its descendants), an object a",
        "constructor or `new` in the same function built, a global / field whose every store holds a proven type, a return value or an",
        "argument when all the function's call sites are known. A store through an unproven pointer is not counted (no alias analysis).",
        "",
        "## By subsystem",
        "",
        "The label of tools/re/match_rotwk.py (RotWK paired with BFME2 1.06 and its decomp) over this inventory, `?` labels folded in;",
        "`library` there is code the port replaces wholesale." if xmap else "No match file given (--xmap): every function is `unclassified`.",
        "",
        "| subsystem | functions | ported | partial | library | none |",
        "|---|---:|---:|---:|---:|---:|",
        *[f"| {s} | {sum(c.values())} | {c['ported']} | {c['partial']} | {c['library']} | {c['none']} |"
          for s, c in sorted(by_sub.items(), key=lambda kv: -sum(kv[1].values()))],
        "",
        "Tables: RW_BINARY_MAP.tsv (one row per function), RW_CLASSES.tsv, RW_VCALLS.tsv.",
        "",
        "## Limits (what the map does not know yet)",
        "",
        f"- Virtual calls: {vcount['object type unproven']} of {len(vc)} have a receiver whose type is not proven, most often an argument of a",
        "  virtual method (its callers are not all known) or a global written by code the inventory does not cover;",
        f"  {vcount['receiver not a vtable load']} call through a pointer that is not a table load; {vcount['table / slot mismatch']} name a table or slot",
        "  the model does not have at that subobject. `resolved virtual callers` is therefore a lower bound: 0 does not mean nobody calls it.",
        f"- Subsystem: {sum(by_sub['unclassified'].values())} functions have no subsystem (the BFME2 pairing found no partner or no label);",
        "  the `?` labels are the pairing's guesses.",
        "- Status is by citation only: a function ported without a `RW 0x...` citation is `none`, a citation inside a body does not count.",
        "- Refreshed by every maps run (tools/colab/nightly_maps.sh, binary_map()): the inventory and facts are cached on the Drive by",
        "  game.dat's SHA256 and the tools' blob ids; the port status and the BFME2 pairing are recomputed each run.",
        "- Users: the task dossier (tools/ci/port_dossier.py, section «Карта бинарника») and its disassembly (refs/arena/inventory holds this",
        "  inventory's exact bounds since 2026-11-24).",
        "",
    ]
    (out_dir / "RW_BINARY_MAP.md").write_text("\n".join(md), encoding="utf-8")
    return counts, vcount


def expr(v, depth=0):
    if v is None:
        return "?"
    if depth > 4:
        return "…"
    k = v[0]
    if k == "ld":
        return f"[{expr(v[1], depth + 1)}+{v[2]:#x}]" if v[1] is not None else f"[0x{v[2]:X}]"
    if k == "add":
        return f"{expr(v[1], depth + 1)}+{v[2]:#x}"
    if k == "ret":
        return f"ret(0x{v[1]:X})"
    if k == "vret":
        return f"vret(0x{v[1]:X})"
    if k == "imm":
        return f"0x{v[1]:X}"
    if k == "arg":
        return f"arg{v[1]}"
    if k == "in":
        return f"in(0x{v[1]:X}:{expr(v[2], depth + 1)})"
    return k

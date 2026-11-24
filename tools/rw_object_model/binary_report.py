"""Reports of the whole-image map (binary_map.py + class_map.py): docs/ennorath/RW_BINARY_MAP.{tsv,md}, RW_CLASSES.tsv,
RW_VCALLS.tsv. Addresses, counts and statuses only: no game bytes."""
from __future__ import annotations

import csv
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

# User-mode 32-bit MSVC C++ code in game.dat never executes ring-0 / 16-bit / BCD instructions (verified 0 occurrences
# across all 43,168 call/vtable-slot functions). Their presence inside an uncalled `evidence == "data"` entry proves the
# pointer is non-code data (e.g. a printable string or integer in .rdata whose bytes happen to fall inside .text).
IMPOSSIBLE_X86_MNEMONICS = frozenset({
    "aaa", "aad", "aam", "aas", "arpl", "bound", "cli", "clts", "daa", "das", "enter", "icebp",
    "insb", "insd", "insw", "int", "invd", "invlpg", "iret", "iretd", "lcall", "lds", "les",
    "lgdt", "lidt", "ljmp", "lldt", "ltr", "out", "outsb", "outsd", "outsw", "rdmsr", "retf",
    "rsm", "salc", "sgdt", "sidt", "sti", "sysenter", "sysexit", "ud2", "wait", "wbinvd", "wrmsr",
})
_SDD_MNEMONICS = frozenset({"push", "pop", "mov", "call", "test", "je", "ret", "leave", "sub", "add", "lea"})
_MSVC_EH_MAGICS = frozenset({0x19930520, 0x19930521, 0x19930522})


def load_xmap(path):
    """tools/re/match_rotwk.py's rw_map.csv over this inventory: {va: (name, subsystem, tier)}; {} when absent."""
    if not path or not Path(path).exists():
        return {}
    out = {}
    with open(path, encoding="utf-8", newline="") as fh:
        for r in csv.DictReader(fh):
            va = 0x400000 + int(r["rotwk_rva"], 16)
            out[va] = ((r.get("decomp_name") or r.get("rotwk_name") or "").strip(), (r.get("subsystem") or "").strip(),
                       (r.get("tier") or "").strip())
    return out


def _va_in_image(b, va: int, size: int = 1) -> bool:
    off = va - b.base
    return 0 <= off and off + size <= len(b.image)


def _va_in_text(b, va: int) -> bool:
    return b.text[0] <= va < b.text[1]


def _parse_funcinfo_unwinds(b, fi_va: int) -> list[int] | None:
    """If fi_va points to a valid MSVC C++ EH FuncInfo struct, return its non-zero unwind funclet VAs; else None."""
    if not _va_in_image(b, fi_va, 12):
        return None
    magic, max_state, unwind_map_va = struct.unpack_from("<III", b.image, fi_va - b.base)
    if magic not in _MSVC_EH_MAGICS or max_state > 500:
        return None
    if max_state == 0:
        return []
    if not _va_in_image(b, unwind_map_va, max_state * 8):
        return None
    out = []
    um_off = unwind_map_va - b.base
    for s in range(max_state):
        to_state, action_va = struct.unpack_from("<iI", b.image, um_off + s * 8)
        if to_state < -1 or to_state >= max_state:
            return None
        if action_va != 0:
            if not _va_in_text(b, action_va):
                return None
            out.append(action_va)
    return out


def proven_seh_glue(b, rec) -> set[int]:
    """Return VAs of proven MSVC C++ EH compiler glue: __ehhandler$ stubs and FuncInfo::pUnwindMap funclets.

    Derived strictly from the PE image's own _EH_prolog / __CxxFrameHandler / FuncInfo tables (magic 0x19930520..22).
    Catch handlers are intentionally NOT included because a catch block can contain game logic.
    """
    handlers: set[int] = set()
    by_target: dict[int, list[tuple[int, list[int]]]] = defaultdict(list)
    for fn in rec["functions"]:
        a = fn["address"]
        if fn["size"] != 10 or not _va_in_image(b, a, 10):
            continue
        off = a - b.base
        if b.image[off] == 0xB8 and b.image[off + 5] == 0xE9:
            fi_va = struct.unpack_from("<I", b.image, off + 1)[0]
            jrel = struct.unpack_from("<i", b.image, off + 6)[0]
            jtgt = (a + 10 + jrel) & 0xFFFFFFFF
            unwinds = _parse_funcinfo_unwinds(b, fi_va)
            if unwinds is not None:
                by_target[jtgt].append((a, unwinds))

    prolog_handlers: set[int] = set()
    for fn in rec["functions"]:
        a = fn["address"]
        if not _va_in_image(b, a, 10):
            continue
        off = a - b.base
        if b.image[off] == 0xB8 and b.image[off + 5] == 0xE8:
            hva = struct.unpack_from("<I", b.image, off + 1)[0]
            if _va_in_text(b, hva) and _va_in_image(b, hva, 10):
                hoff = hva - b.base
                if b.image[hoff] == 0xB8 and b.image[hoff + 5] == 0xE9:
                    fi_va = struct.unpack_from("<I", b.image, hoff + 1)[0]
                    if _parse_funcinfo_unwinds(b, fi_va) is not None:
                        prolog_handlers.add(hva)

    unwinds_all: set[int] = set()
    for jtgt, items in by_target.items():
        if len(items) >= 2 or any(h in prolog_handlers for h, _ in items):
            for h, uws in items:
                handlers.add(h)
                unwinds_all.update(uws)
    for h in prolog_handlers:
        if h not in handlers:
            hoff = h - b.base
            fi_va = struct.unpack_from("<I", b.image, hoff + 1)[0]
            uws = _parse_funcinfo_unwinds(b, fi_va)
            if uws is not None:
                handlers.add(h)
                unwinds_all.update(uws)
    return handlers | unwinds_all


def proven_deleting_dtors(b, rec, xmap: dict) -> set[int]:
    """Return VAs of proven MSVC scalar deleting destructor wrappers (`call ~Class; test [esp/ebp+..], 1; je; call delete; ret 4`).

    Only marks the compiler-generated wrapper when (a) the wrapped ~Class() destructor is itself a separate function in
    the inventory (so real destructor logic is never hidden) and (b) the second callee is a proven delete/free function.
    """
    entries = {fn["address"] for fn in rec["functions"]}
    candidates: list[tuple[int, int, int]] = []
    del_counts: Counter[int] = Counter()
    for fn in rec["functions"]:
        a, sz = fn["address"], fn["size"]
        if sz < 18 or sz > 44 or not _va_in_image(b, a, sz):
            continue
        off = a - b.base
        insns = list(b.md.disasm(b.image[off:off + sz], a))
        if len(insns) < 7 or len(insns) > 15:
            continue
        if insns[-1].mnemonic != "ret" or insns[-1].op_str != "4":
            continue
        if any(i.mnemonic not in _SDD_MNEMONICS for i in insns):
            continue
        calls = [i for i in insns if i.mnemonic == "call" and i.op_str.startswith("0x")]
        if len(calls) != 2:
            continue
        has_test1 = any(
            i.mnemonic == "test" and i.op_str.endswith(", 1") and ("esp" in i.op_str or "ebp" in i.op_str)
            for i in insns
        )
        if not has_test1:
            continue
        dtor_va = int(calls[0].op_str, 16)
        del_va = int(calls[1].op_str, 16)
        if dtor_va not in entries or dtor_va == a:
            continue
        candidates.append((a, dtor_va, del_va))
        del_counts[del_va] += 1

    proven_del = {
        dv for dv, cnt in del_counts.items()
        if cnt >= 10 or xmap.get(dv, ("", "", ""))[0].startswith(("??3", "_GdipFree"))
    }
    return {a for a, _dtor, dv in candidates if dv in proven_del}


def proven_non_code(b, rec, tier: dict[int, str], callers: Counter, resolved_in: Counter, slot_of: dict) -> set[int]:
    """Return VAs of proven phantom `evidence == "data"` entries that are non-code data pointers.

    Requires 0 direct callers, 0 resolved virtual callers, no vtable slot, and either an initial `int3` padding byte or an
    impossible user-mode x86 instruction (ring-0 / 16-bit / BCD) within the decoded entry block.
    """
    out: set[int] = set()
    for fn in rec["functions"]:
        a = fn["address"]
        if tier.get(a, "") != "data":
            continue
        if callers[a] != 0 or resolved_in[a] != 0 or slot_of.get(a):
            continue
        sz = min(fn["size"], 64)
        if sz <= 0 or not _va_in_image(b, a, sz):
            continue
        off = a - b.base
        insns = list(b.md.disasm(b.image[off:off + sz], a))
        if not insns or insns[0].mnemonic == "int3" or any(i.mnemonic in IMPOSSIBLE_X86_MNEMONICS for i in insns):
            out.add(a)
    return out


def load_not_reached_verdicts(verdicts_dir: Path | None) -> set[int]:
    """Load proven `not reached` verdicts from docs/ennorath/rw_verdicts.d/*.tsv.

    `too big` and `unchecked` rows are intentionally ignored so real gaps stay `none` (open).
    """
    out: set[int] = set()
    if not verdicts_dir or not verdicts_dir.is_dir():
        return out
    for p in sorted(verdicts_dir.glob("*.tsv")):
        with p.open(encoding="utf-8", newline="") as fh:
            for r in csv.DictReader(fh, delimiter="\t"):
                if (r.get("verdict") or "").strip().lower() == "not reached" and (r.get("evidence") or "").strip():
                    fn = (r.get("function") or "").strip()
                    if fn:
                        try:
                            out.add(int(fn, 16))
                        except ValueError:
                            pass
    return out


def write_reports(b, rec, m, ty, vc, out_dir: Path, src: Path = Path("engine/src"), xmap_path=None,
                  verdicts_dir: Path | None = None):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "mod"))
    from rw_function_map import scan_citations
    from rw_library_functions import load as load_library

    cites = scan_citations(src) if src and Path(src).exists() else {}
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

    eh_glue = proven_seh_glue(b, rec)
    deleting_dtors = proven_deleting_dtors(b, rec, xmap)
    non_code = proven_non_code(b, rec, tier, callers, resolved_in, m.slot_of)
    if verdicts_dir is None:
        default_vdir = out_dir / "rw_verdicts.d"
        verdicts_dir = default_vdir if default_vdir.is_dir() else None
    not_reached = load_not_reached_verdicts(verdicts_dir)

    def status(a):
        if a in cites:
            return cites[a]
        if a in seh_after and seh_after[a] in cites:
            return cites[seh_after[a]]
        if a in library or xmap.get(a, ("", "", ""))[1] == "library":
            return "library"
        if a in eh_glue and callers[a] == 0 and resolved_in[a] == 0 and not m.slot_of.get(a):
            return "eh-glue"
        if a in deleting_dtors:
            return "compiler-glue"
        if a in non_code:
            return "non-code"
        if a in not_reached:
            return "not-reached"
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
        by_sub[(sub or "unclassified").rstrip("?")][st] += 1
        rows.append(f"0x{a:08X}\t{fn['size']}\t{tier.get(a, '')}\t{st}\t{cls}\t{slot}\t{callers.get(a, 0)}\t{resolved_in.get(a, 0)}\t{sub}\t{name}\t{mt}")
    (out_dir / "RW_BINARY_MAP.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    rows = ["class\ttables\tctors / dtors\tbases\tdescendants\tvirtual methods\tcited\tuncited"]
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
        rows.append("\t".join([f"0x{f:08X}", f"0x{site:08X}", "" if disp is None else f"{disp:#x}", why, tg, expr(obj)]))
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
        f"ported {counts['ported']}, partial {counts['partial']}, library {counts['library']}, "
        f"eh-glue {counts['eh-glue']}, compiler-glue {counts['compiler-glue']}, non-code {counts['non-code']}, "
        f"not-reached {counts['not-reached']}, none {counts['none']}.",
        "`library`: the proven list (tools/mod/rw_library_functions.tsv) and the functions match_rotwk.py labels `library` (see below).",
        "`eh-glue`: proven MSVC C++ EH compiler glue (__ehhandler$ stubs and FuncInfo::pUnwindMap funclets with 0 callers and no vtable slot).",
        "`compiler-glue`: proven MSVC scalar deleting destructor wrappers whose wrapped ~Class() destructor is tracked separately.",
        "`non-code`: proven phantom `evidence=data` entries with 0 callers, no vtable slot, and impossible user-mode x86 opcodes or initial `int3` padding.",
        "`not-reached`: proven unreachable in `docs/ennorath/rw_verdicts.d/*.tsv` (with evidence; `too big` and `unchecked` remain `none`).",
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
        f"{len(vc)} `call [R + d]` sites: {', '.join(f'{k} {v}' for k, v in vcount.most_common())}.",
        "A call is resolved only when its receiver's type is proven: a method's this (the class and its descendants), an object a",
        "constructor or `new` in the same function built, a global / field whose every store holds a proven type, a return value or an",
        "argument when all the function's call sites are known. A store through an unproven pointer is not counted (no alias analysis).",
        "",
        "## By subsystem",
        "",
        "The label of tools/re/match_rotwk.py (RotWK paired with BFME2 1.06 and its decomp) over this inventory, `?` labels folded in;",
        "`library` there is code the port replaces wholesale." if xmap else "No match file given (--xmap): every function is `unclassified`.",
        "",
        "| subsystem | functions | ported | partial | library | glue/non-code | not-reached | none |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for sub, cs in sorted(by_sub.items(), key=lambda kv: -sum(kv[1].values())):
        glue = cs["eh-glue"] + cs["compiler-glue"] + cs["non-code"]
        md.append(f"| {sub} | {sum(cs.values())} | {cs['ported']} | {cs['partial']} | {cs['library']} | {glue} | {cs['not-reached']} | {cs['none']} |")
    md += [
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
        "- Status is by citation in comments only (string literals such as stop descriptions are ignored): a function ported without a",
        "  `RW 0x...` citation is `none`, a citation inside a body does not count.",
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

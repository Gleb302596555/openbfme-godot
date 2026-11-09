#!/usr/bin/env python3
"""Which GameData rows does RotWK 2.01 read, and where? A pass over game.dat for the GameData block.

    gamedata_rows.py --game <game.dat> [--ini <merged INI dir or zip>] [--port <GameLogicSettings.cpp>] --out <dir>

Inputs and outputs are data of two kinds only:
  * `game.dat` reads: the GameData field table (RW 0xBFF580, rows of 16 bytes {name, parser, userData, offset}) and
    the reader search below. No game byte is copied into the output.
  * `--ini` only names the mod's GameData keys (the values are not read or written).
Nothing from the game or the mod is written anywhere but the caller's --out directory.

The reader search, per row member offset `off` (`GameData + off`, the object the global RW 0xDE4364 points at):
  GD  a register loaded from `[0xDE4364]` inside the same function (a `call` clears the tracing: a reader that keeps
      the pointer across a call is not seen, and is reported as "no in-function reader", never as "no reader");
  ABS a direct absolute read of `[0xDE4364 + off]` (the compiler emits these for members below 0x1254);
  ANY every instruction of `.text` whose memory operand has the same displacement, whatever the base register is:
      the count and the first addresses let a human see whether the offset is too common to attribute (the tool
      never attributes these, the class of the base is not proven).
Rows are printed with the table row address, the parser and the offsets; the output is a TSV plus a text report.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import zipfile
from pathlib import Path

import capstone
import pefile

GD_GLOBAL = 0xDE4364          # TheWritableGlobalData, RW 0xDE4364 (see docs/ennorath/arena-triage/batch-21.md)
GD_TABLE = 0xBFF580           # the GameData field table, RW 0xBFF580
GD_TABLE_END = 0xC01210
ROW = 16
GP = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")


def read_cstr(image: bytes, va: int, base: int) -> str:
    off = va - base
    end = image.index(b"\0", off)
    return image[off:end].decode("latin-1")


def game_table(game: Path):
    pe = pefile.PE(str(game), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    rows = []
    va = GD_TABLE
    while va < GD_TABLE_END:
        off = va - base
        name_p, parser, user, member = (int.from_bytes(image[off + i:off + i + 4], "little") for i in (0, 4, 8, 12))
        if name_p == 0:
            break
        if not (base <= name_p < base + len(image)):
            raise SystemExit(f"row RW 0x{va:X}: name pointer 0x{name_p:X} outside the image")
        rows.append({"row": va, "name": read_cstr(image, name_p, base), "parser": parser,
                     "userData": user, "off": member})
        va += ROW
    return pe, base, image, rows


def scan(pe, base: int, image: bytes, offsets: set[int], limit: int = 8):
    """reader hits per member offset, three kinds (GD / ABS / ANY)."""
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    tva, tsize = base + text.VirtualAddress, text.Misc_VirtualSize
    code = image[text.VirtualAddress:text.VirtualAddress + tsize]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    hits: dict[int, dict[str, list]] = collections.defaultdict(lambda: {"GD": [], "ABS": [], "ANY": []})
    any_count = collections.Counter()
    fn_start = tva
    gd = {}          # register -> member delta (its value is &GameData[d])
    last_was_end = False
    for ins in md.disasm(code, tva):
        if last_was_end and ins.address > fn_start:
            fn_start = ins.address
        last_was_end = False
        ops = ins.operands
        wregs = set()
        for op in ops:
            if op.type == capstone.x86.X86_OP_REG:
                if op.access & capstone.CS_AC_WRITE:
                    wregs.add(ins.reg_name(op.reg))
        # direct absolute member reads: mov reg, [0xDE4364 + off]
        for op in ops:
            if op.type != capstone.x86.X86_OP_MEM:
                continue
            m = op.mem
            if m.base == 0 and m.index == 0:
                addr = m.disp
                if addr == GD_GLOBAL:
                    wregs.add("__loadgd__")
                else:
                    d = addr - GD_GLOBAL
                    if d in offsets and d >= 0:
                        hits[d]["ABS"].append((ins.address, fn_start, f"{ins.mnemonic} {ins.op_str}"))
        # memory operand through a traced register
        for op in ops:
            if op.type != capstone.x86.X86_OP_MEM:
                continue
            m = op.mem
            if m.base == 0:
                continue
            bname = ins.reg_name(m.base)
            if bname in gd:
                d = gd[bname] + m.disp
                if d in offsets:
                    kind = "GD"
                    hits[d][kind].append((ins.address, fn_start, f"{ins.mnemonic} {ins.op_str}"))
        # every same-displacement read, base unproven
        for op in ops:
            if op.type == capstone.x86.X86_OP_MEM and op.mem.disp in offsets and op.access & capstone.CS_AC_READ:
                if ins.address not in [h[0] for h in hits[op.mem.disp]["ANY"][:limit]]:
                    any_count[op.mem.disp] += 1
                    if len(hits[op.mem.disp]["ANY"]) < limit:
                        hits[op.mem.disp]["ANY"].append((ins.address, fn_start, f"{ins.mnemonic} {ins.op_str}"))
        # update the tracing: a new GD load, a lea of a member, a clobber
        if "__loadgd__" in wregs:
            wregs.discard("__loadgd__")
            for op in ops:
                if op.type == capstone.x86.X86_OP_REG and op.access & capstone.CS_AC_WRITE:
                    gd[ins.reg_name(op.reg)] = 0
        elif ins.mnemonic == "lea" and len(ops) == 2 and ops[1].type == capstone.x86.X86_OP_MEM:
            dst, m = ops[0], ops[1].mem
            if dst.type == capstone.x86.X86_OP_REG:
                dname = ins.reg_name(dst.reg)
                if m.base and ins.reg_name(m.base) in gd:
                    gd[dname] = gd[ins.reg_name(m.base)] + m.disp
                else:
                    gd.pop(dname, None)
        for r in wregs:
            if r in gd and r not in ("__loadgd__",):
                gd.pop(r, None)
        if ins.mnemonic == "call":
            gd.clear()
        if ins.mnemonic in ("ret", "retf", "int3"):
            gd.clear()
            last_was_end = True
    for d in offsets:
        hits[d]["ANY_COUNT"] = any_count.get(d, 0)  # type: ignore[assignment]
    return hits


def ini_gamedata_keys(path: Path):
    """the GameData block's keys of a merged INI tree or zip; values are never stored."""
    def blocks(text: str, name: str):
        keys = []
        inside = False
        for line in text.splitlines():
            s = line.split(";")[0].strip()
            if not s:
                continue
            if not inside:
                if re.match(rf"^{name}\b", s, re.I):
                    inside = True
                continue
            if re.match(r"^End\b", s, re.I):
                inside = False
                continue
            m = re.match(r"^([A-Za-z][A-Za-z0-9_]*)\s*=", s)
            if m:
                keys.append(m.group(1))
        return keys

    found = {}
    files = []
    if path.is_dir():
        files = [p for p in path.rglob("*.ini")]
    else:
        z = zipfile.ZipFile(path)
        files = [n for n in z.namelist() if n.lower().endswith(".ini")]
    for f in files:
        try:
            text = (Path(f).read_text("latin-1") if path.is_dir() else zipfile.ZipFile(path).read(f).decode("latin-1"))
        except Exception:
            continue
        for block in ("GameData",):
            for k in blocks(text, block):
                found.setdefault(k, set()).add(str(f).replace("\\", "/").lower())
    return found


def port_rows(src: Path):
    text = src.read_text("utf-8", "replace")
    m = re.search(r"kGameDataFields\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        raise SystemExit("kGameDataFields not found in the port source")
    return [r for r in re.findall(r'\{\s*"([A-Za-z0-9_]+)"\s*,', m.group(1))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--ini")
    ap.add_argument("--port", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    pe, base, image, rows = game_table(Path(a.game))
    by_name = {r["name"]: r for r in rows}
    port = port_rows(Path(a.port))
    mod = ini_gamedata_keys(Path(a.ini)) if a.ini else {}
    # the rows this task is about: the mod's GameData keys the port has no table row for (S-152's catch-all population)
    unported = sorted(k for k in mod if k not in port)
    not_in_rotwk = [k for k in unported if k not in by_name]
    offsets = {by_name[k]["off"] for k in unported if k in by_name}
    print(f"game table rows {len(rows)}; port rows {len(port)}; mod GameData keys {len(mod)}; "
          f"keys without a port row {len(unported)} (of them {len(not_in_rotwk)} have no RotWK row either)")
    hits = scan(pe, base, image, offsets, limit=10) if offsets else {}
    lines = ["field\trow\toff\tparser\tmod_files\tGD\tn_GD\tABS\tn_ABS\tANY\tn_ANY\treaders"]
    for k in unported:
        r = by_name.get(k)
        if not r:
            lines.append(f"{k}\t-\t-\t-\t{','.join(sorted(mod[k]))[:120]}\t-\t0\t-\t0\t-\t0\tNO ROTWK ROW")
            continue
        h = hits[r["off"]]
        rd = "; ".join(f"{kind}@{addr:08X} fn {fn:08X} {asm}" for kind in ("GD", "ABS")
                       for addr, fn, asm in h[kind][:10])
        lines.append(f"{k}\t0x{r['row']:X}\t0x{r['off']:X}\t0x{r['parser']:X}\t{','.join(sorted(mod[k]))[:120]}\t"
                     f"{bool(h['GD'])}\t{len(h['GD'])}\t{bool(h['ABS'])}\t{len(h['ABS'])}\t-\t{h['ANY_COUNT']}\t{rd}")
    (out / "gamedata-rows.tsv").write_text("\n".join(lines) + "\n", "utf-8")
    (out / "gamedata-table.json").write_text(json.dumps(
        {"rows": rows, "port": port, "unported": unported, "notInRotwk": not_in_rotwk}, indent=0), "utf-8")
    print("wrote", out / "gamedata-rows.tsv", out / "gamedata-table.json")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Read the GameData field table out of a RotWK game.dat, and list the rows a port does not have.

    gamedata_rows.py --game <game.dat> [--port <GameLogicSettings.cpp>] [--ini <merged INI dir or zip>] --out <dir>

What it proves, and what it does not:
  * the GameData table RW 0xBFF580 (rows of 16 bytes {name, parser, userData, member offset}, end RW 0xC01210) is read
    straight out of the image here, so every row address a verdict page cites can be re-checked on a machine that has
    the game; a name pointer outside the image, a row that does not end before RW 0xC01210 or an empty name stops it;
  * the port's own rows are taken from `kGameDataFields` in the port source; the difference is the set of GameData
    keys the mod writes and the port does not have (stop S-152's catch-all population), with the mod's key names only
    (no value of the mod is read or written).
It does NOT attribute readers. A reader is a specific instruction off `[0xDE4364]` inside a named function; a scan
that guesses the base register from a same-function load is wrong across calls, and a same-displacement hit on an
unnamed base proves nothing (docs/ennorath/arena-triage/README.md, "Уроки" 1 and 3). The verdicts in
docs/ennorath/gamedata-rows.md are cited instruction by instruction; this tool is the table's re-check.
Nothing from the game or the mod is written anywhere but --out: the TSV lists names, offsets and addresses.
"""
from __future__ import annotations

import argparse
import json
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
    ap.add_argument("--port", required=True)
    ap.add_argument("--ini")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    pe, base, image, rows = game_table(Path(a.game))
    by_name = {r["name"]: r for r in rows}
    port = port_rows(Path(a.port))
    mod = ini_gamedata_keys(Path(a.ini)) if a.ini else {}
    missing_in_game = [n for n in port if n not in by_name]
    unported = sorted(k for k in mod if k not in port)
    not_in_rotwk = [k for k in unported if k not in by_name]
    print(f"game table rows {len(rows)} (0x{rows[0]['row']:X} .. 0x{rows[-1]['row']:X}); "
          f"port rows {len(port)}; mod GameData keys {len(mod)}; mod keys without a port row {len(unported)}, "
          f"of them with no RotWK row either {len(not_in_rotwk)}")
    if missing_in_game:
        print("port rows RotWK's table does not have:", ", ".join(sorted(missing_in_game)))
    lines = ["name\trow\toff\tparser\tport_row\tmod_key"]
    for r in rows:
        lines.append(f"{r['name']}\t0x{r['row']:X}\t0x{r['off']:X}\t0x{r['parser']:X}\t"
                     f"{'row' if r['name'] in port else '-'}\t{'key' if r['name'] in mod else '-'}")
    (out / "gamedata-rows.tsv").write_text("\n".join(lines) + "\n", "utf-8")
    (out / "gamedata-unported.tsv").write_text(
        "name\trow\toff\tparser\tnote\n" + "".join(
            (f"{k}\t0x{by_name[k]['row']:X}\t0x{by_name[k]['off']:X}\t0x{by_name[k]['parser']:X}\tmod key, no port row\n"
             if k in by_name else f"{k}\t-\t-\t-\tmod key, no RotWK row (RotWK cannot parse it)\n")
            for k in unported), "utf-8")
    (out / "gamedata-table.json").write_text(json.dumps(
        {"rows": rows, "port": port, "modKeys": sorted(mod), "unported": unported, "notInRotwk": not_in_rotwk}, indent=0), "utf-8")
    print("wrote", out / "gamedata-rows.tsv", out / "gamedata-unported.tsv", out / "gamedata-table.json")


if __name__ == "__main__":
    main()

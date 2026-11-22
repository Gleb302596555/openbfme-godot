"""Build a function inventory (rva,size,start_block_size,name CSV) from function entry points.

Both binaries get the same extent rule so masked hashes compare like with like: a function runs
from its entry to the next known entry in the same section, minus trailing int3 (0xCC) padding.
MSVC 7.1 places switch jump tables right after the code of their function, so the extent keeps
them (the masking in relib zeroes their absolute addresses); Ghidra's own body would drop them.

Entry sources (any number, unioned):
  --ghidra-url URL --program NAME   a GhidraMCP headless server (read-only /list_functions_enhanced)
  --ghidra-json PATH                a saved /list_functions_enhanced response
  --csv PATH[:rva_col[:name_col]]   any CSV with an RVA hex column (or a VA column named address), e.g. Open-BFME-2's
                                    reverse/ghidra_functions.csv (rva,name) or its ledger
                                    reverse/functions.csv (target_rva,name)
  --angr PATH                       a tools/mod/rw_function_map.py angr cache (rw_function_blocks.json):
                                    entries and extents from its CFGFast basic blocks, no Ghidra needed

Names: the first source that names an entry wins (pass the most authoritative first). The angr
cache carries no names, so its entries take names from the other sources.
No retail bytes are written: only addresses, sizes and names.

Extents -- the entry rule vs the angr blocks. The entry rule sizes an entry from its entry to the
next one, minus int3 padding. An --angr function is sized from its blocks instead (the cache
records VAs): size is the total addresses in the blocks (a non-contiguous body does not cover
its gaps) and start_block_size the contiguous run of blocks that starts at the entry -- the same
columns ghidra/export_inventory.java writes from Ghidra bodies. An entry with an angr body uses
its blocks (measured beats the rule); the other entries keep the entry rule. What changes
against a pure entry-rule build: switch jump tables and trailing padding that the "to the next
entry, minus int3" extent kept after the code are no longer inside the function, so they leave
the masked hash (tier A can fall where a pair's hashed bytes used to cover a trailing table),
and where angr splits or merges differently than Ghidra the extents follow angr's blocks. If the
cache records sha256 (rw_function_blocks.json does) it must match --bin.

  python tools/re/inventory.py --bin <game.dat> --ghidra-url http://127.0.0.1:8089 \
      --program rotwk201_game.exe --out rotwk_functions.csv
  python tools/re/inventory.py --bin <game.dat> --angr workspace/rebuild/rw_function_blocks.json \
      --out rotwk_functions.csv
"""
import argparse
import csv
import hashlib
import json
import sys
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import relib  # noqa: E402


def ghidra_entries(data, base):
    out = []
    for f in data["functions"]:
        if f.get("isExternal"):
            continue
        out.append((int(f["address"], 16) - base, f["name"]))
    return out


def fetch_ghidra(url, program):
    q = urllib.parse.urlencode({"offset": 0, "limit": 1000000, "program": program})
    with urllib.request.urlopen(f"{url.rstrip('/')}/list_functions_enhanced?{q}", timeout=600) as r:
        return json.loads(r.read().decode("utf-8"))


def csv_entries(spec, base):
    parts = spec.split(":")
    path = parts[0]
    rva_col = parts[1] if len(parts) > 1 else None
    name_col = parts[2] if len(parts) > 2 else "name"
    out = []
    with open(path, newline="", encoding="utf-8") as fh:
        rd = csv.DictReader(fh)
        col = rva_col or next(c for c in ("rva", "target_rva", "address") if c in rd.fieldnames)
        for row in rd:
            v = row.get(col, "").strip()
            if not v:
                continue
            a = int(v, 16)
            if col == "address":  # VA column; rva / target_rva columns are already RVAs
                a -= base
            out.append((a, row.get(name_col, "") or ""))
    return out


def angr_entries(data, base):
    """rw_function_blocks.json -> {rva: (size, start_block_size)}, extents from the blocks.

    The cache records VAs (tools/mod/rw_function_map.py): every function lists its CFGFast
    basic blocks as [va, size] pairs. size counts the addresses in the blocks; start_block_size
    is the contiguous run of blocks that starts at the entry, the only part guaranteed to be
    disassembled in one piece. Broken records (no blocks, no block at the entry, overlapping or
    empty blocks, duplicate entries) are errors, not repairs.
    """
    out = {}
    for f in data["functions"]:
        va0 = int(f["address"])
        blocks = sorted((int(va) - base, int(sz)) for va, sz in f["blocks"])
        if not blocks:
            raise ValueError(f"function 0x{va0:X}: no blocks")
        rva = va0 - base
        if rva in out:
            raise ValueError(f"duplicate function entry 0x{va0:X}")
        start, size = None, 0
        for i, (va, sz) in enumerate(blocks):
            if sz <= 0:
                raise ValueError(f"function 0x{va0:X}: block 0x{va + base:X} has size {sz}")
            if i and va < blocks[i - 1][0] + blocks[i - 1][1]:
                raise ValueError(f"function 0x{va0:X}: overlapping block 0x{va + base:X}")
            size += sz
            if va == rva:
                start = i
        if start is None:
            raise ValueError(f"function 0x{va0:X}: no block at its entry")
        run = blocks[start][1]
        cursor = start
        while cursor + 1 < len(blocks) and blocks[cursor + 1][0] == blocks[cursor][0] + blocks[cursor][1]:
            cursor += 1
            run += blocks[cursor][1]
        out[rva] = (size, run)
    return out


def build(img, entries, bodies=None):
    """entries: [(rva, name)] -> sorted [(rva, size, blk, name)] for .text-resident entries.

    Extent of an entry: bodies[rva] = (size, start_block_size) from angr_entries() when it has
    an angr body, else the entry rule (to the next entry, minus int3 padding; start_block_size
    equals size because that run is contiguous by construction).
    """
    names = {}
    for rva, name in entries:
        if rva not in names or (not names[rva] and name) or \
                (names[rva].startswith(("FUN_", "thunk_FUN_")) and name
                 and not name.startswith(("FUN_", "thunk_FUN_"))):
            names[rva] = name
    t0, t1 = img.text
    rvas = sorted(r for r in names if t0 <= r < t1)
    out = []
    for i, rva in enumerate(rvas):
        if bodies and rva in bodies:
            size, blk = bodies[rva]
        else:
            end = rvas[i + 1] if i + 1 < len(rvas) else t1
            while end > rva + 1 and img.mem[end - 1] == 0xCC:
                end -= 1
            size = blk = end - rva
        out.append((rva, size, blk, names[rva].replace(",", ";")))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--ghidra-url")
    ap.add_argument("--program")
    ap.add_argument("--ghidra-json")
    ap.add_argument("--save-json", help="also save the fetched Ghidra response here")
    ap.add_argument("--csv", action="append", default=[])
    ap.add_argument("--angr", help="a tools/mod/rw_function_map.py angr cache (rw_function_blocks.json): "
                                   "entries and extents from its CFGFast blocks")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    img = relib.Image(args.bin)
    entries = []
    bodies = {}
    if args.angr:
        record = json.loads(Path(args.angr).read_text(encoding="utf-8"))
        if record.get("sha256"):
            digest = hashlib.sha256(Path(args.bin).read_bytes()).hexdigest()
            if record["sha256"] != digest:
                raise SystemExit(f"{args.angr}: cache is bound to game.dat sha256 {record['sha256']}, "
                                 f"--bin is {digest}")
        bodies = angr_entries(record, img.base)
        entries += [(rva, "") for rva in sorted(bodies)]
    if args.ghidra_url:
        data = fetch_ghidra(args.ghidra_url, args.program)
        if args.save_json:
            Path(args.save_json).write_text(json.dumps(data), encoding="utf-8")
        entries += ghidra_entries(data, img.base)
    if args.ghidra_json:
        entries += ghidra_entries(json.loads(Path(args.ghidra_json).read_text(encoding="utf-8")), img.base)
    for spec in args.csv:
        entries += csv_entries(spec, img.base)
    if not entries:
        raise SystemExit("no entry points given")
    inv = build(img, entries, bodies)
    with open(args.out, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["rva", "size", "start_block_size", "name"])
        for rva, size, blk, name in inv:
            w.writerow(["0x%X" % rva, size, blk, name])
    print(f"{len(inv)} functions ({sum(f[1] for f in inv)} bytes of .text) -> {args.out}")


if __name__ == "__main__":
    main()

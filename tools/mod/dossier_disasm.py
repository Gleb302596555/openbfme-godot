"""dossier_disasm.py: the disassembly half of a task's dossier (owner 2026-11-19; the maps half is tools/ci/port_dossier.py on GitHub).

For every open Port / Local issue: the RotWK functions it names (`RW 0x...`, each mapped to its function by the whole-binary inventory) and the
depth-0 functions of the module classes it names that the function map still has as not ported (at most MAX_CLASS_FUNCS), disassembled block
by block from game.dat, written as `dossier-<issue>.txt` (rewritten only when it changes; `WROTE <issue>` lines say which). Sandbox 3's loop
(tools/colab/autostart.d/dossier.sh) encrypts a changed one with the arena archive's key and pushes it to `refs/arena/dossier/<issue>`, the
way the game files reach the arena (refs/arena/files): an agent's prompt fetches and decrypts its own. The plain text never goes to GitHub.

    dossier_disasm.py --game <game.dat> --inventory <rw_function_blocks_full.json> --map <RW_FUNCTION_MAP.tsv> --out <for-arena dir>
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import citation_check as cc  # noqa: E402

REPO = "Gleb302596555/openbfme-godot"
CITE = re.compile(r"\bRW\s+0x([0-9A-Fa-f]{5,8})\b")
TASK = re.compile(r"^\s*(Port|Local)[\s-]+\d+", re.I)
MAX_CLASS_FUNCS = 40
MAX_INSTRUCTIONS = 1500  # per function: a giant one is cut, with a note


def gh(*args: str) -> str:
    done = subprocess.run(["gh", *args, "-R", REPO], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if done.returncode:
        raise SystemExit(f"gh {' '.join(args[:2])}: {done.stderr.strip()[:300]}")
    return done.stdout


def disassemble(md, data: bytes, base: int, blocks: list[tuple[int, int]]) -> list[str]:
    out, n = [], 0
    for s, size in sorted(blocks):
        out.append(f"  ; block {s:#08x} ({size} bytes)")
        for ins in md.disasm(data[s - base:s - base + size], s):
            out.append(f"  {ins.address:08X}  {ins.mnemonic:8s} {ins.op_str}")
            n += 1
            if n >= MAX_INSTRUCTIONS:
                out.append(f"  ; ... cut at {MAX_INSTRUCTIONS} instructions")
                return out
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True, type=Path)
    ap.add_argument("--inventory", required=True, type=Path)
    ap.add_argument("--map", required=True, type=Path, help="docs/ennorath/RW_FUNCTION_MAP.tsv")
    ap.add_argument("--out", required=True, type=Path)
    a = ap.parse_args(argv)
    import capstone
    import pefile
    pe = pefile.PE(str(a.game), fast_load=True)
    base, data = pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    functions = json.loads(a.inventory.read_text(encoding="utf-8"))["functions"]
    inv = cc.Inventory(functions)
    blocks = {f["address"]: [tuple(b) for b in f["blocks"]] for f in functions}
    with a.map.open(encoding="utf-8", newline="") as s:
        fmap = list(csv.DictReader(s, delimiter="\t", quoting=csv.QUOTE_NONE))
    known = {r["module"] for r in fmap}
    issues = json.loads(gh("issue", "list", "--state", "open", "--limit", "300", "--json", "number,title,body"))
    written = 0
    for i in issues:
        if not TASK.match(i["title"]):
            continue
        text = i["title"] + "\n" + (i["body"] or "")
        named = {inv.owner(int(x, 16)) for x in CITE.findall(text)} - {None}
        classes = sorted({w for w in re.findall(r"\b[A-Z][A-Za-z0-9]{3,}\b", text) if w in known})
        open_funcs = [int(r["function"], 16) for r in fmap if r["module"] in classes and r["depth"] == "0" and r["status"] != "ported"]
        funcs = sorted(named) + [f for f in open_funcs if f not in named][:MAX_CLASS_FUNCS]
        if not funcs:
            continue
        lines = [f"Dossier of issue #{i['number']}: {i['title']}", "RotWK 2.01 game.dat, the functions the issue names, then the depth-0 functions "
                 f"of its classes ({', '.join(classes) or 'none'}) not ported yet (at most {MAX_CLASS_FUNCS}). Private: never paste it to GitHub.", ""]
        for f in funcs:
            if f not in blocks:
                continue
            lines.append(f"== RW 0x{f:06X}" + (" (named in the issue)" if f in named else ""))
            lines += disassemble(md, data, base, blocks[f])
            lines.append("")
        body = "\n".join(lines)
        path = a.out / f"dossier-{i['number']}.txt"
        if not path.exists() or path.read_text(encoding="utf-8", errors="replace") != body:
            path.write_text(body, encoding="utf-8")
            written += 1
            print(f"WROTE {i['number']}")
    print(f"{written} dossiers written")
    return 0


if __name__ == "__main__":
    sys.exit(main())

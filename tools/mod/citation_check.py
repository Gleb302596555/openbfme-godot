#!/usr/bin/env python3
"""RW citations against the RotWK function inventory (owner 2026-11-16): the function map (tools/mod/rw_function_map.py) counts a function as
ported only when its START is cited (`RW 0x<start>` in engine/src). A citation inside a function whose start nobody cites leaves the function
`none` in the map, in MATRIX_GAPS and FIELD_GAPS although the port has it.

    citation_check.py --diff <file|->  [--inventory <rw_function_blocks.json>] [--src engine/src]   # the added lines of a unified diff
    citation_check.py --tree          [--inventory ...] [--src engine/src]                          # every citation of the tree

Used by tools/precommit.py (a warning, never a failure) and tools/colab/pr_checker.py (a section of the PR comment). The inventory is
rw_function_map.py's angr cache: `workspace/rebuild/rw_function_blocks.json`, or $OPENBFME_RW_INVENTORY; on the Colab sandboxes the copy on
the Drive (`/content/drive/MyDrive/openbfme-audit/rw_function_blocks.json`). Without it nothing is checked and the caller says so.
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
import re
import sys
from pathlib import Path

CITE = re.compile(r"\bRW\s+0x([0-9A-Fa-f]{5,8})\b")
DRIVE_COPY = Path("/content/drive/MyDrive/openbfme-audit/rw_function_blocks.json")


class Inventory:
    """Function starts and the byte ranges of their blocks."""

    def __init__(self, functions: list[dict]):
        self.starts = {f["address"] for f in functions}
        spans = sorted((s, s + n, f["address"]) for f in functions for s, n in f["blocks"])
        self._lo = [s for s, _e, _f in spans]
        self._spans = spans

    @classmethod
    def load(cls, path: Path) -> "Inventory":
        return cls(json.loads(path.read_text(encoding="utf-8"))["functions"])

    def owner(self, address: int) -> int | None:
        """The start of the function whose blocks hold `address` (None: not code of the inventory, e.g. a table or a string)."""
        i = bisect.bisect_right(self._lo, address) - 1
        while i >= 0 and self._spans[i][0] <= address:
            s, e, f = self._spans[i]
            if s <= address < e:
                return f
            i -= 1
            if address - s > 0x10000:
                break
        return None


def find_inventory(repo: Path) -> Path | None:
    for p in (os.environ.get("OPENBFME_RW_INVENTORY"), repo / "workspace/rebuild/rw_function_blocks.json", DRIVE_COPY):
        if p and Path(p).exists():
            return Path(p)
    common = os.popen(f'git -C "{repo}" rev-parse --git-common-dir').read().strip()
    if common:
        main = (repo / common).resolve().parent / "workspace/rebuild/rw_function_blocks.json"
        if main.exists():
            return main
    return None


def cited(src: Path) -> set[int]:
    out = set()
    for root, _dirs, files in os.walk(src):
        for f in files:
            if f.endswith((".cpp", ".h", ".hpp", ".inc", ".inl")):
                text = (Path(root) / f).read_text(encoding="utf-8", errors="replace")
                out |= {int(m, 16) for m in CITE.findall(text)}
    return out


def added_citations(diff: str, prefix: str = "engine/src/") -> list[tuple[str, int, int]]:
    """(path, new line number, address) of the citations on the lines a unified diff adds under `prefix`."""
    out, path, line = [], "", 0
    for raw in diff.splitlines():
        if raw.startswith("+++ "):
            path = raw[4:].split("\t", 1)[0].removeprefix("b/")
        elif raw.startswith("@@"):
            m = re.match(r"@@ -\S+ \+(\d+)", raw)
            line = int(m.group(1)) if m else 0
        elif raw.startswith("+") and not raw.startswith("+++"):
            if path.startswith(prefix):
                out += [(path, line, int(a, 16)) for a in CITE.findall(raw)]
            line += 1
        elif not raw.startswith("-"):
            line += 1
    return out


def interior_warnings(citations, inventory: Inventory, all_cited: set[int]) -> list[str]:
    """A citation inside a function (not its start) whose start is cited nowhere: the map keeps that function `none`."""
    out, said = [], set()
    all_cited = set(all_cited) | {a for _p, _l, a in citations}  # a start the same change cites counts
    for path, line, a in citations:
        if a in inventory.starts:
            continue
        f = inventory.owner(a)
        if f is None or f in all_cited or (f, path) in said:
            continue
        said.add((f, path))
        out.append(f"{path}:{line}: RW 0x{a:X} is inside the RotWK function RW 0x{f:X}, whose start is cited nowhere: cite `RW 0x{f:X}` "
                   f"too (where the function begins), or the function map keeps it `none`")
    return out


def newly_ported(citations, inventory: Inventory, before: set[int]) -> set[int]:
    """Function starts the added citations cite for the first time (they move to `ported` in the function map)."""
    return {a for _p, _l, a in citations if a in inventory.starts and a not in before}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--diff", help="a unified diff file, - for stdin")
    g.add_argument("--tree", action="store_true")
    ap.add_argument("--inventory", type=Path)
    ap.add_argument("--src", type=Path, default=Path("engine/src"))
    a = ap.parse_args(argv)
    inv_path = a.inventory or find_inventory(Path.cwd())
    if not inv_path:
        print("citation check skipped: no RotWK function inventory (workspace/rebuild/rw_function_blocks.json, $OPENBFME_RW_INVENTORY)")
        return 0
    inv = Inventory.load(inv_path)
    all_cited = cited(a.src)
    if a.tree:
        cites = []
        for root, _d, files in os.walk(a.src):
            for f in files:
                if f.endswith((".cpp", ".h", ".hpp", ".inc", ".inl")):
                    p = Path(root) / f
                    for i, text in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                        cites += [(p.as_posix(), i, int(x, 16)) for x in CITE.findall(text)]
    else:
        text = sys.stdin.read() if a.diff == "-" else Path(a.diff).read_text(encoding="utf-8", errors="replace")
        cites = added_citations(text)
    warnings = interior_warnings(cites, inv, all_cited)
    for w in warnings:
        print(w)
    print(f"{len(cites)} citations checked, {len(warnings)} interior citations of an uncited function")
    return 0


if __name__ == "__main__":
    sys.exit(main())

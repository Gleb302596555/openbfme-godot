"""maps_land.py: GitHub Actions lands the maps run's commit (branch auto/maps) onto main after checking it (owner 2026-11-20).

Sandbox 3's maps run (tools/colab/nightly_maps.sh) pushes one commit with the generated tables to auto/maps. Main usually moves during
the run (landings), so the commit is not a fast-forward; this tool puts it on top of the current main itself:

  * every file the commit changes must be one of GENERATED, PROGRESS or MERGED (anything else: refuse, Claude lands it by hand);
  * a GENERATED table is taken whole from auto/maps (the run rewrites it from scratch on the main it ran on);
  * PROGRESS.tsv (docs/ennorath/PROGRESS.tsv, one row per day; tools/land.py writes rows too) keeps main's rows and takes the run's
    rows for the dates the run changed;
  * MERGED files (docs/ennorath/README.md: the run rewrites its counts, people edit its text) go through a three-way merge
    (git merge-file); a conflict refuses;
  * sanity: a table keeps its header line, MATRIX.tsv and RW_FUNCTION_MAP.tsv do not lose more than LOSS of main's rows (a run
    that broke half way writes short tables), no table is empty.

    maps_land.py --head origin/auto/maps --main origin/main [--commit] [--dry-run]

Exit 0: landed (or nothing to land); exit 1: refused, the reasons printed (auto/maps stays for Claude). Without --commit it only checks.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

D = "docs/ennorath/"
GENERATED = {D + n for n in (
    "MATRIX.tsv", "MATRIX_FACTIONS.tsv", "MATRIX_REVIEW.md", "MATRIX_GAPS.tsv", "MATRIX_GAPS.md", "FIELD_GAPS.tsv",
    "RW_FUNCTION_MAP.tsv", "RW_FUNCTION_MAP.md", "SCRIPT_GRANTS.tsv", "SCRIPT_MAP.md", "GRANTS_INI.tsv",
    "MODIFIER_MAP.md", "MODIFIER_READERS.tsv", "MODIFIER_PORT_READS.tsv", "MODIFIER_UNRESOLVED.tsv", "MODIFIER_MATRIX.tsv",
    "MATRIX_GREY.tsv", "MATRIX_GREY.md", "MAP_SWEEP.tsv", "MAP_SWEEP.md")}
PROGRESS = D + "PROGRESS.tsv"
MERGED = {D + "README.md"}
# tables whose row count says whether the run finished: no more than this share of main's rows may go
SIZED = {D + "MATRIX.tsv": 0.10, D + "RW_FUNCTION_MAP.tsv": 0.10}


def git(*args: str, check: bool = True) -> str:
    r = subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} failed: {r.stderr.strip()}")
    return r.stdout


def show(rev: str, path: str) -> str | None:
    r = subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True)
    return r.stdout.decode("utf-8", errors="replace") if r.returncode == 0 else None


def merge_progress(base: str, main: str, head: str) -> str:
    """main's rows, with the rows the run changed (against the run's own base) laid over main's row of the same date, column by
    column name (main may have columns the run's tool did not know yet: they keep main's value); one row per date, by date."""
    def rows(text: str) -> tuple[list[str], dict[str, dict[str, str]]]:
        lines = [l for l in text.splitlines() if l.strip()]
        if not lines:
            return [], {}
        hdr = lines[0].split("\t")
        out = {}
        for l in lines[1:]:
            v = l.split("\t")
            out[v[0]] = dict(zip(hdr, v + [""] * (len(hdr) - len(v))))
        return hdr, out
    hdr_m, m = rows(main)
    _, b = rows(base)
    hdr_h, h = rows(head)
    if not hdr_m or hdr_m[0] != "date" or not hdr_h or hdr_h[0] != "date":
        raise ValueError("PROGRESS.tsv: no date header")
    unknown = [c for c in hdr_h if c not in hdr_m]
    if unknown:
        raise ValueError(f"PROGRESS.tsv: the run writes columns main does not have: {', '.join(unknown)}")
    for day, row in h.items():
        if b.get(day) != row:
            merged = dict(m.get(day, {c: "" for c in hdr_m}))
            merged.update(row)
            m[day] = merged
    return "\n".join(["\t".join(hdr_m), *("\t".join(m[d].get(c, "") for c in hdr_m) for d in sorted(m))]) + "\n"


def merge3(base: str, main: str, head: str) -> str | None:
    with tempfile.TemporaryDirectory() as t:
        p = Path(t)
        for n, s in (("main", main), ("base", base), ("head", head)):
            (p / n).write_text(s, encoding="utf-8", newline="")
        r = subprocess.run(["git", "merge-file", "-p", str(p / "main"), str(p / "base"), str(p / "head")], capture_output=True)
        return r.stdout.decode("utf-8") if r.returncode == 0 else None


def sanity(path: str, main: str | None, head: str) -> list[str]:
    bad = []
    lines = [l for l in head.splitlines() if l.strip()]
    if not lines:
        return [f"{path}: empty"]
    if path.endswith(".tsv") and main:
        mlines = [l for l in main.splitlines() if l.strip()]
        if mlines and mlines[0] != lines[0]:
            bad.append(f"{path}: header changed")
        loss = SIZED.get(path)
        if loss is not None and len(lines) < len(mlines) * (1 - loss):
            bad.append(f"{path}: {len(lines)} rows against main's {len(mlines)} (more than {int(loss * 100)}% lost: an unfinished run?)")
    return bad


def plan(head: str, main: str) -> tuple[dict[str, str], list[str]]:
    """The new contents of each file, and the reasons to refuse."""
    base = git("merge-base", head, main).strip()
    if git("rev-parse", head).strip() == base:
        return {}, []  # already in main
    # the run commits once on the main it checked out; anything else is not the run's commit
    if git("rev-list", "--count", f"{base}..{head}").strip() != "1":
        return {}, [f"{head} is not one commit on top of main (base {base[:9]})"]
    changed = [p for p in git("diff", "--name-only", base, head).splitlines() if p]
    out: dict[str, str] = {}
    why: list[str] = []
    for path in changed:
        h = show(head, path)
        if h is None:
            why.append(f"{path}: deleted by the run")
            continue
        m = show(main, path)
        if path in GENERATED:
            why += sanity(path, m, h)
            out[path] = h
        elif path == PROGRESS:
            try:
                out[path] = merge_progress(show(base, path) or "", m or "", h)
            except ValueError as e:
                why.append(str(e))
        elif path in MERGED:
            r = merge3(show(base, path) or "", m or "", h)
            if r is None:
                why.append(f"{path}: the run's change conflicts with main's")
            else:
                out[path] = r
        else:
            why.append(f"{path}: not a maps file (only the generated tables, PROGRESS.tsv and docs/ennorath/README.md land by themselves)")
    return out, why


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--head", default="origin/auto/maps")
    ap.add_argument("--main", default="origin/main")
    ap.add_argument("--commit", action="store_true", help="write the files on a detached main and commit (the workflow pushes)")
    a = ap.parse_args()
    files, why = plan(a.head, a.main)
    if why:
        print("maps not landed:")
        for w in why:
            print("  - " + w)
        return 1
    if not files:
        print("nothing to land")
        return 0
    differs = [p for p, s in files.items() if show(a.main, p) != s]
    print(f"maps check green: {len(files)} file(s), {len(differs)} differ from main")
    if not a.commit or not differs:
        return 0
    git("checkout", "-q", "--detach", a.main)
    for p in differs:
        Path(p).write_text(files[p], encoding="utf-8", newline="")
        git("add", p)
    subject = git("log", "-1", "--format=%s", a.head).strip()
    git("commit", "-q", "-m", f"{subject} (landed by GitHub Actions)",
        "-m", f"auto/maps {git('rev-parse', '--short', a.head).strip()} put on main {git('rev-parse', '--short', a.main).strip()} "
              "by tools/ci/maps_land.py (.github/workflows/maps-land.yml): generated tables whole, PROGRESS.tsv by date, README three-way.")
    print(git("log", "--oneline", "-1").strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())

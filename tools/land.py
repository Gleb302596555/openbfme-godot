#!/usr/bin/env python3
"""Claude's landing of a tested station-0 batch into main, on the owner's PC (PROCESS 7a, 2026-11-14).

Run from the station-0 tree (workspace/wt/claude-station-0) after the batch was merged, built and tested on Colab and
pushed to origin/agent/claude/station-0, and after the landing's hand-written docs (TOP20, PROBLEMS) are committed:

    python tools/land.py [--no-build] [--no-push] [--audit <field-audit.tsv> --audit-name vN] [--batch <name>]

--batch takes a green batch the batch bot pushed (tools/colab/batch_bot.py, origin/batch/<name>) in place of a hand-made one.

1. fast-forward this tree to origin/agent/claude/station-0 (a local commit not on origin stays: it is pushed in 5);
2. fold the stop / night-watch fragments into the tables (tools/fragments.py collect) and sync README's counts;
3. refuse when README has other than one "open stops" line or STOPS.md repeats an id;
4. write today's row of docs/ennorath/PROGRESS.tsv (tools/mod/progress_snapshot.py);
5. commit what 2-4 changed, push station-0, fast-forward the main checkout and push main;
6. refresh the arena's encrypted archive with the audit the maps run put on the Drive (../DashBoard/scripts/arena_refresh.py, if present);
7. build the game in the main checkout with build.bat (full path; the bare name is not found from Git Bash).
Every step prints what it did; any failure stops before main moves.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

TREE = Path(__file__).resolve().parent.parent
MAIN = TREE.parent.parent.parent if TREE.parent.name == "wt" else None
BRANCH = "agent/claude/station-0"


def run(*cmd: str, cwd: Path = TREE, check: bool = True) -> str:
    print("$", " ".join(cmd))
    done = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (done.stdout + done.stderr).strip()
    if out:
        print(out)
    if check and done.returncode:
        raise SystemExit(f"land: failed ({done.returncode}): {' '.join(cmd)}")
    return done.stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-push", action="store_true", help="stop after the local commit (a dry landing)")
    ap.add_argument("--audit")
    ap.add_argument("--audit-name")
    ap.add_argument("--batch", help="a green batch of tools/colab/batch_bot.py (batch/<name>): station-0 fast-forwards to it first")
    a = ap.parse_args()
    if MAIN is None or not (MAIN / ".git").exists():
        raise SystemExit("land: run from workspace/wt/claude-station-0 of the main checkout")
    if run("git", "branch", "--show-current").strip() != BRANCH:
        raise SystemExit(f"land: this tree is not on {BRANCH}")
    if run("git", "status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("land: commit or drop the tree's changes first")

    run("git", "fetch", "-q", "origin", BRANCH, "main")
    behind = run("git", "rev-list", "--count", f"HEAD..origin/{BRANCH}").strip()
    if behind != "0":
        run("git", "merge", "--ff-only", f"origin/{BRANCH}")
    if a.batch:  # the batch bot's branch starts at origin/main: station-0 must be main or behind it to fast-forward
        name = a.batch if a.batch.startswith("batch/") else f"batch/{a.batch}"
        run("git", "fetch", "-q", "origin", name)
        run("git", "merge", "--ff-only", f"origin/{name}")
    if run("git", "rev-list", "--count", "HEAD..origin/main").strip() != "0":
        raise SystemExit("land: origin/main has commits this tree lacks: merge origin/main on Colab and test first")

    py = sys.executable
    run(py, "tools/fragments.py", "collect")
    run(py, "tools/sync_counts.py")
    readme = (TREE / "README.md").read_text(encoding="utf-8")
    if readme.count(" open stops**") != 1:
        raise SystemExit("land: README.md must have exactly one '**N open stops**' line")
    run(py, "tools/stops_count.py")  # raises on a repeated id or an unknown kind
    snap = [py, "tools/mod/progress_snapshot.py", "--write"]
    if a.audit:
        snap += ["--audit", a.audit, "--audit-name", a.audit_name or "?"]
    run(*snap)

    changed = [l[3:] for l in run("git", "status", "--porcelain").splitlines() if l.strip()]
    if changed:
        paths = ["README.md", "docs/STOPS.md", "docs/stops.d", "docs/ennorath/NIGHT_WATCH.md", "docs/ennorath/night_watch.d",
                 "docs/ennorath/PROGRESS.tsv", "engine/tests/test_module_coverage.cpp"]
        # a fragment directory exists only while it holds files (git keeps no empty directory); a deleted fragment is tracked
        tracked = set(run("git", "ls-files", "--", *paths).splitlines())
        run("git", "add", "-A", "--", *[p for p in paths if (TREE / p).exists() or any(t.startswith(p) for t in tracked)])
        left = run("git", "status", "--porcelain", "--untracked-files=no").splitlines()
        stray = [l for l in left if l[:2] in (" M", " D")]
        if stray:
            raise SystemExit(f"land: unexpected changes outside the landing files: {stray}")
        run("git", "commit", "-q", "-m",
            "Landing: stop / night-watch fragments folded in, README counts, PROGRESS.tsv row (tools/land.py)\n\n"
            "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
    if a.no_push:
        print("land: --no-push, main not moved")
        return 0
    run("git", "push", "-q", "origin", f"HEAD:{BRANCH}")
    if run("git", "branch", "--show-current", cwd=MAIN).strip() != "main":
        raise SystemExit("land: the main checkout is not on main")
    run("git", "merge", "--ff-only", "-q", BRANCH, cwd=MAIN)
    run("git", "push", "-q", "origin", "main", cwd=MAIN)
    print("land: main =", run("git", "log", "--oneline", "-1", cwd=MAIN).strip())
    # owner 2026-11-17: the arena's encrypted archive gets the audit the maps run last put on the Drive (the key stays in the private
    # DashBoard repository on this PC); a failure only warns, main is already pushed
    refresh = MAIN.parent / "DashBoard" / "scripts" / "arena_refresh.py"
    if refresh.exists() and not a.no_push:
        done = subprocess.run([sys.executable, str(refresh), "--fork", str(MAIN)])
        if done.returncode:
            print(f"land: warning: the arena archive was not refreshed ({done.returncode}); run {refresh} by hand")
    if not a.no_build:
        bat = MAIN / "build.bat"
        print("$ build.bat (full path)")
        done = subprocess.run(["cmd", "/c", str(bat)], cwd=MAIN)
        if done.returncode:
            raise SystemExit(f"land: build.bat failed ({done.returncode}); main is already pushed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

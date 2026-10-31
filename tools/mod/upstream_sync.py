#!/usr/bin/env python3
"""How far this fork is from upstream, and which of its engine work upstream could take (docs/ennorath/PROCESS.md, section 12).

    upstream_sync.py status [--no-fetch]       commits each way, files apart, and the conflicts a merge would have now (changes nothing)
    upstream_sync.py candidates [--no-fetch]   engine classes and fixes only this fork has, generic ones first, for a pull request

Reads only; it opens no pull request and pushes nothing. A candidate is a proposal: the owner chooses what goes upstream. `generic` means no file of the
change names Ennorath or depends on the mod's install; `mod-tied` means it does, and needs that part split off first.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
UPSTREAM = "upstream/main"
MOD_TIED = re.compile(r"ennorath|OPENBFME_ENNORATH_INSTALL", re.I)
MODULE = re.compile(r"^engine/src/GameLogic/Module/(\w+)\.cpp$")


def git(*args: str, check: bool = True) -> str:
    done = subprocess.run(["git", "-C", str(REPO), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and done.returncode != 0:
        raise SystemExit(f"git {' '.join(args)}: {done.stderr.strip()}")
    return done.stdout.strip()


def conflicts_from_merge_tree(output: str) -> list[str]:
    """The conflicted paths of `git merge-tree --write-tree --name-only`: the lines between the tree id and the first blank line."""
    lines = output.splitlines()[1:]
    return lines[:lines.index("")] if "" in lines else lines


def status() -> int:
    ahead, behind = git("rev-list", "--left-right", "--count", f"HEAD...{UPSTREAM}").split()
    files = git("diff", "--name-only", UPSTREAM, "HEAD").splitlines()
    print(f"this branch: {ahead} commits upstream lacks, {behind} commits to take; {len(files)} files differ")
    done = subprocess.run(["git", "-C", str(REPO), "merge-tree", "--write-tree", "--name-only", "HEAD", UPSTREAM], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if done.returncode not in (0, 1):
        raise SystemExit(f"git merge-tree: {done.stderr.strip()}")
    conflicts = conflicts_from_merge_tree(done.stdout) if done.returncode == 1 else []
    print("a merge now: " + (f"{len(conflicts)} conflicts:\n  " + "\n  ".join(conflicts) if conflicts else "no conflicts"))
    return 0


def is_generic(paths: list[str], read) -> bool:
    return not any(MOD_TIED.search(path) or MOD_TIED.search(read(path)) for path in paths)


def candidates() -> int:
    added = git("diff", "--name-only", "--diff-filter=A", f"{UPSTREAM}...HEAD").splitlines()
    tests = [p for p in added if p.startswith("engine/tests/")]
    read = lambda path: git("show", f"HEAD:{path}", check=False)
    rows = []
    for path in added:
        found = MODULE.match(path)
        if not found:
            continue
        name = found.group(1)
        related = [path, f"engine/src/GameLogic/Module/{name}.h"]
        bodies = {t: read(t) for t in tests}
        own_tests = [t for t, body in bodies.items() if re.search(rf"\b{name}\b", body)]
        rows.append((is_generic([p for p in related if p in added] + own_tests, read), name, own_tests))
    for generic, name, own_tests in sorted(rows, key=lambda r: (not r[0], r[1])):
        print(f"{'generic ' if generic else 'mod-tied'} {name}: {len(own_tests)} test files ({', '.join(Path(t).name for t in own_tests) or 'none'})")
    print(f"{len(rows)} classes only this fork has. Engine fixes outside new files: `git log {UPSTREAM}..HEAD --no-merges -- engine/src`")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("status", "candidates"))
    parser.add_argument("--no-fetch", action="store_true")
    args = parser.parse_args()
    if not args.no_fetch:
        git("fetch", "upstream", "--prune")
    sys.stdout.reconfigure(encoding="utf-8")
    return status() if args.command == "status" else candidates()


if __name__ == "__main__":
    sys.exit(main())

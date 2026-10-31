#!/usr/bin/env python3
"""One branch and one working tree per agent (docs/ennorath/PROCESS.md, section 4). Work left by an agent that stops stays on its branch.

    agent_worktree.py new <agent> <topic>   branch agent/<agent>/<topic> from main, tree workspace/wt/<agent>-<topic>, godot-cpp, the commit gate
    agent_worktree.py list                  each agent branch: its tree, commits ahead of and behind main, uncommitted files
    agent_worktree.py sync [--tests-exe P]  run inside a tree: merge main in, then regenerate the counts (tools/sync_counts.py)
    agent_worktree.py land <branch>         after the owner's approval: fast-forward main to the branch, then remove its tree and branch
    agent_worktree.py remove <branch>       remove a tree and branch that are already in main; unmerged or dirty work is refused
    agent_worktree.py hook                  install the commit gate (shared by every tree)

A new tree builds on its own (engine/build, godot/bin), so it does not hold another tree's library. `land` refuses anything but a fast-forward
and a clean main checkout: nothing is overwritten and nothing is merged silently.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

NAME = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
GODOT_CPP = "engine/thirdparty/godot-cpp"
HOOK = '#!/bin/sh\n# Installed by tools/agent_worktree.py: the gate of the tree being committed in.\nexec python "$(git rev-parse --show-toplevel)/tools/precommit.py"\n'


def git(*args: str, cwd: Path | None = None, check: bool = True) -> str:
    done = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and done.returncode != 0:
        raise SystemExit(f"git {' '.join(args)}: {done.stderr.strip() or done.stdout.strip()}")
    return done.stdout.strip()


def main_root() -> Path:
    """The main checkout: the parent of the common git directory, from any tree."""
    return Path(git("rev-parse", "--path-format=absolute", "--git-common-dir")).parent


def worktrees(root: Path) -> dict[str, Path]:
    """Branch -> tree, from `git worktree list --porcelain`."""
    out: dict[str, Path] = {}
    path = None
    for line in git("worktree", "list", "--porcelain", cwd=root).splitlines():
        if line.startswith("worktree "):
            path = Path(line[len("worktree "):])
        elif line.startswith("branch refs/heads/") and path:
            out[line[len("branch refs/heads/"):]] = path
    return out


def is_merged(branch: str, root: Path) -> bool:
    return subprocess.run(["git", "merge-base", "--is-ancestor", branch, "main"], cwd=root).returncode == 0


def dirty(tree: Path) -> list[str]:
    return [line for line in git("status", "--porcelain", cwd=tree).splitlines() if line]


def install_hook(root: Path) -> Path:
    hook = root / ".git" / "hooks" / "pre-commit"
    hook.parent.mkdir(parents=True, exist_ok=True)
    if hook.exists() and hook.read_text(encoding="utf-8", errors="replace") != HOOK:
        raise SystemExit(f"{hook} exists with other content: not overwritten")
    hook.write_text(HOOK, encoding="utf-8", newline="\n")
    return hook


def cmd_new(agent: str, topic: str) -> int:
    for part in (agent, topic):
        if not NAME.match(part):
            raise SystemExit(f"{part!r}: use lower-case letters, digits, '.', '_' and '-'")
    root = main_root()
    branch, tree = f"agent/{agent}/{topic}", root / "workspace" / "wt" / f"{agent}-{topic}"
    git("worktree", "add", "-b", branch, str(tree), "main", cwd=root)
    reference = root / GODOT_CPP
    extra = ["--reference", str(reference)] if (reference / "CMakeLists.txt").exists() else []
    git("submodule", "update", "--init", *extra, GODOT_CPP, cwd=tree)
    install_hook(root)
    print(f"branch {branch}\ntree   {tree}\nbuild there with build.bat; commit there; `agent_worktree.py sync`, then ask the owner to land it")
    return 0


def cmd_list() -> int:
    root = main_root()
    trees = worktrees(root)
    for branch in git("for-each-ref", "--format=%(refname:short)", "refs/heads/agent", cwd=root).splitlines():
        ahead, behind = git("rev-list", "--left-right", "--count", f"{branch}...main", cwd=root).split()
        tree = trees.get(branch)
        print(f"{branch}: ahead {ahead}, behind {behind}, " + (f"{len(dirty(tree))} uncommitted, {tree}" if tree else "no tree"))
    return 0


def cmd_sync(tests_exe: str | None) -> int:
    tree = Path(git("rev-parse", "--show-toplevel"))
    if dirty(tree):
        raise SystemExit("uncommitted work: commit it on this branch first")
    done = subprocess.run(["git", "merge", "--no-edit", "main"], cwd=tree, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if done.returncode != 0:
        raise SystemExit(f"merge of main stopped; resolve it here and run sync again:\n{git('status', '--short', cwd=tree)}")
    command = [sys.executable, str(tree / "tools" / "sync_counts.py")] + (["--tests-exe", tests_exe] if tests_exe else [])
    return subprocess.run(command, cwd=tree).returncode


def cmd_land(branch: str) -> int:
    root = main_root()
    if dirty(root):
        raise SystemExit(f"{root} has uncommitted files: main is not moved")
    tree = worktrees(root).get(branch)
    if tree and dirty(tree):
        raise SystemExit(f"{tree} has uncommitted files: nothing is landed")
    git("merge", "--ff-only", branch, cwd=root)
    print(f"main is at {git('rev-parse', '--short', 'main', cwd=root)}")
    return cmd_remove(branch)


def cmd_remove(branch: str) -> int:
    root = main_root()
    if not is_merged(branch, root):
        raise SystemExit(f"{branch} has commits main lacks: not removed")
    tree = worktrees(root).get(branch)
    if tree:
        if dirty(tree):
            raise SystemExit(f"{tree} has uncommitted files: not removed")
        # --force: the initialised submodule makes git refuse a clean tree
        git("worktree", "remove", "--force", str(tree), cwd=root)
    git("branch", "-d", branch, cwd=root)
    print(f"removed {branch}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    new = sub.add_parser("new")
    new.add_argument("agent")
    new.add_argument("topic")
    sub.add_parser("list")
    sync = sub.add_parser("sync")
    sync.add_argument("--tests-exe")
    for name in ("land", "remove"):
        sub.add_parser(name).add_argument("branch")
    sub.add_parser("hook")
    args = parser.parse_args()
    if args.command == "new":
        return cmd_new(args.agent, args.topic)
    if args.command == "list":
        return cmd_list()
    if args.command == "sync":
        return cmd_sync(args.tests_exe)
    if args.command == "land":
        return cmd_land(args.branch)
    if args.command == "remove":
        return cmd_remove(args.branch)
    print(install_hook(main_root()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""The test cases a branch's change can reach, so a branch does not run the whole suite (docs/ennorath/PROCESS.md, section 7).

    test_select.py [--base main] [--run EXE] [--log P]    prints the doctest -tc= filter, or runs it

A changed test file selects its own cases. A changed source or header selects the test files that include it through any chain of headers; a .cpp counts
as its header of the same name, or as every test when it has none and is not a test. A change no test can be traced from (a .gd, CMake, or a wide header
that reaches over half of the test files) answers FULL: run the whole suite. The full suite stays required before landing on main.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ROOTS = ("engine/src", "engine/tests")
SOURCE_SUFFIXES = {".cpp", ".h", ".inl"}
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
CASE = re.compile(r'^\s*TEST_CASE\(\s*"((?:[^"\\]|\\.)*)"', re.M)
WIDE_SHARE = 0.5
MAX_FILTER = 30000
ALWAYS = "module coverage*"
REGISTRATION = "engine/src/GameLogic/Module/LogicModules.cpp"
LANE_LIST = re.compile(r"engine/cmake/Modgap\d*\.cmake")


def scan(repo: Path) -> tuple[dict[str, set[str]], dict[str, list[str]]]:
    """(header -> the files that include it, test file -> its case names); paths are repo-relative with forward slashes."""
    files = [p for root in ROOTS for p in (repo / root).rglob("*") if p.suffix in SOURCE_SUFFIXES]
    known = {p.relative_to(repo).as_posix() for p in files}
    by_name: dict[str, list[str]] = defaultdict(list)
    for name in known:
        by_name[name.rsplit("/", 1)[-1]].append(name)
    includers: dict[str, set[str]] = defaultdict(set)
    cases: dict[str, list[str]] = {}
    for path in files:
        rel = path.relative_to(repo).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for target in INCLUDE.findall(text):
            resolved = resolve(target, rel, known, by_name)
            if resolved:
                includers[resolved].add(rel)
        if rel.startswith("engine/tests/"):
            found = CASE.findall(text)
            if found:
                cases[rel] = found
    return includers, cases


def resolve(target: str, includer: str, known: set[str], by_name: dict[str, list[str]]) -> str | None:
    for root in (includer.rsplit("/", 1)[0], *ROOTS):
        candidate = f"{root}/{target}"
        if candidate in known:
            return candidate
    # a project include path other than the roots: unique names resolve, ambiguous ones do not guess
    same = [n for n in by_name.get(target.rsplit("/", 1)[-1], []) if n.endswith("/" + target)]
    return same[0] if len(same) == 1 else None


def reached_tests(changed: str, includers: dict[str, set[str]], cases: dict[str, list[str]]) -> set[str]:
    seen, stack = {changed}, [changed]
    while stack:
        for includer in includers.get(stack.pop(), ()):
            if includer not in seen:
                seen.add(includer)
                stack.append(includer)
    return {name for name in seen if name in cases}


def mask(case: str) -> str:
    """A doctest mask for a case name: up to the first comma or quote escape, which doctest's list syntax cannot carry."""
    head = re.split(r"[,\\]", case, maxsplit=1)[0].strip()
    return head + ("*" if head != case else "")


def select(changed: list[str], repo: Path) -> tuple[bool, str, list[str]]:
    """(needs the full suite, why, masks)."""
    includers, cases = scan(repo)
    tests: set[str] = set()
    for name in changed:
        path = Path(name)
        if name == REGISTRATION or LANE_LIST.fullmatch(name):
            continue  # every new class edits these; the class's own files and test select what it reaches
        if name in cases:
            tests.add(name)
        elif name.startswith("engine/tests/") and path.suffix in SOURCE_SUFFIXES and not (repo / name).exists():
            continue  # a removed test file
        elif name.startswith(ROOTS) and path.suffix in (".h", ".inl"):
            tests |= reached_tests(name, includers, cases)
        elif name.startswith(ROOTS) and path.suffix == ".cpp":
            header = path.with_suffix(".h").as_posix()
            if not (repo / header).exists():
                return True, f"{name} has no header of its name, no test can be traced from it", []
            tests |= reached_tests(header, includers, cases)
        elif name.startswith(("docs/", "tools/")) or path.suffix == ".md":
            continue  # the engine does not read these; tools/ have their own pytest files
        else:
            return True, f"{name}: no test can be traced from this file", []
    if len(tests) > len(cases) * WIDE_SHARE:
        return True, f"the change reaches {len(tests)} of {len(cases)} test files", []
    masks = sorted({mask(case) for name in tests for case in cases[name]} | {ALWAYS})
    if sum(len(m) + 1 for m in masks) > MAX_FILTER:
        return True, f"{len(masks)} cases do not fit one command line", []
    return False, f"{len(tests)} of {len(cases)} test files", masks


def changed_files(base: str, repo: Path) -> list[str]:
    run = lambda *a: subprocess.run(["git", *a], cwd=repo, capture_output=True, text=True, encoding="utf-8", check=True).stdout.splitlines()
    return sorted(set(run("diff", "--name-only", f"{base}...HEAD")) | set(run("diff", "--name-only", "HEAD")) | set(run("ls-files", "--others", "--exclude-standard")))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", default="main")
    parser.add_argument("--run", help="openbfme_tests.exe: run the selection instead of printing it")
    parser.add_argument("--log", help="with --run: write the output here")
    args = parser.parse_args()
    changed = changed_files(args.base, REPO)
    full, why, masks = select(changed, REPO)
    print(f"{len(changed)} changed files; {'FULL suite: ' if full else ''}{why}", file=sys.stderr)
    filters = [] if full else ["-tc=" + ",".join(masks)]
    if not args.run:
        print("(whole suite)" if full else filters[0])
        return 0
    if args.log:
        with open(args.log, "wb") as out:
            return subprocess.run([args.run, *filters], stdout=out, stderr=subprocess.STDOUT).returncode
    return subprocess.run([args.run, *filters]).returncode


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""The test cases a branch's change can reach, so a branch does not run the whole suite (docs/ennorath/PROCESS.md, section 7).

    test_select.py [--base main] [--run EXE] [--log P]    prints the doctest -tc= filters, or runs them

A changed test file selects its own cases. A changed source or header selects the test files that include it through any chain of headers; a .cpp counts
as its header of the same name, else as the headers it includes from its own folder whose name starts its own (AIUpdateCombat.cpp -> AIUpdate.h). A file under
engine/tests/data selects the test files that name it. A change no test can be traced from (a .gd, CMake, a source with neither header, a data file nothing names,
answers FULL: the whole suite runs before the branch lands (PROCESS.md section 15, rule 8; it may run overnight), --run refuses it unless --full is given.
A wide selection is no FULL: it runs, split into as many -tc= runs as the command line needs (each under MAX_FILTER characters), one after the other.

With the test map (tools/test_coverage_map.py, workspace/test-map/map.json in the main checkout) a source selects the test files that really ran it, through virtual calls too, and not the ones
that only include its header. A header selects the test files that ran any file including it: a changed layout or constant changes those files' code. The #include rule stays for a test file
the map does not know (new, changed since the map, crashed, or it starts a helper whose code has no marks), for a source the map has never seen, and for a source with code that runs before
main: every process runs that, so the map cannot tell who reads its statics.
"""
from __future__ import annotations

import argparse
import json
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


def reached_files(changed: str, includers: dict[str, set[str]]) -> set[str]:
    seen, stack = {changed}, [changed]
    while stack:
        for includer in includers.get(stack.pop(), ()):
            if includer not in seen:
                seen.add(includer)
                stack.append(includer)
    return seen


def reached_tests(changed: str, includers: dict[str, set[str]], cases: dict[str, list[str]]) -> set[str]:
    return {name for name in reached_files(changed, includers) if name in cases}


class TestMap:
    """What tools/test_coverage_map.py recorded: source file -> the test files that ran it."""

    def __init__(self, data: dict, stale_tests: set[str] = frozenset()):
        self.commit = data["commit"]
        self.ran_by: dict[str, set[str]] = {name: set() for name in data["files"]}
        for test, files in data["tests"].items():
            for index in files:
                self.ran_by[data["files"][index]].add(test)
        self.startup = {data["files"][index] for index in data.get("startup", ())}
        self.unmapped = set(data["unmapped"]) | set(stale_tests)
        self.known_tests = set(data["tests"]) - self.unmapped

    def ran(self, names: set[str]) -> set[str]:
        return {test for name in names for test in self.ran_by.get(name, ())} & self.known_tests


def map_folder(repo: Path) -> Path:
    """workspace/test-map of the main checkout: every agent's tree reads the one map."""
    done = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=repo, capture_output=True, text=True, encoding="utf-8")
    return Path(done.stdout.strip()).parent / "workspace" / "test-map" if done.returncode == 0 else repo / "workspace" / "test-map"


def load_map(repo: Path, base: str) -> TestMap | None:
    path = map_folder(repo) / "map.json"
    if not path.exists():
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    done = subprocess.run(["git", "diff", "--name-only", data["commit"], base], cwd=repo, capture_output=True, text=True, encoding="utf-8")
    if done.returncode != 0:
        print(f"the test map's commit {data['commit'][:8]} is not comparable with {base}: not using the map", file=sys.stderr)
        return None
    return TestMap(data, {name for name in done.stdout.splitlines() if name.startswith("engine/tests/")})


def part_of_headers(name: str, repo: Path) -> list[str]:
    """The headers of a .cpp that is one part of a class split over files: included from its own folder, with a name its own starts with; the longest such name."""
    path = Path(name)
    text = (repo / name).read_text(encoding="utf-8", errors="replace") if (repo / name).exists() else ""
    found = []
    for target in INCLUDE.findall(text):
        header = Path(target)
        if header.suffix == ".h" and path.stem.startswith(header.stem) and (repo / path.parent / header.name).exists():
            found.append((path.parent / header.name).as_posix())
    longest = max((len(Path(h).stem) for h in found), default=0)
    return sorted({h for h in found if len(Path(h).stem) == longest})


def tests_naming(data_file: str, repo: Path, cases: dict[str, list[str]]) -> set[str]:
    """The test files whose text names this data file; a test header that names it counts for the test files that include it."""
    base = Path(data_file).name
    return {name for name in cases if base in (repo / name).read_text(encoding="utf-8", errors="replace")}


def mask(case: str) -> str:
    """A doctest mask for a case name: up to the first comma or quote escape, which doctest's list syntax cannot carry."""
    head = re.split(r"[,\\]", case, maxsplit=1)[0].strip()
    return head + ("*" if head != case else "")


def select(changed: list[str], repo: Path, test_map: TestMap | None = None) -> tuple[bool, str, list[str]]:
    """(needs the full suite, why, masks)."""
    includers, cases = scan(repo)
    tests: set[str] = set()
    # the #include rule answers only for the test files the map cannot speak for
    traced = (lambda found: found - test_map.known_tests) if test_map else (lambda found: found)
    for name in changed:
        path = Path(name)
        if name == REGISTRATION or LANE_LIST.fullmatch(name):
            continue  # every new class edits these; the class's own files and test select what it reaches
        if name in cases:
            tests.add(name)
        elif name.startswith("engine/tests/") and path.suffix in SOURCE_SUFFIXES and not (repo / name).exists():
            continue  # a removed test file
        elif name.startswith(ROOTS) and path.suffix in (".h", ".inl"):
            reached = reached_files(name, includers)
            direct = {file for file in reached if file in cases}
            tests |= direct | test_map.ran(reached) if test_map else direct
        elif name.startswith(ROOTS) and path.suffix == ".cpp":
            mapped = test_map is not None and name in test_map.ran_by and name not in test_map.startup
            if test_map:
                tests |= test_map.ran({name})
            header = path.with_suffix(".h").as_posix()
            headers = [header] if (repo / header).exists() else part_of_headers(name, repo)
            if not headers:
                if mapped:
                    continue
                return True, f"{name} has no header of its name, no test can be traced from it", []
            for one in headers:
                found = reached_tests(one, includers, cases)
                tests |= traced(found) if mapped else found
        elif name.startswith("engine/tests/data/"):
            named = tests_naming(name, repo, cases)
            if not named:
                return True, f"{name}: no test file names it", []
            tests |= named
        elif name.startswith(("docs/", "tools/")) or path.suffix == ".md":
            continue  # the engine does not read these; tools/ have their own pytest files
        else:
            return True, f"{name}: no test can be traced from this file", []
    masks = sorted({mask(case) for name in tests for case in cases[name]} | {ALWAYS})
    wide = " (a wide change)" if len(tests) > len(cases) * WIDE_SHARE else ""
    return False, f"{len(tests)} of {len(cases)} test files{wide}", masks


def chunks(masks: list[str], limit: int = None) -> list[str]:
    """The masks as -tc= filters, each under `limit` characters (the command line's room); the order of the masks is kept."""
    limit = MAX_FILTER if limit is None else limit
    out, part = [], []
    for m in masks:
        if part and sum(len(x) + 1 for x in part) + len(m) + 1 > limit:
            out.append("-tc=" + ",".join(part))
            part = []
        part.append(m)
    if part:
        out.append("-tc=" + ",".join(part))
    return out


def changed_files(base: str, repo: Path) -> list[str]:
    run = lambda *a: subprocess.run(["git", *a], cwd=repo, capture_output=True, text=True, encoding="utf-8", check=True).stdout.splitlines()
    return sorted(set(run("diff", "--name-only", f"{base}...HEAD")) | set(run("diff", "--name-only", "HEAD")) | set(run("ls-files", "--others", "--exclude-standard")))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", default="main")
    parser.add_argument("--run", help="openbfme_tests.exe: run the selection instead of printing it")
    parser.add_argument("--log", help="with --run: write the output here")
    parser.add_argument("--full", action="store_true", help="with --run: run the whole suite when the answer is FULL (about 5 hours; nightly on main, not on a branch)")
    args = parser.parse_args()
    changed = changed_files(args.base, REPO)
    test_map = load_map(REPO, args.base)
    full, why, masks = select(changed, REPO, test_map)
    source = f"the test map of {test_map.commit[:8]}" if test_map else "no test map, the #include rule"
    print(f"{len(changed)} changed files, {source}; {'FULL suite: ' if full else ''}{why}", file=sys.stderr)
    filters = [[]] if full else [[f] for f in chunks(masks)]
    if not args.run:
        print("(whole suite)" if full else "\n".join(f[0] for f in filters))
        return 0
    if full and not args.full:
        print("FULL: the whole suite runs before the branch lands (docs/ennorath/PROCESS.md section 15, rule 8; overnight is fine): give --full", file=sys.stderr)
        return 2
    if len(filters) > 1:
        print(f"the selection runs in {len(filters)} parts", file=sys.stderr)
    worst = 0
    out = open(args.log, "wb") if args.log else None
    try:
        for f in filters:
            code = subprocess.run([args.run, *f], stdout=out, stderr=subprocess.STDOUT if out else None).returncode
            worst = worst or code
    finally:
        if out:
            out.close()
    return worst


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""The counts every branch would otherwise edit by hand and fight over: run once, after merging main, not in each change.

    sync_counts.py [--tests-exe P | --coverage-log P] [--check]

The README's open stops come from docs/STOPS.md (tools/stops_count.py). The module coverage pins of engine/tests/test_module_coverage.cpp come
from that test's own line "module coverage: ported N, base AI N, parsed only N, missing N", read from a log or by running the test executable.
Without either, the pins are left alone and it says so. --check changes nothing and fails when a file is out of date. A conflict in one of these
lines is resolved by taking either side and running this.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))
import stops_count  # noqa: E402

COVERAGE = REPO / "engine/tests/test_module_coverage.cpp"
LINE = re.compile(r"module coverage: ported (\d+), base AI (\d+), parsed only (\d+), missing (\d+)")
PINS = ("ported", "baseAi", "parsedOnly", "missing")


def read_counts(log: str) -> dict[str, int]:
    """The pins from a test log; a log without the test's line is an error."""
    found = LINE.findall(log)
    if not found:
        raise ValueError("no 'module coverage: ported ...' line in the test output (run the test with -s)")
    return dict(zip(PINS, (int(v) for v in found[-1])))


def apply_pins(source: str, counts: dict[str, int]) -> str:
    for pin, value in counts.items():
        source, n = re.subn(rf"CHECK\(c\.{pin} == \d+\)", f"CHECK(c.{pin} == {value})", source)
        if n != 1:
            raise ValueError(f"test_module_coverage.cpp: {n} pins of c.{pin}, expected 1")
    return source


def apply_stops(readme: str, stops: str) -> str:
    return stops_count.CLAIM.sub(f"**{stops_count.open_count(stops)} open stops**", readme, count=1)


def update(path: Path, new: str, check: bool) -> bool:
    """True when the file was already right."""
    old = path.read_bytes().decode("utf-8")
    if old == new:
        return True
    print(f"{path.name}: {'out of date' if check else 'updated'}")
    if not check:
        path.write_bytes(new.encode("utf-8"))
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--tests-exe", help="openbfme_tests.exe of this tree")
    source.add_argument("--coverage-log", help="a log of the module coverage test run with -s")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    readme = stops_count.README.read_bytes().decode("utf-8")
    good = update(stops_count.README, apply_stops(readme, stops_count.STOPS.read_bytes().decode("utf-8")), args.check)
    log = None
    if args.coverage_log:
        log = Path(args.coverage_log).read_text(encoding="utf-8", errors="replace")
    elif args.tests_exe:
        done = subprocess.run([args.tests_exe, "-tc=module coverage*", "-s"], capture_output=True, text=True, encoding="utf-8", errors="replace")
        log = done.stdout + done.stderr
    if log is None:
        print("module coverage pins not checked: give --tests-exe or --coverage-log")
    else:
        good = update(COVERAGE, apply_pins(COVERAGE.read_bytes().decode("utf-8"), read_counts(log)), args.check) and good
    return 0 if good or not args.check else 1


if __name__ == "__main__":
    sys.exit(main())

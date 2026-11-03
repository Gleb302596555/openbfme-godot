#!/usr/bin/env python3
"""The map test_select.py reads: which source files each test file really runs (docs/ennorath/PROCESS.md, section 7).

    build.bat with OPENBFME_TEST_COVERAGE=1                                    the tests with marks, in engine\\build-cov
    test_coverage_map.py [--exe EXE] [--out DIR] [--only TEXT] [--limit N]     one process per test file, then the map

One process per test file, not one per case: a world or a recorded game a process shares between cases is made once, by whichever case comes first, so only a whole
process's marks show what every case of the file depends on. A run that stops is resumed: the records already made stay. A test file with no record (it crashed, it hung, a
helper it starts runs code that has no marks) is listed in the map as unmapped, and test_select keeps the #include rule for it. What runs before main (static initializers) is recorded apart: every process runs it, so it says nothing of a test; a file with such code keeps the #include rule too, its statics are read through its header. Run at below-normal priority, at night, on main.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
import re
import struct
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_select  # noqa: E402

REPO = test_select.REPO
BELOW_NORMAL = 0x00004000
HOURS_PER_FILE = 2.0
# a helper process runs code the marks of this one do not see
STARTS_A_HELPER = re.compile(r"ProcessTestUtil\.h|processtest::|std::system\(|CreateProcess|_popen")
FLAG = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+__sancov\$2\$(\S+)\s+[0-9a-f]+\s+(\S.*?)\s*$")
FUNCTION = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+f\s+(?:i\s+)?(\S.*?)\s*$")
BASE = re.compile(r"Preferred load address is ([0-9a-f]+)")


def parse_map(text: str) -> tuple[list[tuple[int, str, str]], dict[tuple[str, str], int]]:
    """([(the function's first flag, its name, its object file)] by flag, (name, object file) -> the function's offset in the image)."""
    base = 0
    flags: list[tuple[int, str, str]] = []
    functions: dict[tuple[str, str], int] = {}
    for line in text.splitlines():
        found = BASE.search(line)
        if found:
            base = int(found.group(1), 16)
            continue
        found = FLAG.match(line)
        if found:
            flags.append((int(found.group(2), 16), found.group(3), found.group(4)))
            continue
        found = FUNCTION.match(line)
        if found:
            functions[(found.group(1), found.group(3))] = int(found.group(2), 16) - base
    flags.sort()
    return flags, functions


def flag_owner(flags: list[tuple[int, str, str]]) -> list[int]:
    """The first flags, for bisect: a flag belongs to the last function whose first flag is not after it."""
    return [flag[0] for flag in flags]


def source_files(exe: Path, rvas: list[int]) -> list[str | None]:
    """The file each code offset was compiled from, by the executable's debug file (dbghelp reads the FASTLINK form too)."""

    class Line(ctypes.Structure):
        _fields_ = [("SizeOfStruct", wintypes.DWORD), ("Key", ctypes.c_void_p), ("LineNumber", wintypes.DWORD), ("FileName", ctypes.c_char_p), ("Address", ctypes.c_uint64)]

    dbghelp = ctypes.WinDLL("dbghelp")
    kernel32 = ctypes.WinDLL("kernel32")
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    process = kernel32.GetCurrentProcess()
    dbghelp.SymSetOptions(0x10)  # SYMOPT_LOAD_LINES
    dbghelp.SymInitialize.argtypes = [wintypes.HANDLE, ctypes.c_char_p, wintypes.BOOL]
    if not dbghelp.SymInitialize(process, None, False):
        raise SystemExit("dbghelp: SymInitialize failed")
    dbghelp.SymLoadModuleEx.restype = ctypes.c_uint64
    dbghelp.SymLoadModuleEx.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint64, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
    base = dbghelp.SymLoadModuleEx(process, None, str(exe).encode("mbcs"), None, 0x10000000, 0, None, 0)
    if not base:
        raise SystemExit(f"dbghelp: cannot load {exe}")
    dbghelp.SymGetLineFromAddr64.argtypes = [wintypes.HANDLE, ctypes.c_uint64, ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(Line)]
    answers: list[str | None] = []
    for rva in rvas:
        line = Line()
        line.SizeOfStruct = ctypes.sizeof(Line)
        displacement = wintypes.DWORD()
        ok = dbghelp.SymGetLineFromAddr64(process, base + rva, ctypes.byref(displacement), ctypes.byref(line))
        answers.append(line.FileName.decode("mbcs") if ok else None)
    return answers


def repo_relative(path: str | None, repo: Path) -> str | None:
    """A repository path with forward slashes; the compiler's own headers and the Windows SDK answer None."""
    if not path:
        return None
    try:
        return Path(path).resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        return None


def function_files(exe: Path, repo: Path, cache: Path) -> tuple[list[int], list[str | None]]:
    """(the functions' first flags, each function's source file). Asking the debug file takes minutes, so it stays next to the records."""
    if cache.exists():
        saved = json.loads(cache.read_text(encoding="utf-8"))
        if saved["exe_time"] == exe.stat().st_mtime:
            return saved["starts"], saved["files"]
    flags, functions = parse_map(exe.with_suffix(".map").read_text(encoding="utf-8", errors="replace"))
    if not flags:
        raise SystemExit(f"{exe.with_suffix('.map')}: no flags: build with OPENBFME_TEST_COVERAGE=1")
    known = [(i, functions[(name, obj)]) for i, (_, name, obj) in enumerate(flags) if (name, obj) in functions]
    print(f"{len(flags)} functions with flags, {len(known)} with an address", file=sys.stderr)
    files: list[str | None] = [None] * len(flags)
    for (i, _), path in zip(known, source_files(exe, [rva for _, rva in known])):
        files[i] = repo_relative(path, repo)
    starts = flag_owner(flags)
    cache.write_text(json.dumps({"exe_time": exe.stat().st_mtime, "starts": starts, "files": files}), encoding="utf-8")
    return starts, files


def files_of_record(record: bytes, starts: list[int], files: list[str | None]) -> set[str]:
    """The source files whose flags a process set. The offsets come sorted, as the process writes them."""
    import bisect

    found: set[str] = set()
    for (offset,) in struct.iter_unpack("<I", record):
        i = bisect.bisect_right(starts, offset) - 1
        if i >= 0 and files[i]:
            found.add(files[i])
    return found


def slug(test_file: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.]", "_", test_file.removeprefix("engine/tests/"))


def run_one(exe: Path, test_file: str, out: Path, hours: float) -> bool:
    record, log = out / (slug(test_file) + ".flags"), out / (slug(test_file) + ".log")
    if record.exists():
        return True
    env = dict(os.environ, OPENBFME_COVERAGE_OUT=str(record))
    name = test_file.rsplit("/", 1)[-1]
    with open(log, "wb") as output:
        try:
            subprocess.run([str(exe), f"--source-file=*{name}", "--duration=true"], stdout=output, stderr=subprocess.STDOUT, env=env, cwd=exe.parent, timeout=hours * 3600, creationflags=BELOW_NORMAL)
        except subprocess.TimeoutExpired:
            output.write(b"\ntest_coverage_map: timed out\n")
    return record.exists()


def run_startup(exe: Path, out: Path) -> Path:
    """The flags of the static initializers: what every process runs before main, whatever its tests are."""
    record = out / "startup.flags"
    if not record.exists():
        env = dict(os.environ, OPENBFME_COVERAGE_STARTUP=str(record))
        subprocess.run([str(exe), "--source-file=*no test file has this name"], capture_output=True, env=env, cwd=exe.parent, creationflags=BELOW_NORMAL, check=True)
    return record


def build_map(records: dict[str, set[str]], unmapped: list[str], commit: str, startup: set[str] = frozenset()) -> dict:
    names = sorted({name for found in records.values() for name in found} | set(startup))
    index = {name: i for i, name in enumerate(names)}
    return {"commit": commit, "files": names, "tests": {test: sorted(index[name] for name in found) for test, found in sorted(records.items())}, "unmapped": sorted(unmapped), "startup": sorted(index[name] for name in startup)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, default=REPO / "engine/build-cov/openbfme_tests.exe")
    parser.add_argument("--out", type=Path, help=f"the map's folder (default {test_select.map_folder(REPO)})")
    parser.add_argument("--only", help="only the test files whose path contains this")
    parser.add_argument("--limit", type=int, help="only this many test files; the map is not written")
    parser.add_argument("--hours", type=float, default=HOURS_PER_FILE, help="a test file's limit")
    args = parser.parse_args()
    ctypes.WinDLL("kernel32").SetErrorMode(0x0001 | 0x0002)  # a crash in a test process ends it, no dialog waits all night; the child inherits this
    out = args.out or test_select.map_folder(REPO)
    records_dir = out / "records"
    records_dir.mkdir(parents=True, exist_ok=True)
    exe = args.exe.resolve()
    starts, files = function_files(exe, REPO, out / "functions.json")
    startup = files_of_record(run_startup(exe, records_dir).read_bytes(), starts, files)
    print(f"{len(startup)} source files run before main: {sum(not name.startswith('engine/tests/') for name in startup)} outside the tests", file=sys.stderr)
    _, cases = test_select.scan(REPO)
    tests = sorted(name for name in cases if not args.only or args.only in name)
    if args.limit:
        tests = tests[: args.limit]
    records: dict[str, set[str]] = {}
    unmapped: list[str] = []
    began = time.monotonic()
    for i, test in enumerate(tests, 1):
        if run_one(exe, test, records_dir, args.hours):
            records[test] = files_of_record((records_dir / (slug(test) + ".flags")).read_bytes(), starts, files)
        else:
            unmapped.append(test)
        print(f"{i}/{len(tests)} {(time.monotonic() - began) / 60:.1f} min {test}: {len(records.get(test, ()))} source files", flush=True)
    if args.limit or args.only:
        print("a part of the tests: the map is not written")
        return 0
    helpers = [test for test in records if STARTS_A_HELPER.search((REPO / test).read_text(encoding="utf-8", errors="replace"))]
    commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO, capture_output=True, text=True, check=True).stdout.strip()
    (out / "map.json").write_text(json.dumps(build_map(records, unmapped + helpers, commit, startup)), encoding="utf-8")
    print(f"map of {commit[:8]}: {len(records)} test files, {len(unmapped)} without a record, {len(helpers)} start a helper")
    return 0


if __name__ == "__main__":
    sys.exit(main())

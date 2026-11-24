#!/usr/bin/env python3
"""Which repository files a matrix faction depends on, and which factions a change must rerun (owner 2026-11-24: "без риска").

    matrix_deps.py build  --binary engine/build/openbfme_tests --build-dir engine/build --traces <out>/trace --faction F --deps <cache>/F.deps
    matrix_deps.py select --cache <cache> [--factions ...]          # prints `rerun F reason` / `keep F` lines; exit 0

A faction's set is built from the processes that ran it (the units sweep with its crash reruns, the spell book, the recheck: every one of them
records the engine functions it ran, engine/tests/FuncTrace.cpp, OPENBFME_FUNC_TRACE=ON), widened until no change outside it can alter what ran:
  1. functions -> the translation units that hold them (the binary's DWARF address ranges);
  2. + the units whose global data those units read (nm: every undefined data symbol of a unit in the set, resolved to the unit that defines it,
     to a fixed point): a table in another .cpp changes behaviour without any of its functions running;
  3. -> every file each unit was compiled from (ninja's deps: the .cpp and all headers it includes, so a header's constant, macro or layout
     counts even when none of its functions run; header functions are inlined into, or called from, the instrumented .cpp functions).
Rows of one faction share one world and run in order (MATRIX ORDER rows), so the unit is the faction, never a row.

select reruns a faction when: it has no set or no recorded main; git cannot diff from that main; a changed file is in its set; or a change
touches what every faction depends on outside the engine (GLOBAL below: the matrix tools, the seeds graph, the build). Anything else keeps
the faction's last rows. A set is only written when every process of the faction left a trace and none is marked broken.
"""
from __future__ import annotations

import argparse
import bisect
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# outside the engine's compiled files, what every faction's rows depend on: a change here reruns the whole matrix
GLOBAL = (
    "tools/matrix_run.py", "tools/mod/matrix_deps.py", "tools/mod/ennorath_graph.py", "tools/colab/build_tests.sh", "tools/colab/nightly_maps.sh",
    "engine/CMakeLists.txt", "engine/cmake/", "docs/ennorath/MATRIX_GREY.tsv", "engine/thirdparty/",
    "engine/data/",  # embedded into generated sources by custom commands: ninja's deps log does not list them
)


class Untrusted(Exception):
    pass


def read_traces(trace_dir: Path, faction: str) -> set[int]:
    files = sorted(trace_dir.glob(f"{faction}.*"))
    if any(p.name.endswith(".broken") for p in files):
        raise Untrusted(f"{faction}: a process lost part of its trace (.broken)")
    marks = [p for p in files if p.name.endswith(".expect")]
    data = [p for p in files if not p.name.endswith((".expect", ".broken"))]
    for m in marks:  # matrix_run.py writes <F>.<n>.expect before each process: each must have left a trace
        stem = m.name[: -len(".expect")]
        if not any(p.name.startswith(stem + ".") for p in data):
            raise Untrusted(f"{faction}: process {stem} left no trace")
    offs: set[int] = set()
    for p in data:
        b = p.read_bytes()
        if len(b) % 8:
            raise Untrusted(f"{p.name}: a torn record")
        offs.update(int.from_bytes(b[i:i + 8], "little") for i in range(0, len(b), 8))
    if not offs:
        raise Untrusted(f"{faction}: no functions recorded")
    return offs


def run(cmd: list[str], cwd: Path | None = None) -> str:
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"{' '.join(cmd[:3])} failed: {r.stderr[-1000:]}")
    return r.stdout


def cu_ranges(binary: Path) -> tuple[list[int], list[tuple[int, int, str]]]:
    """Sorted range starts and (start, end, unit source path) from .debug_aranges and the units' names."""
    names: dict[int, str] = {}
    comp: dict[int, str] = {}
    cur = None
    for line in run(["readelf", "--debug-dump=info", "--dwarf-depth=1", str(binary)]).splitlines():
        m = re.match(r"\s*Compilation Unit @ offset (0x[0-9a-f]+):", line)
        if m:
            cur = int(m.group(1), 16)
            continue
        m = re.match(r"\s*<[0-9a-f]+>\s+DW_AT_(name|comp_dir)\s*:\s*(?:\([^)]*\):\s*)?(.*\S)\s*$", line)
        if m and cur is not None:
            (names if m.group(1) == "name" else comp).setdefault(cur, m.group(2))
    ranges = []
    off = None
    for line in run(["readelf", "--debug-dump=aranges", str(binary)]).splitlines():
        m = re.match(r"\s*Offset into \.debug_info:\s*(0x[0-9a-f]+)", line)
        if m:
            off = int(m.group(1), 16)
            continue
        m = re.match(r"\s*([0-9a-f]{8,16})\s+([0-9a-f]{8,16})\s*$", line)
        if m and off is not None:
            lo, n = int(m.group(1), 16), int(m.group(2), 16)
            if n:
                name = names.get(off, "")
                if name and not name.startswith("/") and off in comp:
                    name = str(Path(comp[off]) / name)
                ranges.append((lo, lo + n, name))
    if not ranges:
        raise SystemExit(f"{binary}: no .debug_aranges (build with OPENBFME_FUNC_TRACE=ON: -g1)")
    ranges.sort()
    return [r[0] for r in ranges], ranges


def object_index(build_dir: Path, engine: Path) -> dict[str, Path]:
    """Source path (absolute, resolved) -> its object file: CMake mirrors the source tree under CMakeFiles/<target>.dir/."""
    out = {}
    for o in build_dir.glob("CMakeFiles/*.dir/**/*.o"):
        parts = o.relative_to(build_dir).parts  # CMakeFiles, <target>.dir, <path of the source below engine/>...
        rel = "/".join(parts[2:])
        if rel.startswith("__/"):
            continue  # a source outside engine/ (CMake writes ".." as "__"): not part of the matrix's engine
        src = (engine / rel[: -len(".o")]).resolve()
        out.setdefault(str(src), o)
    return out


def nm_symbols(objs: list[Path]) -> tuple[dict[str, Path], dict[Path, set[str]]]:
    """(global data symbol -> the object defining it, object -> the symbols it leaves undefined)."""
    defs: dict[str, Path] = {}
    undef: dict[Path, set[str]] = {o: set() for o in objs}
    for i in range(0, len(objs), 400):
        chunk = objs[i:i + 400]
        for line in run(["nm", "-A", "-P"] + [str(o) for o in chunk]).splitlines():
            # "<file>: <symbol> <type> [value size]"
            m = re.match(r"(.*?):\s+(\S+)\s+([A-Za-z])\b", line)
            if not m:
                continue
            obj, sym, typ = Path(m.group(1)), m.group(2), m.group(3)
            if typ == "U":
                undef.setdefault(obj, set()).add(sym)
            elif typ in "DBRVGSu":  # global data: initialized, bss, read-only, weak object, small data, unique global
                defs.setdefault(sym, obj)
    return defs, undef


def ninja_deps(build_dir: Path) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    cur = None
    for line in run(["ninja", "-C", str(build_dir), "-t", "deps"]).splitlines():
        if line and not line.startswith(" "):
            cur = line.split(":", 1)[0].strip()
            out[cur] = []
        elif line.strip() and cur is not None:
            out[cur].append(line.strip())
    return out


def build(a) -> int:
    binary, build_dir = Path(a.binary), Path(a.build_dir).resolve()
    engine = (REPO / "engine").resolve()
    try:
        offs = read_traces(Path(a.traces), a.faction)
    except Untrusted as e:
        Path(a.deps).unlink(missing_ok=True)
        print(f"no set for {a.faction}: {e} (it reruns next time)")
        return 0
    starts, ranges = cu_ranges(binary)
    units: set[str] = set()
    outside: list[int] = []
    for off in offs:
        i = bisect.bisect_right(starts, off) - 1
        if i >= 0 and ranges[i][0] <= off < ranges[i][1] and ranges[i][2]:
            units.add(str(Path(ranges[i][2]).resolve()))
        else:
            outside.append(off)
    # GCC's static initializers (.text.startup: __static_initialization_and_destruction_0, _GLOBAL__sub_I_<file>) are not in .debug_aranges; the
    # line table still names their file. A .c / .cpp file is the unit itself; anything else (a header, no line) leaves the set unproven.
    unmapped = 0
    if outside:
        lines = run(["addr2line", "-e", str(binary)] + [hex(o) for o in outside]).splitlines()
        for off, line in zip(outside, lines):
            src = line.rsplit(":", 1)[0]
            cands = [Path(src)] if Path(src).is_absolute() else [REPO / src, build_dir / src]
            hit = next((c for c in cands if c.exists()), None) if src.endswith((".cpp", ".c", ".cc")) else None
            if hit is not None:
                units.add(str(hit.resolve()))
            else:
                unmapped += 1
    if unmapped:
        # a function outside every unit's ranges (a runtime or libc thunk would not be instrumented): the set cannot be proved whole
        Path(a.deps).unlink(missing_ok=True)
        print(f"no set for {a.faction}: {unmapped} of {len(offs)} recorded functions lie in no DWARF unit (it reruns next time)")
        return 0
    index = object_index(build_dir, engine)
    objs = sorted(set(index.values()))
    defs, undef = nm_symbols(objs)
    todo = [index[u] for u in units if u in index]
    have = set(todo)
    while todo:  # data closure: the units whose global data a unit in the set reads
        o = todo.pop()
        for sym in undef.get(o, ()):
            d = defs.get(sym)
            if d is not None and d not in have:
                have.add(d)
                todo.append(d)
    deps = ninja_deps(build_dir)
    files: set[str] = set()
    root = REPO.resolve()
    missing = 0
    for o in have:
        key = o.relative_to(build_dir).as_posix()
        lst = deps.get(key)
        if lst is None:
            missing += 1
            continue
        for f in lst:
            p = Path(f) if Path(f).is_absolute() else build_dir / f
            try:
                files.add(p.resolve().relative_to(root).as_posix())
            except ValueError:
                pass  # system headers: a change there is a new toolchain, which the build key covers
    if missing:
        Path(a.deps).unlink(missing_ok=True)
        print(f"no set for {a.faction}: {missing} units without ninja deps (it reruns next time)")
        return 0
    rev = run(["git", "rev-parse", "HEAD"], cwd=REPO).strip()
    Path(a.deps).write_text(f"# rev {rev}\n" + "".join(f + "\n" for f in sorted(files)), encoding="utf-8")
    print(f"{a.faction}: {len(offs)} functions, {len(units)} units ran, {len(have)} with the data they read, {len(files)} files")
    return 0


def select(a) -> int:
    cache = Path(a.cache)
    for f in a.factions:
        p = cache / f"{f}.deps"
        if not p.exists():
            print(f"rerun {f} no set")
            continue
        text = p.read_text(encoding="utf-8").splitlines()
        rev = text[0][len("# rev "):].strip() if text and text[0].startswith("# rev ") else ""
        if not rev or not (cache / f"{f}.tsv").exists():
            print(f"rerun {f} no recorded main or rows")
            continue
        r = subprocess.run(["git", "diff", "--name-only", rev, "HEAD"], cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"rerun {f} cannot diff from {rev[:8]}")
            continue
        changed = [c for c in r.stdout.splitlines() if c]
        glob = [c for c in changed if c.startswith(GLOBAL)]
        if glob:
            print(f"rerun {f} global input changed: {glob[0]}")
            continue
        mine = set(text[1:])
        hit = [c for c in changed if c in mine]
        print(f"rerun {f} {hit[0]} (+{len(hit) - 1})" if hit else f"keep {f} since {rev[:8]} ({len(changed)} changed files, none it ran)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--binary", required=True)
    b.add_argument("--build-dir", required=True)
    b.add_argument("--traces", required=True)
    b.add_argument("--faction", required=True)
    b.add_argument("--deps", required=True)
    s = sub.add_parser("select")
    s.add_argument("--cache", required=True)
    s.add_argument("--factions", nargs="*", required=True)
    a = ap.parse_args()
    return build(a) if a.cmd == "build" else select(a)


if __name__ == "__main__":
    sys.exit(main())

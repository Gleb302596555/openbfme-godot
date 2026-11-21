"""map_sweep.py: every map of the Ennorath install starts a skirmish and plays (owner 2026-11-20, after the randomizer maps did not start).

The test case "ENNORATH sweep maps" (engine/tests/test_ennorath_map_sweep.cpp) runs one map per process, so a crash or a hang is that map's row:

    python tools/maps/map_sweep.py --tests engine/build/openbfme_tests[.exe] --out <dir> [--jobs 2] [--seconds 60] [--timeout 600] [--only <substring>]
    python tools/maps/map_sweep.py --summary <dir>/map-sweep.tsv --docs docs/ennorath      # MAP_SWEEP.tsv / MAP_SWEEP.md from a finished run

Needs ROTWK_INSTALL, BFME2_INSTALL and OPENBFME_ENNORATH_INSTALL (the test's World). Writes <dir>/map-sweep.tsv (one row per map, appended, so a rerun
of a killed sweep goes on: maps already in the file are skipped), <dir>/logs/<n>.log per map, and with --docs the summary: the maps by status and the
errors grouped by their text with the maps that hit each (one group is one mechanism to fix).
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

CASE = "ENNORATH sweep maps*"
COLUMNS = ["map", "status", "error", "load_errors", "load_first", "mapini_errors", "mapini_first", "start_errors", "play_errors", "play_first",
           "seconds", "map_objects"]


def list_maps(tests: str) -> list[str]:
    with tempfile.TemporaryDirectory() as t:
        out = Path(t) / "maps.txt"
        env = dict(os.environ, OPENBFME_MAP_SWEEP_LIST=str(out))
        r = subprocess.run([tests, f"-tc={CASE}"], env=env, capture_output=True, text=True, errors="replace")
        if not out.exists():
            raise SystemExit(f"the test did not list the maps (exit {r.returncode}):\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
        return [l.strip() for l in out.read_text(encoding="utf-8", errors="replace").splitlines() if l.strip()]


def run_one(tests: str, map_path: str, report: Path, logs: Path, n: int, seconds: float, timeout: float) -> str:
    env = dict(os.environ, OPENBFME_MAP_SWEEP_MAP=map_path, OPENBFME_MAP_SWEEP_REPORT=str(report), OPENBFME_MAP_SWEEP_SECONDS=str(seconds))
    env.setdefault("OPENBFME_LOGIC_THREADS", "2")  # several maps run at once: each process keeps to two logic threads
    log = logs / f"{n:03d}.log"
    try:
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            f.write(map_path + "\n")
            f.flush()
            # below-normal priority on Windows: the sweep runs on the owner's PC while it is used (2026-11-20)
            flags = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0)
            r = subprocess.run([tests, f"-tc={CASE}"], env=env, stdout=f, stderr=subprocess.STDOUT, timeout=timeout, creationflags=flags)
        code = r.returncode
    except subprocess.TimeoutExpired:
        code = None
    text = log.read_text(encoding="utf-8", errors="replace")
    if code is None or "CRASHED" in text or (code != 0 and map_path not in done_maps(report)):
        status = "timeout" if code is None else "crashed" if "CRASHED" in text else f"exit {code}"
        last = [l for l in text.splitlines() if l.strip()][-3:]
        with open(report, "a", encoding="utf-8") as f:
            f.write("\t".join([map_path, status, " ".join(last)[:400].replace("\t", " ")] + [""] * (len(COLUMNS) - 3)) + "\n")
        return f"{map_path}: {status}"
    return f"{map_path}: done"


def done_maps(report: Path) -> set[str]:
    if not report.exists():
        return set()
    return {l.split("\t", 1)[0] for l in report.read_text(encoding="utf-8", errors="replace").splitlines() if l.strip()}


def rows(tsv: Path) -> list[dict[str, str]]:
    out = []
    for l in tsv.read_text(encoding="utf-8", errors="replace").splitlines():
        if not l.strip():
            continue
        v = l.split("\t")
        out.append(dict(zip(COLUMNS, v + [""] * (len(COLUMNS) - len(v)))))
    return out


def norm(msg: str) -> str:
    """An error's group key: numbers, ids and quoted names out, so one mechanism hit on many objects is one row."""
    m = re.sub(r"`[^`']*'|\"[^\"]*\"|'[^']*'", "<name>", msg)
    m = re.sub(r"0x[0-9A-Fa-f]+|\b\d+(\.\d+)?\b", "<n>", m)
    return m.strip()[:160]


def summary(tsv: Path, docs: Path) -> None:
    rs = rows(tsv)
    for r in rs:
        # a map the map cache does not list cannot be started as a skirmish (campaign, War of the Ring, cinematic, tutorial maps, and the
        # maps the mod's mapcache.ini leaves out): its own status, not a failure of the port
        if r["status"] == "failed at prepareNewGame" and "is not in the map cache" in r["error"]:
            r["status"] = "not in the map cache"
    by_status = collections.Counter(r["status"] for r in rs)
    groups: dict[str, set[str]] = collections.defaultdict(set)
    for r in rs:
        for col in ("error", "load_first", "mapini_first", "start_errors", "play_first"):
            for part in (r[col] or "").split(" | "):
                if part.strip() and not (col == "error" and r["status"] == "not in the map cache"):
                    groups[f"{col.split('_')[0]}: {norm(part)}"].add(r["map"])
    docs.mkdir(parents=True, exist_ok=True)
    (docs / "MAP_SWEEP.tsv").write_text("\t".join(COLUMNS) + "\n" + "".join("\t".join(r[c] for c in COLUMNS) + "\n" for r in sorted(rs, key=lambda r: r["map"])),
                                        encoding="utf-8", newline="\n")
    lines = ["# Ennorath map sweep", "",
             "Every map of the Ennorath install starts a two-slot skirmish (FactionMen against an easy computer FactionMordor) and plays; one map per process "
             "(`tools/maps/map_sweep.py`, test `ENNORATH sweep maps`). Rows: `MAP_SWEEP.tsv`. An error group is one text with names and numbers left out: "
             "one group is one mechanism to fix, the maps column says where it shows.", "",
             f"Maps: {len(rs)}; " + ", ".join(f"{s}: {n}" for s, n in by_status.most_common()), "",
             "## Maps that do not start or play", "", "| map | status | error |", "|---|---|---|"]
    for r in sorted(rs, key=lambda r: r["map"]):
        if r["status"] not in ("ok", "errors", "not in the map cache"):
            lines.append(f"| `{r['map']}` | {r['status']} | {r['error'][:200]} |")
    cache = sorted(r["map"] for r in rs if r["status"] == "not in the map cache")
    lines += ["", f"## Not in the map cache ({len(cache)}): not a skirmish map, or left out by the mod's mapcache.ini", "",
              ", ".join(f"`{m.split(chr(92))[1] if chr(92) in m else m}`" for m in cache)]
    lines += ["", "## Error groups (most maps first)", "", "| maps | where | error |", "|---:|---|---|"]
    for key, maps in sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        where, text = key.split(": ", 1)
        lines.append(f"| {len(maps)} | {where} | {text.replace('|', '/')} |")
    (docs / "MAP_SWEEP.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(rs)} maps; {dict(by_status)}; {len(groups)} error groups -> {docs / 'MAP_SWEEP.md'}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tests")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--timeout", type=float, default=600.0)
    ap.add_argument("--only", default="", help="only the maps whose path holds this text (case-insensitive)")
    ap.add_argument("--summary", help="a finished run's map-sweep.tsv")
    ap.add_argument("--docs", default="docs/ennorath")
    a = ap.parse_args()
    if a.summary:
        summary(Path(a.summary), Path(a.docs))
        return 0
    if not a.tests or not a.out:
        ap.error("--tests and --out (or --summary)")
    out = Path(a.out)
    logs = out / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    report = out / "map-sweep.tsv"
    maps = [m for m in list_maps(a.tests) if a.only.lower() in m.lower()]
    todo = [m for m in maps if m not in done_maps(report)]
    print(f"{len(maps)} maps, {len(todo)} to run, {a.jobs} at once", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = [pool.submit(run_one, a.tests, m, report, logs, maps.index(m), a.seconds, a.timeout) for m in todo]
        for i, f in enumerate(concurrent.futures.as_completed(futs), 1):
            print(f"[{i}/{len(todo)}] {f.result()}", flush=True)
    summary(report, Path(a.docs))
    return 0


if __name__ == "__main__":
    sys.exit(main())

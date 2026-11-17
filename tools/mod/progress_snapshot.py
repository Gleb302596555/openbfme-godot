#!/usr/bin/env python3
"""How much Ennorath work is left, one row a day: docs/ennorath/PROGRESS.tsv (the owner's dashboard charts it on /stats).

Columns:
  date             the day of the snapshot (YYYY-MM-DD)
  rev              the commit the docs were read at
  fields_total     Ennorath INI fields in the last field audit (tools/mod/field_audit.py)
  fields_open      fields with work left: parsed-unused, not-read, stub-block, missing-module, parsed-only-module, no-table-row
  modules_missing  docs/ennorath/MODULE_GAPS.md rows with state `missing`
  problems_total   docs/ennorath/PROBLEMS.md entries (### P-N)
  problems_done    entries whose status line is checked ([x])
  stops_open       docs/STOPS.md rows not `closed` (tools/stops_count.py)
  audit            which field audit the two field columns come from ("-" = carried from the row before)
  buttons_red      distinct failing buttons of the button matrix (docs/ennorath/MATRIX.tsv: any result but PASS, STANCE, DISABLED, INVENTORY)
  stops_new        stop ids open now that were not open at the previous row's rev (2026-11-17: the stop curve, per landing)
  stops_closed     stop ids open at the previous row's rev that are closed or gone now
  buttons_total    distinct (object, button) pairs the matrix pressed: the scale of buttons_red (the home page's share)
  gaps_specific    distinct RotWK functions the failing buttons' main classes lack (docs/ennorath/MATRIX_GAPS.tsv, `main class` rows,
                   `specific none`): the work the matrix still points at. Both empty in rows before 2026-11-16 and when a file is missing

Usage:
  python tools/mod/progress_snapshot.py [--audit <field-audit.tsv>] [--audit-name v12] [--date YYYY-MM-DD] [--rev <commit>] [--write]
  python tools/mod/progress_snapshot.py --backfill-stops      # fills stops_new / stops_closed of every row from git (their revs)
Without --audit the field columns are carried from the last row. --rev reads the docs at that commit (the backfill); default the
working tree. --write replaces the row of that date in PROGRESS.tsv (rows stay sorted by date); without it the row is printed.
The audit tsv holds counts only here: no game data enters the repository.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
import stops_count  # noqa: E402

OUT = REPO / "docs" / "ennorath" / "PROGRESS.tsv"
COLUMNS = ["date", "rev", "fields_total", "fields_open", "modules_missing", "problems_total", "problems_done", "stops_open", "audit",
           "buttons_red", "gaps_specific", "stops_new", "stops_closed", "buttons_total"]
GREEN = {"PASS", "STANCE", "DISABLED", "INVENTORY", "TEMPORARY", "MEMBER", "MOD-DATA", "NO-TARGET", "BY-DESIGN", "ORDER"}  # by design, or fine when swept alone (matrix review 2026-11-17)
OPEN_STATUSES = {"parsed-unused", "not-read", "stub-block", "missing-module", "parsed-only-module", "no-table-row"}


def read(path: str, rev: str | None) -> str:
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], check=True, capture_output=True,
                              text=True, encoding="utf-8").stdout
    return (REPO / path).read_text(encoding="utf-8")


def problems(text: str) -> tuple[int, int]:
    total = done = 0
    status_next = False
    for line in text.splitlines():
        if line.startswith("### P-"):
            total += 1
            status_next = True
            continue
        if status_next and line.startswith("- Статус:"):
            done += "[x]" in line[:16]
            status_next = False
    return total, done


def audit_counts(tsv: Path) -> tuple[int, int]:
    with tsv.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    if not rows or "status" not in rows[0]:
        raise SystemExit(f"{tsv}: not a field-audit.tsv (no status column)")
    return len(rows), sum(1 for r in rows if r["status"] in OPEN_STATUSES)


def matrix_counts(rev: str | None) -> tuple[str, str]:
    """(buttons_red, gaps_specific) from the matrix and its join with the function map; "" where a file is missing at `rev`."""
    try:
        matrix = read("docs/ennorath/MATRIX.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return "", ""
    lines = [l for l in matrix.splitlines() if l and not l.startswith("#")]
    red = {(r["object"], r["button"]) for r in csv.DictReader(lines, delimiter="\t") if r["result"] not in GREEN}
    try:
        gaps = read("docs/ennorath/MATRIX_GAPS.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return str(len(red)), ""
    rows = list(csv.DictReader(gaps.splitlines(), delimiter="\t"))
    if not rows or "main class" not in rows[0]:
        return str(len(red)), ""
    fn_col = [k for k in rows[0] if k.startswith("specific none functions")][0]
    funcs = {x.split("@")[0] for r in rows if r["main class"] == "yes" for x in r[fn_col].split() if x.startswith("0x")}
    return str(len(red)), str(len(funcs))


STOP_ROW = re.compile(r"^\|\s*S-(\d+)\s*\|\s*(code|project|closed)\s*\|", re.M)


def open_ids(rev: str | None) -> set[int] | None:
    try:
        text = read("docs/STOPS.md", rev)
    except (OSError, subprocess.CalledProcessError):
        return None
    return {int(i) for i, kind in STOP_ROW.findall(text) if kind != "closed"}


def stop_delta(prev_rev: str | None, rev: str | None) -> tuple[str, str]:
    """(stops_new, stops_closed) between two revs; "" when either STOPS.md cannot be read."""
    if not prev_rev:
        return "", ""
    before, now = open_ids(prev_rev), open_ids(rev)
    if before is None or now is None:
        return "", ""
    return str(len(now - before)), str(len(before - now))


def save(rows: list[dict[str, str]]) -> None:
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            f.write("\t".join(r[c] for c in COLUMNS) + "\n")


FACTIONS_OUT = REPO / "docs" / "ennorath" / "MATRIX_FACTIONS.tsv"


def write_matrix_factions() -> None:
    """The matrix per faction for the dashboard's faction table (2026-11-17; MATRIX.tsv is ~1 MB): pressed object/button pairs that work
    (PASS), that are off by design in the scene (STANCE, DISABLED, INVENTORY), that fail, and the three most common failure kinds."""
    path = REPO / "docs" / "ennorath" / "MATRIX.tsv"
    if not path.exists():
        return
    lines = [l for l in path.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")]
    per: dict[str, dict] = {}
    for r in csv.DictReader(lines, delimiter="\t"):
        f = per.setdefault(r["faction"], {"pass": set(), "idle": set(), "red": {}, })
        key = (r["object"], r["button"])
        if r["result"] == "PASS":
            f["pass"].add(key)
        elif r["result"] in GREEN:
            f["idle"].add(key)
        else:
            f["red"].setdefault(key, r["result"])
    with FACTIONS_OUT.open("w", encoding="utf-8", newline="\n") as out:
        out.write("faction\tpass\tidle\tred\tred_top\n")
        for name, f in sorted(per.items()):
            idle = f["idle"] - f["pass"]
            red = {k: v for k, v in f["red"].items() if k not in f["pass"] and k not in idle}
            kinds: dict[str, int] = {}
            for v in red.values():
                kinds[v] = kinds.get(v, 0) + 1
            top = ", ".join(f"{k} {n}" for k, n in sorted(kinds.items(), key=lambda x: -x[1])[:3])
            out.write(f"{name}\t{len(f['pass'])}\t{len(idle)}\t{len(red)}\t{top}\n")


def matrix_total(rev: str | None) -> str:
    try:
        matrix = read("docs/ennorath/MATRIX.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return ""
    lines = [l for l in matrix.splitlines() if l and not l.startswith("#")]
    return str(len({(r["object"], r["button"]) for r in csv.DictReader(lines, delimiter="\t")}))


def load() -> list[dict[str, str]]:
    if not OUT.exists():
        return []
    with OUT.open(encoding="utf-8", newline="") as f:
        return [{c: r.get(c) or "" for c in COLUMNS} for r in csv.DictReader(f, delimiter="\t")]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", type=Path)
    ap.add_argument("--audit-name", default="")
    ap.add_argument("--date", default=dt.date.today().isoformat())
    ap.add_argument("--rev")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--backfill-stops", action="store_true")
    a = ap.parse_args()

    rows = load()
    if a.backfill_stops:
        for prev, r in zip([None] + rows[:-1], rows):
            r["stops_new"], r["stops_closed"] = stop_delta(prev["rev"] if prev else None, r["rev"])
            if r["buttons_red"]:
                r["buttons_total"] = matrix_total(r["rev"])
        save(rows)
        return 0
    rev = a.rev or subprocess.run(["git", "-C", str(REPO), "rev-parse", "--short=8", "HEAD"], capture_output=True, text=True).stdout.strip()
    total, done = problems(read("docs/ennorath/PROBLEMS.md", a.rev))
    missing = sum(1 for l in read("docs/ennorath/MODULE_GAPS.md", a.rev).splitlines() if "| missing |" in l)
    stops = stops_count.open_count(read("docs/STOPS.md", a.rev))
    if a.audit:
        ftotal, fopen = audit_counts(a.audit)
        audit = a.audit_name or a.audit.parent.name
    else:
        # the day's own row counts too: a landing after the day's audit must not carry yesterday's numbers over it (2026-11-16)
        before = [r for r in rows if r["date"] <= a.date]
        if not before:
            raise SystemExit("no earlier row to carry the field columns from: give --audit")
        last = before[-1]
        ftotal, fopen = int(last["fields_total"]), int(last["fields_open"])
        audit = last["audit"] if last["date"] == a.date else "-"
    row = {"date": a.date, "rev": rev[:8], "fields_total": str(ftotal), "fields_open": str(fopen), "modules_missing": str(missing),
           "problems_total": str(total), "problems_done": str(done), "stops_open": str(stops), "audit": audit}
    row["buttons_red"], row["gaps_specific"] = matrix_counts(a.rev)
    row["buttons_total"] = matrix_total(a.rev)
    earlier = [r for r in rows if r["date"] < a.date]
    row["stops_new"], row["stops_closed"] = stop_delta(earlier[-1]["rev"] if earlier else None, a.rev)
    if not a.write:
        print("\t".join(row[c] for c in COLUMNS))
        return 0
    rows = sorted([r for r in rows if r["date"] != a.date] + [row], key=lambda r: r["date"])
    save(rows)
    if not a.rev:
        write_matrix_factions()
    print(f"{OUT.relative_to(REPO)}: {a.date} written ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

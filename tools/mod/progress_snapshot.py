#!/usr/bin/env python3
"""How much Ennorath work is left, one row a day: docs/ennorath/PROGRESS.tsv (the owner's dashboard charts it on /stats).

Columns:
  date             the day of the snapshot (YYYY-MM-DD)
  rev              the commit the docs were read at
  fields_total     Ennorath INI fields in the last field audit (tools/mod/field_audit.py)
  fields_open      fields with work left: parsed-unused, not-read, stub-block, missing-module, parsed-only-module, no-table-row, less the
                   fields proven unread in RotWK (from 2026-11-17: fields_proven_unread)
  modules_missing  docs/ennorath/MODULE_GAPS.md rows with state `missing`
  problems_total   docs/ennorath/PROBLEMS.md entries (### P-N)
  problems_done    entries whose status line is checked ([x])
  stops_open       docs/STOPS.md rows not `closed` (tools/stops_count.py)
  audit            which field audit the two field columns come from ("-" = carried from the row before)
  buttons_red      distinct failing buttons of the button matrix (docs/ennorath/MATRIX.tsv: any result but PASS, STANCE, DISABLED, INVENTORY)
  stops_new        stop ids open now that were not open at the previous row's rev (2026-11-17: the stop curve, per landing)
  stops_closed     stop ids open at the previous row's rev that are closed or gone now
  buttons_total    distinct (object, button) pairs the matrix pressed: the scale of buttons_red (the home page's share)
  fields_proven_unread  open-status fields a stop proves RotWK never reads (owner 2026-11-17): done for the port, left out of fields_open
                   (the rule: proven_unread below); empty in rows before 2026-11-17 and in rows that carry the field columns
  scripts_total    scripts of the sides' script libraries of the mod (docs/ennorath/SCRIPT_MAP.md, tools/mod/script_map.py; from 2026-11-19)
  scripts_open     of those, scripts with a condition or action type the port's ScriptEngine has no case for (the home page's share)
  funcs_total      distinct RotWK functions of the module (class) map, docs/ennorath/RW_FUNCTION_MAP.tsv (tools/mod/rw_function_map.py:
                   every module the mod's INI uses and what its functions call; from 2026-11-19, owner: the bar must count the class map)
  funcs_open       of those, functions with no `ported` row (none or partial): the home page's share
  mods_total       RotWK's reads of the mod's attribute modifiers (docs/ennorath/MODIFIER_READERS.tsv, tools/mod/modifier_map.py; from 2026-11-19)
  mods_open        of those, reads the port does not have (status not `ported` / `ported-via-call`): the home page's share
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
           "buttons_red", "gaps_specific", "stops_new", "stops_closed", "buttons_total", "fields_proven_unread", "scripts_total", "scripts_open",
           "funcs_total", "funcs_open", "mods_total", "mods_open"]
GREEN = {"PASS", "STANCE", "DISABLED", "INVENTORY", "TEMPORARY", "MEMBER", "MOD-DATA", "NO-TARGET", "BY-DESIGN", "ORDER", "NO-AURA"}  # by design, or fine when swept alone (matrix review 2026-11-17)
# not red, but not judged either (owner 2026-11-18): the faction table counts them in a column of their own, not with the idle ones (tools/matrix_run.py)
UNVERIFIED = {"MOD-DATA", "NO-TARGET", "ORDER", "NO-AURA"}
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


# A field RotWK itself never reads is done for the port (owner 2026-11-17: "засчитывай поля с доказательством как закрытые"): its audit row
# names a stop (`stop` column) whose row in docs/STOPS.md is a no-reader verdict for that field, i.e. the verdict is the stop's title
# ("Locomotor unread fields", "... without a reachable reader") and the stop names the field, or the verdict sentence stands within 300
# characters of the field's name ("`DarkeningFactor` (+ 0x1C) has NO READER in RotWK 2.01"). A stop that only says "not ported" counts
# nothing; neither does a verdict about another field of the same stop.
NO_READER = re.compile(r"reads? none|read by nobody|reads? nothing|no (reachable |known |live )?reader|nothing to port|zero (callers|reads|readers)"
                       r"|never read|not read by|without a (reachable )?reader|unread in RotWK|no instruction reads|no read of|has no reader"
                       r"|no consumer|are dead|dead branch", re.I)
NO_READER_TITLE = re.compile(NO_READER.pattern + r"|\bunread\b|rows without a reader", re.I)
STOP_TEXT = re.compile(r"^\| (S-\d+) \| \w+ \|([^|]*)\|(.*)$", re.M)


def proven_unread(field: str, stop_ids: list[str], stops: dict[str, tuple[str, str]]) -> bool:
    for sid in stop_ids:
        title, body = stops.get(sid, ("", ""))
        text = title + " | " + body
        names = [m.start() for m in re.finditer(rf"\b{re.escape(field)}\b", text)]
        if not names:
            continue
        if NO_READER_TITLE.search(title):
            return True
        if any(abs(v.start() - n) <= 300 for v in NO_READER.finditer(text) for n in names):
            return True
    return False


def audit_counts(tsv: Path, stops_text: str = "") -> tuple[int, int, int]:
    """(fields, open fields, of them proven unread in RotWK); the open count already leaves the proven ones out."""
    with tsv.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE))
    if not rows or "status" not in rows[0]:
        raise SystemExit(f"{tsv}: not a field-audit.tsv (no status column)")
    stops = {m.group(1): (m.group(2), m.group(3)) for m in STOP_TEXT.finditer(stops_text)}
    open_rows = [r for r in rows if r["status"] in OPEN_STATUSES]
    proven = sum(1 for r in open_rows if proven_unread(r["field"], re.findall(r"S-\d+\b", r.get("stop") or ""), stops))
    return len(rows), len(open_rows) - proven, proven


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
        f = per.setdefault(r["faction"], {"pass": set(), "idle": set(), "red": {}, "unverified": set()})
        key = (r["object"], r["button"])
        if r["result"] == "PASS":
            f["pass"].add(key)
        elif r["result"] in UNVERIFIED:
            f["unverified"].add(key)
        elif r["result"] in GREEN:
            f["idle"].add(key)
        else:
            f["red"].setdefault(key, r["result"])
    with FACTIONS_OUT.open("w", encoding="utf-8", newline="\n") as out:
        # the unverified column comes last: a reader of the first five columns (the dashboard before 2026-11-18) still reads them
        out.write("faction\tpass\tidle\tred\tred_top\tunverified\n")
        for name, f in sorted(per.items()):
            unverified = f["unverified"] - f["pass"]
            idle = f["idle"] - f["pass"] - unverified
            red = {k: v for k, v in f["red"].items() if k not in f["pass"] and k not in idle and k not in unverified}
            kinds: dict[str, int] = {}
            for v in red.values():
                kinds[v] = kinds.get(v, 0) + 1
            top = ", ".join(f"{k} {n}" for k, n in sorted(kinds.items(), key=lambda x: -x[1])[:3])
            out.write(f"{name}\t{len(f['pass'])}\t{len(idle)}\t{len(red)}\t{top}\t{len(unverified)}\n")


def matrix_total(rev: str | None) -> str:
    try:
        matrix = read("docs/ennorath/MATRIX.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return ""
    lines = [l for l in matrix.splitlines() if l and not l.startswith("#")]
    return str(len({(r["object"], r["button"]) for r in csv.DictReader(lines, delimiter="\t")}))


def script_counts(rev: str | None) -> tuple[str, str]:
    """scripts_total / scripts_open from SCRIPT_MAP.md's "Library scripts: N; with ... no case for: M." line; empty when the file is missing."""
    try:
        text = read("docs/ennorath/SCRIPT_MAP.md", rev)
    except (OSError, subprocess.CalledProcessError):
        return "", ""
    m = re.search(r"Library scripts: (\d+); with a condition or action the port has no case for: (\d+)\.", text)
    return (m.group(1), m.group(2)) if m else ("", "")


def func_counts(rev: str | None) -> tuple[str, str]:
    """funcs_total / funcs_open from RW_FUNCTION_MAP.tsv: a function counts as ported when any module row has it `ported`; empty when missing."""
    try:
        text = read("docs/ennorath/RW_FUNCTION_MAP.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return "", ""
    status: dict[str, bool] = {}
    for r in csv.DictReader(text.splitlines(), delimiter="\t"):
        status[r["function"]] = status.get(r["function"], False) or r["status"] == "ported"
    return (str(len(status)), str(sum(1 for v in status.values() if not v))) if status else ("", "")


def mod_counts(rev: str | None) -> tuple[str, str]:
    """mods_total / mods_open from MODIFIER_READERS.tsv; empty when missing."""
    try:
        text = read("docs/ennorath/MODIFIER_READERS.tsv", rev)
    except (OSError, subprocess.CalledProcessError):
        return "", ""
    rows = list(csv.DictReader(text.splitlines(), delimiter="\t"))
    if not rows:
        return "", ""
    return str(len(rows)), str(sum(1 for r in rows if r.get("status") not in ("ported", "ported-via-call")))


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
        ftotal, fopen, fproven = audit_counts(a.audit, read("docs/STOPS.md", a.rev))
        audit = a.audit_name or a.audit.parent.name
    else:
        # the day's own row counts too: a landing after the day's audit must not carry yesterday's numbers over it (2026-11-16)
        before = [r for r in rows if r["date"] <= a.date]
        if not before:
            raise SystemExit("no earlier row to carry the field columns from: give --audit")
        last = before[-1]
        ftotal, fopen, fproven = int(last["fields_total"]), int(last["fields_open"]), last.get("fields_proven_unread") or ""
        audit = last["audit"] if last["date"] == a.date else "-"
    row = {"date": a.date, "rev": rev[:8], "fields_total": str(ftotal), "fields_open": str(fopen), "modules_missing": str(missing),
           "problems_total": str(total), "problems_done": str(done), "stops_open": str(stops), "audit": audit,
           "fields_proven_unread": str(fproven)}
    row["buttons_red"], row["gaps_specific"] = matrix_counts(a.rev)
    row["buttons_total"] = matrix_total(a.rev)
    row["scripts_total"], row["scripts_open"] = script_counts(a.rev)
    row["funcs_total"], row["funcs_open"] = func_counts(a.rev)
    row["mods_total"], row["mods_open"] = mod_counts(a.rev)
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

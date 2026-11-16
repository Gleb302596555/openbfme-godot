#!/usr/bin/env python3
"""Start addresses for the mod's unused fields: the field audit's parsed-unused / not-read rows with their owner class's uncited RotWK
functions from the function map (owner 2026-11-16, the cheap link between audit task 1 and task 2).

    field_gaps.py --audit <field-audit-vN/field-audit.tsv> [--map docs/ennorath/RW_FUNCTION_MAP.tsv]
                  [--inventory <rw_function_blocks.json> --game <game.dat>] [--out docs/ennorath/FIELD_GAPS.tsv]

A row's functions are the owner class's `specific none` functions (tools/mod/matrix_gaps.py: reached from at most three classes, not
trivial, not getModuleNameKey / xfer / a deleting destructor). They are candidates, not the field's reader: the reader can be one of them,
a function ported without a citation, or a function of another class that reads the module's data. A row with `how` = manual or a
`stop` may already carry a verdict ("RotWK does not read it", TOP20 / docs/STOPS.md): read it first. The task still proves the reader
(parser -> offset -> every reader, PROCESS 5 step 4). Only block, class and field names, counts and RW addresses are written: no values.
The map starts from module tables only: fields of top-level blocks (Object, Weapon, Locomotor, Upgrade, ...) get no candidates.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_gaps as mg  # noqa: E402

STATUSES = ("parsed-unused", "not-read")
HEADER = ["status", "block", "owner", "field", "defs", "how", "stop", "owner in map", "map none", "specific none", "map partial",
          "candidate functions (address@depth, smallest depth first) / note"]


def defs_of(row) -> int:
    try:
        return int(row.get("defs") or 0)
    except ValueError:
        return 0


def join(audit_rows, gaps, classes, reach=None, sizes=None, skip=None):
    rows = []
    for r in audit_rows:
        if r.get("status") not in STATUSES:
            continue
        owner = r.get("owner") or ""
        base = [r["status"], r.get("block", ""), owner, r.get("field", ""), str(defs_of(r)), r.get("how", ""), r.get("stop", "")]
        if owner not in classes:
            rows.append(base + ["no", "", "", "", "the owner is not a module class of the map (an INI block, or a class the mod's objects do not use)"])
            continue
        none, specific, partial = mg.class_gaps(gaps.get(owner, {}), reach, sizes, skip)
        note = mg.format_functions(specific) or "no specific none function: the reader is ported without a citation, shared, or in another class"
        rows.append(base + ["yes", str(none), str(len(specific)), str(partial), note])
    rows.sort(key=lambda x: (-int(x[4]), x[1], x[2], x[3]))
    return rows


def write(rows, path: Path):
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        w = csv.writer(stream, delimiter="\t", lineterminator="\n")
        w.writerow(HEADER)
        w.writerows(rows)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--audit", required=True, type=Path)
    ap.add_argument("--map", type=Path, default=Path("docs/ennorath/RW_FUNCTION_MAP.tsv"))
    ap.add_argument("--inventory", type=Path, default=Path("workspace/rebuild/rw_function_blocks.json"))
    ap.add_argument("--game", type=Path)
    ap.add_argument("--out", type=Path, default=Path("docs/ennorath/FIELD_GAPS.tsv"))
    a = ap.parse_args(argv)
    with a.audit.open(encoding="utf-8", newline="") as stream:
        audit_rows = list(csv.DictReader(stream, delimiter="\t"))
    gaps, classes, reach = mg.read_map(a.map)
    sizes, skip = mg.load_filters(a.inventory, a.game)
    rows = join(audit_rows, gaps, classes, reach, sizes, skip)
    write(rows, a.out)
    in_map = sum(1 for r in rows if r[7] == "yes")
    with_candidates = sum(1 for r in rows if r[7] == "yes" and r[9] not in ("", "0"))
    print(f"{len(rows)} unused fields, {in_map} of a module class in the map, {with_candidates} with candidate functions -> {a.out}"
          + ("" if skip else " (without --game: boilerplate not filtered)"))


if __name__ == "__main__":
    main()

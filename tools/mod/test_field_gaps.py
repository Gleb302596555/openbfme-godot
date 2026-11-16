"""Synthetic tests of tools/mod/field_gaps.py: a hand-written audit and a small function map, no game data."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import field_gaps as fg  # noqa: E402
import matrix_gaps as mg  # noqa: E402

MAP = "module\tdepth\tfunction\tstatus\tcalled from\tvcalls\tneighbour cited\tdecomp\n" \
      "SpecialAbilityUpdate\t0\t0x00852B14\tnone\t\t\t\t\n" \
      "SpecialAbilityUpdate\t1\t0x00852C00\tnone\t\t\t\t\n" \
      "SpecialAbilityUpdate\t0\t0x00851000\tported\t\t\t\t\n" \
      "ArmorUpgrade\t0\t0x00500000\tported\t\t\t\t\n"

AUDIT = [
    {"status": "parsed-unused", "block": "object", "owner": "SpecialAbilityUpdate", "field": "PreTriggerUnstealthTime", "defs": "40", "how": "struct", "stop": "S-1"},
    {"status": "not-read", "block": "object", "owner": "ArmorUpgrade", "field": "Unknown", "defs": "3"},
    {"status": "not-read", "block": "gamedata", "owner": "", "field": "SomeTiming", "defs": "1"},
    {"status": "used", "block": "object", "owner": "SpecialAbilityUpdate", "field": "SpecialPowerTemplate", "defs": "900"},
]


def test_unused_fields_get_their_owner_class_candidates(tmp_path):
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    rows = fg.join(AUDIT, gaps, classes, reach, {0x852C00: 4})
    assert [r[3] for r in rows] == ["PreTriggerUnstealthTime", "Unknown", "SomeTiming"]  # by definitions, the used field left out
    first = rows[0]
    assert first[5:] == ["struct", "S-1", "yes", "2", "1", "0", "0x00852B14@0"]  # the 4-byte function is trivial
    assert rows[1][7] == "yes" and rows[1][9] == "0" and rows[1][11].startswith("no specific none function")
    assert rows[2][7] == "no" and rows[2][11].startswith("the owner is not a module class")


def test_write_has_the_header(tmp_path):
    out = tmp_path / "FIELD_GAPS.tsv"
    fg.write([["parsed-unused", "object", "C", "F", "1", "struct", "", "yes", "1", "1", "0", "0x00400000@0"]], out)
    lines = out.read_text(encoding="utf-8").splitlines()
    assert lines[0].split("\t") == fg.HEADER and lines[1].endswith("0x00400000@0")

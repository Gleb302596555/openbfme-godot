"""Synthetic tests of tools/mod/matrix_gaps.py: no game data, a hand-written INI tree and a three-row function map."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_graph as eg  # noqa: E402
import matrix_gaps as mg  # noqa: E402

INI = """
CommandButton Command_Charge
  Command = SPECIAL_POWER
  SpecialPower = SpecialAbilityCharge
End
CommandButton Command_Formation
  Command = HORDE_TOGGLE_FORMATION
End
CommandButton Command_Armor
  Command = OBJECT_UPGRADE
  Upgrade = Upgrade_Armor
End
Object TestSoldier
  Behavior = SpecialAbilityUpdate ModuleTag_Charge
    SpecialPowerTemplate = SpecialAbilityCharge
  End
  Behavior = ArmorUpgrade ModuleTag_Armor
    TriggeredBy = Upgrade_Armor
  End
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
End
Object TestHorde
  Behavior = HordeContain ModuleTag_Contain
    RankInfo = RankNumber:1 UnitType:TestSoldier Position:X:0 Y:0
  End
  Behavior = SpecialPowerModule ModuleTag_Starter
    SpecialPowerTemplate = SpecialAbilityCharge
  End
End
"""

MAP = "module\tdepth\tfunction\tstatus\tcalled from\tvcalls\tneighbour cited\tdecomp\n" \
      "SpecialAbilityUpdate\t0\t0x00852B14\tnone\t\t\t\t\n" \
      "SpecialAbilityUpdate\t1\t0x00852000\tpartial\t\t\t\t\n" \
      "SpecialAbilityUpdate\t2\t0x00852B14\tnone\t\t\t\t\n" \
      "SpecialPowerModule\t0\t0x00897E01\tported\t\t\t\t\n" \
      "ArmorUpgrade\t0\t0x00500000\tported\t\t\t\t\n"


def db_of(text: str) -> eg.Db:
    db = eg.Db()
    eg.parse([(line, "test.ini") for line in text.strip().splitlines()], db)
    return db


def test_horde_button_links_its_own_and_its_members_modules(tmp_path):
    db = db_of(INI)
    assert mg.members(db, "TestHorde") == ["TestSoldier"]
    links = mg.linked_modules(db, "TestHorde", {"specialabilitycharge"})
    assert ("TestHorde", "ModuleTag_Starter", "SpecialPowerModule") in links
    assert ("TestSoldier", "ModuleTag_Charge", "SpecialAbilityUpdate") in links
    assert all(cls != "ActiveBody" for _o, _t, cls in links)


def test_join_lists_none_functions_smallest_depth_first_and_notes_unlinked(tmp_path):
    db = db_of(INI)
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    assert gaps["SpecialAbilityUpdate"][0x852B14] == ("none", 0)  # the depth-2 repeat keeps depth 0
    matrix = [{"faction": "F", "object": "TestHorde", "button": "Command_Charge", "result": "NO-FIRE", "detail": ""},
              {"faction": "F", "object": "TestSoldier", "button": "Command_Formation", "result": "FORMATION", "detail": ""},
              {"faction": "F", "object": "TestSoldier", "button": "Command_Armor", "result": "NOT-APPLIED", "detail": ""}]
    rows = mg.join(db, matrix, gaps, classes, reach)
    ability = [r for r in rows if r[8] == "SpecialAbilityUpdate"]
    assert ability and ability[0][9] == "1" and ability[0][10] == "1" and ability[0][11] == "1" and ability[0][12] == "0x00852B14@0"
    starter = [r for r in rows if r[8] == "SpecialPowerModule"]
    assert starter and starter[0][9] == "0"  # ported: no gap
    formation = [r for r in rows if r[2] == "Command_Formation"]
    assert formation == [["F", "TestSoldier", "Command_Formation", "FORMATION", "HORDE_TOGGLE_FORMATION", "", "", "", "-", "", "", "",
                          "no power or upgrade on the button"]]
    armor = [r for r in rows if r[2] == "Command_Armor"]
    assert armor and armor[0][8] == "ArmorUpgrade" and armor[0][9] == "0"


def test_reports_rank_classes_by_failing_buttons(tmp_path):
    db = db_of(INI)
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    matrix = [{"faction": "F", "object": "TestHorde", "button": "Command_Charge", "result": "NO-FIRE", "detail": ""},
              {"faction": "F", "object": "TestSoldier", "button": "Command_Charge", "result": "NO-EFFECT", "detail": ""}]
    rows = mg.mark_main(mg.join(db, matrix, gaps, classes, reach))
    out = tmp_path / "GAPS"
    mg.write(rows, out, "cmd", matrix)
    md = out.with_suffix(".md").read_text(encoding="utf-8")
    assert "| SpecialAbilityUpdate | 2 | 1 | 1 | 1 | 0x00852B14@0 |" in md
    assert out.with_suffix(".tsv").read_text(encoding="utf-8").splitlines()[0].startswith("faction\tobject\tbutton")


def test_shared_helpers_are_not_specific(tmp_path):
    db = db_of(INI)
    m = tmp_path / "map.tsv"
    shared = "".join(f"Class{i}\t0\t0x00400000\tnone\t\t\t\t\n" for i in range(5))
    m.write_text(MAP + shared + "SpecialAbilityUpdate\t1\t0x00400000\tnone\t\t\t\t\n", encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    assert reach[0x400000] == 6
    matrix = [{"faction": "F", "object": "TestSoldier", "button": "Command_Charge", "result": "NO-FIRE", "detail": ""}]
    row = [r for r in mg.join(db, matrix, gaps, classes, reach) if r[8] == "SpecialAbilityUpdate"][0]
    assert row[9] == "2" and row[10] == "1" and "0x00400000" not in row[12]


def test_trivial_functions_are_not_listed(tmp_path):
    db = db_of(INI)
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    matrix = [{"faction": "F", "object": "TestSoldier", "button": "Command_Charge", "result": "NO-FIRE", "detail": ""}]
    row = [r for r in mg.join(db, matrix, gaps, classes, reach, {0x852B14: 6}) if r[8] == "SpecialAbilityUpdate"][0]
    assert row[9] == "1" and row[10] == "0" and row[12] == ""


def test_boilerplate_functions_are_not_listed(tmp_path):
    db = db_of(INI)
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    matrix = [{"faction": "F", "object": "TestSoldier", "button": "Command_Charge", "result": "NO-FIRE", "detail": ""}]
    row = [r for r in mg.join(db, matrix, gaps, classes, reach, {}, {0x852B14: "xfer"}) if r[8] == "SpecialAbilityUpdate"][0]
    assert row[9] == "1" and row[10] == "0"


def test_main_class_is_the_most_specific_link():
    def row(obj, btn, cls, specific):
        return ["F", obj, btn, "NO-FIRE", "SPECIAL_POWER", "p", obj, "T", cls, "1", str(specific), "0", ""]
    rows = [row("A", "B1", "SpecialPowerModule", 4), row("A", "B1", "RepairSpecialPower", 1),
            row("C", "B2", "SpecialPowerModule", 4), row("C", "B2", "OCLSpecialPower", 6),
            row("D", "B3", "SpecialPowerModule", 4),
            ["F", "E", "B4", "FORMATION", "HORDE_TOGGLE_FORMATION", "", "", "", "-", "", "", "", "no power or upgrade on the button"]]
    marked = {(r[1], r[8]): r[13] for r in mg.mark_main(rows)}
    assert marked[("A", "RepairSpecialPower")] == "yes" and marked[("A", "SpecialPowerModule")] == ""
    assert marked[("C", "OCLSpecialPower")] == "yes"
    assert marked[("D", "SpecialPowerModule")] == "yes"  # its only link
    assert marked[("E", "-")] == ""


def test_read_matrix_drops_green_rows(tmp_path):
    p = tmp_path / "m.tsv"
    p.write_text("# head\nfaction\tobject\tbutton\tresult\tdetail\nF\tA\tB\tPASS\t\nF\tA\tC\tNO-FIRE\tx\nF\tA\tD\tDISABLED\t\n", encoding="utf-8")
    assert [r["button"] for r in mg.read_matrix(p)] == ["C"]

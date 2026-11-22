from __future__ import annotations

import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_review as mr  # noqa: E402

INI = """Object TestFort
  CommandSet = FortSet
  Behavior = CommandSetUpgrade ModuleTag_Up
    CommandSet = FortSetUp
    TriggeredBy = Upgrade_X
  End
End

Object TestHero
  CommandSet = HeroSet
  Behavior = OCLSpecialPower ModuleTag_P
    SpecialPowerTemplate = SpecialAbilityListed
  End
End

Object TestMember
  CommandSet = MemberSet
End

Object TestHorde
  CommandSet = MemberSet
  Behavior = HordeContain ModuleTag_H
    InitialPayload = TestMember 5
  End
End
"""
SETS = """CommandSet FortSet
  1 = Command_Hidden
End
CommandSet FortSetUp
  2 = Command_OnlyUpgraded
End
CommandSet HeroSet
  1 = Command_Listed
  2 = Command_Unlisted
End
CommandSet MemberSet
  1 = Command_Formation
End
"""
BUTTONS = """CommandButton Command_Hidden
  Command = SPECIAL_POWER
  Options = HIDE_WHILE_DISABLED
End
CommandButton Command_OnlyUpgraded
  Command = SPECIAL_POWER
End
CommandButton Command_Listed
  Command = SPECIAL_POWER
  SpecialPower = SpecialAbilityListed
End
CommandButton Command_Unlisted
  Command = SPECIAL_POWER
  SpecialPower = SpecialAbilityNowhere
End
CommandButton Command_Formation
  Command = HORDE_TOGGLE_FORMATION
End
CommandButton Command_Stray
  Command = SPECIAL_POWER
End
"""


def mod(tmp_path) -> mr.Mod:
    z = tmp_path / "ini.zip"
    with zipfile.ZipFile(z, "w") as w:
        w.writestr("data/ini/object/test.ini", INI)
        w.writestr("data/ini/commandset.ini", SETS)
        w.writestr("data/ini/commandbutton.inc", BUTTONS)
    return mr.Mod(z)


def test_each_rule_flags_the_sweeps_own_verdicts(tmp_path):
    m = mod(tmp_path)
    st = {
        ("FactionA", "TestFort", "Command_Hidden"): ("NOT-ON-BAR", ""),
        ("FactionA", "TestFort", "Command_OnlyUpgraded"): ("NO-EFFECT", ""),  # wearable through the upgrade set: no L1
        ("FactionA", "TestFort", "Command_Stray"): ("NO-EFFECT", ""),
        ("FactionA", "TestHero", "Command_Listed"): ("MOD-DATA", ""),
        ("FactionA", "TestHero", "Command_Unlisted"): ("NO-MODULE", ""),
        ("FactionA", "TestMember", "Command_Formation"): ("FORMATION", ""),
        ("FactionA", "TestHorde", "Command_Formation"): ("FORMATION", ""),
        ("FactionB", "TestHorde", "Command_Formation"): ("PASS", ""),
    }
    f = mr.lint(st, m)
    rules = lambda k: {x.split(":")[0] for x in f.get(k, [])}
    assert rules(("FactionA", "TestFort", "Command_Hidden")) == {"L3"}
    assert rules(("FactionA", "TestFort", "Command_OnlyUpgraded")) == set()
    assert rules(("FactionA", "TestFort", "Command_Stray")) == {"L1"}
    assert rules(("FactionA", "TestHero", "Command_Listed")) == {"L4"}
    assert rules(("FactionA", "TestHero", "Command_Unlisted")) == {"L4"}
    assert rules(("FactionA", "TestMember", "Command_Formation")) == {"L5"}
    assert rules(("FactionA", "TestHorde", "Command_Formation")) == {"L2"}  # a horde: no L5, but it passes in FactionB
    assert rules(("FactionB", "TestHorde", "Command_Formation")) == set()


def test_status_takes_pass_over_failures():
    rows = [{"faction": "F", "object": "O", "button": "B", "result": "NO-EFFECT", "detail": "x"},
            {"faction": "F", "object": "O", "button": "B", "result": "PASS", "detail": "ok"},
            {"faction": "F", "object": "O", "button": "C", "result": "NO-FIRE", "detail": "y"}]
    st = mr.status(rows)
    assert st[("F", "O", "B")][0] == "PASS" and st[("F", "O", "C")][0] == "NO-FIRE"

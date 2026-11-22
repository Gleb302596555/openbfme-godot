from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_drafts as md  # noqa: E402


def test_draft_has_the_issue_sections_and_the_addresses():
    items = [(("FactionMen", "GondorHero", "Command_Smash"), ("NO-EFFECT", "fired but no result"), {"module tag": "ModuleTag_Smash"})]
    text = md.draft("WeaponFireSpecialAbilityUpdate", items, ["0x006A057C", "0x00893D61"], ["#574 Local 76: WeaponFire..."])
    assert text.startswith("Port <N>: WeaponFireSpecialAbilityUpdate: 1 red button of the matrix (NO-EFFECT 1)")
    for head in ("## Problem", "## Known", "## Task", "## Tests", "## In game", "## Do not touch", "## Done"):
        assert head in text
    assert "RW 0x006A057C" in text and "`Command_Smash`" in text and "#574" in text


def test_covering_matches_a_class_in_the_title_or_an_exact_button():
    issues = [{"number": 1, "title": "Port 9: OCLSpecialPower spawns", "body": ""},
              {"number": 2, "title": "Umbrella", "body": "mentions OCLSpecialPower among many"},
              {"number": 3, "title": "Other", "body": "the button Command_Smash fails"}]
    hits = md.covering(issues, "OCLSpecialPower", ["Command_Smash"])
    assert [h.split()[0] for h in hits] == ["#1", "#3"]

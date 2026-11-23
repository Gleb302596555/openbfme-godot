from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress_snapshot as ps  # noqa: E402

STOPS = {
    "S-1": ("Locomotor unread fields (Local 25)", "RotWK reads none of: 1) `MinTurnSpeed` (+0x54) ... 2) `StickToGround`"),
    "S-2": ("W3DTreeDraw behaviour", "Not ported: sway/wind, and fade (FadeDistance). " + "x" * 400 +
            " `DarkeningFactor` (+ 0x1C) has NO READER in RotWK 2.01"),
    "S-3": ("Pathfinder classification", "SlopeLimits ... the shore passes (no consumer found outside the A* cost)"[:40] + "y" * 400 +
            " no consumer found outside the A* cost"),
    "S-4": ("Some module", "`Foo` is not ported yet"),
}


def test_a_field_counts_only_when_a_stop_proves_retail_never_reads_it():
    assert ps.proven_unread("MinTurnSpeed", ["S-1"], STOPS)          # the verdict is the stop's title, the field is named
    assert ps.proven_unread("DarkeningFactor", ["S-2"], STOPS)       # the verdict stands next to the field's name
    assert not ps.proven_unread("FadeDistance", ["S-2"], STOPS)      # same stop, only "not ported" near this field
    assert not ps.proven_unread("SlopeLimits", ["S-3"], STOPS)       # a verdict about something else, far from the name
    assert not ps.proven_unread("Foo", ["S-4"], STOPS)               # not ported is not proven unread
    assert not ps.proven_unread("Bar", ["S-1"], STOPS)               # the stop does not name the field


def test_modules_missing_reads_the_live_module_queue(tmp_path):
    # 2026-11-22: MODULE_GAPS.md (a 2026-10-30 snapshot) still said 10 missing classes, all ported by then; the maps run's live table says 1
    audit = tmp_path / "field-audit.tsv"
    audit.write_text("x\n", encoding="utf-8")
    (tmp_path / "module-queue.tsv").write_text("class\tstatus\tdefinitions\ttype\nA\traw\t3\tBehavior\nB\tmissing\t1\tBehavior\n"
                                               "C\tparsed-only\t2\tDraw\n", encoding="utf-8")
    assert ps.modules_missing(audit, "| x | missing |\n| y | missing |\n") == 1
    assert ps.modules_missing(None, "| x | missing |\n| y | missing |\n") == 2

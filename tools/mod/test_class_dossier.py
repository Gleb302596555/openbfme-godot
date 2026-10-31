"""tools/mod/class_dossier.py over made-up INI text (no mod or game bytes)."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import class_dossier as cd  # noqa: E402

INI = """Object Carrier
  Behavior = GiveThing Tag_1 ; a comment
    Range = 15.0
    Fade = 0.1 ; another
    Nested
      Inner = 1
    End
  End
  Behavior = Other Tag_2
    Range = 99
  End
End
ChildObject Second Carrier
  Behavior = GiveThing Tag_3
    Range = 20.0
  End
End
"""


def test_blocks_keep_their_own_fields_and_owner():
    blocks = cd.parse_blocks(INI.splitlines(), "GiveThing")
    assert blocks == [("Carrier", {"Range": "15.0", "Fade": "0.1"}), ("Second", {"Range": "20.0"})]


def test_field_census_counts_values():
    census = cd.field_census(cd.parse_blocks(INI.splitlines(), "givething"))
    assert census == ["  Fade: 1/2: 0.1 x1", "  Range: 2/2: 15.0 x1, 20.0 x1"]


def test_nearest_ranks_by_shared_fields_and_skips_itself():
    known = {"A": {"Range"}, "B": {"range", "fade"}, "C": {"Other"}, "Self": {"Range", "Fade"}}
    assert cd.nearest({"Range", "Fade"}, known, "Self") == [("B", 2), ("A", 1)]

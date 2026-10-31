"""tools/mod/upstream_sync.py over made-up text (no mod or game bytes)."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import upstream_sync as us  # noqa: E402


def test_conflicts_are_the_lines_before_the_messages():
    output = "abc123\ndocs/STOPS.md\nengine/x.cpp\n\nAuto-merging docs/STOPS.md\nCONFLICT (content): ...\n"
    assert us.conflicts_from_merge_tree(output) == ["docs/STOPS.md", "engine/x.cpp"]


def test_a_clean_merge_tree_output_has_no_conflicts():
    assert us.conflicts_from_merge_tree("abc123\n") == []


def test_a_class_is_generic_only_when_no_file_names_the_mod():
    bodies = {"a.cpp": "plain code", "t.cpp": "needs OPENBFME_ENNORATH_INSTALL"}
    assert us.is_generic(["a.cpp"], bodies.__getitem__)
    assert not us.is_generic(["a.cpp", "t.cpp"], bodies.__getitem__)
    assert not us.is_generic(["test_ennorath_x.cpp"], lambda _: "plain code")

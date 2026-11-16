"""Synthetic tests of tools/mod/citation_check.py (no game data)."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import citation_check as cc  # noqa: E402

INV = cc.Inventory([
    {"address": 0x401000, "blocks": [[0x401000, 0x20], [0x401100, 0x10]]},   # noncontiguous
    {"address": 0x402000, "blocks": [[0x402000, 0x40]]},
])

DIFF = """diff --git a/engine/src/A.cpp b/engine/src/A.cpp
--- a/engine/src/A.cpp
+++ b/engine/src/A.cpp
@@ -10,0 +11,3 @@
+// RW 0x401008: inside the first function
+// RW 0x402000: a start
+// RW 0x401105 RW 0x500000
diff --git a/engine/tests/T.cpp b/engine/tests/T.cpp
--- a/engine/tests/T.cpp
+++ b/engine/tests/T.cpp
@@ -1,0 +2 @@
+// RW 0x401008 in a test is not a port citation
"""


def test_owner_follows_noncontiguous_blocks():
    assert INV.owner(0x401008) == 0x401000
    assert INV.owner(0x401105) == 0x401000
    assert INV.owner(0x401050) is None  # a gap between the blocks
    assert INV.owner(0x500000) is None


def test_added_citations_only_under_engine_src():
    cites = cc.added_citations(DIFF)
    assert cites == [("engine/src/A.cpp", 11, 0x401008), ("engine/src/A.cpp", 12, 0x402000),
                     ("engine/src/A.cpp", 13, 0x401105), ("engine/src/A.cpp", 13, 0x500000)]


def test_interior_citation_warns_once_unless_the_start_is_cited():
    cites = cc.added_citations(DIFF)
    w = cc.interior_warnings(cites, INV, set())
    assert len(w) == 1 and "RW 0x401008" in w[0] and "RW 0x401000" in w[0]
    assert cc.interior_warnings(cites, INV, {0x401000}) == []


def test_a_start_cited_in_the_same_change_counts():
    cites = cc.added_citations(DIFF) + [("engine/src/B.cpp", 1, 0x401000)]
    assert cc.interior_warnings(cites, INV, set()) == []


def test_newly_ported_starts():
    cites = cc.added_citations(DIFF)
    assert cc.newly_ported(cites, INV, set()) == {0x402000}
    assert cc.newly_ported(cites, INV, {0x402000}) == set()

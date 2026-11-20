import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import stops_merge as sm  # noqa: E402

HEAD = "# Stops\n\n| id | kind | what |\n|---|---|---|\n"
R1 = "| S-1 | code | one: the first state |"
R2 = "| S-2 | code | two: ported the A part; not ported: the B part, the C part |"
R3 = "| S-3 | project | three |"


def text(*rows, tail="\n## Notes\n\nprose\n"):
    return HEAD + "\n".join(rows) + "\n" + tail


def test_rows_edited_on_one_side_each_take_that_side_and_nothing_doubles():
    base = text(R1, R2, R3)
    ours = text(R1.replace("first", "second"), R2, R3)
    theirs = text(R1, R2, R3.replace("three", "three, narrowed"))
    merged, conflicts = sm.merge(base, ours, theirs)
    assert not conflicts
    assert "second" in merged and "three, narrowed" in merged
    assert merged.count("| S-1 |") == merged.count("| S-3 |") == 1


def test_one_row_both_sides_edited_in_different_words_merges_word_by_word():
    base = text(R1, R2, R3)
    ours = text(R1, R2.replace("the A part", "the A part (PORT-280)"), R3)
    theirs = text(R1, R2.replace("the C part", "the C part (PORT-260)"), R3)
    merged, conflicts = sm.merge(base, ours, theirs)
    assert not conflicts
    row = next(l for l in merged.split("\n") if l.startswith("| S-2 |"))
    assert "(PORT-280)" in row and "(PORT-260)" in row


def test_overlapping_edits_of_one_row_are_a_conflict_not_a_guess():
    base = text(R1, R2, R3)
    ours = text(R1, R2.replace("the B part", "the B part done"), R3)
    theirs = text(R1, R2.replace("the B part", "the B part dropped"), R3)
    merged, conflicts = sm.merge(base, ours, theirs)
    assert conflicts == ["S-2"]
    assert "<<<<<<< ours" in merged


def test_prose_and_added_and_removed_rows_merge_as_lines():
    base = text(R1, R2, R3)
    ours = text(R1, R2, R3, tail="\n## Notes\n\nprose, ours\n")
    theirs = text(R1, R3, "| S-4 | code | four |")
    merged, conflicts = sm.merge(base, ours, theirs)
    assert not conflicts
    assert "| S-2 |" not in merged and "| S-4 | code | four |" in merged and "prose, ours" in merged

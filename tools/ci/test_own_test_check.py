import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import own_test_check as ot  # noqa: E402

DIFF = ('+++ b/engine/tests/test_port_289.cpp\n+TEST_CASE("PORT-289 dead targets use the native gates")\n'
        '+TEST_CASE("PORT-289 type 0x8A and RepairSpecialPower")\n context TEST_CASE("PORT-217 old")\n')


def test_the_added_prefixes():
    assert ot.prefixes(DIFF) == ["PORT-289"]
    assert ot.prefixes('+TEST_CASE("LOCAL-90: bodies")\n') == ["LOCAL-90"]


def test_pr_721_without_a_run_is_refused_and_a_passing_line_admits():
    # PR 721's description: a cmake build, no run of its test
    assert ot.missing(DIFF, ["`cmake --build engine/build -j1 --target openbfme_tests` (passed)"]) == ["PORT-289"]
    ok = "PORT-289*: [doctest] test cases:  4 |  4 passed | 0 failed | 7020 skipped"
    assert ot.missing(DIFF, ["", ok]) == []


def test_a_red_or_empty_run_and_another_prefix_do_not_count():
    assert ot.missing(DIFF, ["PORT-289*: [doctest] test cases: 4 | 3 passed | 1 failed | 7 skipped"]) == ["PORT-289"]
    assert ot.missing(DIFF, ["PORT-289*: [doctest] test cases: 0 | 0 passed | 0 failed | 7 skipped"]) == ["PORT-289"]
    assert ot.missing(DIFF, ["PORT-2890*: [doctest] test cases: 4 | 4 passed | 0 failed | 7 skipped"]) == ["PORT-289"]
    assert ot.missing("+++ b/docs/x.md\n+text\n", []) == []  # no added test: not this check's

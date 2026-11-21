"""Tests of tools/ci/pr_rules.py (no git, no network): pytest tools/ci/test_pr_rules.py"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pr_rules as R  # noqa: E402

PORT_BODY = "Closes #1\n## Проверка\nвидно в игре: нет\nправка местная\n## Карты\nнет\n## Осталось\nничего\n"


def diff(path: str, added: list[str], new: bool = True) -> str:
    head = f"diff --git a/{path} b/{path}\n" + ("new file mode 100644\n" if new else "") + f"--- a/{path}\n+++ b/{path}\n@@ -0,0 +1 @@\n"
    return head + "".join("+" + l + "\n" for l in added)


def kinds(found):
    return [k for k, _, _ in found]


def test_sections_port_needs_four_blocks_local_two():
    assert R.sections("Port 12: x", PORT_BODY) == []
    assert kinds(R.sections("Port 12: x", "Closes #1\n## Summary\n")) == ["error"]
    assert R.sections("Local 9: x", "в игре: да\n**Карты**: нет") == []          # a Local PR: in-game and maps only
    assert R.sections("Merge batch 2026-11-20h", "") == []                        # not a task PR
    assert R.sections("PORT-283: x", PORT_BODY.replace("## Осталось\nничего\n", "")) != []


def test_a_test_case_names_its_source_in_its_lines_or_the_new_files_header():
    no_source = diff("engine/tests/test_port_1.cpp", ['TEST_CASE("PORT-1 a")', "{", "\tCHECK(x == 3);", "}"])
    assert kinds(R.test_sources(no_source)) == ["error"]
    in_body = diff("engine/tests/test_port_1.cpp", ['TEST_CASE("PORT-1 a")', "{", "\tCHECK(x == 3); // RW 0x8A25DC", "}"])
    assert R.test_sources(in_body) == []
    in_header = diff("engine/tests/test_port_1.cpp", ["// expectations: data\\ini\\object\\x.ini", "#include \"doctest.h\"",
                                                      'TEST_CASE("PORT-1 a")', "{", "\tCHECK(x == 3);", "}"])
    assert R.test_sources(in_header) == []
    above = diff("engine/tests/test_port_1.cpp", ["// INFERENCE: the donor's order", 'TEST_CASE("PORT-1 a")', "{", "}"], new=False)
    assert R.test_sources(above) == []


def test_uncited_engine_code_is_a_warning_not_an_error():
    block = diff("engine/src/GameLogic/X.cpp", ["\tint a = 1;", "\tint b = 2;", "\tdo(a, b);"], new=False)
    assert kinds(R.code_citations(block)) == ["warning"]
    cited = diff("engine/src/GameLogic/X.cpp", ["\t// RW 0x6F0000: the step", "\tint a = 1;", "\tint b = 2;", "\tdo(a, b);"], new=False)
    assert R.code_citations(cited) == []
    assert R.code_citations(diff("engine/tests/test_x.cpp", ["int a;", "int b;", "int c;"])) == []   # tests are rule 2's


def test_no_issue_number_for_upstream():
    assert kinds(R.upstream_refs("Port 12 (#463): fix", "")) == ["error"]
    assert R.upstream_refs("Port 12 (fork issue 463): fix", "") == []
    assert kinds(R.upstream_refs("", diff("engine/src/a.cpp", ["// see #12"]))) == ["error"]
    assert R.upstream_refs("", diff("engine/src/a.cpp", ["#include <x>", "#define N 3", "Color c = 0x#FFFFFF;"])) == []

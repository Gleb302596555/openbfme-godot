"""tools/test_select.py over a made-up source tree."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_select as ts  # noqa: E402


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    def write(name: str, text: str) -> None:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    write("engine/src/Low.h", "// nothing\n")
    write("engine/src/Low.cpp", '#include "Low.h"\n')
    write("engine/src/High.h", '#include "Low.h"\n')
    write("engine/src/Lone.cpp", "// no header\n")
    write("engine/tests/test_a.cpp", '#include "High.h"\nTEST_CASE("a one") {}\nTEST_CASE("a two, with comma") {}\n')
    write("engine/tests/test_b.cpp", 'TEST_CASE("b one") {}\n')
    write("engine/tests/test_c.cpp", 'TEST_CASE("c one") {}\n')
    return tmp_path


def test_a_changed_test_selects_itself_and_the_pin(repo: Path):
    assert ts.select(["engine/tests/test_b.cpp"], repo)[2] == ["b one", "module coverage*"]


def test_a_source_reaches_tests_through_a_chain_of_headers(repo: Path):
    full, _, masks = ts.select(["engine/src/Low.cpp"], repo)
    assert not full
    assert masks == ["a one", "a two*", "module coverage*"]


def test_a_source_without_a_header_asks_for_the_full_suite(repo: Path):
    full, why, _ = ts.select(["engine/src/Lone.cpp"], repo)
    assert full and "Lone.cpp" in why


def test_a_file_no_test_can_be_traced_from_asks_for_the_full_suite(repo: Path):
    assert ts.select(["godot/scripts/game.gd"], repo)[0]
    assert ts.select(["engine/CMakeLists.txt"], repo)[0]


def test_a_new_class_does_not_ask_for_the_full_suite_through_its_registration(repo: Path):
    changed = ["engine/cmake/Modgap2.cmake", "engine/src/GameLogic/Module/LogicModules.cpp", "engine/tests/test_b.cpp"]
    assert ts.select(changed, repo)[2] == ["b one", "module coverage*"]


def test_documents_and_tools_select_only_the_pin(repo: Path):
    assert ts.select(["docs/x.md", "tools/y.py"], repo) == (False, "0 of 3 test files", ["module coverage*"])


def test_a_wide_change_asks_for_the_full_suite(repo: Path):
    for name in ("test_b.cpp", "test_c.cpp"):
        path = repo / "engine/tests" / name
        path.write_text('#include "High.h"\n' + path.read_text(encoding="utf-8"), encoding="utf-8")
    full, why, _ = ts.select(["engine/src/High.h"], repo)
    assert full and "3 of 3" in why

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


def test_a_part_of_a_split_class_counts_as_the_header_it_shares(repo: Path):
    (repo / "engine/src/HighCombat.cpp").write_text('#include "High.h"\n#include "Low.h"\n', encoding="utf-8")
    full, _, masks = ts.select(["engine/src/HighCombat.cpp"], repo)
    assert not full
    assert masks == ["a one", "a two*", "module coverage*"]


def test_a_source_that_only_includes_an_unrelated_header_still_asks_for_the_full_suite(repo: Path):
    (repo / "engine/src/Other.cpp").write_text('#include "High.h"\n', encoding="utf-8")
    full, why, _ = ts.select(["engine/src/Other.cpp"], repo)
    assert full and "Other.cpp" in why


def test_a_data_file_selects_the_tests_that_name_it(repo: Path):
    path = repo / "engine/tests/test_c.cpp"
    path.write_text('// reads big_battle.hashes\n' + path.read_text(encoding="utf-8"), encoding="utf-8")
    assert ts.select(["engine/tests/data/perf1/big_battle.hashes"], repo)[2] == ["c one", "module coverage*"]


def test_a_data_file_nothing_names_asks_for_the_full_suite(repo: Path):
    full, why, _ = ts.select(["engine/tests/data/orphan.tsv"], repo)
    assert full and "orphan.tsv" in why


def test_run_refuses_the_whole_suite_without_full(repo: Path, monkeypatch, capsys):
    monkeypatch.setattr(ts, "REPO", repo)
    monkeypatch.setattr(ts, "changed_files", lambda base, r: ["godot/scripts/game.gd"])
    monkeypatch.setattr(ts, "load_map", lambda r, base: None)
    called = []
    monkeypatch.setattr(ts.subprocess, "run", lambda *a, **k: called.append(a))
    monkeypatch.setattr(sys, "argv", ["test_select.py", "--run", "tests.exe"])
    assert ts.main() == 2
    assert not called
    assert "not running the whole suite" in capsys.readouterr().err


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


def more_tests(repo: Path) -> None:
    """Enough test files that two selected ones are not a wide change."""
    for name in "defg":
        (repo / f"engine/tests/test_{name}.cpp").write_text(f'TEST_CASE("{name} one") {{}}\n', encoding="utf-8")


def recorded(**tests) -> ts.TestMap:
    """A map: test file (without engine/tests/ and .cpp) -> the source files it ran."""
    files = sorted({name for found in tests.values() for name in found})
    return ts.TestMap({"commit": "0" * 40, "files": files, "tests": {f"engine/tests/{t}.cpp": [files.index(n) for n in found] for t, found in tests.items()}, "unmapped": []})


def test_with_the_map_a_source_selects_who_ran_it_not_who_includes_its_header(repo: Path):
    the_map = recorded(test_a=[], test_b=["engine/src/Low.cpp"], test_c=[])
    assert ts.select(["engine/src/Low.cpp"], repo, the_map)[2] == ["b one", "module coverage*"]


def test_with_the_map_a_source_without_a_header_needs_no_full_suite(repo: Path):
    the_map = recorded(test_a=[], test_b=[], test_c=["engine/src/Lone.cpp"])
    full, _, masks = ts.select(["engine/src/Lone.cpp"], repo, the_map)
    assert not full and masks == ["c one", "module coverage*"]


def test_with_the_map_a_header_selects_who_ran_a_file_including_it(repo: Path):
    more_tests(repo)
    the_map = recorded(test_a=[], test_b=["engine/src/Low.cpp"], test_c=[])
    assert ts.select(["engine/src/Low.h"], repo, the_map)[2] == ["a one", "a two*", "b one", "module coverage*"]


def test_a_test_file_the_map_does_not_know_keeps_the_include_rule(repo: Path):
    more_tests(repo)
    the_map = ts.TestMap({"commit": "0" * 40, "files": ["engine/src/Low.cpp"], "tests": {"engine/tests/test_b.cpp": [0], "engine/tests/test_c.cpp": []}, "unmapped": []})
    assert ts.select(["engine/src/Low.cpp"], repo, the_map)[2] == ["a one", "a two*", "b one", "module coverage*"]


def test_a_test_that_starts_a_helper_keeps_the_include_rule(repo: Path):
    more_tests(repo)
    the_map = ts.TestMap({"commit": "0" * 40, "files": ["engine/src/Low.cpp"], "tests": {"engine/tests/test_a.cpp": [], "engine/tests/test_b.cpp": [0]}, "unmapped": ["engine/tests/test_a.cpp"]})
    assert ts.select(["engine/src/Low.cpp"], repo, the_map)[2] == ["a one", "a two*", "b one", "module coverage*"]


def test_a_source_the_map_never_saw_keeps_the_include_rule(repo: Path):
    the_map = recorded(test_a=[], test_b=["engine/src/Low.h"], test_c=[])
    full, _, masks = ts.select(["engine/src/Low.cpp"], repo, the_map)
    assert not full and masks == ["a one", "a two*", "module coverage*"]


def test_a_source_with_code_before_main_keeps_the_include_rule_too(repo: Path):
    more_tests(repo)
    the_map = ts.TestMap({"commit": "0" * 40, "files": ["engine/src/Low.cpp"], "tests": {"engine/tests/test_a.cpp": [], "engine/tests/test_b.cpp": [0], "engine/tests/test_c.cpp": []}, "unmapped": [], "startup": [0]})
    assert ts.select(["engine/src/Low.cpp"], repo, the_map)[2] == ["a one", "a two*", "b one", "module coverage*"]


def test_a_source_without_a_header_with_code_before_main_asks_for_the_full_suite(repo: Path):
    the_map = ts.TestMap({"commit": "0" * 40, "files": ["engine/src/Lone.cpp"], "tests": {"engine/tests/test_a.cpp": []}, "unmapped": [], "startup": [0]})
    assert ts.select(["engine/src/Lone.cpp"], repo, the_map)[0]

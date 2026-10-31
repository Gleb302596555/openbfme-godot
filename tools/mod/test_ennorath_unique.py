"""tools/mod/ennorath_unique.py over made-up table rows and a throwaway git repository (no mod or game bytes)."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_unique as eu  # noqa: E402

ROWS = """| 2 | `RubbleRiseUpdate` | missing | 0 | — | 6 (3) | — | x | S-140 | НЕ НАЧАТО |
| 3 | `FlingThing` | partial (агент B) | 4 | Mordor | 1 (1) | — | x | S-1 | ЗАНЯТО |
| 4 | `DoneThing` | ported (2026-10-30) | 75 | все 11 | 13 (2) | 6 | x | S-2 | ВЫПОЛНЕНО |
| 13 | условие скрипта `PLAYER_BUILT_UPGRADE` | не перенесено | — | ИИ | — | 600 | x | S-3 | НЕ НАЧАТО |
"""


def test_open_classes_skip_ported_rows_and_script_conditions():
    assert eu.open_classes(ROWS) == {"RubbleRiseUpdate": (2, "missing", "0"), "FlingThing": (3, "partial (агент B)", "4")}


def test_retail_objects_reads_the_first_count():
    assert eu.retail_objects("AODCrushCollide\t4\t2\t4\nNobody\t0\t0\t0\n") == {"AODCrushCollide": 4, "Nobody": 0}


@pytest.mark.parametrize(
    "name,kind",
    [("InMain", "upstream-main"), ("InWork", "upstream-work"), ("Retail", "shared"), ("ZeroRetail", "ennorath-only"), ("Absent", "ennorath-only")],
)
def test_classify_takes_the_first_kind_that_applies(name, kind):
    retail = {"InMain": 9, "InWork": 9, "Retail": 3, "ZeroRetail": 0}
    assert eu.classify(name, {"InMain"}, {"InWork": ["PR #7"]}, retail) == kind


def test_update_gaps_replaces_only_between_the_markers():
    text = f"before\r\n{eu.BEGIN}\r\nold\r\n{eu.END}\r\nafter\r\n"
    assert eu.update_gaps(text, "a\nb") == f"before\r\n{eu.BEGIN}\r\na\r\nb\r\n{eu.END}\r\nafter\r\n"
    with pytest.raises(ValueError):
        eu.update_gaps("no markers", "a")


def run(repo: Path, *args: str) -> None:
    subprocess.run(["git", "-C", str(repo), "-c", "user.name=t", "-c", "user.email=t@t", *args], check=True, capture_output=True)


def test_found_in_sees_definitions_and_bindings_not_data_classes_or_forward_declarations(tmp_path, monkeypatch):
    run(tmp_path, "init", "-q")
    src = tmp_path / "engine/src"
    src.mkdir(parents=True)
    (src / "a.h").write_text(
        "class Defined : public UpdateModule\n{\n};\n"
        "class ForwardOnly;\n"
        "class DataOnlyModuleData : public ModuleData\n{\n};\n"
        "void f() { bindRuntime<Bound, BoundData>(m, \"Bound\"); }\n"
        "class StandIn : public UnportedUpdateModule\n{\n};\n"
        "void h() { if (cls.name == \"Member\") { read(); } if (p->name == \"Arrow\") { read(); } }\n"
        "void g() { if (name == \"ByName\") { return std::make_unique<Other>(); } modules.bindModuleProc(\"ByProc\", x); }\n",
        encoding="utf-8",
    )
    run(tmp_path, "add", ".")
    run(tmp_path, "commit", "-q", "-m", "x")
    monkeypatch.setattr(eu, "REPO", tmp_path)
    names = ["Arrow", "Bound", "ByName", "ByProc", "DataOnly", "Defined", "ForwardOnly", "Member", "Missing", "StandIn"]
    assert set(eu.found_in(eu.runtime_pattern(names), ["HEAD"])) == {"Bound", "ByName", "ByProc", "Defined"}


def test_registry_names():
    assert eu.registry_names('{"classes": [{"name": "BoredUpdate"}, {"name": "OCLUpdate"}]}') == {"BoredUpdate", "OCLUpdate"}

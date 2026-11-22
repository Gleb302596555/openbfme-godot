from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fragments  # noqa: E402


def setup(tmp_path, monkeypatch, sid: int):
    stops = tmp_path / "STOPS.md"
    stops.write_text("| S-7000 | code | old | x | y |\n", encoding="utf-8")
    d = tmp_path / "stops.d"
    d.mkdir()
    (d / f"S-{sid}.md").write_text(f"| S-{sid} | code | new | x | y |\n", encoding="utf-8")
    monkeypatch.setattr(fragments, "STOPS", stops)
    monkeypatch.setattr(fragments, "STOPS_D", d)
    monkeypatch.setattr(fragments, "REPO", tmp_path)
    monkeypatch.setattr(fragments, "NIGHT_D", tmp_path / "none")
    monkeypatch.setattr(fragments, "reservations", lambda: [("578", 7423, 7425), ("566", 7390, 7392)])
    monkeypatch.delenv("OPENBFME_ISSUE", raising=False)
    monkeypatch.setattr(fragments, "my_issue", lambda: "")


def test_an_id_reserved_for_another_issue_is_an_error(tmp_path, monkeypatch):
    setup(tmp_path, monkeypatch, 7424)
    problems = fragments.check("566")
    assert len(problems) == 1 and "reserved for issue #578" in problems[0] and "S-7390..S-7392" in problems[0]


def test_own_range_free_id_and_unknown_issue_pass(tmp_path, monkeypatch):
    setup(tmp_path, monkeypatch, 7424)
    assert fragments.check("578") == []
    assert fragments.check("") == []  # a batch tree: no issue, no reservation check
    setup(tmp_path / "b", monkeypatch, 7500) if (tmp_path / "b").mkdir() is None else None
    assert fragments.check("566") == []  # an id nobody reserved (`fragments.py next`)

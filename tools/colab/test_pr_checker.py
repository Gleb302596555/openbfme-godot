from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pr_checker as pc  # noqa: E402

A, B = "a" * 40, "b" * 40


def test_the_newest_real_check_is_the_last_report_not_a_skip_note():
    comments = [{"body": f"<!-- prcheck {A} -->\n**Проверка**"}, {"body": f"<!-- prcheck {B} -->\n⏸️ не проверялся заново"},
                {"body": "arena-pr=523 status=rework"}]
    assert pc.last_checked_head(comments) == A
    assert pc.last_checked_head([{"body": "nothing"}]) == ""


def test_a_restored_state_learns_checked_and_reworked_prs_from_github(monkeypatch):
    saved = []
    monkeypatch.setattr(pc, "save_state", lambda s: saved.append(1))
    views = {"1": [{"body": f"<!-- prcheck {A} -->\nok"}], "2": [{"body": f"<!-- prcheck {A} -->\nok"}], "3": []}
    monkeypatch.setattr(pc, "gh", lambda *a, **k: __import__("json").dumps({"comments": views[a[2]]}))
    state = {"checked": {}}
    for n, head in (("1", A), ("2", B), ("3", B)):
        pc.sync_from_comments({"number": int(n), "headRefOid": head, "isDraft": False}, state)
    assert state["checked"] == {"1": A, "2": A}   # 1: done on this head; 2: checked before, new head = rework first; 3: new
    calls = len(saved)
    pc.sync_from_comments({"number": 3, "headRefOid": B, "isDraft": False}, state)   # once per head
    assert len(saved) == calls


def test_a_hash_test_red_on_main_that_the_pr_moves_is_seen_by_its_values():
    main_out = "/x/test_ai.cpp:120: ERROR: CHECK( hash == 0x1234 ) is NOT correct!\n  values: CHECK( 0x9999 == 0x1234 )\n"
    same_out = "/y/other/test_ai.cpp:121: ERROR: CHECK( hash == 0x1234 ) is NOT correct!\n  values: CHECK( 0x9999 == 0x1234 )\n"
    moved_out = "/x/test_ai.cpp:120: ERROR: CHECK( hash == 0x1234 ) is NOT correct!\n  values: CHECK( 0x7777 == 0x1234 )\n"
    assert pc.signature("pass", "") == "pass"
    assert pc.signature("fail", main_out) == pc.signature("fail", same_out)   # paths and line numbers do not count
    assert pc.signature("fail", main_out) != pc.signature("fail", moved_out)
    tc = pc.BASE[0]
    pc.SIGS.clear()
    pc.SIGS[tc] = pc.signature("fail", moved_out)
    res = {t: ("fail" if t == tc else "pass", "") for t in pc.BASE}
    base = {t: res[t][0] for t in pc.BASE}
    main_sigs = {t: base[t] for t in pc.BASE} | {tc: pc.signature("fail", main_out)}
    assert pc.moved_tests(res, base, main_sigs) == [tc]
    pc.SIGS[tc] = pc.signature("fail", same_out)
    assert pc.moved_tests(res, base, main_sigs) == []
    assert pc.MOVES.search(pc.moves_mark([])).group(1) == ""
    assert pc.MOVES.search(pc.moves_mark(["perf1*", "uber1*"])).group(1) == "perf1*,uber1*"
    assert "❌" not in pc.moves_mark(["perf1*"])   # the dashboard and the batch read ❌ as a red verdict

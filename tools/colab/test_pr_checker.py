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


def test_a_pr_claimed_on_the_other_sandbox_is_left_and_this_sandboxs_claim_holds(monkeypatch, tmp_path):
    (tmp_path / "runs").mkdir()
    claims = tmp_path / "runs" / "prcheck" / "claims"
    monkeypatch.setattr(pc, "CLAIMS", claims)
    monkeypatch.setattr(pc, "SANDBOX", "1")
    p = {"number": 7, "headRefOid": "abc"}
    assert pc.claim_check(p, settle=0)
    assert not pc.held_elsewhere(p, pc.check_claims())
    (claims / "7.json").write_text('{"sandbox": "2", "head": "abc", "t": %f}' % __import__("time").time())
    assert pc.held_elsewhere(p, pc.check_claims()) == "2"
    assert not pc.held_elsewhere({"number": 7, "headRefOid": "new"}, pc.check_claims())
    assert not pc.held_elsewhere(p, pc.check_claims(now=__import__("time").time() + pc.CLAIM_S + 1))
    pc.release_check(p)   # not ours: stays
    assert (claims / "7.json").exists()


def test_reworked_and_never_checked_prs_take_turns_oldest_head_first():
    # 2026-11-19: PRs 653..671 waited a day behind reworked ones
    prs = [{"number": n} for n in (523, 653, 662, 680, 711)]
    checked = {"523": A, "680": B}
    when = {523: 50, 653: 10, 662: 20, 680: 40, 711: 5}
    order = [p["number"] for p in pc.queue_order(prs, checked, lambda p: when[p["number"]], False)]
    assert order == [680, 711, 523, 653, 662]
    order = [p["number"] for p in pc.queue_order(prs, checked, lambda p: when[p["number"]], True)]
    assert order == [711, 680, 653, 523, 662]
    assert pc.queue_order([], checked, lambda p: 0, False) == []


def test_main_runs_only_the_tests_where_the_pr_differs_from_mains_last_result():
    # 2026-11-19: a whole main baseline per landing took 35-80 of a check's 70-80 minutes
    last = {"perf1*": ["pass", "pass"], "save4b*": ["fail", "fail:845d"], "uber1*": ["pass", "pass"], "mapobj*": ["pass", "pass"]}
    pr = {"perf1*": ["pass", "pass"], "save4b*": ["fail", "fail:845d"], "uber1*": ["fail", "fail:aaaa"], "mapobj*": ["pass", "pass"],
          "LOCAL-*": ["pass", "pass"]}
    need, view = pc.main_needed(pr, {"mapobj*": ["fail", "fail:bbbb"]}, last)
    assert need == ["uber1*", "LOCAL-*"]   # differs from main's last result / never measured on main
    assert view == {"perf1*": ["pass", "pass"], "save4b*": ["fail", "fail:845d"], "mapobj*": ["fail", "fail:bbbb"]}  # this main's own wins


def test_the_old_whole_baseline_state_becomes_the_last_known_main(monkeypatch, tmp_path):
    monkeypatch.setattr(pc, "MAIN_DRIVE", tmp_path)
    (tmp_path / f"{A}.json").write_text('{"uber1*": ["pass", "pass"]}')
    state = {"main": {"sha": A, "results": {"save4b*": "fail"}, "sigs": {"save4b*": "fail:845d"}, "audit": "/x/field-audit.tsv"}}
    known = pc.main_known(state, A)
    assert known == {"save4b*": ["fail", "fail:845d"], "uber1*": ["pass", "pass"]}  # the other sandbox's result from the Drive too
    assert state["main_last"] == {"save4b*": ["fail", "fail:845d"]}
    assert state["main_audit"] == ["/x/field-audit.tsv", A]


def test_a_pr_without_its_own_test_shown_passing_is_refused_before_the_build(monkeypatch):
    diff = '+TEST_CASE("PORT-289 dead targets")\n'
    monkeypatch.setattr(pc, "gh", lambda *a: '{"comments": []}')
    new = {"number": 9, "body": "Closes #1\n`cmake --build` passed", "createdAt": "2026-11-20T00:00:00Z"}
    assert pc.own_test_missing(new, diff) == ["PORT-289"]
    ok = dict(new, body="Closes #1\nPORT-289*: [doctest] test cases:  4 |  4 passed | 0 failed | 7020 skipped")
    assert pc.own_test_missing(ok, diff) == []
    assert pc.own_test_missing(dict(new, createdAt="2026-11-18T00:00:00Z"), diff) == []  # opened before the rule

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import batch_bot as bb  # noqa: E402


def pr(**kw):
    p = {"number": 7, "title": "Port 7: x", "body": "", "headRefOid": "abc123", "headRefName": "arena/1-x", "isDraft": False,
         "isCrossRepository": False, "headRepositoryOwner": {"login": bb.GH_REPO.split("/")[0]}}
    p.update(kw)
    return p


def fake_view(monkeypatch, comments, commits=("2026-11-17T10:00:00Z",)):
    data = {"comments": comments, "commits": [{"committedDate": c} for c in commits]}
    monkeypatch.setattr(bb, "gh", lambda *a: json.dumps(data))


def test_verdict_takes_only_a_clean_report_on_the_current_head(monkeypatch):
    fake_view(monkeypatch, [{"body": "<!-- prcheck abc123 -->\n✅ ok", "createdAt": "2026-11-17T11:00:00Z"}])
    assert bb.verdict(pr()) == "green"
    fake_view(monkeypatch, [{"body": "<!-- prcheck abc123 -->\n❌ red", "createdAt": "2026-11-17T11:00:00Z"}])
    assert bb.verdict(pr()).startswith("red")
    fake_view(monkeypatch, [{"body": "<!-- prcheck old999 -->\n✅ ok", "createdAt": "2026-11-17T11:00:00Z"}])
    assert bb.verdict(pr()).startswith("not checked")
    fake_view(monkeypatch, [{"body": "<!-- prcheck abc123 -->\n✅ ok", "createdAt": "2026-11-17T11:00:00Z"},
                            {"body": "<!-- returned --> Не принят", "createdAt": "2026-11-17T12:00:00Z"}])
    assert bb.verdict(pr()).startswith("returned")
    assert bb.verdict(pr(isDraft=True)) == "draft"


def test_a_fork_pr_needs_githubs_check_and_the_owners_review_of_its_head(monkeypatch):
    owner = bb.GH_REPO.split("/")[0]
    fork = dict(isCrossRepository=True, headRepositoryOwner={"login": "someone"}, files=[{"path": "engine/src/a.cpp"}])
    monkeypatch.setattr(bb, "fork_check", lambda head: "success")
    fake_view(monkeypatch, [])  # no sb1 report: a fork's PR never gets one
    assert "not reviewed" in bb.verdict(pr(**fork))
    fake_view(monkeypatch, [{"body": "<!-- reviewed old999 -->", "author": {"login": owner}}])
    assert "not reviewed" in bb.verdict(pr(**fork))  # an older head's review
    fake_view(monkeypatch, [{"body": "<!-- reviewed abc123 -->", "author": {"login": "someone"}}])
    assert "not reviewed" in bb.verdict(pr(**fork))  # not the owner's comment
    reviewed = [{"body": "<!-- reviewed abc123 --> read the whole diff", "author": {"login": owner}}]
    fake_view(monkeypatch, reviewed)
    assert bb.verdict(pr(**fork)) == "green"
    assert ".github/" in bb.verdict(pr(**dict(fork, files=[{"path": ".github/workflows/fork-pr.yml"}])))
    monkeypatch.setattr(bb, "fork_check", lambda head: "failure")
    assert "not green" in bb.verdict(pr(**fork))
    monkeypatch.setattr(bb, "fork_check", lambda head: "success")
    fake_view(monkeypatch, [{"body": "<!-- prcheck abc123 -->\n✅ ok", "createdAt": "2026-11-17T11:00:00Z"}])
    assert bb.verdict(pr(isCrossRepository=None)).startswith("from another account's fork")  # unknown: fail closed, an sb1 ✅ is no pass
    fake_view(monkeypatch, [{"body": "<!-- prcheck abc123 -->\n✅ ok", "createdAt": "2026-11-17T11:00:00Z"}])
    assert bb.verdict(pr()) == "green"  # the owner's own agents' PRs: the sb1 report, no review mark


def test_in_game_line_and_return_comment():
    body = "Closes #5\n2. **Проверка**\n- фильтр: PORT-7*\n- видно в игре: да — Гэндальф, Cheat Mode, нажать кнопку\n  он светится\n" \
           "- правка местная\n3. **Что делает RotWK**"
    assert bb.in_game(pr(body=body)) == "- видно в игре: да — Гэндальф, Cheat Mode, нажать кнопку он светится"
    c = bb.return_comment("18b", pr(), [("PORT-191*", "x.cpp:3: ERROR: CHECK( a ) is NOT correct!"), ("perf1*", "")])
    assert c.startswith("<!-- returned -->") and "`PORT-191*`" in c and "сдвигает пин" in c


def test_checker_pins_credit_the_prs_whose_report_named_the_pin():
    named = "✅ Собирается\n⚠️ `perf1*` сдвигает пин, причина в PR названа: владелец перемерит при вливании."
    a, b, c = pr(number=1, _report=named), pr(number=2, _report=named.replace("perf1*", "uber1*")), pr(number=3, _report="✅ ok")
    got = bb.checker_pins([a, b, c])
    assert {t: [p["number"] for p in ps] for t, ps in got.items()} == {"perf1*": [1], "uber1*": [2]}
    assert bb.checker_pins([pr(_report="⚠️ `save4b*` сдвигает пин, причина в PR названа")]) == {}  # not a pin of the bot


def test_the_checker_runs_the_batchs_whole_set():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import pr_checker as pc
    assert {t for t, _ in bb.SUITE} <= set(pc.BASE)
    assert set(bb.PINS) == pc.PINS

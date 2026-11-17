from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import batch_bot as bb  # noqa: E402


def pr(**kw):
    p = {"number": 7, "title": "Port 7: x", "body": "", "headRefOid": "abc123", "headRefName": "arena/1-x", "isDraft": False}
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


def test_in_game_line_and_return_comment():
    body = "Closes #5\n2. **Проверка**\n- фильтр: PORT-7*\n- видно в игре: да — Гэндальф, Cheat Mode, нажать кнопку\n  он светится\n" \
           "- правка местная\n3. **Что делает RotWK**"
    assert bb.in_game(pr(body=body)) == "- видно в игре: да — Гэндальф, Cheat Mode, нажать кнопку он светится"
    c = bb.return_comment("18b", pr(), [("PORT-191*", "x.cpp:3: ERROR: CHECK( a ) is NOT correct!"), ("perf1*", "")])
    assert c.startswith("<!-- returned -->") and "`PORT-191*`" in c and "сдвигает пин" in c

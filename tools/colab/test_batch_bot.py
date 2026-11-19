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


def test_a_batch_takes_at_most_five_green_prs_oldest_first(monkeypatch):
    prs = [pr(number=n) for n in (612, 601, 640, 605, 633, 650, 620)]
    monkeypatch.setattr(bb, "gh", lambda *a, **k: json.dumps(prs))
    monkeypatch.setattr(bb, "verdict", lambda p: "green" if p["number"] != 620 else "red")
    take, wait = bb.candidates(None)
    assert [p["number"] for p in take] == [601, 605, 612, 633, 640]
    assert [p["number"] for p, _ in wait] == [650, 620]
    assert "следующая партия" in wait[0][1]
    take, wait = bb.candidates(None, 0)
    assert len(take) == 6


def test_the_loop_starts_a_batch_at_three_green_prs_or_one_after_the_idle_time():
    assert not bb.should_start(0, 10 * 3600, 3, 2)
    assert not bb.should_start(2, 3600, 3, 2)
    assert bb.should_start(3, 0, 3, 2)
    assert bb.should_start(1, 2 * 3600, 3, 2)


def test_the_loop_names_the_next_free_letter_and_sees_hand_runs_only():
    assert bb.next_name("2026-11-18", {"2026-11-18a", "2026-11-18b", "2026-11-18c"}) == "2026-11-18d"
    assert bb.is_run(["python3", "/content/sandbox/batch_bot.py", "run", "--name", "x"])
    assert bb.is_run(["python3", "tools/colab/batch_bot.py", "run"])
    assert not bb.is_run(["python3", "/content/sandbox/batch_bot.py", "loop"])


def test_a_pr_claimed_by_a_running_batch_on_another_sandbox_waits(monkeypatch, tmp_path):
    (tmp_path / "2026-11-18c").mkdir()
    (tmp_path / "2026-11-18c" / "claim.json").write_text(json.dumps({"prs": [601, 605]}))
    (tmp_path / "2026-11-18b").mkdir()   # a red run: its report is written and it has no branch
    (tmp_path / "2026-11-18b" / "claim.json").write_text(json.dumps({"prs": [612]}))
    (tmp_path / "2026-11-18b" / "report.md").write_text("red")
    monkeypatch.setattr(bb, "RUNS", tmp_path)
    monkeypatch.setattr(bb, "sh", lambda *a, **k: type("R", (), {"returncode": 1, "stdout": ""})())   # no batch branches on origin
    monkeypatch.setattr(bb, "gh", lambda *a, **k: json.dumps([pr(number=n) for n in (601, 605, 612, 620)]))
    monkeypatch.setattr(bb, "verdict", lambda p: "green")
    take, wait = bb.candidates(None)
    assert [p["number"] for p in take] == [612, 620]
    assert dict((p["number"], why) for p, why in wait) == {601: "уже в партии 2026-11-18c", 605: "уже в партии 2026-11-18c"}
    take, _ = bb.candidates(None, own="2026-11-18c")   # the batch itself does not wait for its own claim
    assert len(take) == 4


def test_a_pr_leaves_the_batch_with_a_mark_unless_its_return_comment_says_why():
    take = [pr(number=n) for n in (601, 605, 612)]
    rows = {"verdict": "зелёный", "conflicted": [(take[1], "conflicts in x")]}
    assert [p["number"] for p, _ in bb.left_batch(take, rows, {612: "ломает t"})] == [605]
    rows = {"verdict": "красный: инструменты", "conflicted": []}
    assert [p["number"] for p, _ in bb.left_batch(take, rows, {612: "ломает t"})] == [601, 605]
    assert bb.taken_comment("2026-11-18e", take[0], "2").startswith(f"<!-- batch 2026-11-18e {take[0]['headRefOid']} -->")


def test_a_pr_that_conflicts_with_main_itself_goes_back_to_its_agent():
    # 2026-11-19a .. d: four PRs checked on an old main conflicted with main, left every batch and were taken by the next one
    take = [pr(number=n) for n in (579, 615)]
    rows = {"verdict": "пустой: все PR исключены",
            "conflicted": [(take[0], bb.BASE_CONFLICT + "conflicts in engine/src/a.cpp with the PRs merged before it"),
                           (take[1], "conflicts in docs/b.md with the PRs merged before it")]}
    left = dict((p["number"], why) for p, why in bb.left_batch(take, rows, {}))
    body = bb.left_comment("2026-11-19e", take[0], left[579])
    assert "engine/src/a.cpp" in body and bb.RETURNED.search(body) and not body.startswith("<!-- prcheck")
    assert not bb.RETURNED.search(bb.left_comment("2026-11-19e", take[1], left[615]))   # a conflict with an earlier PR waits
    assert bb.EMPTY_RC not in (0, 1)   # an empty batch is neither green nor red: the loop does not hold the next one for it


def test_main_results_are_shared_between_batches_on_one_main(tmp_path):
    f = tmp_path / "main-1910e2523abc.json"
    assert bb.load_main_results(f) == {}
    bb.save_main_results(f, {"perf1*": "fail"})
    bb.save_main_results(f, {"uber1*": "pass"})
    assert bb.load_main_results(f) == {"perf1*": "fail", "uber1*": "pass"}
    f.write_text("not json")
    assert bb.load_main_results(f) == {}


def test_a_pr_that_moves_hash_tests_goes_alone_and_the_oldest_decides_the_kind():
    quiet1, mover, quiet2 = pr(number=1, _moves=[]), pr(number=2, _moves=["perf1*"]), pr(number=3, _moves=None)
    take, wait = bb.split_movers([quiet1, mover, quiet2])
    assert [p["number"] for p in take] == [1, 3] and [p["number"] for p, _ in wait] == [2]
    take, wait = bb.split_movers([mover, quiet1, quiet2])
    assert [p["number"] for p in take] == [2] and bb.solo_mover(take) and len(wait) == 2
    assert not bb.solo_mover([quiet1])


def test_the_batch_reads_the_checkers_moves_mark(monkeypatch):
    p = pr()
    fake_view(monkeypatch, [{"body": f"<!-- prcheck {p['headRefOid']} -->\n✅ ok\n<!-- prcheck-moves uber1*,aiexpand1* -->",
                             "createdAt": "2026-11-18T10:00:00Z"}])
    assert bb.verdict(p) == "green" and p["_moves"] == ["uber1*", "aiexpand1*"]


def test_the_next_batch_waits_seven_minutes_for_a_landing_then_goes_on_top_of_the_unlanded_one():
    assert bb.stack_base(None, False, 0) == ("origin/main", 0.0)
    assert bb.stack_base("2026-11-18f", False, 10) == ("origin/main", 0.0)   # landed or given up
    base, wait = bb.stack_base("2026-11-18f", True, 60)
    assert base is None and wait == bb.STACK_AFTER_S - 60 == 360
    assert bb.stack_base("2026-11-18f", True, 7 * 60) == ("origin/batch/2026-11-18f", 0.0)


def test_a_renumbered_stop_moves_only_the_prs_own_lines_of_a_shared_table(monkeypatch, tmp_path):
    # 2026-11-18h / i: main had a row S-7493; #654's fragment S-7493 went to S-7507 and the global replace renamed main's row too
    (tmp_path / "docs" / "stops.d").mkdir(parents=True)
    stops = tmp_path / "docs" / "STOPS.md"
    stops.write_text("| S-7493 | code | someone else's stop |\n| S-1022 | code | emotions, see S-7493 |\n", encoding="utf-8")
    monkeypatch.setattr(bb, "WT", tmp_path)
    monkeypatch.setattr(bb, "next_free_id", lambda: 7507)
    monkeypatch.setattr(bb, "git", lambda *a: "")
    monkeypatch.setattr(bb, "pr_files", lambda n: ["docs/STOPS.md"])
    monkeypatch.setattr(bb, "pr_added_lines", lambda n, f: ["| S-1022 | code | emotions, see S-7493 |"])
    done = []
    bb.renumber(654, 7493, "| S-7493 | code | the PR's stop |\n", "taken", done)
    assert stops.read_text(encoding="utf-8") == "| S-7493 | code | someone else's stop |\n| S-1022 | code | emotions, see S-7507 |\n"
    assert (tmp_path / "docs" / "stops.d" / "S-7507.md").read_text(encoding="utf-8").startswith("| S-7507 |")
    assert done == ["#654: S-7493 -> S-7507 (taken)"]


def test_a_stop_row_one_side_edited_keeps_only_the_edit_after_the_union_merge():
    # 2026-11-19e: Port 271 narrowed S-705; the union merge kept main's old row beside it and the landing's fragments collect refused it
    base = "| S-704 | code | a |\n| S-705 | code | old |\n"
    merged = "| S-704 | code | a |\n| S-705 | code | old |\n| S-705 | code | narrowed |\n| S-706 | code | new |\n"
    assert bb.dedupe_rows(merged, base) == "| S-704 | code | a |\n| S-705 | code | narrowed |\n| S-706 | code | new |\n"
    both = "| S-705 | code | ours |\n| S-705 | code | theirs |\n"   # both sides edited the row: a real conflict
    assert bb.dedupe_rows(both, base) is None


def test_a_local_station_pr_is_left_to_claudes_direct_landing():
    p = {"isDraft": False, "headRefName": "agent/local/sb4-st2", "headRefOid": "x", "number": 1}
    assert bb.verdict(p).startswith("Local")


def test_the_batch_loop_checks_a_pr_after_thirty_minutes_without_green_prs_or_batches():
    now = 10_000.0
    assert not bb.helper_due({"finished": now - 600, "loop_started": 0}, now)
    assert not bb.helper_due({"ready_at": now - 1700}, now)
    assert bb.helper_due({"finished": now - 4000, "ready_at": now - 1900}, now)

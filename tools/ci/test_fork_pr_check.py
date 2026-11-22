from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fork_pr_check as f  # noqa: E402


def test_own_tests_from_the_diff_else_the_title():
    diff = '+TEST_CASE("PORT-206: the ring") {\n+TEST_CASE("PORT-206 b") {\n TEST_CASE("OLD-1: kept") {\n'
    assert f.own_tests(diff, "Port 206: x") == ["PORT-206*"]
    assert f.own_tests("", "Port 77: x") == ["PORT-77*"]
    assert f.own_tests("", "Fix things") == []


def test_annotation_escapes_properties_and_message():
    line = f.annotation("error", "Свои тесты PORT-1*: a,b", "line 1\nline 2 100%")
    assert line == "::error title=Свои тесты PORT-1*%3A a%2Cb::line 1%0Aline 2 100%25"


def test_result_kinds():
    ok = "[doctest] test cases: 2 | 2 passed | 0 failed | 0 skipped"
    assert f.test_result(ok, 0) == ("pass", "")
    assert f.test_result("SKIP: needs ROTWK_INSTALL\n" + ok, 0)[0] == "skip"
    assert f.test_result("x.cpp:3: ERROR: CHECK( a ) is NOT correct!\n[doctest] test cases: 1 | 0 passed | 1 failed", 1) == \
        ("fail", "x.cpp:3: ERROR: CHECK( a ) is NOT correct!")
    assert f.test_result("[doctest] test cases: 0 | 0 passed | 0 failed", 0) == ("none", "")


def test_a_pr_without_engine_code_needs_no_own_test(monkeypatch, capsys):
    # Local 121 (PR 828, 2026-11-21): a docs-only "Local N" PR was red for the missing LOCAL-N test
    import subprocess
    monkeypatch.setenv("PR_TITLE", "Local 121: upstream branches")
    monkeypatch.setenv("PR_BODY", "")
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(f, "Path", lambda p: type("P", (), {"exists": lambda self: False})())
    def fake_sh(cmd, **kw):
        out = "docs/ennorath/PR_CANDIDATES.md\n" if "--name-only" in cmd else "+| row |\n"
        return subprocess.CompletedProcess(cmd, 0, out, "")
    monkeypatch.setattr(f, "sh", fake_sh)
    assert f.main() == 0
    assert "Своих тестов не нужно" in capsys.readouterr().out

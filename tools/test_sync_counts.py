"""tools/sync_counts.py and the pure parts of tools/agent_worktree.py over made-up text and a throwaway repository."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import agent_worktree as aw  # noqa: E402
import sync_counts as sc  # noqa: E402

LOG = "x.cpp(98): MESSAGE: module coverage: ported 202, base AI 6, parsed only 13, missing 108\n"
SOURCE = "CHECK(c.ported == 1); // why\nCHECK(c.baseAi == 2);\nCHECK(c.parsedOnly == 3);\nCHECK(c.missing == 4); // why\n"


def test_pins_take_the_tests_own_line_and_keep_the_comments():
    out = sc.apply_pins(SOURCE, sc.read_counts(LOG))
    assert out == "CHECK(c.ported == 202); // why\nCHECK(c.baseAi == 6);\nCHECK(c.parsedOnly == 13);\nCHECK(c.missing == 108); // why\n"


def test_a_log_without_the_line_is_an_error():
    with pytest.raises(ValueError, match="module coverage"):
        sc.read_counts("[doctest] Status: SUCCESS!\n")


def test_a_missing_pin_is_an_error():
    with pytest.raises(ValueError, match="c.baseAi"):
        sc.apply_pins("CHECK(c.ported == 1);\n", {"baseAi": 1})


def test_the_readme_takes_the_stops_files_count():
    stops = "| S-1 | code | a |\n| S-2 | closed | b |\n| S-3 | project | c |\n"
    assert sc.apply_stops("There are **9 open stops** today.", stops) == "There are **2 open stops** today."


def run(*args: str, cwd: Path) -> None:
    subprocess.run(args, cwd=cwd, check=True, capture_output=True)


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    run("git", "init", "-q", "-b", "main", cwd=tmp_path)
    run("git", "config", "user.email", "t@example.invalid", cwd=tmp_path)
    run("git", "config", "user.name", "t", cwd=tmp_path)
    (tmp_path / "a.txt").write_text("a\n", encoding="utf-8")
    run("git", "add", "a.txt", cwd=tmp_path)
    run("git", "commit", "-qm", "base", "--no-verify", cwd=tmp_path)
    return tmp_path


def test_unmerged_work_is_not_removed(repo: Path, monkeypatch: pytest.MonkeyPatch):
    monkeypatch.chdir(repo)
    run("git", "checkout", "-qb", "agent/a/t", cwd=repo)
    (repo / "b.txt").write_text("b\n", encoding="utf-8")
    run("git", "add", "b.txt", cwd=repo)
    run("git", "commit", "-qm", "work", "--no-verify", cwd=repo)
    run("git", "checkout", "-q", "main", cwd=repo)
    with pytest.raises(SystemExit, match="commits main lacks"):
        aw.cmd_remove("agent/a/t")
    assert aw.git("branch", "--list", "agent/a/t", cwd=repo)


def test_a_dirty_main_is_not_moved(repo: Path, monkeypatch: pytest.MonkeyPatch):
    monkeypatch.chdir(repo)
    run("git", "branch", "agent/a/t", cwd=repo)
    (repo / "a.txt").write_text("changed\n", encoding="utf-8")
    with pytest.raises(SystemExit, match="uncommitted"):
        aw.cmd_land("agent/a/t")


def test_landing_moves_main_and_removes_the_branch(repo: Path, monkeypatch: pytest.MonkeyPatch):
    monkeypatch.chdir(repo)
    run("git", "checkout", "-qb", "agent/a/t", cwd=repo)
    (repo / "b.txt").write_text("b\n", encoding="utf-8")
    run("git", "add", "b.txt", cwd=repo)
    run("git", "commit", "-qm", "work", "--no-verify", cwd=repo)
    run("git", "checkout", "-q", "main", cwd=repo)
    assert aw.cmd_land("agent/a/t") == 0
    assert (repo / "b.txt").exists()
    assert not aw.git("branch", "--list", "agent/a/t", cwd=repo)


def test_a_foreign_hook_is_not_overwritten(repo: Path):
    hook = repo / ".git" / "hooks" / "pre-commit"
    hook.write_text("#!/bin/sh\necho mine\n", encoding="utf-8")
    with pytest.raises(SystemExit, match="other content"):
        aw.install_hook(repo)
    assert "mine" in hook.read_text(encoding="utf-8")

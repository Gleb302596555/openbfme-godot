"""tools/mod/matrix_deps.py: the traces it trusts and the factions select reruns (owner 2026-11-24: the incremental matrix without risk)."""
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_deps  # noqa: E402
from matrix_deps import Untrusted, read_traces  # noqa: E402


def offsets(*vals: int) -> bytes:
    return b"".join(v.to_bytes(8, "little") for v in vals)


def test_traces_union_and_every_process_must_leave_one(tmp_path):
    (tmp_path / "F.0.expect").write_text("")
    (tmp_path / "F.0.123").write_bytes(offsets(16, 32))
    (tmp_path / "F.1.expect").write_text("")
    (tmp_path / "F.1.124").write_bytes(offsets(32, 48))
    assert read_traces(tmp_path, "F") == {16, 32, 48}
    (tmp_path / "F.2.expect").write_text("")  # a process that left nothing (crashed before its first function, or no instrumented build)
    with pytest.raises(Untrusted):
        read_traces(tmp_path, "F")


def test_traces_broken_or_torn_are_not_trusted(tmp_path):
    (tmp_path / "F.0.expect").write_text("")
    (tmp_path / "F.0.9").write_bytes(offsets(16) + b"\x01\x02")  # a record cut short
    with pytest.raises(Untrusted):
        read_traces(tmp_path, "F")
    (tmp_path / "F.0.9").write_bytes(offsets(16))
    (tmp_path / "F.0.9.broken").write_text("")
    with pytest.raises(Untrusted):
        read_traces(tmp_path, "F")


def git(repo: Path, *args: str) -> str:
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args], cwd=repo, check=True, capture_output=True, text=True).stdout


def test_select_reruns_only_what_a_change_can_touch(tmp_path, monkeypatch, capsys):
    repo = tmp_path / "repo"
    (repo / "engine" / "src").mkdir(parents=True)
    (repo / "tools").mkdir()
    for f in ("engine/src/a.cpp", "engine/src/a.h", "engine/src/b.cpp", "tools/matrix_run.py"):
        (repo / f).write_text("x\n")
    git(repo, "init", "-q")
    git(repo, "add", ".")
    git(repo, "commit", "-qm", "1")
    rev = git(repo, "rev-parse", "HEAD").strip()
    cache = tmp_path / "cache"
    cache.mkdir()
    for f, files in (("FA", ["engine/src/a.cpp", "engine/src/a.h"]), ("FB", ["engine/src/b.cpp"])):
        (cache / f"{f}.deps").write_text(f"# rev {rev}\n" + "".join(x + "\n" for x in files))
        (cache / f"{f}.tsv").write_text("rows\n")
    monkeypatch.setattr(matrix_deps, "REPO", repo)

    def run(*factions):
        matrix_deps.select(type("A", (), {"cache": str(cache), "factions": list(factions)})())
        return {l.split(" ")[1]: l.split(" ")[0] for l in capsys.readouterr().out.splitlines()}

    assert run("FA", "FB", "FC") == {"FA": "keep", "FB": "keep", "FC": "rerun"}  # FC: no set
    (repo / "engine/src/a.h").write_text("y\n")  # a header of FA's units: FA only
    git(repo, "commit", "-qam", "2")
    assert run("FA", "FB") == {"FA": "rerun", "FB": "keep"}
    (repo / "tools/matrix_run.py").write_text("y\n")  # a global input: everyone
    git(repo, "commit", "-qam", "3")
    assert run("FA", "FB") == {"FA": "rerun", "FB": "rerun"}
    (cache / "FB.deps").write_text("# rev 0123456789abcdef0123456789abcdef01234567\n")  # a main git does not know
    (cache / "FA.tsv").unlink()  # no rows to keep
    assert run("FA", "FB") == {"FA": "rerun", "FB": "rerun"}

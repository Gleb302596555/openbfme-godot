"""Lane MODS-2: tools/mod/aio_package.py laying out a package from a folder the All In One launcher installed (no mod or game bytes)."""
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("aio_package", HERE / "aio_package.py")
aio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aio)


def entry(files, language="ALL"):
    return {"Name": "Made Up", "Game": 2, "Type": 0, "Files": [
        {"Name": n, "Md5": hashlib.md5(b).hexdigest(), "Size": len(b), "Language": language if "lang" in n else "ALL",
         "Url": "http://127.0.0.1:9/unreachable"} for n, b in files]}


def test_language_picks_the_files_of_that_language_besides_all():
    e = entry([("lang\\Russian.big", b"ru"), ("data.big", b"d")], language="RU")
    assert [n for _, n, _ in aio.plan_entry(e, set(), [], [], "RU")] == ["lang\\Russian.big", "data.big"]
    assert [n for _, n, _ in aio.plan_entry(e, set(), [], [], "EN")] == ["data.big"]


def test_from_local_links_matching_files_in_any_case_and_never_takes_other_bytes(tmp_path, monkeypatch):
    game = tmp_path / "game"
    (game / "APT").mkdir(parents=True)
    (game / "APT" / "mainmenu.BIG").write_bytes(b"menu")
    (game / "_mod.big").write_bytes(b"overwritten by a later layer")
    e = entry([("apt\\MainMenu.big", b"menu"), ("_mod.big", b"the package's bytes")])
    plan = aio.plan_entry(e, set(), [], [])
    fetched = []
    monkeypatch.setattr(aio.urllib.request, "urlopen", lambda req, timeout: fetched.append(req.full_url) or (_ for _ in ()).throw(OSError("offline")))
    out = tmp_path / "out"
    try:
        aio.download(plan, str(out), aio.local_index(str(game)))
    except OSError:
        pass
    assert (out / "install" / "apt" / "MainMenu.big").read_bytes() == b"menu"
    assert fetched == ["http://127.0.0.1:9/unreachable"]  # only _mod.big: its local bytes are not the package's
    assert not (out / "install" / "_mod.big").exists()

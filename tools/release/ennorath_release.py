#!/usr/bin/env python3
"""Build (and optionally publish) the fork's Ennorath release: one Windows x64 zip a player unpacks and starts with OpenBFME.exe.

The zip holds no game file and no mod file. The launcher (tools/release/ennorath_exe/launcher.cpp) asks for the player's own
"RotWK + Ennorath" folder (the All In One BFME Launcher install) and BFME2 folder on its first start and lays out its own copy with
tools/mod/aio_package.py; nothing of EA's or of the Ennorath team's is shipped.

Inputs
  --tree   a built work tree (build.bat ran: godot/bin holds the GDExtension DLL), normally a station synced to main;
  --base   the package skeleton: OpenBFME.exe (the launcher, cl /MT), python/ (the embeddable CPython), game/Godot_v4.7.2-stable_win64.exe,
           README.txt. Default: workspace/dist/openbfme-ennorath-exe/OpenBFME-Ennorath.
Steps
  1. copy the skeleton into workspace/dist/<name>/, replace game/godot with the tree's godot/ (no *.pdb, no .godot/ editor cache) and
     tools/mod/aio_package.py with the tree's;
  2. audit: every file under game/godot must have an extension of the project's own kinds (ALLOWED_PROJECT); no file anywhere may have a
     game-data extension (FORBIDDEN) or be named game.dat; the GDExtension DLL must be the tree's, byte for byte;
  3. zip it as workspace/dist/<name>.zip with a .sha256 beside it and write the release notes (the commit, the date, the sha256);
  4. with --publish only: `gh release create <tag> -R Gleb302596555/openbfme-godot` with the zip, the .sha256 and the notes. Without it
     nothing leaves the machine.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REPO = "Gleb302596555/openbfme-godot"
# workspace/ lives in the main checkout; from a station (a git worktree) it is the parent of the common .git directory
ROOT = Path(subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=Path(__file__).resolve().parent, check=True,
                           capture_output=True, text=True).stdout.strip()).parent
DEFAULT_BASE = ROOT / "workspace" / "dist" / "openbfme-ennorath-exe" / "OpenBFME-Ennorath"
DLL = "openbfme.windows.template_debug.x86_64.dll"
SKELETON = ("OpenBFME.exe", "README.txt", "python", "game/Godot_v4.7.2-stable_win64.exe")
# the Godot project's own file kinds (scripts, scenes, shaders, its icons and imports, the engine DLL); anything else under game/godot fails
ALLOWED_PROJECT = {".gd", ".uid", ".tscn", ".tres", ".gdshader", ".gdshaderinc", ".cfg", ".py", ".png", ".md5", ".import", ".ctex", ".cpp", ".h",
                   ".ico", ".icns", ".gdignore", ".txt", ".godot", ".gdextension", ".dll", ".md"}
# retail / mod data kinds: never in the package
FORBIDDEN = {".big", ".w3d", ".w3x", ".str", ".csf", ".map", ".wnd", ".apt", ".const", ".dds", ".tga", ".wav", ".mp3", ".bik", ".vp6", ".ini",
             ".inc", ".dat", ".lua", ".skudef"}


def run(cmd: list[str], cwd: Path | None = None) -> str:
    return subprocess.run(cmd, cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def copy_project(src: Path, dst: Path) -> None:
    def ignore(directory: str, names: list[str]) -> set[str]:
        return {n for n in names if n.endswith(".pdb") or (n == ".godot" and Path(directory) == src)}
    shutil.copytree(src, dst, ignore=ignore)


def audit(pkg: Path, tree: Path) -> list[str]:
    problems = []
    project = pkg / "game" / "godot"
    for f in pkg.rglob("*"):
        if not f.is_file():
            continue
        ext = f.suffix.lower() or f.name.lower()  # a dot file (.gdignore) has no suffix: its name is its kind
        rel = f.relative_to(pkg).as_posix()
        if f.name.lower() == "game.dat" or ext in FORBIDDEN:
            problems.append(f"game or mod data kind: {rel}")
        if project in f.parents and ext not in ALLOWED_PROJECT:
            problems.append(f"not a project file kind: {rel}")
    dll = project / "bin" / DLL
    if not dll.is_file():
        problems.append(f"missing {dll.relative_to(pkg).as_posix()}")
    elif sha256(dll) != sha256(tree / "godot" / "bin" / DLL):
        problems.append("the DLL is not the tree's")
    for s in SKELETON:
        if not (pkg / s).exists():
            problems.append(f"missing {s}")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tree", required=True, type=Path)
    ap.add_argument("--base", default=DEFAULT_BASE, type=Path)
    ap.add_argument("--date", default=dt.date.today().isoformat())
    ap.add_argument("--publish", action="store_true", help="create the GitHub release on the fork (otherwise build and audit only)")
    a = ap.parse_args()
    tree, base = a.tree.resolve(), a.base.resolve()
    if not (tree / "godot" / "bin" / DLL).is_file():
        print(f"error: {tree} has no built {DLL} (run build.bat there)", file=sys.stderr)
        return 2
    if run(["git", "status", "--porcelain"], tree):
        print(f"error: {tree} has uncommitted changes: build the release from a clean, committed tree", file=sys.stderr)
        return 2
    commit = run(["git", "rev-parse", "HEAD"], tree)
    name = f"OpenBFME-Ennorath-{a.date}-windows-x64"
    tag = f"ennorath-{a.date}"
    out = ROOT / "workspace" / "dist"
    pkg = out / name / "OpenBFME-Ennorath"
    if pkg.parent.exists():
        shutil.rmtree(pkg.parent)
    pkg.mkdir(parents=True)
    for s in ("OpenBFME.exe", "README.txt"):
        shutil.copy2(base / s, pkg / s)
    shutil.copytree(base / "python", pkg / "python")
    (pkg / "game").mkdir()
    shutil.copy2(base / "game" / "Godot_v4.7.2-stable_win64.exe", pkg / "game" / "Godot_v4.7.2-stable_win64.exe")
    copy_project(tree / "godot", pkg / "game" / "godot")
    (pkg / "tools" / "mod").mkdir(parents=True)
    shutil.copy2(tree / "tools" / "mod" / "aio_package.py", pkg / "tools" / "mod" / "aio_package.py")
    problems = audit(pkg, tree)
    if problems:
        print("audit failed:", *problems, sep="\n  ", file=sys.stderr)
        return 1
    archive = out / f"{name}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for f in sorted(pkg.rglob("*")):
            if f.is_file():
                z.write(f, f.relative_to(pkg.parent).as_posix())
    digest = sha256(archive)
    (out / f"{name}.zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    notes = out / f"{name}.notes.md"
    notes.write_text(
        f"OpenBFME with the **Ennorath 1.9.8.2** mod, Windows 10/11 x64, test build of {a.date}.\n\n"
        f"Built from commit `{commit}` of this fork.\n\n"
        "The archive contains **no game files and no mod files**: you need your own Rise of the Witch-king with Ennorath (installed by the "
        "All In One BFME Launcher) and Battle for Middle-earth II 1.06. Unpack the whole zip into a folder with a few GB free (not Program Files) "
        "and start `OpenBFME.exe`; README.txt inside explains the first start.\n\n"
        f"SHA-256 of `{archive.name}`: `{digest}`\n\n"
        "Work in progress: see the open problems in the repository's `docs/ennorath/PROBLEMS.md`.\n",
        encoding="utf-8")
    print(f"package {archive} ({archive.stat().st_size // (1 << 20)} MB), sha256 {digest}, commit {commit}")
    if not a.publish:
        print(f"not published (dry run); publish with --publish: tag {tag} on {REPO}")
        return 0
    run(["gh", "release", "create", tag, str(archive), str(out / f"{name}.zip.sha256"), "-R", REPO, "--target", commit,
         "--title", f"OpenBFME Ennorath {a.date}", "--notes-file", str(notes), "--prerelease"])
    print(f"published https://github.com/{REPO}/releases/tag/{tag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

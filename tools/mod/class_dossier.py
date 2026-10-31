#!/usr/bin/env python3
"""Everything about one module class before porting it, in one reading (docs/ennorath/PROCESS.md, section 5, steps 1-4):

    class_dossier.py <Class> [--ini DIR] [--game-dat P] [--decomp DIR] [--no-fetch] [--samples N]

The row of MODULE_GAPS.md, upstream's state (ennorath_unique.py), the registry's addresses, the fields the mod sets with their values and the objects
that declare the class, the BFME2 decompile files and the ported class that shares the most fields. --game-dat also lists the class name's address and
references in the binary (tools/mod/rw.py, needs capstone and pefile); the virtual table comes from the registry. A source that is missing is reported
as missing, never skipped silently. Prints to the local terminal: INI values are the mod's data, so do not paste this output into the repository.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_queue  # noqa: E402

REGISTRY = REPO / "engine/data/rotwk-201/module-registry.json"
MODULES = REPO / "engine/src/GameLogic/Module"
BLOCK = re.compile(r"^\s*(?:Behavior|Body|ClientUpdate|ClientBehavior|Draw)\s*=\s*(\w+)", re.I)
OBJECT = re.compile(r"^\s*(?:Object|ChildObject|ObjectReskin)\s+(\w+)", re.I)
FIELD = re.compile(r'\{\s*"(\w+)"\s*,')
OPENS = re.compile(r"^\s*(?:Object|ChildObject|Behavior|Body|Draw|ClientUpdate|ClientBehavior|\w*Weapon\w*|ModelConditionState|TransitionState|AnimationState|IdleAnimationState|DefaultModelConditionState|Turret|AltTurret|ArmorSet|WeaponSet|LocomotorSet|UnitSpecificSounds|UnitSpecificFX|Prerequisites|InheritableModule|OverrideableByLikeKind|ReplaceModule|AddModule|Geometry\w*)\b", re.I)


def main_root() -> Path:
    done = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--path-format=absolute", "--git-common-dir"], capture_output=True, text=True)
    if done.returncode != 0:
        raise SystemExit(done.stderr.strip())
    return Path(done.stdout.strip()).parent


def parse_blocks(lines: list[str], name: str) -> list[tuple[str, dict[str, str]]]:
    """(object, fields) of each block of class `name`: `Field = value` lines until the block's own End (nested blocks counted, their lines left out)."""
    out = []
    owner = "?"
    i = 0
    while i < len(lines):
        line = lines[i].split(";")[0]
        found = OBJECT.match(line)
        if found:
            owner = found.group(1)
        block = BLOCK.match(line)
        i += 1
        if not block or block.group(1).lower() != name.lower():
            continue
        fields: dict[str, str] = {}
        depth = 1
        while i < len(lines) and depth:
            text = lines[i].split(";")[0].strip()
            i += 1
            if not text:
                continue
            if text.lower() == "end":
                depth -= 1
            elif "=" not in text or BLOCK.match(text):
                depth += 1
            elif depth == 1:
                key, value = text.split("=", 1)
                fields[key.strip()] = " ".join(value.split())
        out.append((owner, fields))
    return out


def read_ini(root: Path, name: str) -> list[tuple[str, dict[str, str]]]:
    blocks = []
    for path in sorted(p for p in root.rglob("*") if p.suffix.lower() in (".ini", ".inc")):
        blocks += parse_blocks(path.read_text(encoding="latin-1").splitlines(), name)
    return blocks


def field_census(blocks: list[tuple[str, dict[str, str]]], limit: int = 6) -> list[str]:
    values: dict[str, Counter] = defaultdict(Counter)
    for _, fields in blocks:
        for key, value in fields.items():
            values[key][value] += 1
    return [f"  {key}: {len(blocks) and sum(c.values())}/{len(blocks)}: " + ", ".join(f"{v} x{n}" for v, n in c.most_common(limit)) + (" ..." if len(c) > limit else "")
            for key, c in sorted(values.items())]


def ported_fields() -> dict[str, set[str]]:
    """The INI fields of each ported class, from the field tables of its source files."""
    out: dict[str, set[str]] = {}
    for path in MODULES.glob("*.cpp"):
        fields = set(FIELD.findall(path.read_text(encoding="utf-8", errors="replace")))
        if fields:
            out[path.stem] = fields
    return out


def nearest(fields: set[str], known: dict[str, set[str]], name: str, count: int = 3) -> list[tuple[str, int]]:
    """The ported classes sharing the most field names with `fields`: a pattern to start from, not a claim of kinship."""
    wanted = {f.lower() for f in fields}
    scored = [(n, len(wanted & {f.lower() for f in fs})) for n, fs in known.items() if n != name]
    return [s for s in sorted(scored, key=lambda s: (-s[1], s[0])) if s[1]][:count]


def section(title: str) -> None:
    print(f"\n## {title}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name")
    root = main_root()
    parser.add_argument("--ini", default=str(root / "workspace/ennorath-audit/agent-b-ini"), help="the mod's effective INI (dumpini.py)")
    parser.add_argument("--decomp", default=str(root / "workspace/reference/open-bfme-2/Code/GameEngine/Source"))
    parser.add_argument("--game-dat", help="RotWK/game.dat: list the class name's references too")
    parser.add_argument("--no-fetch", action="store_true")
    parser.add_argument("--samples", type=int, default=8)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    print(f"# {args.name}")

    section("MODULE_GAPS")
    row = [r for r in ennorath_queue.rows(ennorath_queue.GAPS.read_text(encoding="utf-8")) if r.name == args.name]
    print(f"group {row[0].group}, {row[0].status}, reach {row[0].reach} ({row[0].factions}); {row[0].what}; state: {row[0].state}" if row else "MISSING: no row for this class")

    section("Upstream")
    check = [sys.executable, str(REPO / "tools/mod/ennorath_unique.py"), "--check", args.name] + (["--no-fetch"] if args.no_fetch else [])
    done = subprocess.run(check, capture_output=True, text=True, encoding="utf-8", errors="replace")
    print((done.stdout + done.stderr).strip(), f"(exit {done.returncode}: 0 free, 1 upstream has it or works on it, 2 unknown)")

    section("Registry")
    classes = {c["name"]: c for c in json.loads(REGISTRY.read_text(encoding="utf-8"))["classes"]}
    entry = classes.get(args.name)
    print({k: entry[k] for k in ("vtable", "create", "createData", "type", "mask")} if entry else "MISSING: not in the retail module registry")

    section("Mod data")
    ini = Path(args.ini)
    blocks = read_ini(ini, args.name) if ini.is_dir() else None
    fields: set[str] = set()
    if blocks is None:
        print(f"MISSING: {ini} (workspace/ennorath-audit/tools/dumpini.py makes it)")
    else:
        owners = Counter(owner for owner, _ in blocks)
        print(f"{len(blocks)} declarations on {len(owners)} objects: " + ", ".join(list(owners)[:args.samples]) + (" ..." if len(owners) > args.samples else ""))
        print("\n".join(field_census(blocks)) or "  no fields set")
        fields = {key for _, block in blocks for key in block}

    section("Nearest ported classes (shared field names)")
    print(nearest(fields, ported_fields(), args.name) or "none share a field")

    section("BFME2 decompile")
    decomp = Path(args.decomp)
    if not decomp.is_dir():
        print(f"MISSING: {decomp}")
    else:
        files = sorted(p for p in decomp.rglob("*") if p.suffix in (".cpp", ".h") and args.name.lower() in p.name.lower())
        print("\n".join(f"  {p.relative_to(decomp)} ({sum(1 for _ in p.open(encoding='utf-8', errors='replace'))} lines)" for p in files) or "no file named after the class: search the bodies")

    section("RotWK binary")
    if not args.game_dat:
        print("not read: give --game-dat. Virtual table above; `tools/mod/rw.py <game.dat> dis <va>` reads a function")
    else:
        done = subprocess.run([sys.executable, str(REPO / "tools/mod/rw.py"), args.game_dat, "str", args.name], capture_output=True, text=True, encoding="utf-8", errors="replace")
        print((done.stdout + done.stderr).strip() or "the name is not in the binary")
    return 0


if __name__ == "__main__":
    sys.exit(main())

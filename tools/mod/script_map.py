#!/usr/bin/env python3
"""The script map of Ennorath: every script of the script libraries the game runs for the sides (Libraries\\<name>\\<name>.map: the human
side's Multiplayer_Human, the AI sides' libraries) and of the .scb script files, as the mod's mount resolves them, with what each script
reads and what it gives (owner 2026-11-19: the Manwe science comes from Multiplayer_Human's ImladrisTechManve, not from any INI module, and
neither the agents nor the matrix could see it: the libraries are RefPack-compressed maps).

    python tools/mod/script_map.py <RotWK install> <mod install> [--out docs/ennorath]

Mount: the mod's archives first (case-insensitive name order, the first archive that holds a path wins: more leading underscores first,
as the mod names them), then the RotWK 2.01 archives in the pinned policy order (tools/fx/bigfs.py). The script tree and its records are
tools/script/script_census.py's reader (ZH Scripts.cpp layouts as BFME2 writes them); every condition / action is resolved against the
binary's registry (engine/src/GameLogic/ScriptEngine/ScriptTemplateTables.inc) with RotWK's parser rules (script_census.resolve).

Writes (names only, no retail or mod bytes) into --out:
- SCRIPT_MAP.tsv (into --full only, never the repository: it transcribes the scripts): one line per script: library, group, script, flags, conditions, actions, else actions, the condition / action types the
  port has no case for;
- SCRIPT_GRANTS.tsv: one line per thing a script gives a player (a science, an upgrade, an object, ...): kind, name, the action, library,
  script, and the objects / upgrades / sciences its conditions read (the trigger);
- SCRIPT_MAP.md: counts and the condition / action types the mod's scripts use that the port does not execute.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "fx"))
sys.path.insert(0, str(ROOT / "tools" / "maps" / "oracle"))
sys.path.insert(0, str(ROOT / "tools" / "script"))
sys.path.insert(0, str(ROOT / "tools" / "mod"))

import bigfs  # noqa: E402

census = sc = None  # imported in main(): census.py reads ROTWK_INSTALL / BFME2_INSTALL at import, this tool takes the install as an argument

ENGINE = ROOT / "engine" / "src" / "GameLogic" / "ScriptEngine"
GIVE = re.compile(r"GRANT|GIVE|PURCHASE|CREATE|SPAWN|UPGRADE|SCIENCE|MONEY|ADD_|SET_.*(UPGRADE|SCIENCE)", re.I)


def mount(base: Path, overlay: Path) -> bigfs.Mount:
    m = bigfs.Mount.__new__(bigfs.Mount)
    m.files = {}
    for p in sorted(overlay.glob("*.big"), key=lambda q: q.name.lower()):
        m._add(p)
    policy = json.loads((bigfs.POLICY_DIR / "rotwk-201-english-archives.json").read_text())
    for a in policy["archives"]:
        m._add(bigfs._find(base, a["path"]))
    return m


def ini_names(m: bigfs.Mount) -> dict[str, str]:
    """name -> kind for every Object / Science / Upgrade the mount's INI defines (the mod's files win, as above)."""
    kinds: dict[str, str] = {}
    pat = re.compile(r"^[ \t]*(Object|ChildObject|ObjectReskin|Science|Upgrade|SpecialPower|PlayerTemplate)[ \t]+(\w+)", re.M)
    for n in m.list("data\\ini\\"):
        if not n.endswith((".ini", ".inc")):
            continue
        text = m.read(n).decode("latin-1")
        for t, name in pat.findall(text):
            kind = {"ChildObject": "object", "ObjectReskin": "object", "Object": "object"}.get(t, t.lower())
            kinds.setdefault(name, kind)
    return kinds


# The INI fields that give a player or an object an upgrade / a science (MATRIX_TODO 13, 2026-11-19): a button's Upgrade / Science, an
# ExperienceLevel's Upgrades, a ModifierList's Upgrade, GrantUpgradeCreate's UpgradeToGrant, ObjectCreationUpgrade's / an OCL's GrantUpgrade,
# a PlayerTemplate's starting upgrades and sciences, a science's BonusScience, the Create-a-Hero blings. Fields that only test or remove one
# (TriggeredBy, ConflictsWith, NeededUpgrade, Required*, Forbidden*, RemoveUpgrade, UpgradeToRemove) are not grants.
GRANT_KEYS = {"Upgrade", "Upgrades", "UpgradeToGrant", "GrantUpgrade", "Science", "IntrinsicSciences", "IntrinsicSciencesMP", "InitialUpgrades",
              "BonusScience", "BlingUpgradeName", "BlingUpgrades", "UpgradeForRingEntry", "UpgradeName"}
TOP = re.compile(r"^(\w+)(?:[ \t]+(\S+))?")
FIELD = re.compile(r"^[ \t]*(\w+)[ \t]*=[ \t]*(.*)$")
BEHAVIOR = re.compile(r"^[ \t]*(?:Behavior|Body|Draw|ClientUpdate)[ \t]*=[ \t]*(\w+)")


def ini_grants(m: bigfs.Mount, kinds: dict[str, str]) -> list[tuple[str, str, str, str, str, str]]:
    """(kind, name, field, block, module, file) for every upgrade / science an INI field of GRANT_KEYS gives (names only)."""
    out = []
    for n in m.list("data\\ini\\"):
        if not n.endswith((".ini", ".inc")):
            continue
        block, module = "", ""
        for raw in m.read(n).decode("latin-1").splitlines():
            line = raw.split(";", 1)[0].split("//", 1)[0].rstrip()
            if not line.strip():
                continue
            if not line[0].isspace():
                t = TOP.match(line)
                if t and t.group(1) not in ("End", "END", "#include", "#define"):
                    block, module = (t.group(1) + " " + (t.group(2) or "")).strip(), ""
                continue
            b = BEHAVIOR.match(line)
            if b:
                module = b.group(1)
                continue
            f = FIELD.match(line)
            if not f or f.group(1) not in GRANT_KEYS:
                continue
            for tok in f.group(2).split():
                if kinds.get(tok) in ("upgrade", "science"):
                    out.append((kinds[tok], tok, f.group(1), block, module, n))
    return out


def port_cases() -> tuple[set[str], set[str]]:
    """Condition / action names the port's script engine handles: `case NAME:` in the conditions, `"NAME"` in the actions."""
    conds, acts = set(), set()
    for p in ENGINE.glob("*.cpp"):
        t = p.read_text(encoding="utf-8", errors="replace")
        conds |= set(re.findall(r"\bcase\s+([A-Z][A-Z0-9_]+)\s*:", t))
        acts |= set(re.findall(r'"([A-Z][A-Z0-9_]{3,})"', t))
    return conds, acts


def music_cases() -> set[str]:
    """The music library runs on the client (engine/src/GameClient/MusicScripts.cpp: `case C_<condition>:` / `case A_<action>:`)."""
    t = (ROOT / "engine" / "src" / "GameClient" / "MusicScripts.cpp").read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r"\bcase\s+[CA]_([A-Z][A-Z0-9_]+)\s*:", t))


def scripts_of(body: bytes, reg) -> list[dict]:
    names, p = census.parse_toc(body)
    rec = []
    census.walk(body, p, len(body), names, "", rec)
    out: list[dict] = []
    groups: list[tuple[int, str]] = []
    cur = None
    for full, ver, size, pos in sc.script_records(body, names, rec):
        depth = full.count("/")
        leaf = full.rsplit("/", 1)[-1]
        r = sc.Reader(body, pos, pos + size, names)
        groups = [(d, g) for d, g in groups if d < depth]
        if leaf == "ScriptGroup":
            groups.append((depth, r.astr()))
        elif leaf == "Script":
            nm = r.astr()
            r.astr(); r.astr(); r.astr()
            flags = [r.u8() for _ in range(6)]
            cur = {"script": nm, "group": "/".join(g for _, g in groups), "active": flags[0], "oneShot": flags[1], "depth": depth,
                   "cond": [], "act": [], "else": [], "or": 0}
            out.append(cur)
        elif leaf == "OrCondition" and cur is not None:
            cur["or"] += 1
            if cur["cond"]:
                cur["cond"].append("OR")
        elif leaf in ("Condition", "ScriptAction", "ScriptActionFalse") and cur is not None:
            kind = {"Condition": "condition", "ScriptAction": "action", "ScriptActionFalse": "action"}[leaf]
            typ = r.i32()
            name = r.namekey() if ver >= (4 if kind == "condition" else 2) else None
            vals: list = []
            ps = sc.params(r, vals)
            inverted = enabled = None
            if kind == "condition" and ver >= 5:
                enabled, inverted = r.i32(), r.i32()
            if kind == "action" and ver >= 3:
                enabled = r.i32()
            table = reg[kind]
            resolved = sc.resolve(table, kind, typ, name, len(ps), ver)
            rname = table[resolved][0] if resolved in table else (name or "?%d" % typ)
            args = []
            for t, v in vals:
                args.append(("%.0f,%.0f,%.0f" % v) if t == 16 else (v[2] if v[2] else ("%d" % v[0] if v[1] == 0 else "%d/%g" % (v[0], v[1]))))
            item = {"name": rname, "args": args, "off": enabled == 0, "not": bool(inverted)}
            cur[{"Condition": "cond", "ScriptAction": "act", "ScriptActionFalse": "else"}[leaf]].append(item)
    return out


def fmt(item) -> str:
    if item == "OR":
        return "OR"
    s = "%s%s(%s)" % ("NOT " if item["not"] else "", item["name"], ", ".join(item["args"]))
    return s + (" [off]" if item["off"] else "")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("base", type=Path)
    ap.add_argument("overlay", type=Path)
    ap.add_argument("--out", type=Path, default=ROOT / "docs" / "ennorath")
    ap.add_argument("--full", type=Path, help="where SCRIPT_MAP.tsv (every script's conditions and actions with their parameters) goes: the audit"
                    " folder (Drive, the arena archive), never the repository: it transcribes the mod's scripts")
    a = ap.parse_args()
    import os
    os.environ.setdefault("ROTWK_INSTALL", str(a.base))
    os.environ.setdefault("BFME2_INSTALL", str(a.base))  # census.py only checks it is set; BFME2's archives are not mounted here
    global census, sc
    import census as _census
    import script_census as _sc
    census, sc = _census, _sc
    m = mount(a.base, a.overlay)
    reg = sc.load_registry()
    kinds = ini_names(m)
    pconds, pacts = port_cases()
    music = music_cases()
    # the ordinals the ScriptEngine runs itself (script_census.ENGINE_OWN: never reach the conditions / actions switch)
    own = {reg[k][i][0] for k, ks in (("condition", "condition"), ("action", "action")) for i in sc.ENGINE_OWN[ks] if i in reg[k]}
    files = [n for n in m.files if (n.startswith("libraries\\") and n.endswith(".map")) or n.endswith(".scb")]
    rows, grants = [], []
    unported = collections.Counter()
    lib_total = lib_open = 0  # the progress share (owner 2026-11-19): scripts of the sides' libraries, and those with a type the port lacks
    unported_where = collections.defaultdict(set)
    for path in sorted(files):
        blob = m.read(path)
        if not blob:
            continue
        body, _ = census.decode(blob)
        src = "mod" if str(m.files[path][0]).startswith(str(a.overlay)) else "rotwk"
        is_music = path.startswith("libraries\\music_")
        for s in scripts_of(body, reg):
            missing = []
            for it in s["cond"]:
                if it != "OR" and it["name"] not in own and it["name"] not in (music if is_music else pconds):
                    missing.append(it["name"])
            for it in s["act"] + s["else"]:
                if it["name"] not in own and it["name"] not in (music if is_music else pacts):
                    missing.append(it["name"])
            if path.startswith("libraries\\"):
                lib_total += 1
                lib_open += 1 if missing else 0
            for n in missing:
                unported[n] += 1
                unported_where[n].add(path)
            trig = sorted({x for it in s["cond"] if it != "OR" for x in it["args"] if x in kinds})
            for it in s["act"] + s["else"]:
                if it["off"] or not GIVE.search(it["name"]):
                    continue
                for x in it["args"]:
                    if x in kinds and kinds[x] in ("science", "upgrade", "object", "specialpower"):
                        grants.append((kinds[x], x, it["name"], path, s["script"], "yes" if s["active"] else "no", " ".join(trig),
                                       "no" if it["name"] in missing else "yes"))
            rows.append((path, src, s["group"], s["script"], "%d%d" % (s["active"], s["oneShot"]), " AND ".join(fmt(c) for c in s["cond"]).replace("AND OR AND", "OR"),
                         " ; ".join(fmt(c) for c in s["act"]), " ; ".join(fmt(c) for c in s["else"]), " ".join(sorted(set(missing)))))
    a.out.mkdir(parents=True, exist_ok=True)
    clean = lambda v: str(v).replace("\t", " ").replace("\n", " ")  # noqa: E731
    if a.full:
        a.full.mkdir(parents=True, exist_ok=True)
        with open(a.full / "SCRIPT_MAP.tsv", "w", encoding="utf-8", newline="\n") as f:
            f.write("# The Ennorath script map (tools/mod/script_map.py): every script of the sides' script libraries and .scb files of the mod's mount.\n")
            f.write("file\tsource\tgroup\tscript\tactive,oneShot\tconditions\tactions\telse actions\tport has no case for\n")
            for r in rows:
                f.write("\t".join(clean(v) for v in r) + "\n")
    with open(a.out / "SCRIPT_GRANTS.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("# What the scripts give a player (tools/mod/script_map.py): look here before calling a science / upgrade / object 'given by nothing'.\n")
        f.write("kind\tname\taction\tfile\tscript\tactive\ttrigger (INI names its conditions read)\tport runs the action\n")
        for g in sorted(set(grants), key=lambda g: (g[0], g[1].lower(), g[3], g[4])):
            f.write("\t".join(clean(v) for v in g) + "\n")
    # MATRIX_TODO 13: the INI half of "who gives it"; with SCRIPT_GRANTS.tsv, a name in neither is given by nothing in the game
    ini = sorted(set(ini_grants(m, kinds)), key=lambda g: (g[0], g[1].lower(), g[3], g[2]))
    with open(a.out / "GRANTS_INI.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("# What the mod's INI gives (tools/mod/script_map.py): the field (a button's Upgrade, an ExperienceLevel's Upgrades, GrantUpgradeCreate's\n")
        f.write("# UpgradeToGrant, ...), its block and module. With SCRIPT_GRANTS.tsv: an upgrade / science in neither is given by nothing in the game.\n")
        f.write("kind\tname\tfield\tblock\tmodule\tfile\n")
        for g in ini:
            f.write("\t".join(clean(v) for v in g) + "\n")
    given = {g[1] for g in ini} | {g[1] for g in grants}
    never = sorted(n for n, k in kinds.items() if k in ("upgrade", "science") and n not in given)
    with open(a.out / "GRANTS_INI.tsv", "a", encoding="utf-8", newline="\n") as f:
        for n in never:  # tools/matrix_run.py reads these rows: a matrix row waiting for one of them is MOD-DATA
            f.write("\t".join((kinds[n], n, "(given by nothing)", "-", "-", "-")) + "\n")
    lines = ["# Ennorath script map", "",
             "Generated by `tools/mod/script_map.py` (re-run with the nightly maps). Part of the mod's mechanics is not in INI modules but in the",
             "script libraries the game runs for every side (`Libraries\\<name>\\<name>.map`, RefPack-compressed: a text search of the archives does",
             "not find them). Example: `Multiplayer_Human` / `ImladrisTechManve` grants `SCIENCE_ManveWisdom` while the player owns",
             "`LoremasterThingObjectManveReal` (the Manwe choice).", "",
             "- `SCRIPT_GRANTS.tsv` — every science / upgrade / object / power a script gives, with its trigger. **Before calling something",
             "  'given by nothing' (a button that never opens, a science nobody grants), look it up here.**",
             "- `SCRIPT_MAP.tsv` — every script with its conditions and actions (parameters included) and the condition / action types the port has no",
             "  case for. It transcribes the mod's scripts, so it is not in the repository: `script-map/SCRIPT_MAP.tsv` of the field audit",
             "  (Drive `field-audit-auto`, `ennorath-audit.zip` of the arena archive).", "",
             "Files: %d, scripts: %d, grants: %d." % (len({r[0] for r in rows}), len(rows), len(set(grants))), "",
             # read by tools/mod/progress_snapshot.py (columns scripts_total / scripts_open): keep the wording
             "Library scripts: %d; with a condition or action the port has no case for: %d." % (lib_total, lib_open),
             "(A script whose every type has a case is counted as executed: the case may still differ from RotWK, as a matrix PASS is checked",
             "behaviour and this is not.)", "",
             "`GRANTS_INI.tsv` is the INI half of «who gives it» (%d rows). Upgrades / sciences the mod defines that neither the INI nor a script"
             " gives: %d (a module or button waiting for one of them never runs in the original either; the matrix calls such a row MOD-DATA)."
             % (len(ini), len(never)), "",
             "## Condition / action types the mod's scripts use that the port does not execute", "",
             "| type | uses | files |", "|---|---|---|"]
    for n, c in unported.most_common():
        lines.append("| `%s` | %d | %s |" % (n, c, ", ".join(sorted(unported_where[n]))[:300]))
    (a.out / "SCRIPT_MAP.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("files %d scripts %d grants %d unported types %d" % (len({r[0] for r in rows}), len(rows), len(set(grants)), len(unported)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

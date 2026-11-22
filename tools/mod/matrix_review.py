#!/usr/bin/env python3
"""The button matrix's review (owner 2026-11-17: "find the matrix's own errors automatically, and dig into every change"): layers 2 and 3
after the run (layer 1, every red object swept again alone, is tools/matrix_run.py recheck).

    python3 tools/mod/matrix_review.py --ini <ennorath-ini zip> [--before <git rev>] [--out docs/ennorath/MATRIX_REVIEW.md]

Layer 2, the lint: each verdict checked against the mod's own data (the INI archive, .ini and .inc). A flag says the verdict is likely the
sweep's, not the port's; every rule is a mistake found once by hand (2026-11-17), so the same kind is caught the next time:
  L1 the button is in none of the command sets the object can wear (its CommandSet lines, its CommandSetUpgrade sets): the inventory placed it;
  L2 the same object and button is red in one faction and passes in another: the scene, not the port, decides it;
  L3 a red NOT-ON-BAR on a HIDE_WHILE_DISABLED button: retail hides it while disabled too;
  L4 a MOD-DATA verdict where the object's INI does list a module for the power (the sweep's template check missed it), or a red NO-MODULE
     where the INI lists none (it should read MOD-DATA);
  L5 a red FORMATION on an object without a HordeContain (a member made alone).
Layer 3, the changes: every button whose verdict moved between MATRIX.tsv at --before (the main the previous run measured) and now, with
its lint flags, the recheck's word (ORDER), and the commits since --before whose diff names the button's special power or upgrade.
Only names, verdict words and commit subjects are written: no game data.
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import subprocess
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MATRIX = REPO / "docs" / "ennorath" / "MATRIX.tsv"
NOT_RED = {"PASS", "STANCE", "DISABLED", "INVENTORY", "TEMPORARY", "MEMBER", "MOD-DATA", "NO-TARGET", "BY-DESIGN", "ORDER"}
START = re.compile(r"^(Object|ChildObject|ObjectReskin)[ \t]+(\S+)(?:[ \t]+(\S+))?", re.M)


class Mod:
    """The objects, command sets and command buttons of the mod's INI archive."""

    def __init__(self, zpath: Path):
        z = zipfile.ZipFile(zpath)
        self.objects: dict[str, tuple[str, str | None, str]] = {}
        self.sets: dict[str, str] = {}
        self.buttons: dict[str, str] = {}
        for n in z.namelist():
            if not n.lower().endswith((".ini", ".inc")):
                continue
            t = z.read(n).decode("latin-1")
            starts = list(START.finditer(t))
            for i, m in enumerate(starts):
                end = starts[i + 1].start() if i + 1 < len(starts) else len(t)
                self.objects.setdefault(m.group(2), (m.group(1), m.group(3), t[m.start():end]))
            for kind, store in (("CommandSet", self.sets), ("CommandButton", self.buttons)):
                for m in re.finditer(rf"^{kind}[ \t]+(\S+)", t, re.M):
                    end = t.find("\nEnd", m.end())
                    store.setdefault(m.group(1), t[m.end():end if end > 0 else len(t)])

    def text(self, obj: str) -> str | None:
        out, seen = [], 0
        while obj in self.objects and seen < 8:
            kind, parent, body = self.objects[obj]
            out.append(body)
            seen += 1
            if kind != "ChildObject" or not parent:
                break
            obj = parent
        return "".join(out) if out else None

    def wearable_sets(self, obj: str) -> set[str] | None:
        t = self.text(obj)
        if t is None:
            return None
        sets = set(re.findall(r"^\s*CommandSet\s*=\s*(\S+)", t, re.M))
        for block in re.findall(r"CommandSetUpgrade[^\n]*\n(.*?)\n\s*End", t, re.S):
            sets |= set(re.findall(r"CommandSet\w*\s*=\s*(\S+)", block))
        return sets

    def set_has(self, name: str, button: str) -> bool:
        return re.search(r"=\s*" + re.escape(button) + r"\b", self.sets.get(name, "")) is not None

    def option(self, button: str, opt: str) -> bool:
        m = re.search(r"^\s*Options\s*=\s*([^\n;]+)", self.buttons.get(button, ""), re.M)
        return bool(m and opt in m.group(1).split())

    def field(self, button: str, key: str) -> str:
        m = re.search(rf"^\s*{key}\s*=\s*(\S+)", self.buttons.get(button, ""), re.M)
        return m.group(1) if m else ""

    def lists_power(self, obj: str, power: str) -> bool:
        objs = [obj] + re.findall(r"InitialPayload\s*=\s*(\S+)", self.text(obj) or "")
        return any(re.search(r"SpecialPowerTemplate\s*=\s*" + re.escape(power) + r"\b", self.text(o) or "") for o in objs)


def rows_of(text: str) -> list[dict[str, str]]:
    return list(csv.DictReader([l for l in text.splitlines() if l and not l.startswith("#")], delimiter="\t"))


def status(rows) -> dict[tuple[str, str, str], tuple[str, str]]:
    """(faction, object, button) -> (category, detail): PASS if any line passed, else the first other line (as the README counts)."""
    out: dict[tuple[str, str, str], tuple[str, str]] = {}
    for r in rows:
        if not r["button"] or r["object"] == "(inventory)":
            continue
        k = (r["faction"], r["object"], r["button"])
        if r["result"] == "PASS":
            out[k] = ("PASS", r["detail"])
        else:
            out.setdefault(k, (r["result"], r["detail"]))
    return out


def lint(st, mod: Mod) -> dict[tuple[str, str, str], list[str]]:
    flags: dict[tuple[str, str, str], list[str]] = collections.defaultdict(list)
    by_pair = collections.defaultdict(dict)
    for (fac, obj, btn), (cat, _) in st.items():
        by_pair[(obj, btn)][fac] = cat
    for (fac, obj, btn), (cat, detail) in st.items():
        k = (fac, obj, btn)
        sets = mod.wearable_sets(obj)
        if sets is not None and btn in mod.buttons and not any(mod.set_has(s, btn) for s in sets):
            flags[k].append("L1: none of the object's command sets has the button (the inventory placed it)")
        if cat not in NOT_RED and any(c == "PASS" for f, c in by_pair[(obj, btn)].items() if f != fac):
            flags[k].append("L2: passes in " + ", ".join(f for f, c in by_pair[(obj, btn)].items() if c == "PASS" and f != fac)[:80])
        if cat == "NOT-ON-BAR" and mod.option(btn, "HIDE_WHILE_DISABLED"):
            flags[k].append("L3: HIDE_WHILE_DISABLED: hidden while disabled in retail too")
        power = mod.field(btn, "SpecialPower")
        if power and mod.text(obj) is not None:
            if cat == "MOD-DATA" and mod.lists_power(obj, power):
                flags[k].append(f"L4: MOD-DATA, but the INI lists a module for {power}")
            if cat == "NO-MODULE" and not mod.lists_power(obj, power):
                flags[k].append(f"L4: NO-MODULE, and the INI lists no module for {power} either: MOD-DATA")
        if cat == "FORMATION" and mod.text(obj) is not None and "HordeContain" not in (mod.text(obj) or ""):
            flags[k].append("L5: no HordeContain: a member made alone has no horde to form")
    return flags


def commits_naming(before: str, name: str) -> list[str]:
    if not name or not before:
        return []
    out = subprocess.run(["git", "-C", str(REPO), "log", "--format=%h %s", f"{before}..HEAD", "-S", name, "--", "engine/src"],
                         capture_output=True, text=True, encoding="utf-8").stdout.splitlines()
    return [l[:110] for l in out[:3]]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--ini", required=True, type=Path)
    ap.add_argument("--before", default="", help="the git rev whose MATRIX.tsv the previous run wrote")
    ap.add_argument("--out", type=Path, default=REPO / "docs" / "ennorath" / "MATRIX_REVIEW.md")
    a = ap.parse_args()
    mod = Mod(a.ini)
    now = status(rows_of(MATRIX.read_text(encoding="utf-8")))
    flags = lint(now, mod)
    lines = ["# Matrix review (tools/mod/matrix_review.py)", "",
             "Layer 2 (lint) flags verdicts the mod's own data contradicts; layer 3 lists every button whose verdict moved since the "
             "previous run. A flagged red is likely the sweep's; an unflagged red that moved is the one to check against RotWK.", ""]
    red_flagged = {k: v for k, v in flags.items() if now[k][0] not in NOT_RED}
    lines += [f"## Lint: {len(flags)} flagged, {len(red_flagged)} of them red", ""]
    by_rule = collections.Counter(f.split(":")[0] for v in flags.values() for f in v)
    lines.append("By rule: " + (", ".join(f"{k} {n}" for k, n in sorted(by_rule.items())) or "none"))
    lines.append("")
    for (fac, obj, btn), fl in sorted(red_flagged.items())[:80]:
        lines.append(f"- {fac[7:]} `{obj}` `{btn}` ({now[(fac, obj, btn)][0]}): " + "; ".join(fl))
    if a.before:
        try:
            old_text = subprocess.run(["git", "-C", str(REPO), "show", f"{a.before}:docs/ennorath/MATRIX.tsv"], capture_output=True,
                                      text=True, encoding="utf-8", check=True).stdout
        except subprocess.CalledProcessError:
            old_text = ""
        was = status(rows_of(old_text)) if old_text else {}
        moved = sorted(k for k in now.keys() | was.keys() if was.get(k, ("-",))[0] != now.get(k, ("-",))[0])
        worse = [k for k in moved if now.get(k, ("-",))[0] not in NOT_RED and was.get(k, ("-",))[0] in NOT_RED | {"-"}]
        lines += ["", f"## Changes since {a.before[:8]}: {len(moved)} buttons moved, {len(worse)} turned red", ""]
        for k in worse + [k for k in moved if k not in worse][:200 - len(worse)]:
            fac, obj, btn = k
            b, n = was.get(k, ("-", ""))[0], now.get(k, ("-", ""))
            name = mod.field(btn, "SpecialPower") or mod.field(btn, "Upgrade")
            cs = commits_naming(a.before, name) if n[0] not in NOT_RED else []
            lines.append(f"- {fac[7:]} `{obj}` `{btn}`: {b} -> {n[0]}" + (f" — {n[1][:140]}" if n[0] not in NOT_RED else "")
                         + (f"; lint: {'; '.join(flags[k])}" if flags.get(k) else "")
                         + (f"; commits naming `{name}`: {' | '.join(cs)}" if cs else ""))
    a.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"matrix review: {len(flags)} flagged ({len(red_flagged)} red) -> {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

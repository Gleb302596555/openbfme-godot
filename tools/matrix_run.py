#!/usr/bin/env python3
"""The Ennorath button matrix (docs/ennorath/BUTTON_SWEEP.md): press every ability and upgrade button of every faction on the mod's data and
collect what each did. Runs on Colab (the engine build and the game data are there), never on the owner's PC.

    python3 tools/matrix_run.py seeds   [--factions ...]            # the objects the data graph reaches, per faction
    python3 tools/matrix_run.py run     [--factions ...] [--jobs 3]  # one test process per faction, in parallel
    python3 tools/matrix_run.py collect                              # docs/ennorath/MATRIX.tsv + the faction table of docs/ennorath/README.md
    python3 tools/matrix_run.py diff [--against origin/main]          # PASS buttons that got worse; each needs a matrix_explain.d line

`all` does the three in order. The work folder (--out, default workspace/matrix) keeps per faction `<F>.seeds`, `<F>.tsv`, `<F>.log`.

How a faction runs: `engine/build/openbfme_tests -tc="ENNORATH sweep units*"` with
  OPENBFME_SWEEP_OVERLAY  the mod's install (its .big files mount over RotWK),
  OPENBFME_SWEEP_FACTION  the PlayerTemplate,
  OPENBFME_SWEEP_SEEDS    <F>.seeds: the objects tools/mod/ennorath_graph.py reaches past the command sets (forms, summons, objects an upgrade
                          or a power creates); the test's own inventory walks from them on,
  OPENBFME_SWEEP_REPORT   <F>.tsv: one line per pressed button (faction, object, button, PASS/FAIL, what happened),
  OPENBFME_SWEEP_MAP      the map (the mod's map cache has no "fall back 4p").
The test spawns each reached unit, horde, hero and structure, wears each of its command sets in turn, presses every ability / upgrade /
weapon-toggle button through the HUD and reads the world before and after (test_hud_factb_units.cpp unitsOf).

Only names and the test's own words reach docs/ (never game bytes).
"""
from __future__ import annotations

import argparse
import collections
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools" / "mod"))

FACTIONS = ["FactionArnor", "FactionMen", "FactionRohan", "FactionElves", "FactionImladris", "FactionDwarves", "FactionIsengard",
            "FactionMordor", "FactionWild", "FactionAngmar", "FactionEvilmen"]
MATRIX = REPO / "docs" / "ennorath" / "MATRIX.tsv"
README = REPO / "docs" / "ennorath" / "README.md"
TEST_CASE = "ENNORATH sweep units*"
DEFAULT_MAP = "map mp fall back 8p"
# graph edges (field names, lower case) that name an object as a requirement, a price rule or a label: the object is not the faction's content
NOT_CONTENT = {"requiredobject", "buildingrequired", "displaynameinvisibleforenemy", "useobjecttemplateforcostdiscount", "beaconname",
               "objecttodestroyforringentry", "workername"}

# what a FAIL line means, first match wins (the words are unitsOf's)
CATEGORIES = [
    ("CRASH", re.compile(r"^CRASH")),                                # the engine died at this press: the first thing to fix
    ("STANCE", re.compile(r"^S-289")),                              # the stance button opens the radial: a known stop, not a fault
    ("NO-STANCE", re.compile(r"^STANCE: ")),                          # MATRIX_TODO 3: the stance radial pressed, a stance not taken (red)
    ("NO-AURA", re.compile(r"^NO-AURA: ")),                            # MATRIX_TODO 5: an active aura that put its modifier on no target in range (red)
    ("UNPORTED", re.compile(r"^UNPORTED: ")),                          # MATRIX_TODO 4: the click refused by an unported per-type object test (red: a port gap)
    ("NO-END", re.compile(r"never expires, though its Duration")),        # MATRIX_TODO 2: a modifier applied for good though its list has a Duration (RW 0x805A8E)
    ("UPGRADE-IDLE", re.compile(r"bought, but .* did not execute")),      # MATRIX_TODO 1: an upgrade bought whose triggered module did not run (UpgradeMux)
    ("NO-FIRE", re.compile(r"did not fire")),
    ("NO-RULE", re.compile(r"no effect of .* is asserted")),         # the test cannot judge it yet
    ("NO-EFFECT", re.compile(r"fired but|fired with \d+ unported")),
    ("STUCK", re.compile(r"^STUCK")),
    ("NOT-ON-BAR", re.compile(r"not on the bar")),
    # 2026-11-17 (matrix review): by design, not faults: a temporary object leaves at once; a horde member made alone has no horde to form
    ("TEMPORARY", re.compile(r"^TEMPORARY")),
    ("MEMBER", re.compile(r"^MEMBER")),
    ("NO-TARGET", re.compile(r"^NO-TARGET")),                         # the scene has no fitting target: not tested
    ("MOD-DATA", re.compile(r"^MOD-DATA")),                           # the mod gives the button no module: RotWK presses it to no effect too
    ("NO-SELECT", re.compile(r"could not be selected")),
    ("LEFT-WORLD", re.compile(r"left the world")),
    ("INVENTORY", re.compile(r"does not exist")),
    ("GREY-BUG", re.compile(r"^GREY-BUG")),                          # still grey with every condition met (PORT-530): the port's availability rule differs from RotWK's
    ("DISABLED", re.compile(r"\(not enabled\)|^DISABLED: needs")),    # the button stays greyed at the top level: a requirement the test or the port misses
    ("NOT-APPLIED", re.compile(r"not on the unit after")),            # an upgrade button whose upgrade never lands
    ("NO-MODULE", re.compile(r"no module of the unit")),              # the power has no module on the unit to run it
    ("NO-SEND", re.compile(r"sent nothing")),                         # the press or its target click sent no message
    ("FORMATION", re.compile(r"HORDE_TOGGLE_FORMATION left the state")),
]


# Not red: the grey verdicts and the ones found to be the scene's or the mod's own (matrix review 2026-11-17). ORDER: red in the full run,
# fine when the object is swept alone (tools/matrix_run.py recheck): leftovers of the earlier objects, not the port.
NOT_RED = {"PASS", "STANCE", "DISABLED", "INVENTORY", "TEMPORARY", "MEMBER", "MOD-DATA", "NO-TARGET", "BY-DESIGN", "ORDER"}


def category(verdict: str, detail: str) -> str:
    if verdict == "PASS":
        return "PASS"
    for name, rx in CATEGORIES:
        if rx.search(detail):
            return name
    return "FAIL"


def env_path(name: str, arg: str | None) -> Path:
    p = arg or os.environ.get(name)
    if not p:
        raise SystemExit(f"{name} is not set (pass it or export it: /root/.agent_env on Colab)")
    return Path(p)


def cmd_seeds(a) -> None:
    import ennorath_graph as g

    base, mod = env_path("ROTWK_INSTALL", a.base), env_path("OPENBFME_ENNORATH_INSTALL", a.overlay)
    bfme2 = Path(a.bfme2 or os.environ.get("BFME2_INSTALL", "")) if (a.bfme2 or os.environ.get("BFME2_INSTALL")) else None
    db = g.load_db(g.mount(base, mod, bfme2))
    a.out.mkdir(parents=True, exist_ok=True)
    for f in a.factions:
        reached, order, _unresolved, _mods = g.walk(db, f)
        rows = []
        for node in order:
            typ, name = node
            if typ not in ("object", "objectreskin"):
                continue
            kinds = g.kinds_of(db, name)
            # what a player can click: a selectable object, not a projectile or an effect object
            if "SELECTABLE" not in kinds or "PROJECTILE" in kinds:
                continue
            # reached only through a field that names an object as a condition or a label, not as something the player gets
            if all(key.rsplit("/", 1)[-1].lower() in NOT_CONTENT for _src, key in reached[node]):
                continue
            via = ",".join(f"{src[1]}.{key}" for src, key in reached[node][:2])
            rows.append(f"{name}\t{via}")
        (a.out / f"{f}.seeds").write_text("\n".join(rows) + "\n", encoding="utf-8")
        print(f"{f}: {len(rows)} selectable objects reached")


TRACE = re.compile(r"^factb trace: (.+)$", re.M)
MAX_CRASHES = 40


def run_one(a, faction: str) -> tuple[str, int, float]:
    """One faction, rerun after each crash: the report is appended to as the test goes, the crashed press (or object, when a rerun crashes
    again before its first press) gets a CRASH line and is skipped with every press already reported."""
    seeds = a.out / f"{faction}.seeds"
    if not seeds.exists():
        raise SystemExit(f"{seeds} is missing: run `seeds` first")
    tsv, skip, log_path = a.out / f"{faction}.tsv", a.out / f"{faction}.skip", a.out / f"{faction}.log"
    env = dict(os.environ)
    env.update(OPENBFME_SWEEP_OVERLAY=str(env_path("OPENBFME_ENNORATH_INSTALL", a.overlay)), OPENBFME_SWEEP_FACTION=faction,
               OPENBFME_SWEEP_SEEDS=str(seeds), OPENBFME_SWEEP_REPORT=str(tsv), OPENBFME_SWEEP_MAP=a.map, OPENBFME_SWEEP_SKIP=str(skip),
               OPENBFME_SWEEP_GREY=str(REPO / "docs" / "ennorath" / "MATRIX_GREY.tsv"),  # PORT-530: the grey buttons' conditions (Port 220); absent -> the sweep keeps its old verdicts
               FACTB_TRACE="1")
    tsv.unlink(missing_ok=True)
    skip.write_text("", encoding="utf-8")
    t0 = time.time()
    rc = 0
    crashed: list[str] = []
    for attempt in range(MAX_CRASHES + 1):
        with open(log_path, "ab") as log:
            log.write(f"\n===== attempt {attempt + 1} =====\n".encode())
            log.flush()
            rc = subprocess.call([str(a.tests), f"-tc={TEST_CASE}"], cwd=REPO, env=env, stdout=log, stderr=subprocess.STDOUT)
        text = log_path.read_text(encoding="utf-8", errors="replace").rsplit("===== attempt", 1)[-1]
        if "CRASHED" not in text:
            break
        traces = TRACE.findall(text)
        last = traces[-1].strip() if traces else ""
        if not last:
            break  # crashed before the first object: nothing to skip, the log says why
        skips = set(skip.read_text(encoding="utf-8").splitlines())
        if last in skips:
            last = last.split(" ", 1)[0]  # the same press again (a rerun's spawn): skip the whole object
        crashed.append(last)
        obj, _, button = last.partition(" ")
        with open(tsv, "a", encoding="utf-8") as out:
            out.write(f"{faction}\t{obj}\t{button}\tFAIL\tCRASH: the test process died (SIGSEGV or similar) at this {'press' if button else 'object'}\n")
        done = set()
        for line in tsv.read_text(encoding="utf-8", errors="replace").splitlines():
            p = line.split("\t")
            if len(p) == 5 and p[2]:
                done.add(f"{p[1]} {p[2]}")
        skip.write_text("\n".join(sorted(done | skips | {last})) + "\n", encoding="utf-8")
    if crashed:
        print(f"{faction}: {len(crashed)} crash(es): {', '.join(crashed)}", flush=True)
    return faction, rc, time.time() - t0


def cmd_recheck(a) -> None:
    """Layer 1 of the matrix review (owner 2026-11-17): every object with a red button in this run is swept again alone (FACTB_ONLY), one
    process per faction; a button red in the full run and not red alone is ORDER (the earlier objects' leftovers broke the scene: ArnorArveduiNew
    and HarbringerOfDark left the world in the full run and live alone). Writes <out>/recheck/order.tsv for collect."""
    rec = a.out / "recheck"
    rec.mkdir(parents=True, exist_ok=True)
    red: dict[str, dict[tuple[str, str], str]] = collections.defaultdict(dict)
    for f in a.factions:
        p = a.out / f"{f}.tsv"
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split("\t")
            if len(parts) == 5 and parts[2] and category(parts[3], parts[4]) not in NOT_RED and not parts[4].startswith("CRASH"):
                red[f][(parts[1], parts[2])] = category(parts[3], parts[4])
    passed = collections.defaultdict(set)
    for f in a.factions:
        for line in (a.out / f"{f}.tsv").read_text(encoding="utf-8", errors="replace").splitlines() if (a.out / f"{f}.tsv").exists() else []:
            parts = line.split("\t")
            if len(parts) == 5 and parts[3] == "PASS":
                passed[f].add((parts[1], parts[2]))
    for f in list(red):
        red[f] = {k: v for k, v in red[f].items() if k not in passed[f]}

    def alone(f: str) -> tuple[str, int]:
        objs = sorted({o for o, _ in red[f]})
        if not objs:
            return f, 0
        tsv = rec / f"{f}.tsv"
        tsv.unlink(missing_ok=True)
        env = dict(os.environ)
        env.update(OPENBFME_SWEEP_OVERLAY=str(env_path("OPENBFME_ENNORATH_INSTALL", a.overlay)), OPENBFME_SWEEP_FACTION=f,
                   OPENBFME_SWEEP_SEEDS=str(a.out / f"{f}.seeds"), OPENBFME_SWEEP_REPORT=str(tsv), OPENBFME_SWEEP_MAP=a.map,
                   OPENBFME_SWEEP_SKIP=str(rec / "none.skip"), OPENBFME_SWEEP_GREY=str(REPO / "docs" / "ennorath" / "MATRIX_GREY.tsv"),
                   FACTB_ONLY=",".join(objs))
        (rec / "none.skip").write_text("", encoding="utf-8")
        with open(rec / f"{f}.log", "wb") as log:
            subprocess.call([str(a.tests), f"-tc={TEST_CASE}"], cwd=REPO, env=env, stdout=log, stderr=subprocess.STDOUT)
        return f, len(objs)

    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for f, n in pool.map(alone, [f for f in a.factions if red.get(f)]):
            print(f"recheck {f}: {n} objects swept alone", flush=True)
    rows = []
    for f, buttons in red.items():
        tsv = rec / f"{f}.tsv"
        if not tsv.exists():
            continue
        again: dict[tuple[str, str], list[str]] = collections.defaultdict(list)
        for line in tsv.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split("\t")
            if len(parts) == 5:
                again[(parts[1], parts[2])].append(category(parts[3], parts[4]))
        for key, was in buttons.items():
            cats = again.get(key)
            if cats and ("PASS" in cats or all(c in NOT_RED for c in cats)):
                rows.append(f"{f}\t{key[0]}\t{key[1]}\t{'PASS' if 'PASS' in cats else cats[0]}")
    (rec / "order.tsv").write_text("".join(r + "\n" for r in rows), encoding="utf-8")
    print(f"recheck: {sum(len(b) for b in red.values())} red buttons, {len(rows)} fine alone (ORDER)", flush=True)


def cmd_run(a) -> None:
    (a.out / "recheck" / "order.tsv").unlink(missing_ok=True)  # a new run: the old recheck does not apply
    if not a.tests.exists():
        raise SystemExit(f"{a.tests} is missing: build with tools/colab/build_tests.sh")
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for faction, rc, sec in pool.map(lambda f: run_one(a, f), a.factions):
            got = (a.out / f"{faction}.tsv").exists()
            print(f"{faction}: exit {rc}, {sec / 60:.1f} min, report {'yes' if got else 'MISSING (see ' + faction + '.log)'}", flush=True)


# Buttons whose failing verdict is the mod's own design, confirmed by the owner (2026-11-17): object, button, why. They count with the grey ones.
BY_DESIGN_FILE = REPO / "docs" / "ennorath" / "matrix_bydesign.tsv"
BY_DESIGN = {}
if BY_DESIGN_FILE.exists():
    for _line in BY_DESIGN_FILE.read_text(encoding="utf-8").splitlines():
        _f = _line.split("\t")
        if len(_f) >= 3 and not _line.startswith(("#", "object	")):
            BY_DESIGN[(_f[0], _f[1])] = _f[2]


def cmd_collect(a) -> None:
    order = {}
    if (a.out / "recheck" / "order.tsv").exists():
        for line in (a.out / "recheck" / "order.tsv").read_text(encoding="utf-8").splitlines():
            f = line.split("\t")
            if len(f) == 4:
                order[(f[0], f[1], f[2])] = f[3]
    rows = []
    for f in FACTIONS:
        p = a.out / f"{f}.tsv"
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split("\t")
            if len(parts) == 5:
                fac, obj, button, verdict, detail = parts
                cat = category(verdict, detail)
                alone = order.get((fac, obj, button))
                if cat not in NOT_RED and alone:
                    cat, detail = "ORDER", f"ORDER: red in the full run, swept alone {alone} (was: {detail})"
                why = BY_DESIGN.get((obj, button))
                if cat != "PASS" and why:  # the owner's word on the mod's own design (docs/ennorath/matrix_bydesign.tsv)
                    cat, detail = "BY-DESIGN", f"BY-DESIGN: {why} (was: {detail})"
                rows.append((fac, obj, button, cat, detail))
    if not rows:
        raise SystemExit(f"no <faction>.tsv in {a.out}: run `run` first")
    rows = list(dict.fromkeys(rows))  # a rerun after a crash repeats the inventory lines
    order = {f: i for i, f in enumerate(FACTIONS)}
    rows.sort(key=lambda r: (order.get(r[0], 99), r[1], r[2], r[3]))
    head = "# The Ennorath button matrix (tools/matrix_run.py collect; docs/ennorath/BUTTON_SWEEP.md). One line per pressed button and failure.\n"
    head += "faction\tobject\tbutton\tresult\tdetail\n"
    MATRIX.write_text(head + "".join("\t".join(r) + "\n" for r in rows), encoding="utf-8")

    # per faction: buttons pressed (an object with an empty button is an object-level line), passed, the red ones by category
    per = collections.defaultdict(lambda: {"buttons": {}, "objects": set()})
    for fac, obj, button, cat, _ in rows:
        if obj == "(inventory)":
            continue
        per[fac]["objects"].add(obj)
        if button:
            key = (obj, button)
            prev = per[fac]["buttons"].get(key)
            # a button with several failures counts once, by its first non-PASS category
            if prev is None or prev == "PASS":
                per[fac]["buttons"][key] = cat
    stamp = time.strftime("%Y-%m-%d")
    cells = {}
    for fac, d in per.items():
        cats = collections.Counter(d["buttons"].values())
        # the stance button (S-289: it opens the radial) is on every unit: a known stop, not counted as a button of the matrix
        total, ok = sum(cats.values()) - cats.get("STANCE", 0), cats.get("PASS", 0)
        # a greyed button is mostly a requirement the test does not meet (a forge the faction's objects do not have here): counted apart.
        # GREY-BUG (PORT-530) is a port bug, not an unmet requirement: it counts as red, and the per-faction line names it.
        grey = sum(v for k, v in cats.items() if k in NOT_RED and k not in ("PASS", "STANCE"))  # by design / not tested: not red either
        red = total - ok - grey
        cells[fac] = (f"[{total} кнопок: ✓{ok} ✗{red} серых {grey}](MATRIX.tsv) ({stamp})", str(ok))
        print(f"{fac}: {total} buttons, {ok} pass, {red} red, {grey} grey: " + ", ".join(f"{k} {v}" for k, v in cats.most_common() if k != "PASS"))
    update_readme(cells)
    print(f"wrote {MATRIX.relative_to(REPO)} ({len(rows)} lines) and the table of {README.relative_to(REPO)}")


# A matrix change may not hide a button that passed before (owner, 2026-11-16, after PR #587's report left out 54 PASS rows that turned red):
# `diff` compares MATRIX.tsv with the one at a git ref (the PR checker: main), per button (its first non-PASS line, as the README table counts).
# Every PASS -> anything else needs a line in docs/ennorath/matrix_explain.d/<task>.md naming `<object> <button>` and why (the test was wrong
# before, a stricter rule found a real gap with the module named, ...); unexplained ones fail the check.
EXPLAIN = REPO / "docs" / "ennorath" / "matrix_explain.d"


def button_status(text: str) -> dict[tuple[str, str, str], tuple[str, str]]:
    out: dict[tuple[str, str, str], tuple[str, str]] = {}
    for line in text.splitlines():
        parts = line.split("\t")
        if line.startswith(("#", "faction\t")) or len(parts) != 5 or not parts[2]:
            continue
        key = (parts[0], parts[1], parts[2])
        prev = out.get(key)
        if prev is None or (prev[0] == "PASS" and parts[3] != "PASS"):
            out[key] = (parts[3], parts[4])
    return out


def cmd_diff(a) -> None:
    old_text = subprocess.run(["git", "show", f"{a.base}:docs/ennorath/MATRIX.tsv"], cwd=REPO, capture_output=True, text=True,
                              encoding="utf-8", errors="replace")
    if old_text.returncode:
        raise SystemExit(f"no MATRIX.tsv at {a.base}: {old_text.stderr.strip()}")
    old, new = button_status(old_text.stdout), button_status(MATRIX.read_text(encoding="utf-8", errors="replace"))
    moves = collections.Counter((old[k][0], new[k][0]) for k in new if k in old and old[k][0] != new[k][0])
    explained = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(EXPLAIN.glob("*.md"))) if EXPLAIN.is_dir() else ""
    worse = [k for k in new if k in old and old[k][0] == "PASS" and new[k][0] != "PASS"]
    gone = [k for k in old if k not in new and old[k][0] == "PASS"]
    loose = [k for k in worse + gone if f"{k[1]} {k[2]}" not in explained]
    print(f"matrix diff against {a.base}: {len(new)} buttons ({len(old)} before)")
    for (f, t), n in moves.most_common():
        print(f"  {f} -> {t}: {n}")
    if gone:
        print(f"  PASS buttons no longer in the matrix: {len(gone)}")
    for k in loose[: a.limit]:
        was = old[k][1][:120]
        now = new[k][0] + ": " + new[k][1][:160] if k in new else "(gone from the matrix)"
        print(f"  UNEXPLAINED {k[0]} {k[1]} {k[2]}\n      was PASS: {was}\n      now {now}")
    if len(loose) > a.limit:
        print(f"  ... and {len(loose) - a.limit} more")
    if loose:
        raise SystemExit(f"{len(loose)} PASS buttons got worse without a line in {EXPLAIN.relative_to(REPO)}/<task>.md "
                         "(`<object> <button>`: why)")
    print("every PASS that got worse is explained" if worse or gone else "no PASS got worse")


def update_readme(cells: dict[str, tuple[str, str]]) -> None:
    """The faction table's "Матрица" and "Проверено" columns from the matrix; the other columns stay."""
    text = README.read_text(encoding="utf-8")
    out = []
    for line in text.splitlines(keepends=True):
        m = re.match(r"^\|[^|]*\|\s*`(Faction\w+)`\s*\|", line)
        if m and m.group(1) in cells:
            parts = line.rstrip("\r\n").split("|")
            # | name | id | buttons | powers | objects | registry | matrix | verified |
            if len(parts) >= 10:
                parts[7] = " " + cells[m.group(1)][0] + " "
                parts[8] = " " + cells[m.group(1)][1] + " "
                line = "|".join(parts) + "\n"
        out.append(line)
    README.write_text("".join(out), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=["seeds", "run", "recheck", "collect", "all", "diff"])
    ap.add_argument("--against", dest="base_ref", default="origin/main", help="diff: the git ref whose MATRIX.tsv is the baseline")
    ap.add_argument("--limit", type=int, default=60, help="diff: unexplained rows printed")
    ap.add_argument("--factions", nargs="*", default=FACTIONS)
    ap.add_argument("--jobs", type=int, default=3, help="faction processes at once (each loads the whole game: ~3-5 GB)")
    ap.add_argument("--out", type=Path, default=REPO / "workspace" / "matrix")
    ap.add_argument("--tests", type=Path, default=REPO / "engine" / "build" / "openbfme_tests")
    ap.add_argument("--map", default=DEFAULT_MAP)
    ap.add_argument("--base", help="RotWK 2.01 install (default $ROTWK_INSTALL)")
    ap.add_argument("--overlay", help="the mod's install (default $OPENBFME_ENNORATH_INSTALL)")
    ap.add_argument("--bfme2", help="BFME2 1.06 install (default $BFME2_INSTALL)")
    a = ap.parse_args()
    if a.step == "diff":
        a.base = a.base_ref
        cmd_diff(a)
        return
    unknown = [f for f in a.factions if f not in FACTIONS]
    if unknown:
        raise SystemExit(f"unknown factions: {unknown}")
    steps = ["seeds", "run", "recheck", "collect"] if a.step == "all" else [a.step]
    for s in steps:
        {"seeds": cmd_seeds, "run": cmd_run, "recheck": cmd_recheck, "collect": cmd_collect}[s](a)


if __name__ == "__main__":
    main()

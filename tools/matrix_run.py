#!/usr/bin/env python3
"""The Ennorath button matrix (docs/ennorath/BUTTON_SWEEP.md): press every ability and upgrade button of every faction on the mod's data and
collect what each did. Runs on Colab (the engine build and the game data are there), never on the owner's PC.

    python3 tools/matrix_run.py seeds   [--factions ...]            # the objects the data graph reaches, per faction
    python3 tools/matrix_run.py run     [--factions ...] [--jobs 3]  # one test process per faction, in parallel
    python3 tools/matrix_run.py collect                              # docs/ennorath/MATRIX.tsv + the faction table of docs/ennorath/README.md

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
    ("NO-FIRE", re.compile(r"did not fire")),
    ("NO-RULE", re.compile(r"no effect of .* is asserted")),         # the test cannot judge it yet
    ("NO-EFFECT", re.compile(r"fired but|fired with \d+ unported")),
    ("STUCK", re.compile(r"^STUCK")),
    ("NOT-ON-BAR", re.compile(r"not on the bar")),
    ("NO-SELECT", re.compile(r"could not be selected")),
    ("LEFT-WORLD", re.compile(r"left the world")),
    ("INVENTORY", re.compile(r"does not exist")),
    ("DISABLED", re.compile(r"\(not enabled\)")),                     # the button stays greyed at the top level: a requirement the test or the port misses
    ("NOT-APPLIED", re.compile(r"not on the unit after")),            # an upgrade button whose upgrade never lands
    ("NO-MODULE", re.compile(r"no module of the unit")),              # the power has no module on the unit to run it
    ("NO-SEND", re.compile(r"sent nothing")),                         # the press or its target click sent no message
    ("FORMATION", re.compile(r"HORDE_TOGGLE_FORMATION left the state")),
]


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


def cmd_run(a) -> None:
    if not a.tests.exists():
        raise SystemExit(f"{a.tests} is missing: build with tools/colab/build_tests.sh")
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for faction, rc, sec in pool.map(lambda f: run_one(a, f), a.factions):
            got = (a.out / f"{faction}.tsv").exists()
            print(f"{faction}: exit {rc}, {sec / 60:.1f} min, report {'yes' if got else 'MISSING (see ' + faction + '.log)'}", flush=True)


def cmd_collect(a) -> None:
    rows = []
    for f in FACTIONS:
        p = a.out / f"{f}.tsv"
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            parts = line.split("\t")
            if len(parts) == 5:
                fac, obj, button, verdict, detail = parts
                rows.append((fac, obj, button, category(verdict, detail), detail))
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
        # a greyed button is mostly a requirement the test does not meet (a forge the faction's objects do not have here): counted apart
        grey = cats.get("DISABLED", 0)
        red = total - ok - grey
        cells[fac] = (f"[{total} кнопок: ✓{ok} ✗{red} серых {grey}](MATRIX.tsv) ({stamp})", str(ok))
        print(f"{fac}: {total} buttons, {ok} pass, {red} red, {grey} grey: " + ", ".join(f"{k} {v}" for k, v in cats.most_common() if k != "PASS"))
    update_readme(cells)
    print(f"wrote {MATRIX.relative_to(REPO)} ({len(rows)} lines) and the table of {README.relative_to(REPO)}")


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
    ap.add_argument("step", choices=["seeds", "run", "collect", "all"])
    ap.add_argument("--factions", nargs="*", default=FACTIONS)
    ap.add_argument("--jobs", type=int, default=3, help="faction processes at once (each loads the whole game: ~3-5 GB)")
    ap.add_argument("--out", type=Path, default=REPO / "workspace" / "matrix")
    ap.add_argument("--tests", type=Path, default=REPO / "engine" / "build" / "openbfme_tests")
    ap.add_argument("--map", default=DEFAULT_MAP)
    ap.add_argument("--base", help="RotWK 2.01 install (default $ROTWK_INSTALL)")
    ap.add_argument("--overlay", help="the mod's install (default $OPENBFME_ENNORATH_INSTALL)")
    ap.add_argument("--bfme2", help="BFME2 1.06 install (default $BFME2_INSTALL)")
    a = ap.parse_args()
    unknown = [f for f in a.factions if f not in FACTIONS]
    if unknown:
        raise SystemExit(f"unknown factions: {unknown}")
    steps = ["seeds", "run", "collect"] if a.step == "all" else [a.step]
    for s in steps:
        {"seeds": cmd_seeds, "run": cmd_run, "collect": cmd_collect}[s](a)


if __name__ == "__main__":
    main()

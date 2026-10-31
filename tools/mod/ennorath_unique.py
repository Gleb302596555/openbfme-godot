#!/usr/bin/env python3
"""Which open classes of docs/ennorath/MODULE_GAPS.md are Ennorath's own to port, and which would repeat upstream's work.

Upstream's lane MODCOMPAT-1 takes its gap census top-down, by how many mods hit a class (docs/MOD-COMPAT.md). Every RotWK mod inherits retail's
templates, so a class retail RotWK uses ranks high there. Each open class of the table lands in one of four kinds, the first that applies:

  upstream-main   upstream/main has its runtime class: do not port, merge upstream
  upstream-work   an unmerged upstream branch or pull request has it: look at that work first
  shared          retail RotWK's templates use it (engine/tests/data/object_model/rotwk201_module_histogram.tsv): upstream is likely to reach it
  ennorath-only   no retail RotWK template uses it: nobody else's census ranks it for retail's sake; another mod could still ask for it

    ennorath_unique.py [--no-fetch] [--update]     the table on stdout; --update also writes it into MODULE_GAPS.md between the markers
    ennorath_unique.py --check <Class> [--no-fetch]  before taking a class: exit 0 free to port, 1 upstream has it or works on it, 2 unknown class

Identifiers and counts only: no game data. The pull requests are fetched read-only into refs/upstream-pr/ (a local namespace, never pushed).
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
GAPS = REPO / "docs/ennorath/MODULE_GAPS.md"
HISTOGRAM = REPO / "engine/tests/data/object_model/rotwk201_module_histogram.tsv"
REGISTRY = REPO / "engine/data/rotwk-201/module-registry.json"
REMOTE = "upstream"
MAIN = f"{REMOTE}/main"
BEGIN = "<!-- ennorath_unique.py: begin -->"
END = "<!-- ennorath_unique.py: end -->"
ORDER = ("upstream-main", "upstream-work", "shared", "ennorath-only")
ROW = re.compile(r"^\|\s*(\d+)\s*\|\s*`(\w+)`\s*\|\s*([^|]*?)\s*\|\s*(\d+|—)\s*\|")


def git(*args: str) -> str:
    done = subprocess.run(["git", "-C", str(REPO), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    # git grep answers 1 for "no match"
    if done.returncode != 0 and not (args[0] == "grep" and done.returncode == 1):
        raise RuntimeError(f"git {' '.join(args[:3])}: {done.stderr.strip()}")
    return done.stdout


def open_classes(text: str) -> dict[str, tuple[int, str, str]]:
    """class -> (group, status, reachable objects) for the table's rows whose status is not ported"""
    out: dict[str, tuple[int, str, str]] = {}
    for line in text.splitlines():
        m = ROW.match(line)
        if m and not m.group(3).startswith("ported"):
            out[m.group(2)] = (int(m.group(1)), m.group(3), m.group(4))
    return out


def retail_objects(text: str) -> dict[str, int]:
    """class -> retail RotWK templates using it (the histogram's first count)"""
    out: dict[str, int] = {}
    for line in text.splitlines():
        cols = line.split("\t")
        if len(cols) >= 2 and cols[1].isdigit():
            out[cols[0]] = int(cols[1])
    return out


def registry_names(text: str) -> set[str]:
    return set(re.findall(r'"name": "(\w+)"', text))


def classify(name: str, in_main: set[str], in_work: dict[str, list[str]], retail: dict[str, int]) -> str:
    if name in in_main:
        return "upstream-main"
    if name in in_work:
        return "upstream-work"
    return "shared" if retail.get(name, 0) > 0 else "ennorath-only"


def runtime_pattern(names: list[str]) -> str:
    """a runtime class's definition or binding; ModuleData classes and forward declarations do not match. The class can be another's: AIUpdate.cpp
    binds "AnimalAIUpdate" to its AmbientAIUpdate by the name"""
    alt = "|".join(sorted(names))
    return rf"""(^class +({alt})\b[^;]*$|bindRuntime<({alt})\b|make_unique<({alt})\b|bindModuleProc\("({alt})"|[^.>]name == "({alt})")"""


def found_in(pattern: str, refs: list[str]) -> dict[str, set[str]]:
    """class -> the refs whose engine/src has its runtime"""
    out: dict[str, set[str]] = {}
    names = re.compile(r"""(?:class +|bindRuntime<|make_unique<|bindModuleProc\("|name == ")(\w+)""")
    for i in range(0, len(refs), 20):
        # each line is <ref>:<path>:<match> (a ref name holds no ':')
        for line in git("grep", "-E", "-o", pattern, *refs[i:i + 20], "--", "engine/src").splitlines():
            ref, _, rest = line.partition(":")
            m = names.search(rest.partition(":")[2])
            if m and "Unported" not in rest:  # an UnportedModule stand-in (SpawnUnitBehavior) is no port
                out.setdefault(m.group(1), set()).add(ref)
    return out


def fetch() -> None:
    git("fetch", "--quiet", REMOTE)
    git("fetch", "--quiet", REMOTE, f"+refs/pull/*/head:refs/upstream-pr/*")


def unmerged_refs() -> list[str]:
    """upstream's branches and pull requests with commits upstream/main lacks"""
    lines = git("for-each-ref", "--no-merged", MAIN, "--format=%(refname)", f"refs/remotes/{REMOTE}", "refs/upstream-pr").splitlines()
    return [r for r in lines if not r.endswith("/HEAD")]


def short_ref(ref: str) -> str:
    if ref.startswith("refs/upstream-pr/"):
        return "PR #" + ref.rsplit("/", 1)[1]
    return ref.replace(f"refs/remotes/{REMOTE}/", "")


def survey(names: list[str]) -> tuple[set[str], dict[str, list[str]]]:
    pattern = runtime_pattern(names)
    in_main = set(found_in(pattern, [MAIN]))
    work = {k: sorted(short_ref(r) for r in v) for k, v in found_in(pattern, unmerged_refs()).items() if k not in in_main}
    return in_main, work


def table(classes: dict[str, tuple[int, str, str]], in_main: set[str], in_work: dict[str, list[str]], retail: dict[str, int]) -> str:
    rows: dict[str, list[str]] = {k: [] for k in ORDER}
    # within a kind the fewest retail templates first: those rank lowest in upstream's census
    for name, (group, status, reach) in sorted(classes.items(), key=lambda kv: (retail.get(kv[0], 0), kv[1][0], kv[0])):
        kind = classify(name, in_main, in_work, retail)
        note = ", ".join(in_work.get(name, []))
        rows[kind].append(f"| {kind} | `{name}` | {group} | {status} | {reach} | {retail.get(name, 0)} | {note} |")
    counts = ", ".join(f"{k} {len(rows[k])}" for k in ORDER)
    head = ["| Вид | Класс | Гр. | Статус | Достижимых в моде | Объектов в RotWK | Где у upstream |", "|---|---|---:|---|---:|---:|---|"]
    return "\n".join([f"Итого: {counts}.", "", *head, *(r for k in ORDER for r in rows[k])])


def update_gaps(text: str, body: str) -> str:
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        raise ValueError(f"{GAPS.name}: expected one pair of the markers {BEGIN} / {END}")
    nl = "\r\n" if "\r\n" in text else "\n"
    start = text.index(BEGIN) + len(BEGIN)
    return text[:start] + nl + body.replace("\n", nl) + nl + text[text.index(END):]


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-fetch", action="store_true", help="use the upstream refs already here")
    ap.add_argument("--update", action="store_true", help="write the table into MODULE_GAPS.md between the markers")
    ap.add_argument("--check", metavar="CLASS", help="exit 0 free to port, 1 upstream has it or works on it, 2 no such class")
    a = ap.parse_args(argv)
    if not a.no_fetch:
        fetch()
    gaps_text = GAPS.read_bytes().decode("utf-8")
    retail = retail_objects(HISTOGRAM.read_text(encoding="utf-8"))
    if a.check:
        if not re.fullmatch(r"\w+", a.check):
            print(f"{a.check}: not a class name", file=sys.stderr)
            return 2
        if a.check not in registry_names(REGISTRY.read_text(encoding="utf-8")):
            print(f"{a.check}: not a module class of the retail registry", file=sys.stderr)
            return 2
        in_main, work = survey([a.check])
        kind = classify(a.check, in_main, work, retail)
        print(f"{a.check}: {kind} (retail RotWK templates: {retail.get(a.check, 0)}){' ' + ', '.join(work[a.check]) if a.check in work else ''}")
        return 1 if kind in ("upstream-main", "upstream-work") else 0
    classes = open_classes(gaps_text)
    if not classes:
        print(f"{GAPS.name}: no open class rows found", file=sys.stderr)
        return 2
    in_main, work = survey(sorted(classes))
    body = table(classes, in_main, work, retail)
    sys.stdout.reconfigure(encoding="utf-8")
    print(body)
    if a.update:
        GAPS.write_bytes(update_gaps(gaps_text, body).encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

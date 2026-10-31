#!/usr/bin/env python3
"""The two work queues of the Ennorath port, read from docs/ennorath/MODULE_GAPS.md and PROBLEMS.md (docs/ennorath/PROCESS.md).

    ennorath_queue.py priority [--update]    open classes by how many objects of the mod reach them, unreachable ones apart;
                                             --update also writes the list into MODULE_GAPS.md between the markers
    ennorath_queue.py checklist [--update]   everything waiting for a check, one list: --update writes docs/ennorath/CHECKLIST.md

A class with a scenario in engine/tests/test_ennorath_scenarios.cpp (TEST_CASE "ENNORATH scenario <Class>: ...") is checked by that
scenario on the mod's own data: it leaves the owner's list unless its row says the check needs eyes or ears. Reach counts objects, not how
much a player notices: the owner still reads the top of the priority list. Identifiers and counts only: no game data.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
GAPS = REPO / "docs/ennorath/MODULE_GAPS.md"
PROBLEMS = REPO / "docs/ennorath/PROBLEMS.md"
CHECKLIST = REPO / "docs/ennorath/CHECKLIST.md"
SCENARIOS = REPO / "engine/tests/test_ennorath_scenarios.cpp"
BEGIN = "<!-- ennorath_queue.py priority: begin -->"
END = "<!-- ennorath_queue.py priority: end -->"
OPEN_STATUS = ("missing", "parsed-only", "partial")
WAITING = ("ждёт проверки", "не проверено")
TOP = 15
SCENARIO = re.compile(r'TEST_CASE\("ENNORATH scenario (\w+):')
PROBLEM = re.compile(r"^### (P-\d+)\. (.*)$", re.M)
PROBLEM_STATUS = re.compile(r"^- Статус: (.*)$", re.M)


@dataclass
class Row:
    group: str
    name: str
    status: str
    reach: int
    factions: str
    what: str
    state: str


def rows(text: str) -> list[Row]:
    """The class rows of the table; a row whose reach is not a number or a dash is an error, never a zero."""
    out: list[Row] = []
    for n, line in enumerate(text.splitlines(), 1):
        cells = [c.strip() for c in line.split("|")]
        if len(cells) < 12 or not cells[1].isdigit() or not re.fullmatch(r"`\w+`", cells[2]):
            continue
        if cells[4] != "—" and not cells[4].isdigit():
            raise ValueError(f"MODULE_GAPS.md:{n}: reach {cells[4]!r} of {cells[2]} is neither a number nor a dash")
        out.append(Row(cells[1], cells[2].strip("`"), cells[3], 0 if cells[4] == "—" else int(cells[4]), cells[5], cells[8], cells[10]))
    return out


def is_open(row: Row) -> bool:
    return row.status.startswith(OPEN_STATUS)


def priority(table: list[Row]) -> tuple[list[Row], list[Row]]:
    """(reachable open classes, most objects first; open classes nothing in the mod reaches)."""
    live = sorted((r for r in table if is_open(r) and r.reach > 0), key=lambda r: (-r.reach, r.name))
    return live, sorted((r for r in table if is_open(r) and r.reach == 0), key=lambda r: r.name)


def priority_text(table: list[Row]) -> str:
    live, idle = priority(table)
    total = sum(r.reach for r in live)
    lines = [f"Открытых классов: {len(live) + len(idle)}, достижимых объектов мода: {total}. Сверху вниз, занятые тоже показаны:", ""]
    lines += ["| № | Класс | Объектов | Статус | Что делает | Состояние |", "|---|---|---|---|---|---|"]
    for n, r in enumerate(live[:TOP], 1):
        lines.append(f"| {n} | `{r.name}` | {r.reach} | {r.status} | {r.what} | {r.state} |")
    if len(live) > TOP:
        lines += ["", "Дальше: " + ", ".join(f"`{r.name}` ({r.reach})" for r in live[TOP:]) + "."]
    if idle:
        lines += ["", "Отложены, ни один объект мода их не достигает: " + ", ".join(f"`{r.name}`" for r in idle) + "."]
    return "\n".join(lines) + "\n"


def write_priority(table: list[Row]) -> None:
    text = GAPS.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        raise ValueError(f"{GAPS.name}: needs one {BEGIN} and one {END}")
    head, rest = text.split(BEGIN)
    GAPS.write_text(head + BEGIN + "\n" + priority_text(table) + END + rest.split(END)[1], encoding="utf-8", newline="")


def scenario_classes(source: str) -> set[str]:
    return set(SCENARIO.findall(source))


def waiting_classes(table: list[Row]) -> list[Row]:
    return [r for r in table if any(w in r.state for w in WAITING)]


def waiting_problems(text: str) -> list[tuple[str, str, str]]:
    """(id, title, status) of each owner's entry whose status says a check is waiting."""
    heads = list(PROBLEM.finditer(text))
    out = []
    for i, head in enumerate(heads):
        body = text[head.end():heads[i + 1].start() if i + 1 < len(heads) else len(text)]
        status = PROBLEM_STATUS.search(body)
        if status and status.group(1).startswith("[ ]") and "провер" in status.group(1):
            out.append((head.group(1), head.group(2), status.group(1)))
    return out


def checklist_text(table: list[Row], problems: list[tuple[str, str, str]], scripted: set[str]) -> str:
    wait = sorted(waiting_classes(table), key=lambda r: (-r.reach, r.name))
    auto = [r for r in wait if r.name in scripted]
    owner = [r for r in wait if r.name not in scripted]
    lines = ["# Проверка партии", "",
             "Файл создан `python tools/mod/ennorath_queue.py checklist --update`, вручную не правится. Один список на партию вместо отдельных просьб. "
             "Итог каждой строки записывается в `MODULE_GAPS.md` или `PROBLEMS.md`.", ""]
    lines += [f"## Записи владельца ({len(problems)})", ""]
    lines += [f"- [ ] **{pid}**. {title}" for pid, title, _ in problems] or ["Нет."]
    lines += ["", f"## Классы, которые нужно посмотреть в игре ({len(owner)})", "", "Сверху те, что встречаются в моде чаще.", ""]
    lines += [f"- [ ] `{r.name}` ({r.reach}; {r.factions}): {r.what}" for r in owner] or ["Нет."]
    lines += ["", f"## Проверено сценарием на данных мода ({len(auto)})", "",
              "Логику проверяет `openbfme_tests -tc=\"ENNORATH scenario*\"`. Смотреть в игре стоит только внешний вид и звук.", ""]
    lines += [f"- `{r.name}` ({r.reach})" for r in auto] or ["Нет."]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("queue", choices=("priority", "checklist"))
    parser.add_argument("--update", action="store_true", help="write the list into the repository too")
    args = parser.parse_args()
    table = rows(GAPS.read_text(encoding="utf-8"))
    if not table:
        raise SystemExit(f"{GAPS}: no class rows found")
    sys.stdout.reconfigure(encoding="utf-8")
    if args.queue == "priority":
        print(priority_text(table), end="")
        if args.update:
            write_priority(table)
        return 0
    source = SCENARIOS.read_text(encoding="utf-8") if SCENARIOS.exists() else ""
    text = checklist_text(table, waiting_problems(PROBLEMS.read_text(encoding="utf-8")), scenario_classes(source))
    print(text, end="")
    if args.update:
        CHECKLIST.write_text(text, encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    sys.exit(main())

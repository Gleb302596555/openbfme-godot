"""tools/mod/ennorath_queue.py over made-up table rows (no mod or game bytes)."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_queue as eq  # noqa: E402

TABLE = """| 1 | `Big` | missing | 500 | все 11 | 1 (1) | — | loud thing | S-1 | НЕ НАЧАТО |
| 2 | `Small` | partial (2026-10-30) | 3 | Men | 1 (1) | — | small thing | S-2 | ЗАНЯТО — Codex, 2026-10-31 |
| 3 | `Idle` | missing | 0 | — | 1 (1) | — | idle thing | S-3 | НЕ НАЧАТО |
| 4 | `Done` | ported (2026-10-30) | 9 | Men | 1 (1) | — | done thing | S-4 | СДЕЛАНО, ждёт проверки владельца |
| 5 | `Dash` | parsed-only | — | — | 1 (1) | — | dash thing | S-5 | НЕ НАЧАТО |
| 6 | `Script` | ported (2026-10-30) | 7 | Men | 1 (1) | — | scripted | S-6 | код upstream, не проверено на Ennorath в игре |
| 13 | условие скрипта `X` | не перенесено | — | ИИ
"""


def test_rows_read_the_class_rows_only():
    assert [r.name for r in eq.rows(TABLE)] == ["Big", "Small", "Idle", "Done", "Dash", "Script"]


def test_a_bad_reach_is_an_error_not_a_zero():
    with pytest.raises(ValueError, match="many"):
        eq.rows("| 1 | `Big` | missing | many | все 11 | 1 | — | x | S-1 | НЕ НАЧАТО |\n")


def test_priority_sorts_by_reach_and_sets_unreachable_classes_apart():
    live, idle = eq.priority(eq.rows(TABLE))
    assert [r.name for r in live] == ["Big", "Small"]
    assert [r.name for r in idle] == ["Dash", "Idle"]


def test_a_scenario_moves_a_class_off_the_owners_list():
    source = 'TEST_CASE("ENNORATH scenario Script: the mod\'s data")\n'
    problems = [("P-1", "звук", "[ ] **ЗАНЯТО** — ждёт проверки владельца")]
    text = eq.checklist_text(eq.rows(TABLE), problems, eq.scenario_classes(source))
    owner, auto = text.split("## Проверено сценарием")
    assert "`Done`" in owner and "`Script`" not in owner and "**P-1**" in owner
    assert "`Script`" in auto and "`Done`" not in auto


def test_waiting_problems_take_open_entries_that_wait_for_a_check():
    text = ("### P-1. Открыта\n- Статус: [ ] **ЗАНЯТО** — ждёт проверки владельца\n"
            "### P-2. Закрыта\n- Статус: [x] **ВЫПОЛНЕНО** — подтверждено проверкой\n"
            "### P-3. В работе\n- Статус: [ ] **ЗАНЯТО**\n")
    assert [p[0] for p in eq.waiting_problems(text)] == ["P-1"]

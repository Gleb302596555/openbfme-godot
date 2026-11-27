# Приёмка PR арены, 2026-11-27 (песочница 2)

Продолжение `ARENA_ACCEPTANCE_2026-11-26.md`. Агент на песочнице 2 (промпт `workspace/for-arena/PROMPT-arena-accept-sb2-2026-11-27.md`)
коммитов не делал. Его логи были только в `/content/arena-save` песочницы 2; туннель упал в тот же день, логи, вероятно, потеряны.
Ниже — его отчёт (числа — строки `[doctest]` из его прогонов, Colab GCC).

## Базовая линия: `main` 62697c032 (все ветки review стоят на нём)

Падает и на чистом `main`, значит, не вина PR:

| Фильтр | test cases | assertions | Где |
|---|---|---|---|
| `hero facing*` | 7: 5 / 2 failed | 305: 291 / 14 | `test_hero_facing_execution.cpp:174`, `:313` (P-194) |
| `AIPOW-1: end to end*`, `RAI-POW-2: DEBUFFECONOMYBUILDING*` | 2: 0 / 2 | 53: 50 / 3 | `test_aipow1.cpp:687` `22==23`, `:1624` `9==5`, `:1682` `9==6` |
| `*b2spell1*` | 3: 1 / 2 | 130: 106 / 24 | `test_b2spell1.cpp:45` `4==3`, `test_ownerfix1_input.cpp:1172` |
| `play4*heroes*` | 2: 1 / 1 | 9214: 9196 / 18 | `test_play4_heroes_retail.cpp:414..444` |
| `attack1 heroes*` | 1: 0 / 1 | 592: 571 / 21 | `test_play4_heroes_retail.cpp:672..718` |
| `siege1 gate*` | 6: 4 / 2 | 540: 526 / 14 | `test_siege1_gate.cpp:254..308` |
| `BACKLOG2 S-950*` | 1: 0 / 1 | 32: 29 / 3 | `test_backlog2_retail.cpp:363..365` |
| `backlog4 defect r1*` | 5: 4 / 1 | 146: 144 / 2 | `test_backlog4_defect.cpp:514..515` (P-195) |
| `bridgeclick1*` | 1: 0 / 1 | 65: 64 / 1 | `test_bridge1_bridges.cpp:167` (вне границ форка) |

Огонь на базе зелёный: `fire logic*` 10/10, `PORT-161*` 8/8, `LOCAL-140*` 4/4, `ENNORATH scenario FireLogic*` 3/3.

## Вердикты

| PR | Port | Голова | Вердикт |
|---|---|---|---|
| #901 | 333 | `41754b4c8` | готов; перенесён в `main` 022375296. Только документация, `engine/src` не тронут; вердикт RW 0x68BF44 сверен с `rw.py dis` (единственный вызывающий 0x860ED6, `beginSlowDeath`). `*SlowDeath*` 10/10; пины без сдвига: perf1 11/11, uber1 7/7, save4b 18/18, Evendim 1/1 |
| #618 | 216 | `52e35e2b3` | готов, пины не досняты (туннель). `PORT-216*` 8/8 (318 assertions), `*eometry*` 59/59, `*overlap*` 22/22; все падения широкого прогона есть и на базе |
| #635 | 266 | `d4e0b66d0` | готов, пины не досняты (туннель). Сборка с GDExtension прошла; `PORT-266*` 8/8, `PORT-70*` 4/4 |
| #924 | 337 | `c6a12fcc0` | вернуть автору (комментарий в PR). `FireLogicConfig.h:73-77` обнуляет материалы и ставит `maxCellsBurning = 0x7fffffff` вместо 1000; `FireLogicSystem.cpp` (`newMap`, `update`, `resetCellToOriginalFlammability`) при `terrain() == nullptr` берёт тип 0 вместо 1 (Grass). Ломает `PORT-161*` (2/8), `LOCAL-140*` (3/4), `ENNORATH scenario FireLogic*` (2/3). С откатом этих двух кусков всё зелёное, `PORT-337*` 8/8 |
| #907 | 321 | `461ac8dab` | вернуть автору (комментарий в PR). `PORT-321*` 5/6: `test_port_321.cpp:258` требует шаблон `TestDozer321`, его нет в `kPort321Objects`. Плюс `INFERENCE (S-7661)` в `PlaceEventTranslator.cpp` / `WallSpan.cpp` меняет видимое поведение без адреса RW — нужно решение владельца |
| #742 | 235 | `deee9c136` | вернуть автору (комментарий в PR). Черновик: из 28 полей issue #558 перенесено 1, S-255 не тронут, 8 полей под S-8031/S-8032 без доведённых читателей, лишний `docs/ennorath/manual-check.tsv`, номера стопов в тексте PR не совпадают с веткой, нет обязательных блоков Port PR |

## Осталось

- #618 и #635: пины (`perf1*`, `uber1*`, `save4b*`, `skirmish?ai?vs?ai: Evendim*`) на свежем `main`; Claude снимает их на ПК
  (ветка `agent/claude/pins-618-635`; при слиянии #635 в `test_port_70.cpp` строк стало 74, gameplay 21, presentation 10).

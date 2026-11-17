# PORT-250 — кнопка SPECIAL_POWER, у которой нет модуля (issue #578)

Данные: Ennorath 1.9.8.3 (`ennorath-ini-1.9.8.3.zip`), матрица `docs/ennorath/MATRIX.tsv` (62 строки `NO-MODULE`, 12 сил).
Бинарник: `game.dat` RotWK 2.01, все адреса ниже перечитаны `python tools/mod/rw.py workspace/RotWK/game.dat dis|xref <адрес>`.
Донор BFME2 1.06 (`/tmp/open-bfme-2`, файл `Code/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarRappellerCount.cpp`, `ControlBar::getCommandAvailability`
строка 405) прочитан как механизм: **`DONOR BFME2 0x0053BD66`**.

## 1. Что делает RotWK: плечо 0x943343 функции доступности RW 0x942733

Таблица переходов RW 0x9438CF отправляет команды 0x18 (`SPECIAL_POWER`), 0x20 (`SPECIAL_POWER_FROM_COMMAND_CENTER`) и 0x25
(`SPECIAL_POWER_TOGGLE`) в одно плечо — RW 0x943343. Прочитано `rw.py dis`:

| адрес RW | что делает | ответ доступности |
|---|---|---|
| 0x943343 | `cmp byte [edi + 0x106], 0` — ShowButton кнопки | 0 → RW 0x942C5D = **3 (скрыта)** |
| 0x94334A / 0x943356 | `esi = [edi + 0x44]` — указатель SpecialPowerTemplate, разрешённый при разборе INI; `test esi, esi` | null → RW 0x942C5D = **3 (скрыта)** |
| 0x94335E | RW 0x68B678 — владелец объекта; null → 0x942C5D | 0 → скрыта |
| 0x943372 | RW 0x6AAC52 на владельце (игрок активен) | 0 → RW 0x942C5D = скрыта |
| 0x94337F / 0x943385 | RW 0x6A0713 — науки шаблона (вектор по `[ebp - 0x2C]`), RW 0x6AC207 — владение первой | ни одной → RW 0x942C5D = скрыта |
| 0x9433D6 .. 0x9433F0 | RW 0x6AC25F (наука выключена) / RW 0x6AC287 (наука скрыта) | да → RW 0x9438C8 = **0 (серая)** |
| 0x94341F .. 0x943433 | `lea eax,[esi+0x64]` (маска PreventActivationConditions шаблона) в RW 0x75CDC4 на `Object + 0x94` | любой статус → RW 0x9438C8 = 0 |
| **0x943439 .. 0x943480** | **ворота UnitCost**: `RW 0x688D3C` (getFinalOverride) → `+ 0x80` (UnitCost); если не 0, то contain объекта (`Object + 0x258`, слот 0x7C) и его счётчик членов (слот 0x188, RW 0x943477) | `UnitCost > счётчик` → RW 0x9438C8 = **0 (серая)** |
| 0x943486 | RW 0x68C26D — модуль по SpecialPowerTemplate среди **своих** модулей объекта (`[obj + 0x24C]`, без поиска по членам отряда) | — |
| 0x9434A9 | RW 0x68C461 — SpecialAbilityUpdate по типу силы (final override `+ 0x1C`), RW 0x851371 — имя цепочки | — |
| 0x9434F7 | `test esi, esi` — модуля силы нет: скан CommandTrigger RW 0x94351A .. 0x943562 (`+ 0x14` кнопки-триггера = 0x18, её `+ 0x44` — сила); **все выходы цикла** идут в RW 0x942FF1 (`xor eax,eax; inc eax`) | **1 (доступна)** |
| 0x943569 .. 0x943606 | модуль есть: RW 0x943569 (requirementsMet), RW 0x943585 (isReadyForDisplay, проценты в `[ebp + 0x14]`), RW 0x9435C3 / 0x9435DC (RW 0x942490, модельные условия), ветка BattlePlan (`+ 0x1C == 0x24`) | 4/5/…, 1, 2 |

**Ответ для строки NO-MODULE: сила есть как шаблон, а модуля нет → 1, кнопка доступна.** RotWK её не прячет и не сереет: нажатие
уходит в RW 0x940435 (случай 0x18) как `MSG_DO_SPECIAL_POWER`, и в логике его некому исполнить — ничего не происходит.
Кнопка, у которой `+ 0x44` пуст (силы нет даже как шаблона), — наоборот, скрыта (3): такие строки в матрицу не попадают, потому что
её инструмент разрешает имя силы.

Порт это уже делал: `ControlBar::evaluate`, плечо `GUI_COMMAND_SPECIAL_POWER` (`engine/src/GameClient/ControlBar.cpp`) — `!sp` →
`ButtonState::Enabled` (комментарий «RW 0x9434F7..0x943564 … availability 1»), `!t` → `ButtonState::Hidden`. **Чего не было — ворот
UnitCost (RW 0x943439 .. 0x943480)**: `ActionManager::unitCostMet` (RW 0x82E000 / 0x82D605) их знает, а панель нет, поэтому сила,
которая тратит членов отряда, показывалась живой кнопкой, а нажатие молча ничего не делало. PORT-250 переносит эти ворота в плечо
(одна и та же проверка в двух местах RW — в `ActionManager` и в панели; тест `PORT-250*` падает без неё).

## 2. Двенадцать сил: почему у объектов нет модуля и что видит игрок

Строки матрицы — это «надеть на объект каждый его CommandSet». Модуль силы часто лежит на **отряде** (`...Horde`), а набор команд,
в котором стоит кнопка, объявлен и у членов отряда (`CommandSet = <Faction>SlavesHordeCommandSet` у `HaradArnorSlaveAxe`), — в игре
выбранным объектом бывает отряд, и модуль находится. Это `scene` (стенд матрицы), а не ошибка порта.

| сила | кнопок | где модуль по данным 1.9.8.3 | почему у объекта строки модуля нет | итог |
|---|---:|---|---|---|
| `SpecialAbilityKampfschreiEffectWK` | 15 | `AngmarWitchkingNew`, `AngmarRogashEnnorath` (Angmar) | `MordorWitchKing` / `…OnFellBeast` / `…Pferd` несут `SpecialAbilityKampfschreiEffect` (без `WK`), а кнопка `Command_SpecialAbilityMordorWitchkingScreechEffeckt` называет силу `WK`: опечатка данных мода | `as RotWK`: доступна (RW 0x942FF1), нажатие ничего не делает |
| `SpecialAbilityHaradSlavesBuildBoost` | 11 | отряды `HaradArnorSlaveAxeHorde`, `HaradDwarvenSlaveHorde`, `HaradElfSlaveHorde`, `HaradRohanSlaveHorde`, `HaradrrimSlaveAxeHorde`, `HaradGondorSlaveHorde`, `HaradSlaveOrcHorde` | у членов отряда тот же набор команд, что у отряда (`SpecialPowerModule` + `WeaponFireSpecialAbilityUpdate` — на отряде) | `scene` (у `HaradOrcSlaveAxe` / `HaradOrcSlaveHammer` / `HaradRohanSlave1/2` отряда-носителя в данных нет: набор команд недостижим, `as RotWK`) |
| `SpecialAbilityHaradMillWorking` | 11 | те же отряды (`SpecialAbilityHaradMillWorking` + `…WorkingAI`) | то же | `scene` |
| `SpecialAbilityElfCloak` | 6 | `ElvenMirkwoodArcherHorde`, `MirkwoodAvariWarriorHorde`, `MirkwoodWachterPfadeHorde`, `MirkwoodWachterPfadeSummoned(Horde/TrandHorde)` | у лучников и стражей тот же набор с кнопкой плаща, модуль (`SpecialPowerModule` + `ToggleHiddenSpecialAbilityUpdate`) на отряде | `scene`; у `MirkwoodWarriorHorde` модуля нет ни у кого — `as RotWK` |
| `SpecialAbilityWargHowl` | 6 | `IsengardWargBowHorde`, `GundabadWargRiderHorde`, `IsengardWargPackHorde`, `AngmarWolfRiders`, `IsengardBergwargreiterHorde` | у всадников тот же набор команд, вой на отряде (`SpecialPowerModule` + `…Update`) | `scene`; `RhudaurWolfRider` без носителя — `as RotWK` |
| `SpecialAbilityBerserkerLightTorch` | 5 | `WeaponChangeSpecialPowerModule` у наёмников Ханда/Харада (`KhandMerchenaryHorde`, `KhandGanbatar`, `HaradAlatar_mod`, …) | кнопки `Command_WindRidersWeaponToggle` / `Command_ToggleEisenbergEliteWeapon` — общая кнопка смены оружия, взятая в набор Ривенделла и гномов; у `ElvenRivendellLancerMod` свои силы `SpecialAbilityNoldorMastery` | `as RotWK`: доступна, нажатие ничего не делает |
| `SpecialAbilityBlackArrows` | 2 | `DwarvenMenOfDaleHorde`, `DwarvenMenOfDaleHordeMod` | у `DwarvenMenOfDale` набор отряда | `scene`; `DwarvenMenOfDale_Summoned` без носителя — `as RotWK` |
| `SpecialAbilityAngmarCharge` | 2 | ни у кого (`AngmarDarkDunedain`, `AngmarSnowTroll` только объявляют кнопку) | сила объявлена, модуля в данных нет | `as RotWK`: доступна, нажатие ничего не делает |
| `SpecialAbilityArnorFarmWorking` | 1 | `HobbitPikemanHorde` (`SpecialPowerModule` + `WeaponFireSpecialAbilityUpdate`) | `HobbitPikeman` носит набор `RohanHobbitPikeCommandSet` отряда | `scene` |
| `SpecialAbilityAngmarThrallMasterSummonWolfRiders` | 1 | `IsengardVerraeterHorde` (+ отряды Дол-Гулдура/Мордора) | кнопка `Command_SpecialAbilityToggleMountedHorde` в наборе `IsengardVerraeterCommandSet`; модуль на отряде | `scene` |
| `SpecialAbilityHaradrimArcherBarbedArrowNew` | 1 | `HaradrimArcherHordeNew` | у `HaradrimArcherModNew` набор отряда | `scene` |
| `SpecialAbilityHaradrimArcherPoisonArrow` | 1 | `HaradrimRangerModHorde` | у `HaradrimRangerMod` набор `HaradRangerCommandSet` отряда | `scene` |

Итог по 62 строкам: **ошибки порта нет ни в одной**; 46 строк — `scene` (модуль на отряде, стенд надевает набор члена), 16 —
`as RotWK` (доступная кнопка без модуля; нажатие не даёт ничего), 0 — правок данных.

## 3. Тесты

`engine/tests/test_port_250.cpp`, лейн `engine/cmake/Port250.cmake`, фильтр `-tc="PORT-250*"`:

* ворота UnitCost: отряд с одним членом и силой `UnitCost = 3` → серая; отряд с четырьмя → доступна (падает без правки);
* сила без модуля → доступна (RW 0x9434F7 → 0x942FF1), соседняя кнопка с модулем на том же объекте — доступна;
* сила, которой нет как шаблона (`+ 0x44` пуст) → скрыта (RW 0x94334A → 0x942C5D).

## 4. Осталось

* Стенд `unitsSweep` (`engine/tests/test_hud_factb_units.cpp`) считает строки `NO-MODULE` падениями; чтобы матрица показывала только
  настоящие ошибки, его строка должна стать `pass` с причиной из таблицы выше. В песочнице он не запускается (нужны данные мода), а
  править его без прогона — риск для ночного прогона; поэтому эта часть не сделана (стоп не нужен: это работа по данным, не поведение).
* Цепочка SpecialAbilityUpdate (RW 0x9434B7 .. 0x9434F4, RW 0x851371 → команда-триггер) и ветка BattlePlan (RW 0x9435EA .. 0x943606,
  `+ 0x1C == 0x24`) в порте не перенесены — они в стопе `S-850` (`docs/STOPS.md`), задача их не расширяет.

# PORT-275 (fork issue 642): анализ 7 кнопок NOT-ON-BAR в Ennorath

Дата: 2026-11-17, закрыт 2026-11-18. Матрица кнопок от 2026-11-17 (main c73bb057); все строки INI перевыверены
по архиву мода `ennorath-ini-1.9.8.3`. Вывод по движку: все семь кнопок удерживают уже перенесённые правила
RotWK — расхождения порта нет, движок не тронут. По Galadriel первоначальная гипотеза (слоты 3/4 за
`InitialVisible`) опровергнута чтением INI: истинная причина — порядок обхода свипа и штатный замок
«одна способность на зеркало».

## Сводная таблица

| кнопка | объект | причина | RW-адрес | INI |
|---|---|---|---|---|
| `Command_PurchaseUpgradeRohanForgedBladesForPeasants` | RohanPeasantHorde | `Upgrade_RohanForgedBladesForPeasants` в Ennorath не объявлен (только закомментирован в `SubUpgradeTemplateNames`, upgrade.ini:2125) -> null `UpgradeTemplate` -> Hidden | RW 0x943050 | commandbutton.ini:13273/13276, commandset.ini:2878 |
| `Command_PurchaseUpgradeRohanHeavyArmorForPeasants` | RohanPeasantHorde | `Upgrade_RohanHeavyArmorForPeasants` не объявлен (upgrade.ini:2158, закомментирован) -> null -> Hidden | RW 0x943050 | commandbutton.ini:13365/13368, commandset.ini:2879 |
| `Command_SpecialAbilityFortressHaradSpyTechAI` | HaradFortressCitadel | слот 32 (индекс 31) вне окон push 8-15 / 16-24 / 25-30 при `InitialVisible = 8`; кнопка с `AutoAbility = Yes` для ИИ | RW 0x97D718, RW 0x943D6F | commandset.inc:25069-25109 (слот 32 — :25108), commandbutton.inc:46920 |
| `Command_GaladrielMirrorDissentLocked` | GaladrielDarkMirror | свип нажимает слоты по порядку; Vision (слот 2) срабатывает, замок «одна способность» сменяет набор на Used -> слот 4 уходит с бара | RW 0x8B7C68, RW 0x943D6F | galadrielmirror.ini:181-184, commandset.inc:27548 |
| `Command_GaladrielMirrorWardByMirror` (база) | GaladrielDarkMirror | то же (слот 3) | те же | те же |
| `Command_GaladrielMirrorDissent` | GaladrielDarkMirror8 | то же (слот 4 набора `Set8`) | те же | commandset.inc:27540 |
| `Command_GaladrielMirrorWardByMirror` (8) | GaladrielDarkMirror8 | то же (слот 3) | те же | те же |

## Rohan: ForgedBladesForPeasants / HeavyArmorForPeasants

`RohanPeasantHordeCommandSet` (commandset.ini:2875): слот 3 — `Command_PurchaseUpgradeRohanForgedBladesForPeasants`
(:2878), слот 4 — `Command_PurchaseUpgradeRohanHeavyArmorForPeasants` (:2879). Кнопки commandbutton.ini:13273 и
:13365 ссылаются на `Upgrade_RohanForgedBladesForPeasants` / `Upgrade_RohanHeavyArmorForPeasants`. В upgrade.ini
объявлений этих апгрейдов нет — только закомментированные упоминания в `SubUpgradeTemplateNames` (upgrade.ini:2125,
2158); MATRIX.tsv:944-945 помечает оба как несуществующие. Правило RW (getCommandAvailability, arm OBJECT_UPGRADE):

- RW 0x943050: кнопка `OBJECT_UPGRADE` резолвит `UpgradeTemplate` по имени; null -> RW 0x942C5D (Hidden).
- DONOR BFME2 0x00339679 (`INI_parseUpgradeTemplate.cpp`): неизвестный апгрейд хранится как null.

Вывод: RotWK прячет обе кнопки точно так же (мод не додал апгрейды крестьянам). Для `matrix_bydesign.tsv`:
INI commandbutton.ini:13273 / 13365, RW 0x943050.

## Harad: SpyTechAI

`HaradFortressCommandSet` (commandset.inc:25069): `InitialVisible = 8` (:25070). Push-окна (0-based, «Starts its
counting at 0»): `Command_SelectUpgradesHaradFaction` start 8 count 8 (commandbutton.inc:46227-28),
`Command_SelectRevivablesHaradFortress` start 16 count 9 (:46252-53), `Command_SelectUpgradesSpyMenu` start 25
count 6 (:46241-42). Покрываются индексы 0-7 (база), 8-15, 16-24, 25-30. `Command_SpecialAbilityFortressHaradSpyTechAI`
— слот 32, индекс 31 (commandset.inc:25108): вне всех окон. Правило RW:

- RW 0x97D718 (`resetRanges`): пустой стек диапазонов получает `(0, InitialVisible)`.
- RW 0x71CF0A / RW 0x75CA97: пустой стек -> `(0, 33)`; иначе вершина стека.
- RW 0x943D6F (`populateCommand`): окно i показывает слот `rangeStart + i`, только `i < rangeCount`.

Кнопка (commandbutton.inc:46920, `SpecialAbilityFortressHaradSpyTrainingAI`, `AutoAbility = Yes`) — для
компьютерного игрока, человек её никогда не получает; в RotWK она недостижима так же. Для `matrix_bydesign.tsv`:
INI commandset.inc:25070 (`InitialVisible = 8`) и :25108 (слот 32), RW 0x97D718 / RW 0x943D6F.

## Galadriel: DissentLocked / WardByMirror — истинная причина

Перевыверенные данные мода:

- Наборы (commandset.inc:27532 базовый / :27540 `GaladrielDarkMirrorCommandSet8` / :27548 Used): слоты 1 Passive,
  2 Vision, 3 WardByMirror, 4 DissentLocked (в `Set8` — 4 Dissent); **`InitialVisible` нет ни в одном** — все слоты
  в диапазоне.
- Кнопки (commandbutton.inc:54359 Passive, :54371 Vision, :54385 WardByMirror, :54400 Dissent, :54418 DissentLocked):
  у всех `InPalantir = Yes` и `Radial = Yes`; у DissentLocked — `NEED_UPGRADE Upgrade_Level_8` с комментарием мода
  «Discord shown (locked) on the Mirror placed by a Dark Galadriel below level 8» (:54417).
- Силы (specialpower.ini:3901 Vision / :3908 WardByMirror / :3918 Dissent): без `RequiredSciences`.
- Модули: базовое зеркало (galadrielmirror.ini:10) несёт `OCLSpecialPower ModuleTag_MirrorVisionPower` (:162) и
  `ModuleTag_MirrorWardPower` (:171); модуль `ModuleTag_MirrorDissentPower` (:296) — только у ChildObject
  `GaladrielDarkMirror8` (:293), который и носит `GaladrielDarkMirrorCommandSet8` (:294).

Следовательно, на свежем зеркале все четыре кнопки на баре (DissentLocked — серая: NEED_UPGRADE без апгрейда
даёт Restricted, RW 0x9428A3..0x942993). Строки NOT-ON-BAR объясняет порядок свипа
(`test_hud_factb_units.cpp` обходит слоты набора по возрастанию) и штатный замок мода «только ОДНА способность
на зеркало» (комментарий galadrielmirror.ini:179-180):

1. Свип нажимает слот 2, `Command_GaladrielMirrorVision` срабатывает (MATRIX.tsv:1606/1610: «fired after 44 frames»).
2. Каст ставит на зеркало `USER_5` (мгновенное посерение, `SetModelCondition` у OCLSpecialPower :166/:175/:301) и
   создаёт объект-метку (`GaladrielMirrorVisionBase`, galadrielmirror.ini:206), чья аура
   (`ModuleTag_MirrorUsedMark`, :223-229, RefreshDelay 33 мс, фильтр `+GaladrielDarkMirror +GaladrielDarkMirror8
   SAME_PLAYER`) вешает `GaladrielMirrorUsedModifier` (attributemodifier.inc:15996) ->
   `Upgrade_GaladrielMirrorUsed` (upgrade.ini:3445).
3. `CommandSetUpgrade ModuleTag_MirrorUsedLock` зеркала (galadrielmirror.ini:181-184) срабатывает
   (RW 0x8B7C68 -> `Object::m_commandSetOverride`) — бар показывает `GaladrielDarkMirrorUsedCommandSet`
   (commandset.inc:27548): только Passive.
4. Когда свип доходит до слотов 3 и 4 (WardByMirror, DissentLocked; у `GaladrielDarkMirror8` — WardByMirror,
   Dissent), их уже нет на баре -> «not on the bar» (MATRIX.tsv:1604-1605, 1607-1608, 1611).

Для игрока в RotWK всё то же самое: после единственного каста остальные кнопки исчезают. Расхождения порта нет.
Тесты `PORT-275*` пинают: правило окна `InitialVisible` (слоты за ним не на баре), четыре кнопки свежего зеркала
на баре (DissentLocked — серая), свап набора уровня 8 выводит Dissent на бар в следующем update, и замок
«одна способность» оставляет на баре только Passive.

## Что не потребовалось

Движок не менялся: все правила порта совпадают с RW (доказано дизассемблером и тестами). В PR — только тесты
`engine/tests/test_port_275.cpp`, лейн `engine/cmake/Port275.cmake`, эта записка и строка ночного дозора.

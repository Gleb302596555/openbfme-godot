# Модули Ennorath, которых нет в порте (MODULE_GAPS)

Снимок 2026-10-30. Только идентификаторы и счётчики, данных игры нет. Инструменты лежат в игнорируемой `workspace/ennorath-audit/tools`
(`reach.py`, `mapscan.py`, `sample.py`, `mkdoc.py` и прежние `dumpini.py`, `modcensus2.py`, `rw.py`). Эффекты, прочитанные только по именам полей, помечены INFERENCE.

## Что сняли

- **Список движка** (`OPENBFME_MODULE_COVERAGE_OUT`, `test_module_coverage.cpp`): 329 классов, перенесено 167, базовый ИИ 6, только читаются 14, отсутствуют 142.
- **Эффективные INI мода** (BFME2 → RotWK 2.01 → Ennorath, `dumpini.py`): модули в строках Behavior/Body/ClientUpdate/ClientBehavior/Draw. Из 263 имён 7 это частицы вне реестра (`DefaultDraw`, `RenderObjectDraw`, `GpuDraw`, `LightningDraw`, `ButterflyDraw`, `StreakDraw`, `QuadDraw`).
  По записям цифры совпали с исходными: 164 перенесённых класса, 76 отсутствующих (2239 записей), 14 только читаются.
- **Достижимость**: `tools/mod/ennorath_graph.py`, обход `PlayerTemplate` всех 11 игровых фракций. Достижимо 2708 объектов из 8956 эффективных определений. По фракциям: Men 366, Arnor 379, Rohan 385, Elves 280, Imladris 453, Dwarves 325, Isengard 304, Mordor 364, Wild 335, Angmar 366, Evilmen 335.
- **На картах**: имя объекта встречается как строка в одном из 267 файлов карт `_____enrth_maps.big` (`mapscan.py`; INFERENCE: имя в строке скрипта тоже считается). Граф от фракций нейтральные объекты карт (логова крипов, сундуки, руины, птицы) не видит, поэтому «достижимо» для них 0, а «на картах» нет.
- **Перепись игры** (`tools/mod/gap_census.py`, 3000 кадров, Fall Back 8p, один бездействующий игрок и три сложных ИИ): класс, который реально попал на живой объект, получает число в колонке «живых».

### Две поправки к исходным цифрам

1. Граф находит модуль только по тегу `ModuleTag_*`; у `AimWeaponModuleTag`, `WatchTheTreb`, `Module_EmotionTracker` и подобных тег другой. Первый прогон занижал, например, `AimWeaponBehavior` (4 вместо 32 достижимых). Прогон исправлен: класс берётся из значения поля, тег не нужен.
2. Перепись показала то, чего нет в списке по INI: **условия скриптов ИИ** `PLAYER_HAS_NUM_UNITS_WITH_UPGRADE` (18600 раз) и `PLAYER_BUILT_UPGRADE` (600) не перенесены и всегда ложны, а действие `FOCAL_LENGTH_CAMERA` ничего не делает. Это не модуль, но так компьютерный противник не видит свои улучшения. Заводится как отдельная строка ниже (группа 13).

## Порядок работы (первые 10 групп)

Записи владельца идут первыми. Первая незавершённая, **P-9** (приказы по Alt), не модуль, а отдельная подсистема (`AiOrdersManager`, очереди приказов, сохранение); в рабочем дереве от прошлой сессии лежат её незаконченные `AIOrdersManager.h` / `ObjectOrderQueue.h` и правки `AIGroup`/`CommandXlat`, другой сессии сейчас нет. Я её **не взял**, чтобы не смешивать два больших переноса; нужно решение владельца. Следующая запись, связанная с этой работой, **P-10**, идёт группой 1.

| № | Группа | Классы | Кто заметит | Запись |
|---:|---|---|---|---|
| 1 | Сундуки | SalvageCrateCollide + статические контакты | Дворфы, Харад, Дикие + сундуки на 8 типах объектов карт | P-10 |
| 2 | Логова и руины | RebuildHoleExposeDie, RebuildHoleBehavior, RubbleRiseUpdate | крипы на картах (логова до 173 карт), Дикие | P-29 (руины и сундуки) |
| 3 | Захват и бросок | GrabPassengerSpecialPower, FlingPassengerSpecialAbilityUpdate, AutoPickUp/Bored/ClickReaction | Мордор, Дикие, Ангмар, Изенгард | P-16 |
| 4 | Настенные требушеты | OCLUpdate, SlaveWatcherBehavior | все фракции (замки) | — |
| 5 | Отражение снарядов | DeflectSpecialPower | Арнор, Харад, Рохан; 24 живых в переписи; в моде бездействует (S-5601) | — |
| 6 | Башни и отражение урона | EvacuateDamage, ReflectDamage | боевые башни 8 фракций, Ангмар | — |
| 7 | Разделение отряда | SplitHordeSpecialPower, CombineHordeSpecialPower | Изенгард, Мордор | — |
| 8 | Смерть летунов | GiantBirdSlowDeathBehavior, ShipSlowDeath, ClearanceTestingSlowDeath | все 11 фракций (орлы, назгулы на птицах) | P-12 (рядом) |
| 9 | Герои и способности | FreeLifeBody, EntEnraged, OneRingPenalty, SpecialEnemySense, GiveUpgrade, Oathbreakers*, Replace/Teleport/Store, LocomotorSetUpgrade | Имладрис, Арнор, Рохан, Эльфы, Харад | P-14 (кольцо), P-13 |
| 10 | Звук и подсказки | ModelConditionAudioLoop, AudioLoopUpgrade, EvaAnnounceClientCreate, AnnounceBirthAndDeath, TooltipUpgrade | все 11 | P-23, P-24 (тест звука) |

Дальше: 11 геометрия и прицел (GeometryUpgrade, AimWeaponBehavior, мумаки), 12 эффекты и рендер (W3DStreakDraw, Tornado, Light, ProjectileStream, мост P-18), 13 ИИ (PickupStuffUpdate, CommandButtonHuntUpdate, условия скриптов), 14 «статус отсутствует, но работает» (TerrainResourceClientBehavior, W3DBuffDraw).

## Таблица

«Достижимых» — объектов, до которых доходит хотя бы одна из 11 фракций; «на картах» — объектов (макс. число карт у одного); «живых» — объектов с этим классом на конец переписи ИИ. Состояние: НЕ НАЧАТО / ЗАНЯТО / ПРИОСТАНОВЛЕНО / БЛОКИРОВАНО / ВЫПОЛНЕНО (только после ответа владельца).

| Гр. | Класс | Статус | Достижимых | Фракции | На картах | Живых | Что заметит игрок | Стоп | Состояние |
|---:|---|---|---:|---|---|---:|---|---|---|
| 1 | `SalvageCrateCollide` | ported (2026-10-30) | 6 | Dwarves, Evilmen, Wild | 8 (4) | — | подбор сундуков (с карты, от дани, от трибьюта): деньги/награда; сейчас сундук ничего не даёт (P-10) | S-260, S-220, S-581 | СДЕЛАНО В КОДЕ, тест 6/6, ждёт проверки владельца в игре |
| 2 | `RebuildHoleExposeDie` | partial (2026-10-30) | 10 | Wild | 15 (173) | 6 | уничтоженное логово крипа или здание оставляет яму, через время её отстраивают заново (P-29, INFERENCE по полям HoleName/HoleMaxHealth) | S-5603 | СДЕЛАНО В КОДЕ, тест (test_modgap1_hole.cpp), ждёт проверки владельцем: после разрушения главного здания крипа остаётся яма (руины), её можно сломать, из неё падают сундуки; не тронутая через 2 минуты она отстраивает логово заново. Не перенесено: ветка рабочего (Дикие, Имладрис), TransferAttackers (S-5603) |
| 2 | `RebuildHoleBehavior` | partial (2026-10-30) | 5 | Wild | — | — | сама яма: рабочий появляется через WorkerRespawnDelay и отстраивает объект | S-5603 | СДЕЛАНО В КОДЕ, тест (test_modgap1_hole.cpp), ждёт проверки владельцем: после разрушения главного здания крипа остаётся яма (руины), её можно сломать, из неё падают сундуки; не тронутая через 2 минуты она отстраивает логово заново. Не перенесено: ветка рабочего (Дикие, Имладрис), TransferAttackers (S-5603) |
| 2 | `RubbleRiseUpdate` | missing | 0 | — | 6 (3) | — | руины поднимаются из земли после разрушения здания (P-29, INFERENCE) | S-140 | НЕ НАЧАТО |
| 2 | `StructureToppleUpdate` | missing | 0 | — | 4 (5) | — | падение разрушаемого здания/башни на карте (INFERENCE) | S-140 | НЕ НАЧАТО |
| 2 | `ToppleUpdate` | missing | 0 | — | 3 (14) | — | падение дерева/объекта при смерти (INFERENCE) | S-140 | НЕ НАЧАТО |
| 3 | `ClickReactionBehavior` | missing | 16 | Imladris, Mordor, Wild | 3 (1) | 3 | реакция-анимация на щелчок по юниту (INFERENCE) | S-1995 | ЗАНЯТО — агент B, 2026-10-30 |
| 3 | `AutoPickUpUpdate` | missing | 10 | Mordor, Wild | — | 3 | горный тролль сам хватает и ест подходящего юнита (INFERENCE) | S-1995 | ЗАНЯТО — агент B, 2026-10-30 |
| 3 | `GrabPassengerSpecialPower` | missing | 9 | Mordor, Wild | — | 3 | тролль хватает юнита и бросает его (пещерные тролли, Рогаш); сейчас кнопка есть, эффекта нет (P-16) | S-1995 | ЗАНЯТО — агент B, 2026-10-30 |
| 3 | `BoredUpdate` | partial (агент B) | 4 | Isengard, Wild | — | — | скучающий тролль сам применяет способность (INFERENCE) | S-1995, S-6000, S-142 | ЗАНЯТО — агент B, 2026-10-30; тест ветви без цели в RotWK проходит, фильтрованная цель и BFME2 останавливаются с S-6000. SPECIAL_MTTROLL_BORED (0x40): RW 0x82DAD4 после общих проверок ведёт к успеху RW 0x82DB63. Targetless RW 0x82D5DA проверяет стоимость отряда, BFME2 0x81CCD5 — нет; нужны варианты проверки, общие проверки цели и тест |
| 3 | `FlingPassengerSpecialAbilityUpdate` | partial (агент B) | 4 | Angmar, Isengard, Mordor, Wild | 1 (1) | — | бросок захваченного (Шарку, Морчант, горные тролли) | S-1995, S-6001 | ЗАНЯТО — агент B, 2026-10-30; тест освобождения первого пассажира, оружия и стопа проходит, без привязки падал. RW 0x89875A / BFME2 0x894FCC; строки данных RW 0xC0556C / BFME2 0xBEF20C. Проверка владельца в игре и остальные контейнеры остаются |
| 4 | `SlaveWatcherBehavior` | ported (2026-10-30) | 75 | все 11 | 13 (2) | 6 | при смерти подчинённого (требушет, баллиста) снимается улучшение у хозяина (RemoveUpgrade), и его можно купить снова | S-1995 | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает |
| 4 | `OCLUpdate` | ported (2026-10-30) | 3 | Arnor, Dwarves, Imladris, Men, Mordor, Rohan | — | — | настенный требушет возникает из здания-заготовки через OCL (по полям Amount/MinDelay; INFERENCE) | S-140 | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает (CreateAtEdge: S-5501) |
| 5 | `DeflectSpecialPower` | ported (2026-10-30) | 12 | Arnor, Evilmen, Rohan | 1 (1) | 24 | копейщики отбивают снаряды. **В Ennorath не видно**: способность ничто не запускает (кнопка и слот в commandset.ini закомментированы), а отбиваемые снаряды NO_COLLIDE и не попадают в пары столкновений (RW 0x68E31F) | S-5601 | СДЕЛАНО В КОДЕ, тест; для игрока ничего не меняется, проверять нечего |
| 6 | `EvacuateDamage` | partial (2026-10-30) | 13 | Arnor, Dwarves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan | 13 (22) | — | боевая башня при попадании указанного оружия (MordorCatapultHumanHeads) выгоняет гарнизон | S-1995, S-5602 | СДЕЛАНО В КОДЕ, тест (test_modgap1_evacuate.cpp), ждёт проверки владельцем: башню с гарнизоном бьёт катапульта Мордора с головами, гарнизон выходит. Не перенесено: паника при большом уроне за 6 кадров (команда ИИ 0x2E не определена, S-5602) |
| 6 | `ReflectDamage` | ported (2026-10-30) | 5 | Angmar, Arnor, Imladris, Men, Mordor, Rohan | 2 (1) | — | часть урона возвращается нападающему (Король-чародей Ангмара: 30% от входящего урона, по типам CRUSH/SLASH/HERO/URUK/SPECIALIST; Назгул 2: 50%) | — | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает (test_modgap1_reflect.cpp) |
| 7 | `SplitHordeSpecialPower` | missing | 10 | Isengard, Mordor | — | — | кнопка разделения составного отряда (Изенгард, Мордор) | S-140 | НЕ НАЧАТО |
| 7 | `CombineHordeSpecialPower` | missing | 0 | — | — | — | обратное слияние отрядов | S-140 | НЕ НАЧАТО |
| 8 | `GiantBirdSlowDeathBehavior` | missing | 19 | все 11 | 2 (2) | — | падение орлов, назгулов на птицах, дракона: сейчас обычная смерть | S-140 | НЕ НАЧАТО |
| 8 | `ClearanceTestingSlowDeathBehavior` | missing | 4 | Evilmen, Mordor | 1 (5) | — | смерть, проверяющая свободное место (INFERENCE) | S-140 | НЕ НАЧАТО |
| 8 | `ShipSlowDeathBehavior` | missing | 0 | — | 4 (2) | — | тонущие корабли | S-140 | НЕ НАЧАТО |
| 9 | `SpecialEnemySenseUpdate` | missing | 13 | Dwarves, Elves, Evilmen, Imladris, Men, Mordor, Wild | 4 (3) | — | назгул чувствует кольцо в радиусе 400 (INFERENCE: сигнал, не атака) | S-140 | НЕ НАЧАТО |
| 9 | `EntEnragedUpdate` | missing | 10 | Elves, Rohan | 2 (1) | — | энты впадают в ярость рядом с лесопилками врага или гибелью своих | S-983 (частично) | НЕ НАЧАТО |
| 9 | `ReplaceObjectUpdate` | missing | 6 | Imladris, Rohan | — | — | способности героя-создания, заменяющие объект (Word of Poultry) | S-1226 | НЕ НАЧАТО |
| 9 | `OathbreakersFadeAwayBehavior` | missing | 5 | Men, Rohan | — | — | призраки растворяются в конце срока (P-13) | S-5310 | НЕ НАЧАТО |
| 9 | `GiveUpgradeUpdate` | missing | 4 | Elves, Evilmen, Imladris | 3 (4) | 1 | носильщик Харада выдаёт улучшение ближайшему | S-140 | НЕ НАЧАТО |
| 9 | `StoreObjectsSpecialPower` | missing | 4 | Dwarves, Evilmen, Imladris | — | — | запоминает отряд для телепорта | S-813 (смежный) | НЕ НАЧАТО |
| 9 | `TeleportToCasterSpecialPower` | missing | 4 | Dwarves, Evilmen, Imladris | — | — | телепорт отряда к герою-создателю | S-813 (смежный) | НЕ НАЧАТО |
| 9 | `AODCrushCollide` | missing | 3 | Men, Rohan | — | — | призраки давят/замораживают при касании | S-5310 | НЕ НАЧАТО |
| 9 | `LocomotorSetUpgrade` | missing | 3 | Arnor, Evilmen | — | — | улучшение меняет набор передвижения (лазание по стенам героя-создания) | S-140 | НЕ НАЧАТО |
| 9 | `FreeLifeBody` | missing | 2 | Arnor, Imladris | — | — | Глорфиндел воскресает на месте после смерти на 10 уровне (FreeLifeTime) | S-140 | НЕ НАЧАТО |
| 9 | `OneRingPenaltyUpdate` | missing | 1 | Elves | 1 (1) | — | Око Саурона вызывается, если Фродо носит кольцо слишком долго | S-140 | НЕ НАЧАТО |
| 10 | `ModelConditionAudioLoopClientBehavior` | missing | 500 | все 11 | 71 (12) | — | голосовые петли насмешек и ликования отрядов в бою, звук обрушения здания | S-140 | НЕ НАЧАТО |
| 10 | `EvaAnnounceClientCreate` | missing | 21 | все 11 | 3 (145) | — | голос Эвы при появлении объекта (кольцо найдено, герой вышел) | S-140 | НЕ НАЧАТО |
| 10 | `AudioLoopUpgrade` | missing | 15 | все 11 | 5 (2) | 4 | звук стройки/улучшения здания, пока оно строится | S-1995 | НЕ НАЧАТО |
| 10 | `TooltipUpgrade` | missing | 9 | все 11 | 6 (2) | — | после улучшения кнопка меняет название и описание | S-140 | НЕ НАЧАТО |
| 10 | `AnnounceBirthAndDeathBehavior` | missing | 6 | Dwarves, Men, Mordor | 2 (1) | — | голос при появлении/смерти Саурона и больших героев | S-1995 | НЕ НАЧАТО |
| 11 | `GeometryUpgrade` | missing | 71 | все 11 | 20 (2) | 27 | улучшение здания переключает его геометрию: след, путь юнитов, выбор; в переписи 27 живых | S-1995 | НЕ НАЧАТО |
| 11 | `AimWeaponBehavior` | missing | 32 | Angmar, Dwarves, Elves, Evilmen, Imladris, Mordor, Rohan, Wild | 7 (2) | 49 | прицеливание оружия по порогам AimLow/High и дистанциям (Haradrim, демон); эффект по декомпиляции не установлен; в переписи 49 живых | S-1995 | НЕ НАЧАТО |
| 11 | `HordeTransportContainDamage` | missing | 3 | Evilmen | 1 (1) | — | мумак: урон делится на бойцов-пассажиров | S-1103 | НЕ НАЧАТО |
| 11 | `RunOffMapBehavior` | missing | 2 | Evilmen | 1 (1) | — | мумак убегает за край карты по точке | S-1995 | НЕ НАЧАТО |
| 12 | `W3DStreakDraw` | parsed-only | 52 | все 11 | — | — | следы за стрелами и снарядами; класс помечен как не рисуемый (S-113), 52 снаряда | S-113 | НЕ НАЧАТО |
| 12 | `W3DBuffDraw` | missing | 50 | все 11 | — | — | свечение баффов (лечение, оглушение, мороз): по S-113 уже рисуется как баф BuffNugget; статус «отсутствует» может не значить «не работает»; проверить | S-113, S-4642 | НЕ НАЧАТО |
| 12 | `W3DTornadoDraw` | missing | 11 | Arnor, Elves, Evilmen, Imladris | — | — | смерч Радагаста, ветры Алатара, вихрь Элронда | S-113 | НЕ НАЧАТО |
| 12 | `OilSpillUpdate` | missing | 2 | Dwarves, Mordor | — | — | масляный след и поджиг (Mordor, Dwarves) | S-140 | НЕ НАЧАТО |
| 12 | `DestroyEnvironmentUpdate` | missing | 1 | Dwarves, Imladris, Men | — | — | та же сила: разрушение деревьев/камня в области | S-140 | НЕ НАЧАТО |
| 12 | `PartTheHeavensUpdate` | missing | 1 | Dwarves, Imladris, Men | — | — | раскрытие небес: расходящийся круг света (сила) | S-140 | НЕ НАЧАТО |
| 12 | `ProjectileStreamUpdate` | missing | 1 | Wild | — | — | логика потока: дальность и цепочка снарядов | S-113 | НЕ НАЧАТО |
| 12 | `RadarMarkerClientUpdate` | missing | 1 | все 11 | — | — | метка на миникарте | S-140 | НЕ НАЧАТО |
| 12 | `StrafeAreaUpdate` | missing | 1 | Wild | — | — | дракон обстреливает область (сила Dragon Strike) | S-140 | НЕ НАЧАТО |
| 12 | `W3DLightDraw` | missing | 1 | Mordor | 4 (7) | — | световые пятна факелов, костров, вспышек (на картах) | S-113 | НЕ НАЧАТО |
| 12 | `W3DProjectileStreamDraw` | missing | 1 | Wild | — | — | поток огня/струя (огненный дракон) | S-113 | НЕ НАЧАТО |
| 12 | `WallUpgradeUpdate` | missing | 1 | все 11 | 5 (2) | — | апгрейд стены меняет её вид | S-140 | НЕ НАЧАТО |
| 12 | `CritterEmitterUpdate` | missing | 0 | — | 1 (22) | 13 | птицы/мелкая живность на картах; в переписи 13 живых | S-140 | НЕ НАЧАТО |
| 12 | `SwayClientUpdate` | missing | 0 | — | 2 (2) | — | покачивание объекта (деревья, знамёна) | S-140 | НЕ НАЧАТО |
| 13 | `PickupStuffUpdate` | missing | 347 | все 11 | 21 (3) | 17 | только компьютерный игрок (SkirmishAIOnly=Yes): отряды ИИ подбирают сундуки; 692 объекта в данных, 17 живых в переписи | S-1422 | НЕ НАЧАТО |
| 13 | `CommandButtonHuntUpdate` | missing | 84 | Angmar, Arnor, Dwarves, Elves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan | 8 (3) | 4 | охота по кнопкам (скрипты/ИИ); блок в данных пустой; в переписи 4 живых | S-140 | НЕ НАЧАТО |
| 13 | `SpellRechargeModifierUpgrade` | missing | 0 | — | 1 (183) | 2 | улучшение ускоряет перезарядку сил; в переписи 2 живых | S-140 | НЕ НАЧАТО |
| 13 | `ThreatFinderUpdate` | missing | 0 | — | 30 (47) | — | тактический маркер ИИ | S-140 | НЕ НАЧАТО |
| 14 | `TerrainResourceClientBehavior` | missing | 26 | все 11 | 4 (113) | — | круги зон ферм РИСУЮТСЯ (S-5030); статус «отсутствует» не значит «не работает», проверить, что именно не покрыто | S-5030 | НЕ НАЧАТО |
| 14 | `AnimalAIUpdate` | base-ai | 11 | Elves, Imladris, Rohan | 18 (145) | — | животные на картах работают как базовый ИИ; различие не установлено | S-140 | НЕ НАЧАТО |
| 14 | `HordeWorkerAIUpdate` | base-ai | 2 | Rohan | — | — | строители-отряды Рохана работают как базовый ИИ; различие не установлено | S-140 | НЕ НАЧАТО |
| 14 | `SpawnUnitBehavior` | parsed-only | 2 | Dwarves, Wild | — | — | здание рождает юнитов (S-207) | S-207 | НЕ НАЧАТО |
| 13 | условие скрипта `PLAYER_HAS_NUM_UNITS_WITH_UPGRADE` | не перенесено | — | ИИ всех фракций | — | 18600 | ИИ не видит свои улучшения (INFERENCE по имени) | S-1180 | НЕ НАЧАТО |
| 13 | условие скрипта `PLAYER_BUILT_UPGRADE` | не перенесено | — | ИИ всех фракций | — | 600 | то же | S-1180 | НЕ НАЧАТО |

### Только читаются (рисуются другим кодом)

Классы `W3D*Draw` со статусом «только читаются»: рисуются устройством, а не модулем. Кроме `W3DStreakDraw` (в таблице), логики в них нет: `W3DDefaultDraw`, `W3DFloorDraw`, `W3DHordeModelDraw`, `W3DPropDraw`, `W3DQuadrupedDraw`, `W3DSailModelDraw`, `W3DScriptedModelDraw`, `W3DSupplyDraw`, `W3DTankDraw`, `W3DTreeDraw`, `W3DTruckDraw`. Не пробел логики; регрессия рисования — в тестах `mapobj`.

### Отсутствуют, но ни фракции, ни карты на них не выходят

Не работать нечему до тех пор, пока мод не получит эти объекты в игре (кампания, отладочные и неиспользуемые определения): `BaseUpgrade` (1), `BridgeBehavior` (1), `CivilianSpawnCollide` (2), `CivilianSpawnUpdate` (1), `DamageFieldUpdate` (1), `DelayedLuaEventUpdate` (1), `DetachableRiderBody` (2), `DetachableRiderUpdate` (2), `DynamicShroudClearingRangeUpdate` (1), `FadeAndDieOrnamentUpdate` (1), `GateProxyBehavior` (3), `LaserUpdate` (3), `ManTheWallsSpecialPower` (2), `PoisonedBehavior` (2), `ProductionQueueHordeContain` (2), `VeterancyCrateCollide` (2), `W3DDebrisDraw` (1), `W3DLaserDraw` (3).

## Не вошедшее в список по классам

Эти два вида проблем список по INI не находит. Идёт параллельно с переносом, результаты записываются здесь.

### А. Написано, но никем не вызывается

Пример: `PillageModule::onDamageDealt` (P-19). Проход по перенесённым модулям: для каждой точки входа (onDamage, onDie, onCollide, onCreate, onUpgrade, update-вызов) найти вызов из игровой логики. Не начато; результаты ниже по мере прохода.

| Модуль / точка входа | Вызывается из | Проверка | Состояние |
|---|---|---|---|
| `PillageModule::onDamageDealt` | `ActiveBody::attemptDamage` (исправлено в P-19) | RW 0x8C4378 -> 0x68B8A3 | исправлено, ждёт проверки владельца |
| `BodyModuleInterface::isIndestructible` | не вызывается снаружи; сам `ActiveBody::attemptDamage` читает флаг (RW 0x8C3FDA) | чтение кода | не дефект |
| `ExitInterface::exitObjectInAHurry` / `useSpawnRallyPoint` | не вызываются; в портe все реализации пустые или `false` | `QueueProductionExitUpdate.h`, `ProductionExitModules.h` | проверить по RW, кто их вызывает (слоты 5 и 6 интерфейса) |
| `ProjectileUpdateInterface::projectileIsArmed` / `projectileGetLauncherID` | не вызываются | `ProjectileModules.h` (RW 0xC585E0) | проверить по RW (мины, ловушки) |
| `HordeContainInterface::lastMemberEnteredFrame` | пишется в сохранение и хеш, не читается | `HordeContainRuntime.cpp` | проверить по RW |
| `AIUpdateInterface::machineName` | диагностика | — | не дефект |

Метод прохода (скрипты в `workspace/ennorath-audit/tools`): (1) не-virtual `on*`/`handle*`/`apply*` методы модулей без вызовов вне своего .cpp (18 кандидатов, у всех есть вызов внутри класса или это определение); (2) 19 аксессоров интерфейсов `BehaviorModule` (`getDie`, `getDamage`, `getCollide` ...): у всех есть вызовы, у `getUpdate` нет вызовов, потому что обновления ведёт планировщик; (3) методы 23 интерфейсов, реализованные, но не вызываемые (список выше). Это кандидаты, а не доказанные дефекты.

### Б. Отличается поведение или отрисовка

Известные: неизвестный `RadiusCursorType` считается отсутствующим (P-23, RW 0x75CAA9, исправлено); частицы `Type = DRAWABLE` не рисуются (S-195, P-18); звук теряется (P-23, P-24); тролли замирают при ходьбе (P-16).

Агент B, 2026-10-30: в данных Ennorath `GoblinCaveTroll` использует `TrollLocomotor`, где `TurnThreshold = 3`. Перенос условий поворота уже есть в `LocomotorMove.cpp` (RW 0x5E5603–0x5E5774); это не подтверждает причину P-16. Следующий шаг — покадровый тест движения и состояния анимации на профиле Ennorath.

# Модули Ennorath, которых нет в порте (MODULE_GAPS)

**Слияние с upstream 2026-10-30.** Upstream сам перенёс `SalvageCrateCollide`, `RebuildHoleExposeDie`, `RebuildHoleBehavior`, `SlaveWatcherBehavior`, `BoredUpdate` и ещё восемь классов этого списка; их строки ниже отмечены, числа покрытия в этом файле остались от снимка. Для пересекающихся классов за основу взят код upstream (полнее по RW: ветка рабочего, `TransferAttackers`, вызов при цели), тесты `test_modgap*` перенесены на него, отличия Ennorath сохранены: бесцелевая форма `BoredUpdate` и фильтр союзников RW 0x660B3E. Контакты сундуков теперь идут через `AIWorld::processCrateContacts`, стоп S-5603 снят (закрыт S-1427).

Снимок 2026-10-30. Только идентификаторы и счётчики, данных игры нет. Инструменты лежат в игнорируемой `workspace/ennorath-audit/tools`
(`reach.py`, `mapscan.py`, `sample.py`, `mkdoc.py` и прежние `dumpini.py`, `modcensus2.py`, `rw.py`). Эффекты, прочитанные только по именам полей, помечены INFERENCE.

## Что брать дальше

Список создаёт `python tools/mod/ennorath_queue.py priority --update`.

<!-- ennorath_queue.py priority: begin -->
Открытых классов: 25, достижимых объектов мода: 167. Сверху вниз, занятые тоже показаны:

| № | Класс | Объектов | Статус | Что делает | Состояние |
|---|---|---|---|---|---|
| 1 | `W3DStreakDraw` | 52 | parsed-only | следы за стрелами и снарядами; рисуется клиентом (RENDER-1 / PROJ-2: точки следа RW 0x4CF884, V, режимы смешивания, цвет, погодная текстура), статус «parsed-only» верен для модульной таблицы, но не значит «не рисуется» | ЗАНЯТО — Copilot, 2026-11-01: проверено, что покрыто и что нет. Покрыто тестами `render streak:*`, `proj2 arrows:*`. Не хватало геометрии ленты (была лента лицом к камере, вывод): перенесён `SimpleStreakRendererClass::RenderStreak` (BFME2 Rva 0x126433, WB 0xA1E840; RW 0x525E1F — INFERENCE: тот же размер 11 984 Б, те же шесть аргументов, константы 0.9 и 0.0001; построчно не сверялось). Ждёт проверки владельцем в игре: стрелы лучников — лента ровная, не перекручена при повороте, V на оперении 0, на наконечнике 1 |
| 2 | `W3DBuffDraw` | 50 | parsed-only (2026-11-01) | свечение баффов (лечение, оглушение, мороз): рисуется на юнитах через `ClientBuffs` (S-4642); сам `W3DBuffDraw::doDrawModule` в обеих редакциях пустой (RW 0xBE3010 / BFME2 0xBCD178 слот 11 RW 0x9F3A3C = `ret 4`); перенесён типизированный `W3DBuffDrawModuleData` (`ModelName` +0x08, `PreDraw` +0x0C, RW 0x4CE61A / 0xBE318C, BFME2 0x4CEE4C / 0xBCD2F4) и передача `PreDraw` в `LiveFX::turnOnBuff` / `ClientBuffs::turnOn` (RW 0x707FFF / 0x4D1981, BFME2 0x70682A / 0x4D21B5) | СДЕЛАНО В КОДЕ для обеих редакций, тесты в test_mapobj_draw_data.cpp и test_spell4_buffs.cpp, ждёт проверки владельцем |
| 3 | `EvaAnnounceClientCreate` | 21 | partial | голос Эвы при появлении объекта (кольцо найдено, герой вышел) | СДЕЛАНО В КОДЕ — Codex, 2026-11-01; RW 0x8CDED6 / 0x8CDF07, BFME2 0x4C9A51 / 0x4C9A82; S-7091; тесты и сценарий Ennorath прошли, ждёт проверки владельцем |
| 4 | `EvacuateDamage` | 13 | partial (2026-10-30) | боевая башня при попадании указанного оружия (MordorCatapultHumanHeads) выгоняет гарнизон | СДЕЛАНО В КОДЕ, тест (test_modgap1_evacuate.cpp), ждёт проверки владельцем: башню с гарнизоном бьёт катапульта Мордора с головами, гарнизон выходит. Не перенесено: паника при большом уроне за 6 кадров (команда ИИ 0x2E не определена, S-5602) |
| 5 | `ReplaceObjectUpdate` | 6 | partial (2026-10-31) | способности героя-создания, заменяющие объект (Word of Poultry) | СДЕЛАНО В КОДЕ агентом B без Scatter, тест test_modgap2_replace.cpp, ждёт проверки владельцем |
| 6 | `OathbreakersFadeAwayBehavior` | 5 | partial (2026-10-31) | призраки растворяются после исполнения клятвы (P-13) | СДЕЛАНО В КОДЕ для обеих редакций: RW 0x8BCC50 / 0x8BCC82, BFME2 0x8B8B0F / 0x8B8B41. Отрицательная проверка и тесты исчезновения, орды, сохранения и клиентского события прошли; выход из орды частичен (S-6006). BFME2 1.06 game.dat MD5 подтверждён, индекс 0x26 в обеих таблицах — HORDE_MEMBER, а не FAERIE_FIRE как подписано в декомпиляции. Подготовлен, но ещё не собран тест отсутствия фиктивного слота для MACHINE. Источник установки OATH_FULLFILLED в игровом сценарии ещё не установлен. Ждёт игровой проверки |
| 7 | `ClearanceTestingSlowDeathBehavior` | 4 | partial (2026-10-31) | выбор варианта смерти с проверкой свободного места и высоты местности | ЗАНЯТО — агент, 2026-10-31. Обе редакции: RW 0x887191 / BFME2 0x883F19, выбор RW 0x861868; сэмплер RW 0x8E0354 / 0x8E047D (BFME2 0x8DC096 / 0x8DC1BF). Предыдущая версия: `modgap2 clearance*` 5 / 119 прошли, без привязки 4 теста класса падают. После чтения RW 0xAD2CE0 и 0x8E047D устранён ошибочный отказ при ненулевой оси: она не читается при проверке области. Новый тест оси и тест повёрнутой геометрии прошли в обеих редакциях; S-6005 закрыт, повторная сборка и тесты прошли; игровая проверка отложена по просьбе владельца |
| 8 | `FlingPassengerSpecialAbilityUpdate` | 4 | partial (2026-10-31) | бросок захваченного (Шарку, Морчант, горные тролли) | ЗАНЯТО — агент, 2026-10-31. Тест OpenContain проходит; HordeContain требует отдельного переноса: хотя слоты извлечения совпадают (RW 0x866367 / 0x8665BE, BFME2 0x863291 / 0x863509), обычный `removeMember` в порте не воспроизводит все действия OpenContain при выходе (маска статусов и `Object::onRemovedFrom` RW 0x69024C). S-6001 остаётся |
| 9 | `RunOffMapBehavior` | 2 | missing | мумак убегает за край карты по точке | БЛОКИРОВАНО — Claude, 2026-10-31: в моде бездействует: все 10 блоков RequiresSpecificTrigger = Yes, а запускает его только DetachableRiderUpdate, до которого фракции не доходят; нужны и команды ИИ 0x38 / 0x41 (RW 0x865332 / 0x86539E) |
| 10 | `SpawnUnitBehavior` | 2 | parsed-only | здание рождает юнитов (S-207) | НЕ НАЧАТО |
| 11 | `DestroyEnvironmentUpdate` | 1 | partial (2026-11-01) | та же сила: разрушение деревьев/камня в области | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8AFEB5 / BFME2 0x4AC84B; поля StartTime и DestructionTime, задержка, xfer), тесты test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре эффекта нет: цель ищется через слот 31 (RW 0x8AFC5F), которого не реализует ни один модуль, поэтому объект не находится и модуль засыпает (S-7092). Проверять в игре нечего, пока слот не перенесён |
| 12 | `PartTheHeavensUpdate` | 1 | partial (2026-11-01) | раскрытие небес: расходящийся круг света (сила) | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8B0214 / BFME2 0x4ACBB3; кривые Radius, Angle и Opacity, xfer, `FunctionCurve`), тесты test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре круга света нет: проекция декали (RW 0x8B0075 / BFME2 0x4AC9FA) не перенесена, значения кривых только считаются (S-7093) |
| 13 | `ProjectileStreamUpdate` | 1 | missing | логика потока: дальность и цепочка снарядов | НЕ НАЧАТО |
| 14 | `RadarMarkerClientUpdate` | 1 | partial (2026-11-01) | метка на миникарте | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8CE116 / BFME2 0x4C9CF7; поле MarkerType и регистрация), тест test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре метка на миникарте не двигается: запрос маркера у TheRadar (vslot 0x30, RW 0x44F0FA) не перенесён (S-2451) |
| 15 | `StrafeAreaUpdate` | 1 | missing | дракон обстреливает область (сила Dragon Strike) | НЕ НАЧАТО |

Дальше: `W3DLightDraw` (1), `W3DProjectileStreamDraw` (1), `WallUpgradeUpdate` (1).

Отложены, ни один объект мода их не достигает: `CombineHordeSpecialPower`, `RubbleRiseUpdate`, `SpellRechargeModifierUpgrade`, `StructureToppleUpdate`, `SwayClientUpdate`, `ThreatFinderUpdate`, `ToppleUpdate`.
<!-- ennorath_queue.py priority: end -->

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

## Что брать первым: уникальное для Ennorath

Таблицу ниже пишет `python tools/mod/ennorath_unique.py --update`, руками её не правим. Upstream идёт по своей переписи: чем больше модов задевает класс, тем раньше (`docs/MOD-COMPAT.md`). Порядок для нас: сначала `ennorath-only`, затем `shared`, от меньшего числа объектов в RotWK к большему. `upstream-main` не переносим, только проверяем на Ennorath; `upstream-work` сначала смотрим у них. Вид `shared` с малым числом объектов не даёт гарантии: upstream уже перенёс `BoredUpdate` с 7 объектами. Перед тем как взять класс: `python tools/mod/ennorath_unique.py --check <Класс>`.

<!-- ennorath_unique.py: begin -->
Итого: upstream-main 1, upstream-work 0, shared 25, ennorath-only 1.

| Вид | Класс | Гр. | Статус | Достижимых в моде | Объектов в RotWK | Где у upstream |
|---|---|---:|---|---:|---:|---|
| upstream-main | `AnimalAIUpdate` | 14 | base-ai | 11 | 36 |  |
| shared | `DestroyEnvironmentUpdate` | 12 | partial (2026-11-01) | 1 | 1 |  |
| shared | `PartTheHeavensUpdate` | 12 | partial (2026-11-01) | 1 | 1 |  |
| shared | `ProjectileStreamUpdate` | 12 | missing | 1 | 1 |  |
| shared | `StrafeAreaUpdate` | 12 | missing | 1 | 1 |  |
| shared | `W3DProjectileStreamDraw` | 12 | missing | 1 | 1 |  |
| shared | `SpellRechargeModifierUpgrade` | 13 | missing | 0 | 1 |  |
| shared | `HordeWorkerAIUpdate` | 14 | base-ai | 2 | 1 |  |
| shared | `ReplaceObjectUpdate` | 9 | partial (2026-10-31) | 6 | 2 |  |
| shared | `SwayClientUpdate` | 12 | missing | 0 | 2 |  |
| shared | `ToppleUpdate` | 2 | missing | 0 | 3 |  |
| shared | `ClearanceTestingSlowDeathBehavior` | 8 | partial (2026-10-31) | 4 | 4 |  |
| shared | `WallUpgradeUpdate` | 12 | missing | 1 | 4 |  |
| shared | `SpawnUnitBehavior` | 14 | parsed-only | 2 | 4 |  |
| shared | `StructureToppleUpdate` | 2 | missing | 0 | 5 |  |
| shared | `FlingPassengerSpecialAbilityUpdate` | 3 | partial (2026-10-31) | 4 | 5 |  |
| shared | `OathbreakersFadeAwayBehavior` | 9 | partial (2026-10-31) | 5 | 6 |  |
| shared | `RadarMarkerClientUpdate` | 12 | partial (2026-11-01) | 1 | 9 |  |
| shared | `RunOffMapBehavior` | 11 | missing | 2 | 11 |  |
| shared | `RubbleRiseUpdate` | 2 | missing | 0 | 17 |  |
| shared | `EvaAnnounceClientCreate` | 10 | partial | 21 | 21 |  |
| shared | `W3DBuffDraw` | 12 | parsed-only (2026-11-01) | 50 | 22 |  |
| shared | `EvacuateDamage` | 6 | partial (2026-10-30) | 13 | 23 |  |
| shared | `W3DStreakDraw` | 12 | parsed-only | 52 | 33 |  |
| shared | `W3DLightDraw` | 12 | missing | 1 | 37 |  |
| shared | `ThreatFinderUpdate` | 13 | missing | 0 | 40 |  |
| ennorath-only | `CombineHordeSpecialPower` | 7 | missing | 0 | 0 |  |
<!-- ennorath_unique.py: end -->

## Порядок работы (первые 10 групп)

Записи владельца идут первыми. **P-9** (приказы по Alt) — отдельная подсистема, не модуль (`AiOrdersManager`, очереди приказов, сохранение): **ЗАНЯТО — Copilot, 2026-11-01**, очередь для приказов движения подключена и проверена тестами, ждёт проверки владельца в игре; остальные классы приказов, патруль и планирование не перенесены (S-1831). Следующая запись, связанная с этой работой, **P-10**, идёт группой 1.

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
| 1 | `SalvageCrateCollide` | ported (upstream + тесты Ennorath) | 6 | Dwarves, Evilmen, Wild | 8 (4) | — | подбор сундуков (с карты, от дани, от трибьюта): деньги/награда; сейчас сундук ничего не даёт (P-10) | S-260, S-220, S-581 | СДЕЛАНО В КОДЕ, тест 6/6, ждёт проверки владельца в игре |
| 2 | `RebuildHoleExposeDie` | ported (upstream + Ennorath) | 10 | Wild | 15 (173) | 6 | уничтоженное логово крипа или здание оставляет яму, через время её отстраивают заново (P-29, INFERENCE по полям HoleName/HoleMaxHealth) | S-1427 | СДЕЛАНО В КОДЕ (код upstream), тесты test_modgap1_hole.cpp и test_rebuildhole.cpp, ждёт проверки владельцем: после разрушения главного здания крипа остаётся яма (руины), её можно сломать, из неё падают сундуки; не тронутая через 2 минуты она отстраивает логово заново. У Диких и Имладриса отстраивает рабочий |
| 2 | `RebuildHoleBehavior` | ported (upstream + Ennorath) | 5 | Wild | — | — | сама яма: рабочий появляется через WorkerRespawnDelay и отстраивает объект | S-1427 | СДЕЛАНО В КОДЕ (код upstream), тесты test_modgap1_hole.cpp и test_rebuildhole.cpp, ждёт проверки владельцем: после разрушения главного здания крипа остаётся яма (руины), её можно сломать, из неё падают сундуки; не тронутая через 2 минуты она отстраивает логово заново. У Диких и Имладриса отстраивает рабочий |
| 2 | `RubbleRiseUpdate` | missing | 0 | — | 6 (3) | — | руины поднимаются из земли после разрушения здания (P-29, INFERENCE) | S-140 | НЕ НАЧАТО |
| 2 | `StructureToppleUpdate` | missing | 0 | — | 4 (5) | — | падение разрушаемого здания/башни на карте (INFERENCE) | S-140 | НЕ НАЧАТО |
| 2 | `ToppleUpdate` | missing | 0 | — | 3 (14) | — | падение дерева/объекта при смерти (INFERENCE) | S-140 | НЕ НАЧАТО |
| 3 | `ClickReactionBehavior` | ported (upstream, MODCOMPAT-MOD1) | 16 | Imladris, Mordor, Wild | 3 (1) | 3 | реакция-анимация на щелчок по юниту (INFERENCE) | S-1995 | код upstream, ждёт проверки владельцем |
| 3 | `AutoPickUpUpdate` | ported (upstream, MODCOMPAT-MOD1) | 10 | Mordor, Wild | — | 3 | горный тролль сам хватает и ест подходящего юнита (INFERENCE) | S-1995 | код upstream, ждёт проверки владельцем |
| 3 | `GrabPassengerSpecialPower` | ported (upstream, MODCOMPAT-MOD1) | 9 | Mordor, Wild | — | 3 | тролль хватает юнита и бросает его (пещерные тролли, Рогаш); сейчас кнопка есть, эффекта нет (P-16) | S-1995 | код upstream, ждёт проверки владельцем |
| 3 | `BoredUpdate` | ported (upstream + Ennorath) | 4 | Isengard, Wild | — | — | скучающий тролль сам применяет способность (INFERENCE) | S-7161, S-6000, S-142 | код upstream с бесцелевой формой MODGAP-2, тесты test_bored_modules.cpp и test_modgap2_bored.cpp, ждёт проверки владельцем |
| 3 | `FlingPassengerSpecialAbilityUpdate` | partial (2026-10-31) | 4 | Angmar, Isengard, Mordor, Wild | 1 (1) | — | бросок захваченного (Шарку, Морчант, горные тролли) | S-1995, S-6001 | ЗАНЯТО — агент, 2026-10-31. Тест OpenContain проходит; HordeContain требует отдельного переноса: хотя слоты извлечения совпадают (RW 0x866367 / 0x8665BE, BFME2 0x863291 / 0x863509), обычный `removeMember` в порте не воспроизводит все действия OpenContain при выходе (маска статусов и `Object::onRemovedFrom` RW 0x69024C). S-6001 остаётся |
| 4 | `SlaveWatcherBehavior` | ported (2026-10-30) | 75 | все 11 | 13 (2) | 6 | при смерти подчинённого (требушет, баллиста) снимается улучшение у хозяина (RemoveUpgrade), и его можно купить снова | S-1995 | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает |
| 4 | `OCLUpdate` | ported (2026-10-30) | 3 | Arnor, Dwarves, Imladris, Men, Mordor, Rohan | — | — | настенный требушет возникает из здания-заготовки через OCL (по полям Amount/MinDelay; INFERENCE) | S-140 | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает (CreateAtEdge: S-5501) |
| 5 | `DeflectSpecialPower` | ported (2026-10-30) | 12 | Arnor, Evilmen, Rohan | 1 (1) | 24 | копейщики отбивают снаряды. **В Ennorath не видно**: способность ничто не запускает (кнопка и слот в commandset.ini закомментированы), а отбиваемые снаряды NO_COLLIDE и не попадают в пары столкновений (RW 0x68E31F) | S-5601 | СДЕЛАНО В КОДЕ, тест; для игрока ничего не меняется, проверять нечего |
| 6 | `EvacuateDamage` | partial (2026-10-30) | 13 | Arnor, Dwarves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan | 13 (22) | — | боевая башня при попадании указанного оружия (MordorCatapultHumanHeads) выгоняет гарнизон | S-1995, S-5602 | СДЕЛАНО В КОДЕ, тест (test_modgap1_evacuate.cpp), ждёт проверки владельцем: башню с гарнизоном бьёт катапульта Мордора с головами, гарнизон выходит. Не перенесено: паника при большом уроне за 6 кадров (команда ИИ 0x2E не определена, S-5602) |
| 6 | `ReflectDamage` | ported (2026-10-30) | 5 | Angmar, Arnor, Imladris, Men, Mordor, Rohan | 2 (1) | — | часть урона возвращается нападающему (Король-чародей Ангмара: 30% от входящего урона, по типам CRUSH/SLASH/HERO/URUK/SPECIALIST; Назгул 2: 50%) | — | ВЫПОЛНЕНО: владелец проверил в игре 2026-10-30, работает (test_modgap1_reflect.cpp) |
| 7 | `SplitHordeSpecialPower` | ported (2026-10-31) | 10 | Isengard, Mordor | — | — | кнопка разделения составного отряда (Изенгард, Мордор) | S-6004 | СДЕЛАНО В КОДЕ, тест test_modgap2_split.cpp, ждёт проверки владельцем |
| 7 | `CombineHordeSpecialPower` | missing | 0 | — | — | — | обратное слияние отрядов | S-140 | БЛОКИРОВАНО — Claude, 2026-10-30: нужна команда ИИ слияния отрядов и вместимость HordeContain (RW 0x8CCCB4, контейнер слот 0x70, RW 0x66-команда), в порте их нет; MSG_COMBINE_HORDES_WITH_OBJECT без обработчика. Объекты мода до класса не доходят (0 достижимых) |
| 8 | `GiantBirdSlowDeathBehavior` | ported (2026-10-30) | 19 | все 11 | 2 (2) | — | падение орлов, назгулов на птицах, дракона: сейчас обычная смерть | S-5606 | СДЕЛАНО В КОДЕ, тест test_modgap1_unique.cpp, ждёт проверки владельцем |
| 8 | `ClearanceTestingSlowDeathBehavior` | partial (2026-10-31) | 4 | Evilmen, Mordor | 1 (5) | — | выбор варианта смерти с проверкой свободного места и высоты местности | S-653, S-161 | ЗАНЯТО — агент, 2026-10-31. Обе редакции: RW 0x887191 / BFME2 0x883F19, выбор RW 0x861868; сэмплер RW 0x8E0354 / 0x8E047D (BFME2 0x8DC096 / 0x8DC1BF). Предыдущая версия: `modgap2 clearance*` 5 / 119 прошли, без привязки 4 теста класса падают. После чтения RW 0xAD2CE0 и 0x8E047D устранён ошибочный отказ при ненулевой оси: она не читается при проверке области. Новый тест оси и тест повёрнутой геометрии прошли в обеих редакциях; S-6005 закрыт, повторная сборка и тесты прошли; игровая проверка отложена по просьбе владельца |
| 8 | `ShipSlowDeathBehavior` | ported (2026-10-31) | 0 | — | 4 (2) | — | тонущие корабли | S-140 | СДЕЛАНО В КОДЕ агентом B, тест test_modgap2_ship_death.cpp, ждёт проверки владельцем |
| 9 | `SpecialEnemySenseUpdate` | ported (2026-10-31) | 13 | Dwarves, Elves, Evilmen, Imladris, Men, Mordor, Wild | 4 (3) | — | назгул чувствует особого врага в радиусе 400 | — | СДЕЛАНО В КОДЕ для обеих редакций: RW 0x89D7E6, BFME2 0x89A037. Отрицательная проверка и тесты отдельного объекта и орды прошли; ждёт игровой проверки |
| 9 | `EntEnragedUpdate` | ported (2026-10-30) | 10 | Elves, Rohan | 2 (1) | — | энты впадают в ярость рядом с лесопилками врага или гибелью своих | S-5605 | СДЕЛАНО В КОДЕ, тест test_modgap1_unique.cpp, ждёт проверки владельцем |
| 9 | `ReplaceObjectUpdate` | partial (2026-10-31) | 6 | Imladris, Rohan | — | — | способности героя-создания, заменяющие объект (Word of Poultry) | S-6002 | СДЕЛАНО В КОДЕ агентом B без Scatter, тест test_modgap2_replace.cpp, ждёт проверки владельцем |
| 9 | `OathbreakersFadeAwayBehavior` | partial (2026-10-31) | 5 | Men, Rohan | — | — | призраки растворяются после исполнения клятвы (P-13) | S-5310, S-6006 | СДЕЛАНО В КОДЕ для обеих редакций: RW 0x8BCC50 / 0x8BCC82, BFME2 0x8B8B0F / 0x8B8B41. Отрицательная проверка и тесты исчезновения, орды, сохранения и клиентского события прошли; выход из орды частичен (S-6006). BFME2 1.06 game.dat MD5 подтверждён, индекс 0x26 в обеих таблицах — HORDE_MEMBER, а не FAERIE_FIRE как подписано в декомпиляции. Подготовлен, но ещё не собран тест отсутствия фиктивного слота для MACHINE. Источник установки OATH_FULLFILLED в игровом сценарии ещё не установлен. Ждёт игровой проверки |
| 9 | `GiveUpgradeUpdate` | ported (частично) | 4 | Elves, Evilmen, Imladris | 3 (4) | 1 | поиск цели RW 0x89FEBD / BFME2 Rva 0x49C73A, выдача RW 0x8A01B2 / BFME2 Rva 0x49CA2F; явная цель, ближайший союзник и отряд проверены тестом обеих редакций; поиск сверен с BFME2 GetDeliveryTarget, всадники, вложенные цели и эффекты остаются за S-6011 | S-6011 | СДЕЛАНО В КОДЕ (Codex, завершил Claude, 2026-10-31), ждёт проверки владельца: носильщик доносит улучшение ближайшему отряду и растворяется |
| 9 | `StoreObjectsSpecialPower` | ported (2026-10-31) | 4 | Dwarves, Evilmen, Imladris | — | — | запоминает отряд для телепорта | S-523 | СДЕЛАНО В КОДЕ, тест test_modgap2_store_objects.cpp, ждёт проверки владельцем |
| 9 | `TeleportToCasterSpecialPower` | ported (2026-10-31) | 4 | Dwarves, Evilmen, Imladris | — | — | телепорт отряда к герою-создателю | S-6003 | СДЕЛАНО В КОДЕ, тест test_modgap2_store_teleport.cpp, ждёт проверки владельцем |
| 9 | `AODCrushCollide` | ported (2026-10-31, синтетические тесты) | 3 | Men, Rohan | — | — | призраки давят/замораживают при касании | S-5310, S-6007, S-6008 | ЗАНЯТО — агент, 2026-10-31: проверка уникальности пройдена; 5 объявлений в 4 файлах. Collision RW 0x8C0201 / BFME2 0x8BC0B8, update RW 0x8C042F / BFME2 0x8BC2E6, таблица полей RW 0xC710A8. В декомпиляции `KIND_RAW_D7` — model condition `OATH_FULLFILLED` (RW 0x46E918 / BFME2 0x46F039), а поля `otherBody` / `selfBody` в RW — PhysicsBehavior (+0x264), флаг +0x5C — stunned: после вызова орды у цели обязателен физический модуль и он должен быть не stunned. Условие FIRING_A (41) держится до строго позже последнего контакта + 10 кадров. `DeadCollideSize` RW ThingTemplate +0x605: SMALL=0, MEDIUM=1, LARGE=2, по умолчанию SMALL (RW 0x73FFAF); выбирает пару FX/OCL. Уведомление AODHordeContain — интерфейсный слот 0x130, RW 0x87E894 / BFME2 0x87B628 (большая цель, данные высоты); исходный объект берётся из containedBy (RW +0x27C / BFME2 +0x274), а не из producer ID; в BFME2 тот же слот; этот участок пока не портирован. Синтетические тесты союзника, оглушения, объекта без физики, обычного и особого урона, самоурона, FIRING_A, выбора FX и стопа при крупной цели собраны и проходят (GCC, Linux); модуль зарегистрирован в LogicModules и добавлен в Modgap2.cmake. Неперенесённый трекер большой цели обозначен S-6007, неполный контекст OCL — S-6008 |
| 9 | `LocomotorSetUpgrade` | ported (2026-10-31) | 3 | Arnor, Evilmen | — | — | улучшение меняет набор передвижения (лазание по стенам героя-создания) | S-140 | СДЕЛАНО В КОДЕ агентом B, тест test_modgap2_locomotor_upgrade.cpp, ждёт проверки владельцем |
| 9 | `FreeLifeBody` | ported (2026-10-30) | 2 | Arnor, Imladris | — | — | Глорфиндел воскресает на месте после смерти на 10 уровне (FreeLifeTime) | — | СДЕЛАНО В КОДЕ, тест test_modgap1_unique.cpp, ждёт проверки владельцем |
| 9 | `OneRingPenaltyUpdate` | ported (2026-10-30) | 1 | Elves | 1 (1) | — | Око Саурона вызывается, если Фродо носит кольцо слишком долго | S-5604 | СДЕЛАНО В КОДЕ, тест test_modgap1_unique.cpp, ждёт проверки владельцем |
| 10 | `ModelConditionAudioLoopClientBehavior` | ported (2026-11-01) | 500 | все 11 | 71 (12) | — | голоса насмешек, ликования, указания и подъёма флага (EMOTION_TAUNTING, EMOTION_CELEBRATING, EMOTION_POINTING, RAISING_FLAG), звук обрушения здания (RUBBLE) | S-6024 | СДЕЛАНО В КОДЕ: RW 0x8D07CF, 0x8D02D9, 0x8D0425, 0x8D03F8; тест test_modgap2_audio_loop.cpp, сценарий ENNORATH; ждёт проверки владельца на слух |
| 10 | `EvaAnnounceClientCreate` | partial | 21 | все 11 | 3 (145) | — | голос Эвы при появлении объекта (кольцо найдено, герой вышел) | S-7091 | СДЕЛАНО В КОДЕ — Codex, 2026-11-01; RW 0x8CDED6 / 0x8CDF07, BFME2 0x4C9A51 / 0x4C9A82; S-7091; тесты и сценарий Ennorath прошли, ждёт проверки владельцем |
| 10 | `AudioLoopUpgrade` | ported (upstream, MODCOMPAT-MOD1) | 15 | все 11 | 5 (2) | 4 | звук стройки/улучшения здания, пока оно строится | S-1995 | код upstream, не проверено на Ennorath в игре |
| 10 | TooltipUpgrade | ported (2026-11-01) | 9 | все 11 | 6 (2) | — | после улучшения кнопка меняет название и описание | S-6013 | СДЕЛАНО В КОДЕ для обеих редакций: RW 0x8BBBCC / 0x8BBC0A, BFME2 0x8B7A9D / 0x8B7ADB; тест test_modgap2_tooltip_upgrade.cpp, сценарий ENNORATH; канонический модуль хранит строки, S-6013 |
| 10 | AnnounceBirthAndDeathBehavior | ported (2026-11-01) | 6 | Dwarves, Men, Mordor | 2 (1) | — | голос при появлении/смерти Саурона и больших героев | S-6012 | СДЕЛАНО В КОДЕ для обеих редакций: RW 0x888439 / 0x888453, BFME2 0x8851AD / 0x8851C7; тест test_modgap2_announce.cpp, сценарий ENNORATH; подсчёт рождений и смертей, S-6012 |
| 11 | `GeometryUpgrade` | ported (upstream, MODCOMPAT-MOD1) | 71 | все 11 | 20 (2) | 27 | улучшение здания переключает его геометрию: след, путь юнитов, выбор; в переписи 27 живых | S-1995 | код upstream, не проверено на Ennorath в игре |
| 11 | `AimWeaponBehavior` | ported (2026-10-31) | 32 | Angmar, Dwarves, Elves, Evilmen, Imladris, Mordor, Rohan, Wild | 7 (2) | 49 | прицеливание оружия по порогам AimLow/High и дистанциям (Haradrim, демон); эффект по декомпиляции не установлен; в переписи 49 живых | S-5607 | СДЕЛАНО В КОДЕ, тест test_modgap1_aim.cpp, ждёт проверки владельцем |
| 11 | `HordeTransportContainDamage` | ported (2026-10-31) | 3 | Evilmen | 1 (1) | — | мумак: урон делится на бойцов-пассажиров | — | СДЕЛАНО В КОДЕ, тест формулы test_modgap1_aim.cpp; сквозного теста с мумаком нет, ждёт проверки владельцем |
| 11 | `RunOffMapBehavior` | missing | 2 | Evilmen | 1 (1) | — | мумак убегает за край карты по точке | S-1995 | БЛОКИРОВАНО — Claude, 2026-10-31: в моде бездействует: все 10 блоков RequiresSpecificTrigger = Yes, а запускает его только DetachableRiderUpdate, до которого фракции не доходят; нужны и команды ИИ 0x38 / 0x41 (RW 0x865332 / 0x86539E) |
| 12 | `W3DStreakDraw` | parsed-only | 52 | все 11 | — | — | следы за стрелами и снарядами; рисуется клиентом (RENDER-1 / PROJ-2: точки следа RW 0x4CF884, V, режимы смешивания, цвет, погодная текстура), статус «parsed-only» верен для модульной таблицы, но не значит «не рисуется» | S-391, S-1002 | ЗАНЯТО — Copilot, 2026-11-01: проверено, что покрыто и что нет. Покрыто тестами `render streak:*`, `proj2 arrows:*`. Не хватало геометрии ленты (была лента лицом к камере, вывод): перенесён `SimpleStreakRendererClass::RenderStreak` (BFME2 Rva 0x126433, WB 0xA1E840; RW 0x525E1F — INFERENCE: тот же размер 11 984 Б, те же шесть аргументов, константы 0.9 и 0.0001; построчно не сверялось). Ждёт проверки владельцем в игре: стрелы лучников — лента ровная, не перекручена при повороте, V на оперении 0, на наконечнике 1 |
| 12 | `W3DBuffDraw` | parsed-only (2026-11-01) | 50 | все 11 | — | — | свечение баффов (лечение, оглушение, мороз): рисуется на юнитах через `ClientBuffs` (S-4642); сам `W3DBuffDraw::doDrawModule` в обеих редакциях пустой (RW 0xBE3010 / BFME2 0xBCD178 слот 11 RW 0x9F3A3C = `ret 4`); перенесён типизированный `W3DBuffDrawModuleData` (`ModelName` +0x08, `PreDraw` +0x0C, RW 0x4CE61A / 0xBE318C, BFME2 0x4CEE4C / 0xBCD2F4) и передача `PreDraw` в `LiveFX::turnOnBuff` / `ClientBuffs::turnOn` (RW 0x707FFF / 0x4D1981, BFME2 0x70682A / 0x4D21B5) | S-113, S-4642 | СДЕЛАНО В КОДЕ для обеих редакций, тесты в test_mapobj_draw_data.cpp и test_spell4_buffs.cpp, ждёт проверки владельцем |
| 12 | `W3DTornadoDraw` | ported (2026-11-01) | 11 | Arnor, Elves, Evilmen, Imladris | — | — | смерч Радагаста, ветры Алатара, вихрь Элронда: модуль отрисовки декалей смерча и следования за объектом | S-113 | СДЕЛАНО В КОДЕ (обе редакции: RW 0x00464930, BFME2 0x0046513A), тесты test_modgap2_tornado.cpp и test_mapobj_draw_data.cpp, ждёт проверки владельцем |
| 12 | `OilSpillUpdate` | ported (2026-11-01) | 2 | Dwarves, Mordor | — | — | масляный след и поджиг (Mordor, Dwarves): спавн хлебных крошек (OilSpill), поджог и FXEvent | S-6025 | СДЕЛАНО В КОДЕ (обе редакции: RW 0x0064CC80, BFME2 0x0024CFA7), тест test_modgap2_oilspill.cpp, ждёт проверки владельцем |
| 12 | `DestroyEnvironmentUpdate` | partial (2026-11-01) | 1 | Dwarves, Imladris, Men | — | — | та же сила: разрушение деревьев/камня в области | S-7092 | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8AFEB5 / BFME2 0x4AC84B; поля StartTime и DestructionTime, задержка, xfer), тесты test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре эффекта нет: цель ищется через слот 31 (RW 0x8AFC5F), которого не реализует ни один модуль, поэтому объект не находится и модуль засыпает (S-7092). Проверять в игре нечего, пока слот не перенесён |
| 12 | `PartTheHeavensUpdate` | partial (2026-11-01) | 1 | Dwarves, Imladris, Men | — | — | раскрытие небес: расходящийся круг света (сила) | S-7093 | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8B0214 / BFME2 0x4ACBB3; кривые Radius, Angle и Opacity, xfer, `FunctionCurve`), тесты test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре круга света нет: проекция декали (RW 0x8B0075 / BFME2 0x4AC9FA) не перенесена, значения кривых только считаются (S-7093) |
| 12 | `ProjectileStreamUpdate` | missing | 1 | Wild | — | — | логика потока: дальность и цепочка снарядов | S-113 | НЕ НАЧАТО |
| 12 | `RadarMarkerClientUpdate` | partial (2026-11-01) | 1 | все 11 | — | — | метка на миникарте | S-2451 | СДЕЛАНО В КОДЕ частично (обе редакции: RW 0x8CE116 / BFME2 0x4C9CF7; поле MarkerType и регистрация), тест test_modgap2_env_heavens_radar.cpp и сценарий ENNORATH. В игре метка на миникарте не двигается: запрос маркера у TheRadar (vslot 0x30, RW 0x44F0FA) не перенесён (S-2451) |
| 12 | `StrafeAreaUpdate` | missing | 1 | Wild | — | — | дракон обстреливает область (сила Dragon Strike) | S-140 | НЕ НАЧАТО |
| 12 | `W3DLightDraw` | missing | 1 | Mordor | 4 (7) | — | световые пятна факелов, костров, вспышек (на картах) | S-113 | НЕ НАЧАТО |
| 12 | `W3DProjectileStreamDraw` | missing | 1 | Wild | — | — | поток огня/струя (огненный дракон) | S-113 | НЕ НАЧАТО |
| 12 | `WallUpgradeUpdate` | missing | 1 | все 11 | 5 (2) | — | апгрейд стены меняет её вид | S-140 | ПРИОСТАНОВЛЕНО — Claude, 2026-10-31: прочитано RW 0x8AEBD3 (update), 0x8AEB10 (поиск стройки: STRUCTURE + WALK_ON_TOP_OF_WALL, радиус 200), 0x8AEAE6 (RUBBLE: 40 кадров), 0x8AEAA9 (активно: UPGRADE_GARRISON / POSTERN_GATE / TREBUCHET). Сначала нужен сброс улучшений объекта RW 0x68E083 (его же ждут замки, S-952) и слот тела 0x28 (RW 0x8C1BF9), затем RW 0x8AEC79 |
| 12 | `CritterEmitterUpdate` | ported (upstream, MODCOMPAT-MOD1) | 0 | — | 1 (22) | 13 | птицы/мелкая живность на картах; в переписи 13 живых | S-140 | код upstream, не проверено на Ennorath в игре |
| 12 | `SwayClientUpdate` | missing | 0 | — | 2 (2) | — | покачивание объекта (деревья, знамёна) | S-140 | НЕ НАЧАТО |
| 13 | `PickupStuffUpdate` | ported (upstream, MODCOMPAT-MOD1) | 347 | все 11 | 21 (3) | 17 | только компьютерный игрок (SkirmishAIOnly=Yes): отряды ИИ подбирают сундуки; 692 объекта в данных, 17 живых в переписи | S-1422 | код upstream, не проверено на Ennorath в игре |
| 13 | `CommandButtonHuntUpdate` | ported (upstream, MODCOMPAT-MOD1) | 84 | Angmar, Arnor, Dwarves, Elves, Evilmen, Imladris, Isengard, Men, Mordor, Rohan | 8 (3) | 4 | охота по кнопкам (скрипты/ИИ); блок в данных пустой; в переписи 4 живых | S-140 | код upstream, не проверено на Ennorath в игре |
| 13 | `SpellRechargeModifierUpgrade` | missing | 0 | — | 1 (183) | 2 | улучшение ускоряет перезарядку сил; в переписи 2 живых | S-140 | НЕ НАЧАТО |
| 13 | `ThreatFinderUpdate` | missing | 0 | — | 30 (47) | — | тактический маркер ИИ | S-140 | НЕ НАЧАТО |
| 14 | `TerrainResourceClientBehavior` | ported (2026-11-01) | 26 | все 11 | 4 (113) | — | круги зон ферм и дуги контуров (S-5030); перенесён `TerrainResourceClientBehavior` (RW 0x6549B1 / 0x652B8C / 0x8D0A08 / 0x8D0A58 / 0x8D0A78, BFME2 0x654C56 / 0x652DEB / 0x8CC55A / 0x8CC5AA / 0x8CC5CA), слоты 17/18 `DrawableModule` (`createSelectionFeedback` / `removeSelectionFeedback`, RW 0x672ED1 / 0x672F0D, BFME2 0x672BAB / 0x672BE7) и счётчик пользователей `TerrainResourceFeedback::show` / `hide` (RW 0x8EC271 / 0x8EC449, BFME2 0x8E7B0C / 0x8E7D1D) | S-5030 | СДЕЛАНО В КОДЕ для обеих редакций, тесты в test_ownerfix2.cpp и test_logic_objects.cpp, ждёт проверки владельцем |
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

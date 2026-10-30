# Гондор / Люди Запада (`FactionMen`) — рабочий реестр Ennorath 1.9.8

[Общий план и правило статусов](../ENNORATH_FACTION_PLAN.md). Начало работы: 2026-10-30. Реестры Арнора ведёт другой исполнитель — этот файл их не трогает.
Машинно построенные списки: [граф и таблица книги](FactionMen_Graph.md), [герои](FactionMen_Heroes.md), [здания](FactionMen_Buildings.md),
[юниты и призывы](FactionMen_Units.md), [улучшения](FactionMen_Upgrades.md). Они получены `tools/mod/ennorath_docs.py` по смонтированному тексту данных
и имеют статус **INVENTORIED**: это перечень найденного, не проверка поведения. Граф — завышенная оценка по ссылочным полям (не разбор движка), поэтому
числа в нём ещё не знаменатель готовности.

## Состояние задач

- [ ] **ЗАНЯТО — Claude, 2026-10-30:** A (полный реестр и старт). Сделано: граф достижимости из `PlayerTemplate FactionMen`, таблица 12 покупок → 12 применений,
  инвентарь героев, зданий, юнитов, улучшений, автотест на смонтированном профиле, запуск skirmish за Гондор в игре. Открыто: сверка с оригинальным Ennorath
  (оригинала в этой среде нет), доказательство достижимости каждой ветви (см. «Открыто»), проверка стартовых объектов и ресурсов по данным.
- [ ] **ЗАНЯТО — Claude, 2026-10-30:** B — книга заклинаний. Первый перебор через интерфейс сделан (покупка всех 12 и применение слотов вводом, результаты в таблице ниже); из 12 сил: 5 с подтверждённым эффектом (`Heal`, `RallyingCall`, `LoneTower`, `Adler`, `ArmyoftheDead`), 2 нестабильны (`Rebuild`, `CloudBreak`), 1 пассивная, 1 с зарегистрированным stop (S-856), 3 не оценены сценарием. Отрицательные сценарии, пассивные ауры и остальные состояния палантира не пройдены.
- [ ] **НЕ НАЧАТО:** C, D, E, F, G.

## E0 — профиль этого прохода

- Исходный commit `9896fa40997abb333697bb21f907155b8e3fdc80`; рабочее дерево содержит чужие незакоммиченные изменения (около 46 путей, в основном движок и
  документы Арнора). Они не мои и не откатывались.
- Раскладка: английская. `ROTWK_INSTALL` = `workspace/games-en/Vanilla_(2.01)/install`, `BFME2_INSTALL` = `C:/Program Files (x86)/BFME2`, мод =
  `workspace/mods-en/Ennorath/install` (AIO guid `05a64473-c7f5-4fff-8e00-f30e06db9d96`, версия 1.9.8, `engine_edits` пусты).
- SHA-256 `____enrth_data.big`: `5a1995f0b382b1fe4d05814279ec7bc31cca1d145ffce7edb6d1d6b4fae946e9` (тот же, что в реестре Арнора). Остальные архивы профиля,
  хеш бинарника эталона и порядок монтирования всех `.big` как в рантайме ещё не зафиксированы — E0 не закрыт.
- Порядок слоёв, которым пользуется скрипт: BFME2 1.06 (`ini.big`, `_patch101.big`, `_patch103.big`, `bases.big`) < RotWK 2.01 (`INI.big`, `Bases.big`,
  `_patch201ini.big`) < `.big` мода. Схемы замка (`Bases/<имя>/<имя>.bse`) лежат в BFME2: `Fortress_Men` взят оттуда.

## Что найдено в данных (все пункты — данные, не поведение)

1. **Старт.** `StartingBuilding = MenFortress` — только фундамент (`BASE_FOUNDATION`). Действующая цитадель — `MenFortressCitadel` (набор `MenFortressCommandSet`);
   её выдаёт `CastleBehavior` по `CastleToUnpackForFaction = Men Fortress_Men`, раскладка из `Bases/Fortress_Men/Fortress_Men.bse` (BFME2). Её же клетки —
   `MenFortressExpansionPadSide` / `MenFortressExpansionPadCorner`. Стартовые строители: два `MenPorter` (`StartingUnit0/1`), третий старт-объект
   `AIEmitterPlaceholder` — нейтральный невидимый носитель модулей (CostModifier по уровню ИИ, `WallFixEmmiter`, звуки по апгрейдам).
   `StartMoney = 0`, `InitialUpgrades = Upgrade_MenFaction Upgrade_AllFactionUpgrade`, `IntrinsicSciencesMP = SCIENCE_MEN`.
2. **Книга — названия кнопок покупки не совпадают с науками.** В `GondorSpellStoreCommandSet` слот 2 `Command_PurchaseSpellHealMod` покупает `SCIENCE_RebuildMen`,
   слот 3 `Command_PurchaseSpellBaumeisterGondors` — `SCIENCE_HealMen`, слот 6 `Command_PurchaseSpellGandalftheWhite` — `SCIENCE_EinsamerTurmMen`,
   слот 7 `Command_PurchaseSpellLoneTowerMen_Edain` — `SCIENCE_GandalftheWhite`. Набор применения `GondorSpellBookCommandSet` начинается со слота 3 и идёт в другом
   порядке (покупка слот 4 Lehen → применение слот 9, и т. д.). Связь только через `RequiredSciences` у `SpecialPower`: порт идёт по данным, а не по именам
   (проверено тестом, ниже).
3. **Составные условия наук** (`PrerequisiteSciences`) — это ИЛИ групп И: например `SCIENCE_GOOD OR SCIENCE_MEN SCIENCE_HornMen` = {GOOD} или {MEN и HornMen}.
   Порт читает те же группы (тест).
4. **Варианты книги, замещаемые условием.** Для ИИ (`Upgrade_Easy/Medium/Hard/BrutalAISinglePlayer/MultiPlayer`) модуль `CommandSetUpgrade` `ModuleTag_AICommandSet`
   подменяет набор применения на `GondorSpellBookCommandSetAI` (другой порядок: слот 6 `Command_SpellBookLoneTower_Edain`, слот 8 `Command_SpellBookGandalftheWhite`;
   слот 4 `Command_SpellBookHornGondorsAI` → `SpellBookRallyingCallAI`; слот 5 `...BaumeisterGondorsAI` → `SpellBookRebuildAI`; слот 14 `...SummonRohanGondorAI`).
   Отдельный набор `MMGondorSpellBookCommandSet` (объект `MMGondorSpellBook`) даёт `SpellBookBeistandDolAmroths`, `SpellBookDieGraueSchaarMM`, `SpellBookSummonRohanMM`;
   `Command_SpellGondorRitter` (`SpellBookGondorRitter`, наука `SCIENCE_Adler`) не входит ни в один набор. Достижимость этих ветвей не доказана: вопрос, играет ли
   `MMGondorSpellBook` у игрока в skirmish и что делает кнопка вне набора.
5. **Ссылка без определения:** `OCL_PartTheHeavensMod` (поле `OCL` у `ModuleTag_09` объектов `GondorGandalfVerfhrte` и `GondorGandalfGeprfte`) нигде в смонтированном
   тексте не определён. Движок грузит данные без ошибок (corpus gate), поэтому вопрос, что делает оригинал с неизвестным OCL, остаётся открытым (вероятно, ничего).
6. **Герои.** Начальный ряд `BuildableHeroesMP`: `CreateAHeroDummy GondorGandalf_mod GondorBoromir_mod GondorAragorn_mod GondorFaramir_mod GondorBeregond RohanPippin_mod
   GondorImrahil`; кольцевой `RingHeroDummy`. Формы по данным: Гэндальф — `GondorGandalfWhite_mod` (по `ModuleTag_Weie`) и `GondorGandalfVerfhrte`
   (по `ModuleTag_Verfhrte`), второй набор команд `GondorGandalfModFixCommandSet`; Арагорн — `GondorAragorn_king`, наборы `GondorAragornsEntwicklung1CommandSet`,
   `GondorAragornsAndurilCommandSet`, `GondorAragornFixCrownCommandSet`; Боромир — `GondorBoromir_OneRing`; Фарамир, Берегонд, Имрахиль, Пиппин — по одному набору.
   Ни одна форма, смена набора, уровень или смерть пока не проверены.

## Проверки этого прохода

| Что | Среда | Результат | Статус |
|---|---|---|---|
| `ENNORATH Gondor: the mounted spellbook resolves all twelve purchase sciences to the right cast powers` (`engine/tests/test_ennorath_men.cpp`) | Release MSVC, `openbfme_tests.exe`, профиль выше | 1 тест, 339 проверок, 0 ошибок, 0 пропусков | данные + разрешение ссылок движком: 12 кнопок покупки, 12 наук, стоимость MP, группы условий, 12 кнопок применения, 12 `SpecialPower`, 12 модулей `GondorSpellBook` (тип и тег), фракция, строители, 8 героев, `RingHeroDummy`. Поведение не проверено |
| `ENNORATH Gondor: every reached command set, button and object is in the engine's parse as the mod's data has it` | то же; ожидания выгружены из данных `ennorath_docs.py --expected` в `workspace/` (не в git) | 1299 слотов наборов, 404 кнопки (Object / Upgrade / SpecialPower / Science), 366 объектов: расхождений нет, 6624 проверки | данные: всё достижимое из графа Гондора движок читает как данные мода; поведение не проверено |
| skirmish за Гондор (`--auto --faction=FactionMen --ai=2 --advance=60`) | Godot 4.7.2, `GDExtension` из `godot/bin` от 2026-10-29 23:43 | лобби показывает `Gondor`, матч стартует, на кадре 60 с видна цитадель; `GAME load errors: 0`; логи — `workspace/ennorath-audit/profile-en/FactionMen/start*` | запуск, не сравнение с оригиналом. Название `Gondor` взято из `INI:FactionMen` (не `MISSING`). Содержимое кадра (цитадель по центру, фигурки вдали) на строителей и стартовые объекты не проверено |

Повторяющиеся в логе `call without a function: method 'gotoAndPlay' ... in _level2.SpellBookUI` встречаются и в прогонах обычного RotWK (`workspace/skirmish_van.log`): это
известное поведение оболочки (S-380, retail пропускает такие вызовы), к Гондору не относится.

## B — книга заклинаний через интерфейс (первый перебор)

Сценарий: `--auto --faction=FactionMen --ai=2 --spell3=spellbook` (существующий `godot/scripts/spell3_player.gd`; он выдаёт очки силы тестовым хуком, покупает каждую силу
в окне магазина кликами, затем нажимает каждый слот палантира и судит результат по эффекту). Профиль тот же, логи в `workspace/ennorath-audit/profile-en/FactionMen/spell3*`.
Для сравнения тот же сценарий на ванильном RotWK (`spell3-vanilla-run.log`): 1 ok, 0 failed — то есть сам сценарий и порт для ванильной книги работают.

| Слот применения | Сила (`SpecialPower`) | Результат первого прогона | Статус |
|---:|---|---|---|
| 3 | `SpellBookHeal` | лечение 3 объектов, слот ушёл на перезарядку, звук идёт | эффект подтверждён; число 0.4 и радиус не сверены с оригиналом |
| 4 | `SpellBookRallyingCall` | модификатор получили 2 цели (было 0), перезарядка, звук | эффект подтверждён; значения модификатора не сверены |
| 5 | `SpellBookRebuild` | прошёл в полном прогоне (лечение строения +386), **не сработал** во втором прогоне (`healed 0`, слот остался `_up`) | PARTIAL: нестабильно, причина не найдена |
| 6 | `SpellBookGandalftheWhite` | пассивная: в палантире `_static`, апгрейд `Upgrade_GandalfWhite` даёт триггер | PARTIAL: срабатывание триггера не проверено, смена облика Гэндальфа не проверена |
| 7 | `SpellBookWlderIthilliens` | роща создана, бонусы области не портированы (stop S-856) | BLOCKED на S-856 |
| 8 | `SpellBookSpawnLoneTower_Edain` | создана `GondorSentryTower_Independant` | объект подтверждён; стрельба, срок жизни, цена не проверены |
| 9 | `SpellBookLehenAllies` | не применён: курсор красный на каждом месте с зондами | не проверено сценарием (см. ниже) |
| 10 | `SpellBookDieGraueSchaar` | то же | не проверено сценарием |
| 11 | `SpellBookCloudBreak` | в изоляции: погода 0 → 4, лучей 0 → 225, перезарядка, звук; в полном прогоне: не сработал | PARTIAL: нестабильно; `WeatherKiller` у `CloudBreakSunbeam` не проверен |
| 12 | `SpellBookAdler` | создано 3 объекта (`ImladrisGwaihir_Summoned`, `GondorGwaihir_Summoned` ×2) | объекты подтверждены; полёт, бой, срок не проверены |
| 13 | `SpellBookArmyoftheDead` | созданы `GondorArmyofTheDeadSmallEggs` ×6, `GondorKDTSmallEggs`, затем `RohanOathbreakerHordeSmall` | объекты подтверждены; бой и исчезновение не проверены |
| 14 | `SpellBookSummonRohanGondor` | не применён: сценарий не нашёл принимаемое место за 12 кругов | не проверено сценарием (см. ниже) |

Ни одна строка не **VERIFIED**: нет сравнения с оригинальным Ennorath, числа эффектов не сверены с данными по каждому полю, отрицательные сценарии и отмена не пройдены.

**Почему три силы не проверены.** У `SpellBookLehenAllies`, `SpellBookDieGraueSchaar` и `SpellBookSummonRohanGondor` флаг `NO_FORBIDDEN_OBJECTS` и `ForbiddenObjectFilter =
NO_SUMMON_NEAR_OBJECT_FILTER_SPELLBOOK` (пехота, конница, строения, чудовища и др.) в радиусе `ForbiddenObjectRange = 25`. Сценарий всегда сажает «зонды» (солдат, ферму) прямо в точку применения,
поэтому порт честно отказывает (красный курсор) — это согласуется с данными и с RW 0x82D2FD, но силу сценарий применить не может. Нужен вариант сценария, который целится в пустое место
рядом с зондами (дальше 25), чтобы оценить результат. Это дефект харнесса, не движка.

**Нестабильность (открыто, причина не найдена).** Две силы в разных прогонах то применялись, то нет (слот остаётся `_up`, перезарядки и эффекта нет): `CloudBreak` (без цели) и `Rebuild`. Поправка к первой записи: `Command_SpellBookBaumeisterGondors` имеет `Options = NEED_TARGET_ALLY_OBJECT`, то есть `Rebuild` — сила с целью-объектом (свой объект-строение), а не без цели; прежняя гипотеза «теряется сила без цели после силы с целью» для него неверна. Повторы с выводом `SPELL5` (последовательность `RallyingCall` → `Rebuild` → `LoneTower` → `CloudBreak`, три запуска): `Rebuild` не сработал в обоих завершившихся прогонах (в одном нажатие не включило прицел, в другом включило, но сценарий не выбрал объект-цель; счётчик `m_refused` не выводился), `CloudBreak` сработал оба раза; третий запуск не стартовал (`GAME FAIL: no Skirmish item`, сбой самого сценария запуска). Следующее: выяснить, как сценарий выбирает объект-цель для `NEED_TARGET_ALLY_OBJECT` (кликает ли по ферме-зонду) и почему в одном прогоне прицел не включился; `CloudBreak` один раз не сработал в полном прогоне, причина тоже не найдена. Переменная `SPELL3_ONLY` и вывод `SPELL5` добавлены в `spell3_player.gd` для этого.

## Открыто — следующие шаги

1. Следующий конкретный пункт: **B, нестабильное применение `SpellBookRebuild` (цель-объект) и `SpellBookCloudBreak` (без цели)**: вывести счётчик отказов `SpellCommands` (нужна пересборка расширения), проверить выбор объекта-цели в сценарии; затем вариант сценария для `NO_FORBIDDEN_OBJECTS` (Lehen, DieGraueSchaar, SummonRohanGondor), затем
   детали эффектов (числа, радиусы, срок жизни призывов, перезарядка, отмена, отрицательные сценарии) по каждой из 12 сил.
2. Доказать достижимость или недостижимость вариантов из пункта 4 (набор ИИ, `MMGondorSpellBook`, `SpellBookGondorRitter`).
3. Для A осталось: проверить стартовые объекты и ресурсы по данным в игре (число строителей, `1000` в HUD откуда), назвать, откуда в игре берётся портрет и название, сверить с оригиналом,
   когда он появится; закрыть «ссылка без определения» из пункта 5.
4. Граф ещё не разворачивает: условные команды `CommandSetUpgrade` по улучшениям, `ReplaceSelfUpgrade`, содержимое `AlternateFormation`, `ArmySummary`, `ObjectCreationUpgrade`
   (часть охвачена как рёбра, часть как вторичные ссылки). Размеры в [графе](FactionMen_Graph.md) пересчитать после доработки.
5. Общие механизмы, затронутые прогоном: монтирование слоёв (BFME2 `bases.big` для схемы замка), чтение `CastleBehavior`, `PurchaseScienceCommandSetMP`, `CommandSetUpgrade`.
   Исправлений в движке в этом проходе не потребовалось: расхождений порта с данными в проверенных полях не найдено.

## Известные ограничения инструмента

`tools/mod/ennorath_graph.py` читает INI как текст: блок идёт от заголовка до следующего заголовка (а не по `End`), поля вложенных модулей приписываются модулю по
`ModuleTag_*`, макросы `#define` подставляются построчно, `#DIVIDE`/`#MULTIPLY` не считаются. Ссылки из полей-фильтров (`ObjectFilter`, `Query`, `Requirements*` и т. п.) в граф
не входят: они проверяют цель, а не выдают объект. Эти допущения заданы правилами в начале файла и подлежат проверке на других фракциях; расхождение с разбором движка
должно закрываться тестом, как в тесте Гондора.

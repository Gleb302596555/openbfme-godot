# Арнор — рабочий реестр Ennorath 1.9.8

[Общий план и правило статусов](../ENNORATH_FACTION_PLAN.md). Начало работы: 2026-10-29. Здесь зафиксированы исходные точки аудита, а не полный граф доступности и не подтверждение игровых механик.

## Состояние задач

- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** прочитаны исходные списки кнопок покупки и применения заклинаний, начальное меню строителя и командный набор крепости из локального архива Ennorath; результаты ниже. Это инвентаризация конкретных наборов, не проверка поведения.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** собраны исходные стоимости и имена зависимостей всех 12 наук из `science.ini`; проверка условий в движке и оригинале ещё открыта.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** сопоставлены все 12 команд применения с науками через `RequiredSciences`; результаты в таблице ниже. Игровая проверка остаётся открытой.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** запущен тест загрузки корпуса Ennorath на локальном тестовом бинарнике: `MODS-1 mod corpus` — 1 тест, 14 проверок, 0 ошибок INI, 11 фракций, 267 карт, 86 Apt-фильмов. Прочие 4656 тестов отфильтрованы, а не пройдены. Использованы `workspace/games/Vanilla_(2.01)/install`, `C:/Program Files (x86)/BFME2`, `workspace/mods`. Тест проверяет загрузку, а не игровые функции.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** headless-прогон: Арнор (первый игрок без команд) против сложного ИИ Гондора, Fall Back 8p, seed 7, 300 кадров; процесс завершился успешно, 0 desync, у Арнора на конце 2 юнита и 1 строение. Это проверка запуска и ограниченного течения матча, а не тест управления, способностей или соответствия оригиналу. Отчёт: `workspace/ennorath-audit/local/FactionArnor/peer-report.txt`.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** headless-прогон 3000 кадров: сложный ИИ Арнора против бездействующего игрока Гондора, Fall Back 8p, seed 7. Процесс завершился с 0 desync; отчёт насчитал у Арнора 292 произведённых юнита и 17 построенных зданий. Файл `workspace/ennorath-audit/local/FactionArnor/peer-ai-3000.txt`. Это проверка устойчивости данного сценария, не сравнение с оригиналом и не полный перебор контента.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** `build.bat` собрал движок, тестовый исполняемый файл и вспомогательные программы после добавления Windows PowerShell в PATH; полный `run_tests.bat` не запускался. CMake сообщил об отсутствии инструментов для MP3 fixtures.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** общий тест `LUMBER1 warehouse` на поиск доступного склада и забор ящиков: 1 тест, 25 проверок, без ошибок (`test_supply_warehouse.cpp`). Тест проверяет механизм на тестовых данных, не всю цепочку каменотёсов Ennorath.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** построен предварительный граф из Ennorath и чистых баз через `tools/mod/ennorath_graph.py`, результаты в `workspace/ennorath-audit/local/FactionArnor/graph-candidate/`. Найдены все семь стартовых героев, стартовые крепость/строитель и проверенные точки входа книги; инструмент сообщил 0 отсутствующих include и 460 неразрешённых токенов. Это кандидатный граф, а не доказательство полноты: 415 сообщений относятся к `Weapon`, среди них ключевые слова вроде `PRIMARY`, поэтому сообщения нужно классифицировать, остальные зависимости и приоритеты сверить с runtime.
- [ ] **ЗАНЯТО — Codex, 2026-10-30:** полный граф достижимости A: исходные наборы записаны ниже; далее раскрыть объекты, наследование, замены меню и условные ветви, сверить с реальным смонтированным профилем и оригинальной игрой.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** составлен исходный перечень команд семи заявленных героев и девяти наборов: [герои Арнора](FactionArnor_Heroes.md). Полнота форм и работа команд ещё не проверены.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** описаны начальные строительные объекты и найденные меню зданий / уровней: [здания Арнора](FactionArnor_Buildings.md). Полнота и поведение не проверены.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** составлен список прямого производства из уже найденных меню зданий: [юниты Арнора](FactionArnor_Units.md); призывы и скрытые ветви ещё не исследованы.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** найдены модули и исходные ссылки эффектов всех 12 заклинаний; таблица ниже. Их выполнение не проверено.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** автоматический тест кнопок, наук и наследуемых модулей эффектов на смонтированном профиле: 1 тест, 206 проверок, 0 ошибок; `engine/tests/test_mods1_retail.cpp`, повторно на стандартном `engine/build/openbfme_tests.exe` после полной сборки. Проверено также значение исцеления 0,4 из исходных данных. Выполнение эффекта в игре не проверено.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-29:** составлен начальный список локальных и глобальных улучшений из меню зданий: [улучшения Арнора](FactionArnor_Upgrades.md); полный граф и эффекты ещё предстоит проверить.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** для `SpellBookHealingOfTheOld` добавлен интеграционный тест на смонтированном Ennorath: отказ до покупки `SCIENCE_HobbitHouse`, покупка через сообщение, лечение своей пехоты на 40% максимума, отсутствие лечения врага и объекта вне радиуса, перезарядка и отказ повторного применения. `engine/tests/test_mods1_retail.cpp`: 2 теста Арнора, 239 проверок, 0 ошибок. Это логический тест с искусственными объектами, а не проверка UI, FX или сопоставление кадров с оригиналом.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** расширен логический сценарий книги Арнора: покупка и применение рога (`SpellBookHornOfArnor`) дают модификатор своей пехоте в радиусе и не дают врагу или далёкому бойцу; призыв хоббитов (`SpellBookArnorSummonHobbits`) создаёт `ArnorHobbitBowEgg`. Проверены перезарядка и отказ повторного призыва. Оба теста Арнора: 260 проверок, 0 ошибок на смонтированном Ennorath. Последующий выход пяти отрядов проверен отдельным пунктом ниже; UI, FX и оригинал пока не проверены.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** проверено продолжение призыва хоббитов в логическом тесте с pathfinder: объект `ArnorHobbitBowEgg` после своего задержанного FINAL OCL создал пять принадлежащих игроку `ArnorHobbitBowHorde_Summoned`. Два теста Арнора прошли 262 проверки. Состав, срок жизни и поведение отрядов, UI и оригинал ещё проверяются отдельно.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** через покупку наук и сообщения проверено создание исходных объектов для дома хоббитов, силы Нуменора, Глорфинделя, Кирдана, последней надежды и молота Форноста. Проверены перезарядка и отказ повторного применения. В двух тестах Арнора 312 проверок прошли; полные последствия, доступность в UI и сравнение с оригиналом остаются открыты.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** логический тест леса Кардолана после покупки науки проверил создание `CardolanHill` и первого каменного seed; тест звезды Севера проверил изменение погоды на CLOUDY и создание `ArnorNorthStarObject`. Оба теста Арнора — 326 проверок, 0 ошибок. Изменение области земли и пожарные эффекты остаются заблокированы S-921/S-191; UI, FX и сравнение с оригиналом не проверены.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** проверен путь пассивного `GiftOfTheKingdom`: `SpellStoreModel` после выбора формирует покупку науки и `MSG_DO_SPELLBOOK_SPECIAL_POWER` через `CommandTrigger`; обработка сообщений выдаёт игроку `Upgrade_AMerciOfDaKing`. Проверено на смонтированном профиле; 2 теста Арнора, 339 проверок, 0 ошибок. Эффекты улучшения на все отряды и реальный интерфейс ещё требуют проверки.
- [x] **ВЫПОЛНЕНО — Codex, 2026-10-30:** на `ArnorFighterNew` проверено действие `GiftOfTheKingdom`: собственный боец получает `ArnorBibliothekImladrisUpgrade1`, враг — нет; боец, созданный после покупки, тоже получает модификатор и восстанавливает здоровье. Два теста Арнора прошли 348 проверок. Другие типы носителей, точный интервал/величина восстановления и оригинал остаются непроверенными.
- [ ] **ЗАНЯТО — Codex, 2026-10-30:** B — Все 12 основных заклинаний проверены частичными логическими сценариями, включая пассивный CommandTrigger; полный эффект, все цели, условия, UI и визуал против оригинала остаются открыты. Лес и звезда Севера зависят от неперенесённых подсистем S-921/S-191.
- [ ] **ПРИОСТАНОВЛЕНО:** C — начаты [герои и формы](FactionArnor_Heroes.md); раскрыть условия форм и уровней, проверить каждую способность, смерть и воскрешение.
- [ ] **ПРИОСТАНОВЛЕНО:** D — начаты [юниты и орды](FactionArnor_Units.md); раскрыть альтернативные источники и формы, проверить бой, пополнение, навыки и апгрейды.
- [ ] **ПРИОСТАНОВЛЕНО:** E — начаты [здания и меню](FactionArnor_Buildings.md), включая лагеря; проверить строительство, экономику, очереди, все ветви и разрушение.
- [ ] **ПРИОСТАНОВЛЕНО:** F — начаты [улучшения](FactionArnor_Upgrades.md); проверить каждый уровень, все условия, покупку, эффекты и визуальные изменения.
- [ ] **НЕ НАЧАТО:** G — полная игровая приёмка, сравнение с оригиналом и регрессии после исправлений.

Профиль при чтении: локальный пакет `workspace/mods/Ennorath`, версия 1.9.8. SHA-256 `____enrth_data.big`: `5a1995f0b382b1fe4d05814279ec7bc31cca1d145ffce7edb6d1d6b4fae946e9`. Отправная точка кода: commit `44c7ff9805cc48716fdcd3142a51fd9dd2d55d40`, рабочее дерево содержит подготовленные изменения; тест запущен готовым `engine/build/openbfme_tests.exe`, а не после новой сборки. Чтение одного архива ещё не подтверждает итоговый приоритет каждого файла всего runtime mount. Закрепить полный состав профиля, его хеши и свежую сборку при продолжении E0.

## Книга заклинаний: предварительные источники

Фракция задаёт `ArnorSpellStoreCommandSet` и `ArnorSpellBook`; в `data/ini/object/system/system.ini` последний ссылается на `ArnorSpellBookCommandSet`. Таблицы построены по объявлениям в `data/ini/includes/commandset.inc` и полям кнопок из `data/ini/includes/commandbutton.inc` / `data/ini/commandbutton.ini` внутри `____enrth_data.big`.

Каждая строка — **INVENTORIED**, логика/UI/визуал — **TODO**. Слоты покупки и применения нельзя механически сопоставлять: у последних двух порядок отличается, а некоторые имена наук отличаются от названий кнопок. Связь через `RequiredSciences` указана ниже, её использование необходимо проверить в оригинале; не менять данные только из-за имён.

### Покупка

| Слот | CommandButton | Science | Статус |
|---:|---|---|---|
| 1 | `Command_PurchaseSpellHornOfArnor` | `SCIENCE_HornOfArnor` | INVENTORIED; поведение TODO |
| 2 | `Command_PurchaseSpellHealingOfTheOld` | `SCIENCE_HobbitHouse` | INVENTORIED; поведение TODO |
| 3 | `Command_PurchaseSpellArnorHobbitHouse` | `SCIENCE_HealingOfTheOld` | INVENTORIED; поведение TODO |
| 4 | `Command_PurchaseSpellGiftOfTheKingdom` | `SCIENCE_GiftOfTheKingdom` | INVENTORIED; поведение TODO |
| 5 | `Command_PurchaseSpellWaelderCardolans` | `SCIENCE_ElvenWoodArnor` | INVENTORIED; поведение TODO |
| 6 | `Command_PurchaseSpellArnorSummonHobbits` | `SCIENCE_ArnorHobbitAllies` | INVENTORIED; поведение TODO |
| 7 | `Command_PurchaseSpellNumenorMastery` | `SCIENCE_NumenorMastery` | INVENTORIED; поведение TODO |
| 8 | `Command_PurchaseSpellCirdanHelp` | `SCIENCE_SwordsOfThePast` | INVENTORIED; поведение TODO |
| 9 | `Command_PurchaseSpellStarOfTheNorth` | `SCIENCE_StarOfTheNorth` | INVENTORIED; поведение TODO |
| 10 | `Command_PurchaseSpellGlorfindelHelp` | `SCIENCE_AdlerArnor` | INVENTORIED; поведение TODO |
| 11 | `Command_PurchaseSpellArnorLastHope` | `SCIENCE_LastAllianceArnor` | INVENTORIED; поведение TODO |
| 12 | `Command_PurchaseSpellFornostHammer` | `SCIENCE_RunBitchWatersComin` | INVENTORIED; поведение TODO |

### Науки: стоимость и исходные зависимости

Источник: `data/ini/science.ini` в том же архиве. Названия ниже — идентификаторы; константы стоимости и семантику выражений нужно разрешить на эффективном профиле. Во всех 12 определениях также упомянуты `SCIENCE_GOOD` и `SCIENCE_ARNOR`, а варианты разделены `OR`. Колонка «связанные науки» только перечисляет остальные имена и сама по себе не задаёт логику условий.

| Наука | Стоимость: исходное значение/константа | Остальные науки в условии | Статус |
|---|---|---|---|
| `SCIENCE_HornOfArnor` | `GOOD_RANK_1_COST` | нет дополнительных | INVENTORIED; проверка условий TODO |
| `SCIENCE_HobbitHouse` | `GOOD_RANK_1_COST` | нет дополнительных | INVENTORIED; проверка условий TODO |
| `SCIENCE_HealingOfTheOld` | `GOOD_RANK_1_COST` | нет дополнительных | INVENTORIED; проверка условий TODO |
| `SCIENCE_GiftOfTheKingdom` | `GOOD_RANK_2_COST` | `SCIENCE_HornOfArnor` | INVENTORIED; проверка условий TODO |
| `SCIENCE_ElvenWoodArnor` | `10` | `SCIENCE_HornOfArnor`, `SCIENCE_HobbitHouse` | INVENTORIED; проверка условий TODO |
| `SCIENCE_ArnorHobbitAllies` | `GOOD_RANK_2_COST` | `SCIENCE_HealingOfTheOld`, `SCIENCE_HobbitHouse` | INVENTORIED; проверка условий TODO |
| `SCIENCE_NumenorMastery` | `GOOD_RANK_2_COST` | `SCIENCE_HealingOfTheOld` | INVENTORIED; проверка условий TODO |
| `SCIENCE_SwordsOfThePast` | `GOOD_RANK_3_COST` | `SCIENCE_ElvenWoodArnor`, `SCIENCE_GiftOfTheKingdom` | INVENTORIED; проверка условий TODO |
| `SCIENCE_StarOfTheNorth` | `13` | `SCIENCE_ArnorHobbitAllies`, `SCIENCE_ElvenWoodArnor` | INVENTORIED; проверка условий TODO |
| `SCIENCE_AdlerArnor` | `15` | `SCIENCE_NumenorMastery`, `SCIENCE_ArnorHobbitAllies` | INVENTORIED; проверка условий TODO |
| `SCIENCE_LastAllianceArnor` | `GOOD_RANK_4_COST` | `SCIENCE_SwordsOfThePast`, `SCIENCE_StarOfTheNorth` | INVENTORIED; проверка условий TODO |
| `SCIENCE_RunBitchWatersComin` | `GOOD_RANK_4_COST` | `SCIENCE_AdlerArnor`, `SCIENCE_StarOfTheNorth` | INVENTORIED; проверка условий TODO |

### Применение

| Слот | CommandButton | SpecialPower | RequiredSciences | Статус |
|---:|---|---|---|---|
| 1 | `Command_SpellBookHornOfArnor` | `SpellBookHornOfArnor` | `SCIENCE_HornOfArnor` | INVENTORIED; поведение TODO |
| 2 | `Command_SpellBookHealingOfTheOld` | `SpellBookHealingOfTheOld` | `SCIENCE_HobbitHouse` | INVENTORIED; поведение TODO |
| 3 | `Command_SpellBookArnorHobbitHouse` | `SpellBookArnorHobbitHouse` | `SCIENCE_HealingOfTheOld` | INVENTORIED; поведение TODO |
| 4 | `Command_SpellBookGiftOfTheKingdom` | `SpellBookGiftOfTheKingdom` | `SCIENCE_GiftOfTheKingdom` | INVENTORIED; поведение TODO |
| 5 | `Command_SpellBookWaelderCardolans` | `SpellBookWlderCardolans` | `SCIENCE_ElvenWoodArnor` | INVENTORIED; поведение TODO |
| 6 | `Command_SpellBookArnorSummonHobbits` | `SpellBookArnorSummonHobbits` | `SCIENCE_ArnorHobbitAllies` | INVENTORIED; поведение TODO |
| 7 | `Command_SpellBookNumenorHealBuilding` | `SpellBookNumenorHealBuilding` | `SCIENCE_NumenorMastery` | INVENTORIED; поведение TODO |
| 8 | `Command_SpellBookCirdanHelp` | `SpellBookSchwerterderWesternis` | `SCIENCE_SwordsOfThePast` | INVENTORIED; поведение TODO |
| 9 | `Command_SpellBookStarOfTheNorth` | `SpellBookStarOfTheNorth` | `SCIENCE_StarOfTheNorth` | INVENTORIED; поведение TODO |
| 10 | `Command_SpellBookGlorfindelHelp` | `SpellBookAdlerArnor` | `SCIENCE_AdlerArnor` | INVENTORIED; поведение TODO |
| 11 | `Command_SpellBookFornostHammer` | `SpellBookRunBitchWatersComin` | `SCIENCE_RunBitchWatersComin` | INVENTORIED; поведение TODO |
| 12 | `Command_SpellBookArnorLastHope` | `SpellBookSBLetzteAllianzArnor` | `SCIENCE_LastAllianceArnor` | INVENTORIED; поведение TODO |

Связи выше подтверждены полем `RequiredSciences` в `data/ini/includes/specialpower.inc` или `data/ini/specialpower.ini`; каждая из 12 наук применения соответствует одной науке покупки. Слоты 11–12 в двух наборах расположены в разном порядке. Для слотов 2–3 науки названы иначе, чем соответствующие кнопки; слот 7 использует `NumenorMastery` при покупке и `NumenorHealBuilding` при применении. Это данные для дальнейшего выяснения, не подтверждённые ошибки. В частности, нельзя утверждать, что совпадающие номера означают связь между покупкой и применением.

## Модули эффектов книги — исходные зависимости

В `data/ini/object/system/includes/systemgood.inc` найдены следующие модули с `SpecialPowerTemplate` для 12 кнопок. Автоматический тест подтвердил наличие этих 12 наследуемых модулей и их связь с SpecialPower в загруженном шаблоне `ArnorSpellBook`. Это не проверка выполнения модуля или доступности умения игроку. Альтернативные модули и остальные параметры ещё необходимо проверить через оригинал и игровой сценарий.

| SpecialPower | Модуль | Ссылки на данные | Статус |
|---|---|---|---|
| `SpellBookHornOfArnor` | `SpecialPowerModule` / `ModuleTag_HornOfArnor` | attributemodifier: `SpellBookWarChant`, triggerfx: `FX_SpellArnorHorn` | INVENTORIED; выполнение TODO |
| `SpellBookHealingOfTheOld` | `PlayerHealSpecialPower` / `ModuleTag_HealingOfTheOld` | healamount: `0.4`, healfx: `FX_JustAOldHealingFX` | INVENTORIED; выполнение TODO |
| `SpellBookArnorHobbitHouse` | `OCLSpecialPower` / `ModuleTag_ArnorHobbitHouse` | ocl: `OCL_SummonHobbitHouse`, triggerfx: `FX_SummonHobbitHouse` | INVENTORIED; выполнение TODO |
| `SpellBookGiftOfTheKingdom` | `PlayerUpgradeSpecialPower` / `ModuleTag_SpellBookMerciForHealing` | upgradename: `Upgrade_AMerciOfDaKing` | INVENTORIED; выполнение TODO |
| `SpellBookWlderCardolans` | `ElvenWoodSpecialPower` / `ModuleTag_CardolanHills` | elvengroveobject: `CardolanHill`, elvenwoodfx: `FX_CardolanHillsSpellFX`, elvenwoodocl: `OCL_CardolanStonesSeed` | INVENTORIED; выполнение TODO |
| `SpellBookArnorSummonHobbits` | `OCLSpecialPower` / `ModuleTag_ArnorSummonHobbits` | ocl: `OCL_SpawnArnorHobbitBowEgg`, triggerfx: `FX_HobbitSummon`, upgradename: `Upgrade_HobbitFriends` | INVENTORIED; выполнение TODO |
| `SpellBookNumenorHealBuilding` | `OCLSpecialPower` / `ModuleTag_NumenorHealBuilding` | attributemodifier: `SpellBookNumenorHeal`, ocl: `OCL_SpecialPowerNumenorHeal` | INVENTORIED; выполнение TODO |
| `SpellBookSchwerterderWesternis` | `OCLSpecialPower` / `ModuleTag_SummonArnorCirdan` | ocl: `OCL_CirdanUnitsSummonEgg`, triggerfx: `FX_CirdanSummon` | INVENTORIED; выполнение TODO |
| `SpellBookStarOfTheNorth` | `CloudBreakSpecialPower` / `ModuleTag_StarOfTheNorth` | sunbeamobject: `ArnorNorthStarObject` | INVENTORIED; выполнение TODO |
| `SpellBookAdlerArnor` | `OCLSpecialPower` / `ModuleTag_SummonArnorGlorfindel` | ocl: `OCL_GlorfindelUnitsSummonEgg`, triggerfx: `FX_SummonGlorfindelCavalry` | INVENTORIED; выполнение TODO |
| `SpellBookRunBitchWatersComin` | `OCLSpecialPower` / `ModuleTag_FornostHammer` | ocl: `OCL_SpellBookFornostHammer` | INVENTORIED; выполнение TODO |
| `SpellBookSBLetzteAllianzArnor` | `OCLSpecialPower` / `ModuleTag_SummonLastHopeArmy` | ocl: `OCL_SpellBookLastHope`, triggerfx: `FX_ArnorLastHope` | INVENTORIED; выполнение TODO |

## Начальные наборы строителя и крепости

`ArnorPorter` ссылается на `ArnorPorterStoneMakerCommandSet` в `data/ini/object/goodfaction/units/arnor/arnorporter.ini`. Исходные команды:

- 1: `Command_Stop` — INVENTORIED; доступность и эффект TODO.
- 2: `Command_PorterExtinguishFire` — INVENTORIED; доступность и эффект TODO.
- 3: `Command_Repair` — INVENTORIED; доступность и эффект TODO.
- 4: `Command_PorterConstructArnorFarm` — INVENTORIED; доступность и эффект TODO.
- 5: `Command_PorterConstructArnorStoneWorker` — INVENTORIED; доступность и эффект TODO.
- 6: `Command_PorterConstructArnorBarracks` — INVENTORIED; доступность и эффект TODO.
- 7: `Command_PorterConstructArnorStable` — INVENTORIED; доступность и эффект TODO.
- 8: `Command_PorterConstructArnorWorkshop` — INVENTORIED; доступность и эффект TODO.
- 9: `Command_PorterConstructArnorSentryTower` — INVENTORIED; доступность и эффект TODO.
- 10: `Command_PorterConstructArnorWell` — INVENTORIED; доступность и эффект TODO.
- 11: `Command_PorterConstructArnorStatue` — INVENTORIED; доступность и эффект TODO.
- 12: `Command_PorterConstructArnorBlacksmithMod` — INVENTORIED; доступность и эффект TODO.
- 13: `Command_PorterConstructArnorWallHub` — INVENTORIED; доступность и эффект TODO.
- 14: `Command_PorterConstructArnorFortress` — INVENTORIED; доступность и эффект TODO.

Найден также набор `ArnorFortressCommandSet` в `data/ini/commandset.ini`; его связь со всеми состояниями крепости и переключениями ещё требует проверки. Перечень:

- 1: `Command_RingMechanikGondor_forFornost` — INVENTORIED; доступность и эффект TODO.
- 2: `Command_PurchaseUpgradeImladrisAlly` — INVENTORIED; доступность и эффект TODO.
- 3: `Command_PurchaseUpgradeGondorAlly` — INVENTORIED; доступность и эффект TODO.
- 4: `Command_SelectUpgradesArnorFaction` — INVENTORIED; доступность и эффект TODO.
- 5: `Command_SelectRevivablesArnorFortress` — INVENTORIED; доступность и эффект TODO.
- 6: `Command_Sell` — INVENTORIED; доступность и эффект TODO.
- 7: `Command_SpecialAbilityArnorPalantirEye` — INVENTORIED; доступность и эффект TODO.
- 8: `Command_ConstructArnorPorter` — INVENTORIED; доступность и эффект TODO.
- 9: `Command_PurchaseUpgradeArnorFortressBanners` — INVENTORIED; доступность и эффект TODO.
- 10: `Command_PurchaseUpgradeArnorFortressFlamingMunitions` — INVENTORIED; доступность и эффект TODO.
- 11: `Command_PurchaseUpgradeArnorFortressCrystalMoat` — INVENTORIED; доступность и эффект TODO.
- 12: `Command_PurchaseUpgradeArnorFortressNumenorStonework` — INVENTORIED; доступность и эффект TODO.
- 13: `Command_PurchaseUpgradeArnorOldKingStatues` — INVENTORIED; доступность и эффект TODO.
- 14: `Command_PurchaseUpgradeArnorFortressPalantirEye` — INVENTORIED; доступность и эффект TODO.
- 15: `Command_RadialBack` — INVENTORIED; доступность и эффект TODO.
- 16: `Command_FakeRingHeroReviveSlot` — INVENTORIED; доступность и эффект TODO.
- 17: `Command_FakeCreateAHeroReviveSlot` — INVENTORIED; доступность и эффект TODO.
- 18: `Command_GenericReviveSlot1` — INVENTORIED; доступность и эффект TODO.
- 19: `Command_GenericReviveSlot2` — INVENTORIED; доступность и эффект TODO.
- 20: `Command_GenericReviveSlot3` — INVENTORIED; доступность и эффект TODO.
- 21: `Command_GenericReviveSlot4` — INVENTORIED; доступность и эффект TODO.
- 22: `Command_GenericReviveSlot5` — INVENTORIED; доступность и эффект TODO.
- 23: `Command_FakeHeroReviveSlot7Imrahill` — INVENTORIED; доступность и эффект TODO.
- 24: `Command_FakeHeroReviveSlot8Hobbit` — INVENTORIED; доступность и эффект TODO.
- 25: `Command_RadialBack` — INVENTORIED; доступность и эффект TODO.

## Наблюдения headless-прогона

Запущен `engine/build/openbfme_peer.exe` с `--mod-install workspace/mods/Ennorath/install`, чистой базой из `workspace/games/Vanilla_(2.01)/install` и BFME2 из `C:/Program Files (x86)/BFME2`. Использован готовый бинарник; без свежей сборки вывод относится только к нему. Отчёт фиксирует 300 кадров, профиль `49ce66422516f6556e611e1c97da5378`, 0 desync и отсутствие команд игрока. Назначение наблюдённых проблем конкретно Арнору ещё не установлено: отчёт общий по карте и обоим игрокам.

- Неперенесённые модули на живых объектах: `AudioLoopUpgrade`, `AutoPickUpUpdate`, `ClickReactionBehavior`, `CritterEmitterUpdate`, `GeometryUpgrade`, `GrabPassengerSpecialPower`, `RebuildHoleExposeDie`, `SlaveWatcherBehavior`, `SpellRechargeModifierUpgrade`.
- Неперенесённые действие `FOCAL_LENGTH_CAMERA` и условия `PLAYER_BUILT_UPGRADE`, `PLAYER_HAS_NUM_UNITS_WITH_UPGRADE`. Нужно установить, какие из них затрагивают достижимые объекты/сценарии Арнора и когда исполняются; для этого недостаточно одного пассивного прогона.

### Длинный прогон ИИ Арнора

В отчёте 3000 кадров обнаружены также `CommandButtonHuntUpdate` и `PickupStuffUpdate` среди неперенесённых модулей на живых объектах; условия `PLAYER_BUILT_UPGRADE` и `PLAYER_HAS_NUM_UNITS_WITH_UPGRADE` отработали как неподдержанные 600 и 6600 раз соответственно. В этом прогоне у второго игрока (ИИ Арнора) отчёт назвал библиотеку `Libraries/AI_Men Of The West/AI_Men Of The West.map`; требуется выяснить, соответствует ли это данным мода. Сами счётчики общие по матчу, а не доказательство принадлежности каждого случая Арнору. Проверить объект-владелец и последствия, прежде чем приоритизировать исправление.

### Связь отдельных неперенесённых модулей с Арнором

В исходных данных непосредственно производимый `ArnorFighterHordeNew` объявляет `CommandButtonHuntUpdate` и `PickupStuffUpdate` (`data/ini/object/goodfaction/units/arnor/new/arnorfighternew.ini`); `ArnorHarvester` объявляет `CommandButtonHuntUpdate` в `arnorharvester.ini`, а `ArnorTowerShieldGuardHordeNew` — `PickupStuffUpdate` в `arnortowershieldguardnew.ini`. Эти классы есть в отчёте как неперенесённые. Это связь по определениям, но счётчики отчёта не называют конкретных носителей, а игровой эффект и приоритет исправления ещё требуют отдельного сценария. Следующий шаг: выяснить точные бинарные семантики и пути вызова, затем воспроизвести и исправить общий модуль с независимым тестом.

### Классификация предупреждений предварительного графа

Первые результаты не означают 460 отсутствующих игровых ресурсов. В `unresolved.json` среди 415 строк `Weapon` встречаются позиции оружия `PRIMARY` (233), `TERTIARY` (37), `SECONDARY` (22) и `QUINARY` (8), а также имена `AutoResolve_*`; 27 строк `OCL` содержат метки `FINAL` / `INITIAL`, 18 строк `Upgrade` — параметр `Delay`. Эти токены не являются сами по себе отсутствующими определениями соответствующих типов. Остальные сообщения и обработку AutoResolve, загрузку override, наследования и условных ветвей нужно сверить с реальным парсером. Ни один из счётчиков не использовать как подтверждение полного или неполного переноса без такой проверки.

### Выявленное ограничение леса Кардолана

- [ ] **БЛОКИРОВАНО:** `SpellBookWlderCardolans` использует `ElvenWoodSpecialPower` с `CardolanHill`, `OCL_CardolanStonesSeed` и областью действия. `WeatherFamilySpecialPower::seedArea` в `engine/src/GameLogic/Module/SpellBookPowers.cpp` явно не переносит изменение области земли (S-921); создание объектов и FX само по себе не доказывает полный эффект территории. Для завершения изучить реализацию области в оригинале и перенести с проверками, включая взаимодействия с противоположными эффектами. Это существующее ограничение движка, а не новый вывод о целиком неработающем заклинании.

## Открытые задачи и следующий шаг

- Установить из полного эффективного профиля каждую цепочку science → кнопка → special power → эффект, цену и prerequisites, включая пассивную `GiftOfTheKingdom`; проверить порядок в интерфейсе оригинала и порта.
- Развернуть команды строителя и крепости до конечных объектов, всех замен CommandSet, производства, улучшений и зависимостей. Вывести все формы героев и их команды; не делать вывод о полноте по префиксу Arnor.
- Сопоставить каждый узел с работающими/неподдержанными модулями, существующими тестами и `docs/STOPS.md`. Проводить проверки с независимым ожидаемым результатом, затем через настоящий UI.
- Особые известные проблемы: огонь Carthaen/S-191, скорость частиц камней, NoRotate/S-5204 и DynamicDecal/S-5205; конкретные детали в общем плане и MODS.md.
- Для завершения A нужны полный инвентарь, доказанная достижимость и сравнение старта с оригиналом. Текущее количество строк в таблицах не является знаменателем всей фракции.

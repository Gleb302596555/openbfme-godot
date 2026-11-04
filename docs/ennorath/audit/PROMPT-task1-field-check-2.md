# Промт для следующей сессии: задача 1, проверка полей вручную (шаг 2)

Ты продолжаешь задачу 1 из `workspace/ennorath-audit/TASK-1-mod-data-audit.md`. Прочитай её и прошлый промт `PROMPT-task1-field-check.md` (там правила шага 2, формат `manual-check.tsv`, что сдать владельцу, что не делать). Обязательны правила `docs/ennorath/PROCESS.md`: разделы 5, 7, 14, 15 и 16. Отвечай владельцу по-русски, просто и коротко.

## Где работа

- Задача и промты задачи 1 есть в ветке: `docs/ennorath/audit/` (TASK-1, PROMPT-task1-field-audit, PROMPT-task1-field-check, этот файл). На другом компьютере читай их оттуда; `workspace/ennorath-audit/` в git не входит.

- Ветка `agent/claude/field-audit`, отправлена в origin. Последний коммит `df9f3dcf`. Новую ветку не создавать.
- На том же компьютере: дерево `workspace/wt/claude-rule-long-tests` (имя папки старое). На другом компьютере: `git fetch origin`, затем `python tools/agent_worktree.py` (как он умеет взять существующую ветку) или `git worktree add workspace/wt/claude-field-audit agent/claude/field-audit`. Перед началом: в дереве нет незакоммиченных изменений, `git log --oneline -1` = `df9f3dcf`. Если не так, остановись и сообщи владельцу.
- Инструмент `tools/mod/field_audit.py`, тест `tools/mod/test_field_audit.py` (pytest, 7 из 7): `python -m pytest -q tools/mod/test_field_audit.py`.
- Прогон из дерева, около 1,5 минуты (`<main>` — главная копия):
  `python tools/mod/field_audit.py --base "<main>\workspace\games-en\Vanilla_(2.01)\install" --overlay "<main>\workspace\mods-en\Ennorath\install" --coverage "<main>\workspace\ennorath-audit\field-audit-v2\module-coverage.tsv" --out "<main>\workspace\ennorath-audit\field-audit-v2"`
- `workspace/` в git не входит. На другом компьютере нужны установка RotWK и мода в `workspace/games-en`, `workspace/mods-en`, файл `module-coverage.tsv` и `manual-check.tsv` (это данные мода, их не коммитить; если их нет, перенеси с первого компьютера или пересоздай coverage командой из docstring инструмента).
- Python-скрипты не запускать из `%TEMP%`: там лежит чужой `grp.py`, Python 3.14 его импортирует, и он правит `engine/src/GameClient/DrawableIconUI.*`. Скрипты класть в scratchpad сессии и запускать с `python -I`.
- Сообщения коммитов писать в файл без BOM (PowerShell `Out-File utf8` и вывод в pipe дают BOM в теме коммита).

## Что сделано в прошлой сессии (2026-11-04)

Шаг 1 прошлого промта закрыт, плюс найдено и исправлено 9 ошибок инструмента (9-17 ниже), каждая с тестом, который падает без правки:

- `7a489cb4`: звуковая таблица Object (`audioTable`); чтение прошлого TSV с `QUOTE_NONE` (4669 из 4669 строк). Ошибка 9: блок, таблицу которого выбирает значение (`MeleeBehavior = Amoeba`, `dispatch`). Ошибка 10: подтаблица, которая берёт любое имя (`UnitSpecificSounds`, `UnitSpecificFX`, `catchAll`); из-за неё `End` закрывал весь Object и 297 полей не попадали в аудит. Ошибка 11: имя поля — первый токен (`Envelope InitialOpacity = ...`).
- `2a80ae7e`: ошибка 12: текст в кавычках и комментарии `/* */` считались чтением (`"the human side's ..."` в `EndGame.cpp`). Ошибка 13: локальная переменная с именем члена (`side` в `LiveGame.cpp`). Ошибка 14: член с индексом (`ES_OFF(goodMP[0])`) терялся, подставлялся `cap` из тела парсера. Ошибка 15: макрос через член (`ES_OFF(m) = offsetof(State, values.m)`) брал чужую структуру `State` из `SnapshotProbe.h`; 41 поле GameData было ложно parsed-unused.
- `df9f3dcf`: ошибка 16: чтение ищется сначала в файлах, которые называют структуру; если чтение есть только в файле, который её не называет, статус used, но `where` помечен `INFERENCE: the file does not name <struct>` (62 строки). Ошибка 17: параметр с именем члена (`findDozer(const std::string &side)`), в том числе на строке сигнатуры.

В `manual-check.tsv` (78 строк) записаны все ошибки 9-17 с вердиктами по коду. Там же **ошибка 18, не исправлена**: инструмент пропускает функции `parse*`, а `Side` в `ArmyDefinition`, `AIBase`, `AIDozerAssignment` используется именно там, как ключ реестра (`SkirmishAIData.cpp:462/502/521`, `nameToKey(x.side)`), и потом ищется через `findArmy`/`baseTemplatesOfSide`/`findDozer`. Вердикт для всех трёх — used. Исправить: чтение члена в `parse*`-функции другого блока (не в строке таблицы и не в парсере самого поля) считать чтением, хотя бы когда значение идёт в ключ (`nameToKey`, `operator[]`); тест, правка, прогон.

Итог после `df9f3dcf`: 4669 полей. used 3051, parsed-unused 495, no-table-row 372, stub-block 166, dynamic-row 156, custom-parser 131, missing-module 111, unmapped-block 65, used-raw 42, own-reader 37, raw 34, parsed-only-module 9, not-in-rotwk 0 (все 148 прошлых были ошибками инструмента).

## Сначала (до шага 2)

1. **`/LocomotorSet` `Condition`, `Locomotor`, `Speed`** перешли из used в parsed-unused после `df9f3dcf`, по коду не проверены. Найди, где порт читает LocomotorSet (`Locomotor.cpp`, `LocomotorSetEntry`), и реши: верно или ошибка правила о параметрах/локальных. Если ошибка — тест, правка, прогон.
2. **62 строки used с `INFERENCE`.** Пройди их: для каждой найди настоящее чтение члена именно этой структуры или запиши, что его нет (тогда это скрытое parsed-unused). Три `Side` (`aibase`, `aidozerassignment`, `armydefinition`) уже проверены вручную (ошибка 18 выше), остальные 59 — нет. Если картина общая (например, чтение через `auto` или геттер), научи инструмент, а не размечай вручную.
3. В `field-audit.md` поменять известное ограничение: текст и одноимённые локальные больше не путают, остался только член другой структуры в файле, который нашу структуру тоже называет.

## Шаг 2: ручная проверка

Теперь это 389 полей, а не 323: custom-parser 131, dynamic-row 156, own-reader 37, unmapped-block 65. Правила, порядок (по убыванию `defs`) и формат — в прошлом промте. Новое:

- 47 из unmapped-block — поля `/UnitSpecificSounds` (и `/UnitSpecificFX`). Порт хранит блок сырыми строками (`ThingFactory.cpp`, стоп S-071) и читает по имени: `GameLogic/UnitSpecificSound.cpp`, `GameClient/UnitVoiceResponse.cpp` (имена вида `VoiceEnterUnit` + имя шаблона собираются при запуске). Это общий механизм: научи инструмент (например, `NESTED_DEF_STRUCTS` или отдельное правило «чтение по имени в этих файлах, с учётом составных имён») и добавь случай в тест.
- `UpgradeSoundSelectorClientBehavior/SoundUpgrade` и `ModelConditionSoundSelectorClientBehavior/SoundState` — dynamic-row: строки звуковой таблицы, имена в `kRowNames` (`SoundSelectorClientBehavior.cpp:27`). Чтение по индексу строки (RW 0x676A8A) не прослежено.
- Опция `--manual <tsv>` ещё не сделана (описание в прошлом промте).

## Что сдать владельцу

Как в прошлом промте (пункты 1-5), номера новых ошибок инструмента продолжаются с 19.

## Не делать

Как в прошлом промте: не менять движок, не переносить в main без «переноси», не начинать задачу 2, не трогать мод в `RotWK/` и патч-ноут, не коммитить вывод инструмента и данные мода.

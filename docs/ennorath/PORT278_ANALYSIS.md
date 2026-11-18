# PORT-278 (issue #645): buff powers whose attribute modifier does nothing — разбор пяти кнопок

Вердикт заранее: **движок не отличается от RotWK 2.01 ни на одной из пяти кнопок, правка движка не нужна.** Изменения ветки: тесты `engine/tests/test_port_278.cpp` (5 кейсов, `PORT-278*`), этот разбор и строка журнала. Причина вердиктов матрицы — данные мода и сцены свипа, не порт. Владельцу: два `NO-TARGET` (Дорвинион, Рохан) и одно `BY-DESIGN` (рог Боромира: у списка нет `Modifier =`, у модуля нет `AttributeModifierAffects` — кнопка не может изменить ничего ни в порту, ни в ритейле).

## Что прочитано в RotWK 2.01 (game.dat) и что в порту

Линейно, с адресов (tools/mod/rw.py dis), все с оговоркой S-001:

| Что | RotWK 2.01 | Порт |
|---|---|---|
| применение модификатора ко всем выбранным | `RW 0x8977BF` | `SpecialPowerModules.cpp: triggerSpecialPower → RW 0x897BAB..0x897C47` (поиск в радиусе `AttributeModifierRange`, `FROM_CENTER_2D`, `ITER_SORTED_NEAR_TO_FAR`) |
| фильтр `AttributeModifierAffects` | `RW 0x66122D` → `RW 0x7640C1` → `RW 0x763543` | `SpecialPowerModules.cpp:1031` (лямбда `affects`), `ObjectFilterMatch::allows` |
| отношения (флаг 0 фильтра) | `RW 0x660B85` | та же лямбда, `relMask & (1u << getRelationship(o))` |
| alive | `RW 0xC10E20` | `!o.isEffectivelyDead()` |
| матч фильтра: правило | `RW 0x7638B6..0x76392C` (`allowsKindOf` `RW 0x76379C`) | `ObjectFilterMatch::allows`, шаги 1..6 |
| `RULE_NONE` (`NONE +...`) | `RW 0x7638F6..0x7638FD`: непустой include-KindOf-списк и `include ⊆ KindOf` (`RW 0x70B8C7`) | `RULE_NONE → nonEmpty(f.includeKindOf) && subsetOf(kindOf, f.includeKindOf)` |
| `RULE_ANY` (`ANY +...`) | `RW 0x7638D1..0x7638E5`: непустой include-список и пересечение (`RW 0x661359`) | `RULE_ANY → nonEmpty && anyBit` |
| эквивалентность имён (шаблонные `+Имя`) | `RW 0x73D5C2` (equivalence; сначала `a == b`) | `ObjectFilterMatch::equivalentWithLists`, шаги 4–5 до правила |
| опущенный `AttributeModifierAffects` | `RW 0x89693A` / `RW 0x89693D`: `RW 0x763D11` ставит фильтр `NONE` с двумя пустыми масками (+0x80 = 1, +0x88 = 0); совпадений нет ни с кем | `!d->m_attributeModifierAffects → false` (тот же ответ; счётчик `++m_unported` в weather-ветке) |
| применение к одному | `RW 0x89763B` (vslot 0x34): `Object::addAttributeModifier(name, -1)` `RW 0x68F1A8` → `RW 0x805C4E` (entry expire = frame + Duration списка), AntiCategory-блокировка `RW 0x804FCC` до frame + Duration | `applyToVictim` → `victim.addAttributeModifier(d->m_attributeModifier, -1)` |
| Duration списка (мс → кадры) | без адреса в порту: `kDurationMsecScale = 0.005f`, `NumericState::ceilScaled` | `INI::parseDurationUnsignedInt` → `ceilScaled` |
| снятие по истечении | `RW 0x80585F (снятие в update пула, стирание entry — RW 0x8059A8)` (update пула: модель-условия сняты, категория счётчик -1, entry стёрт) | `AttributeModifierPool::update` |

Отменена ложная находка: «локальный `subsetOf` работает наоборот». У `RW 0x70B8C7` третий аргумент — must-be-clear маска; у всех ~123 мест вызова это `0xDE49E4`, 28-байтная маска, все нули в файловом `.data`, и в `.text` нет ни одной абсолютной записи в `0xDE49E4` (скан `C7 05` / `A3` / `8D 05` с ±4, прямой адрес в `OR/ADD/AND/ADC/SUB/MOV`, релокации — ничего). Значит, `test static[i], obj[i]` вакусен, остаётся `include ⊆ obj` — семантика порта (`subsetOf`) совпадает с ритейлом. Правила матча декодированы целиком (rule 0 → записывается в 3 с `sete al` = true; rule 3 → true). Ничего не менялось.

## Пять кнопок по данным (Ennorath 1.9.8.3)

### 1–3. GondorBoromir_mod / GondorBoromir_OneRing / GondorBoromir_mod_ElrondsRat — `Command_SpecialAbilityBoromirHorn` / `Command_SpecialAbilityBoromirHornRing`

`ModuleTag_HornStarter` (все три блока, `object/goodfaction/units/men/boromir.ini` и соседние):

```
Behavior = SpecialPowerModule ModuleTag_HornStarter
  SpecialPowerTemplate = SpecialAbilityHornOfGondor02
  UpdateModuleStartsAttack = Yes
  AttributeModifier = BoromirGondorHornAntiCategory
  AttributeModifierRange = 100
End
```

- **`AttributeModifierAffects` в модуле отсутствует** → обе реализации дают «никто»: в порту `!d->m_attributeModifierAffects → false`; в ритейле `RW 0x89693A` / `RW 0x89693D` ставят `NONE` с пустыми масками, а матч правила `NONE` требует непустую include-маску (`RW 0x7638F6/0x7638FD`) → тоже никто.
- **`AntiCategory` в модуле закомментирован** → даже «эффект-категория» (страхи) не ставится (`until = frame`, блокировки категории нет).
- **`BoromirGondorHornAntiCategory` в данных мода содержит только `Duration = 5000`** — ни одной `Modifier =` строки. Даже если бы кто-то был выбран, `AttributeModifierPool::product()` не нашёл бы модификатора и ничего не изменил бы (ритейл то же: `RW 0x804CB3` не находит записей).
- В матрице (сцена свипа: герой-копия, GondorFighterHorde, GondorBarracks) рог рядом с «союзными войсками» по замыслу мода — никто не получает ничего, потому что никто и не может получить.

Итог: кнопка — **no-op по данным мода** в обеих редакциях. Чтобы заработала, моду нужно выдать фильтр (например, `AttributeModifierAffects = ALLIES +INFANTRY …` / `ALLIES +...`) и, если задумка — «снять страх», раскомментировать `AntiCategory = ...` с непустым списком модификаторов/катegorии. Это решение владельца мода; порт ничего править тут не должен.

### 4. DorwinionNaturmagier — `Command_SpecialAbilityNaturmagierErntesegen`

```
Behavior = SpecialPowerModule ModuleTag_ErntesegenPower
  SpecialPowerTemplate = SpecialAbilityNaturmagierErntesegen
  UpdateModuleStartsAttack = Yes
  AttributeModifier = NaturmagierErntesegenModifier
  AttributeModifierRange = 150000
  AttributeModifierAffectsSelf = No
  AttributeModifierAffects = ANY +ElvenMallornTree ALLIES
End

ModifierList NaturmagierErntesegenModifier
  Category = BUFF
  Duration = 90000
  Modifier = PRODUCTION 125%
End
```

- Фильтр `ANY +ElvenMallornTree ALLIES`: `ElvenMallornTree` — **не KindOf**, это имя шаблона → include-KindOf-маска пуста; правило `ANY` в шаге 6 (`RW 0x7638D1..0x7638E5` / порт `RULE_ANY`) отвечает «нет» для всех, кроме шаблона-тезки (шаг 4, `RW 0x73D5C2`, `a == b`). То есть **выбрать можно только дерево**.
- Сцена свипа DorwinionNaturmagier: герой, союзная его копия, GondorFighterHorde, GondorBarracks и структуры, продающие апгрейды (ставятся у дома). **ElvenMallornTree в сцене нет** → никто → `no result`.
- **`NO-TARGET` — правильный вердикт**, матрице нужен `ElvenMallornTree` в сцене (или вердикт `NO-TARGET` принять). В игре (Дорвинион, деревья вокруг) порт применит `PRODUCTION 125%` на 90 с — доказано синтетикой той же формы фильтра (`PORT-278*`, PRODUCTION читается через `Economy::attributeModifierProduct(ATTRIBUTE_MODIFIER_PRODUCTION, …)` / `ATTRIBUTE_PRODUCTION`).
- Попутно: в `MATRIX_GAPS.tsv` эта же кнопка учитывается и по `SpecialAbilityUpdate` (`SPECIAL_SCREECH`, `0x00851E83@0 0x00852CA2@1`) и по `UnpauseSpecialPowerUpgrade` (`0x00697B6E@1 0x008D280A@3`) — отдельные классы и отдельные строки матрицы (PORT-279 и PORT-251/252), сюда не входят.

### 5. GasthausGhanBuriGhan — `Command_SpecialAbilityGeheimeWege`

```
Behavior = SpecialPowerModule ModuleTag_ElfCloakStarter
  SpecialPowerTemplate = SpecialAbilityWormtongueEscape
  UpdateModuleStartsAttack = Yes
  AttributeModifier = GhanSecretPathsLeadership
  AttributeModifierRange = 200
  AttributeModifierAffectsSelf = No
  AttributeModifierAffects = NONE +DruedainKrieger +DruedainKriegerHorde +DruedainWeiserBanner ALLIES
End

ModifierList GhanSecretPathsLeadership
  Category = LEADERSHIP
  Duration = 15000
  Modifier = DAMAGE_MULT 125%
End
```

- Фильтр `NONE +<шаблоны>`: имена — шаблоны, не KindOf → маска пуста; правило `NONE` требует непустую маску → вне имён никто; имена матчатся в шаге 4 (equivalence, `a == b`) → **выбрать можно только Друэдан** (`DruedainKrieger`, `DruedainKriegerHorde`, `DruedainWeiserBanner`). Снова `ALLIES`-фильтр: только свои.
- Сцена свипа Рохана: герой, союзная его копия, GondorFighterHorde, GondorBarracks. **Друэдан в сцене нет** → никто → `no result`.
- **`NO-TARGET` — правильный вердикт**; матрице нужны Друэдан рядом с Гханом (или принять `NO-TARGET`). В игре (Рохан, друэданы при Гхане) порт применит `DAMAGE_MULT 125%` на 15 с (синтетика той же формы — `PORT-278*`).
- Попутно: `ToggleHiddenSpecialAbilityUpdate` в этой же строке `MATRIX.tsv` — отдельный класс и отдельная задача (Port 279); сюда не входит.

## Почему это не баг порта

1. Синтетический тест той же формы фильтра применяет модификатор союзнику, не подходящему под фильтр — не применяет; вне отношений (враг) — не применяет; самому кастеру при `AffectsSelf No` — не применяет; по истечении `Duration` (5000 мс = 25 кадров) список уходит из пула и значение возвращается.
2. Опущенный фильтр и в порту, и в ритейле выбирает никого (выше).
3. Совпадение с ритейлом по всем точкам кадра проверено по бинарю линейным чтением (таблица выше); расхождений не найдено. Единственное узкое известное отличие всего пути (`RW 0xC0F374`, общий флаг «вне играбельной области») учтено стопом S-920 и на исход не влияет.

## Тесты (`engine/tests/test_port_278.cpp`, синтетические, без игровых файлов)

- `ANY +INFANTRY ALLIES`: союзник в радиусе получает `DAMAGE_MULT 125%` (изменённое значение, не только имя списка), структура вне фильтра, враг и сам кастер (`AffectsSelf No`) — нет.
- `ANY +<шаблон> ALLIES` (форма Ернтесегена): союзное дерево получает `PRODUCTION 125%`; инфант, иная структура, враг-дерево, кастер — нет.
- `NONE +<шаблоны> ALLIES` (форма Гхана): два именованных друэдана получают `DAMAGE_MULT 125%`; инфант, враг-друэдан, кастер — нет.
- Снятие по `Duration`: после 25 кадров список уходит, значение возвращается к 1.0.
- Опущенный `AttributeModifierAffects` (форма рога): никто в радиусе не получает (case рога `ModuleTag_HornStarter`).

Откат правки невозможен (правки движка нет); тесты закрепляют правила бинарника, чтобы будущая регрессия этих путей была красной.

## Что осталось владельцу

1. Матрица (`tools/matrix_run.py` / сцена `test_hud_factb_units.cpp` `refreshTargets`): для `DorwinionNaturmagier` и `GasthausGhanBuriGhan` нужны соответствующие цели в сцене, либо `NO-TARGET` принять (сейчас он не в `NOT_RED`).
2. Мод (Ennorath): рог Боромира по данным — no-op; решение за автором мода (фильтр + AntiCategory + строки `Modifier =`).
3. Порядок переноса: PORT-251 (`#599`, PR #622) **ещё не в `origin/main`**. Ветка не ждёт его по файлам (движок не тронут), но по правилу issue #645 PR переносится после #622 (либо с согласия владельца). S-7475..S-7477 остались неиспользованными (новых стопов нет: ничего не осталось непортированным).

## Проверки

Прогнаны 2026-11-17/18 на ветке `arena/c10fffa7-openbfme-godot` (сборка `engine/build`, `-j2`; `openbfme_tests` собран после добавления `${OPENBFME_PORT278_TESTS}` в источники — до этого ninja о файле не знал):

- `openbfme_tests -tc="PORT-278*"` — **5/5 passed** (127 assertions).
- `-tc="SPELL-2*"` — 12/12 passed; `-tc="*Banner*"` — 40/40 passed (1020 assertions).
- `-tc="HERO-1*"` — 0 кейсов (имя-маска ничему не соответствует; соседи героев покрыты `*Banner*` и `*SpecialPower*`).
- `-tc="*SpecialPower*"` — 48/49 passed; 1 падение — `cbhunt: ActionManager::canDoSpecialPowerAtObject (RW 0x82D925)` (`test_cbhunt.cpp:329-331`): код на ветке идентичен `origin/main` (apply-путь и `ActionManager.cpp` нетронутым), тест детерминированно ждёт `unported = true` у типа `0x33`, но 0x33 ведётся в `canGiveUpgrade` (порт PORT-109) и не считает `objectTypeTests`; `origin/main` (коммит `3d017a4`, единственный вперёд базового `a0302bf`) `ActionManager.cpp`, `test_cbhunt.cpp` и apply-путь не меняет — падение пре-существующее и к Port 278 отношения не имеет. Кандидат на отдельный разбор (либо тест протух, либо `0x33` должен считаться; решение владельца).

Движок не изменён → `*.hashes` не двигались; `git diff --stat -- engine/src` пуст относительно базы ветки. Ветка на 1 коммит позади `origin/main` (`3d017a4`, гигантский импорт инструментов; apply-путь и `engine/tests/test_cbhunt.cpp` не затрагивает), поэтому вердикты переносятся.

# PORT-278 (issue #645): кнопки с модификатором, который «не срабатывает» — полный разбор девяти кнопок

Вердикт: **движок не отличается от RotWK 2.01 ни на одной из девяти кнопок; правка движка не нужна.** Изменения ветки: тесты `engine/tests/test_port_278.cpp` (5 кейсов) и `engine/tests/test_port_278_horn.cpp` (5 кейсов), `docs/ennorath/*`, этот разбор и строка журнала. Причина красных строк матрицы — данные мода и сцени свипа (или, в одном случае, не этот путь), не порт.

## Что прочитано в RotWK 2.01 (game.dat) и что в порту

Линейно, с адресов (tools/mod/rw.py dis), все с оговоркой S-001:

| Что | RotWK 2.01 | Порт |
|---|---|---|
| применение модификатора ко всем выбранным | `RW 0x8977BF` | `SpecialPowerModules.cpp: triggerSpecialPower → RW 0x897BAB..0x897C47` (поиск в радиусе `AttributeModifierRange`, `FROM_CENTER_2D`, `ITER_SORTED_NEAR_TO_FAR`) |
| фильтр `AttributeModifierAffects` | `RW 0x66122D` → `RW 0x7640C1` → `RW 0x763543` | `SpecialPowerModules.cpp:1031` (лямбда `affects`), `ObjectFilterMatch::allows` |
| отношения (флаг 0 фильтра) | `RW 0x660B85` | та же лямбда, `relMask & (1u << getRelationship(o))` |
| alive | `RW 0xC10E20` | `!o.isEffectivelyDead()` |
| матч фильтра: правило | `RW 0x7638B6..0x76392C` (`allowsKindOf` `RW 0x76379C`) | `ObjectFilterMatch::allows`, шаги 1..6 |
| `RULE_NONE` (`NONE +...`) | `RW 0x7638F6..0x7638FD`: непустой include-KindOf-список и `include ⊆ KindOf` (`RW 0x70B8C7`) | `RULE_NONE → nonEmpty(f.includeKindOf) && subsetOf(kindOf, f.includeKindOf)` |
| `RULE_ANY` (`ANY +...`) | `RW 0x7638D1..0x7638E5`: непустой include-список и пересечение (`RW 0x661359`) | `RULE_ANY → nonEmpty && anyBit` |
| эквивалентность имён (шаблонные `+Имя`) | `RW 0x73D5C2` (equivalence; сначала `a == b`) | `ObjectFilterMatch::equivalentWithLists`, шаги 4–5 до правила |
| опущенный `AttributeModifierAffects` | `RW 0x89693A` / `RW 0x89693D`: `RW 0x763D11` ставит фильтр `NONE` с двумя пустыми масками (+0x80 = 1, +0x88 = 0); совпадений нет ни с кем | `!d->m_attributeModifierAffects → false` (тот же ответ) |
| применение к одному | `RW 0x89763B` (vslot 0x34): `Object::addAttributeModifier(name, -1)` `RW 0x68F1A8` → `RW 0x805C4E` (entry expire = frame + Duration списка), AntiCategory-блокировка `RW 0x804FCC` до frame + Duration | `applyToVictim` → `victim.addAttributeModifier(d->m_attributeModifier, -1)` |
| Duration списка (мс → кадры) | без адреса в порту: `kDurationMsecScale = 0.005f`, `NumericState::ceilScaled` | `INI::parseDurationUnsignedInt` → `ceilScaled` |
| снятие по истечении | `RW 0x80585F` (снятие в update пула; стирание entry `RW 0x8059A8`) | `AttributeModifierPool::update` |
| пульс страха парного `ModelConditionSpecialAbilityUpdate` (`GenerateTerror` / `GenerateUncontrollableFear`) | `RW 0x89455B` (`WhichSpecialPower`: 1 → `SPECIAL_POWER_1`, 2 → `SPECIAL_POWER_2`, 3 → `SPECIAL_POWER_3`); эффект — `RW 0x894701`: `ThePartitionManager` в радиусе `EmotionPulseRadius`, фильтр «враги» (`RW 0xC1676C`, ENEMIES); каждой жертве `EmotionTrackerUpdate::request` — `EMOTION_TERROR` / `EMOTION_UNCONTROLLABLE_FEAR`, на 1 кадр (`RW 0x68F37F` / `RW 0x68F383`) | `ModelConditionSpecialAbilityUpdate::triggerAbilityEffect` → `emotionPulse` (SpecialAbilityModules.cpp) — тот же обход, та же фильтрация, та же задержка |
| переключение модели и снятие | `RW 0x89455B` при распаковке (`startUnpacking`), `RW 0x8945C1` при `finishAbility` | `startUnpacking` / `finishAbility` (same) |
| связка UpdateModuleStartsAttack | `vslot 0x38`, `RW 0x897D25` → `RW 0x897987(null)` — SpecialPowerModule исполняется из парного ability-апдейта | `SpecialAbilityUpdate::triggerAbilityEffect` → `SpecialPowerModule::triggerFromUpdate()` |

Отменена ложная находка: «локальный `subsetOf` работает наоборот». У `RW 0x70B8C7` третий аргумент — must-be-clear маска; у всех ~123 мест вызова это `0xDE49E4`, 28-байтная маска, все нули в файловом `.data`, и в `.text` нет ни одной абсолютной записи в `0xDE49E4` (скан `C7 05` / `A3` / `8D 05` с ±4, прямой адрес в `OR/ADD/AND/ADC/SUB/MOV`, релокации — ничего). Значит, `test static[i], obj[i]` вакусен, остаётся `include ⊆ obj` — семантика порта (`subsetOf`) совпадает с ритейлом. Правила матча декодированы целиком (rule 0 → записывается в 3 с `sete al` = true; rule 3 → true). Ничего не менялось.

## Семейство из девяти кнопок (Ennorath 1.9.8.3) — по одной строке

Сцены свипа (герой, его копия, GondorFighterHorde, GondorBarracks, иногда структуры) и строки `MATRIX.tsv`:

| # | Фракция | Объект | Кнопка | Модули (цепочка) | Матрица | Вердикт |
|---|---|---|---|---|---|---|
| 1 | FactionMen | `GondorBoromir_mod` | `Command_SpecialAbilityBoromirHorn` | `ModuleTag_HornStarter` (SpecialPowerModule, пустой список `BoromirGondorHornAntiCategory` (только `Duration = 5000`), фильтра нет) + парный `ModuleTag_HornUpdate` (ModelConditionSpecialAbilityUpdate) | 538 NO-EFFECT: только `SpecialPowerModule AttributeModifier BoromirGondorHornAntiCategory: no result; ` | **не баг; рабочая паника** (см. ниже) |
| 2 | FactionMen | `GondorBoromir_OneRing` | `Command_SpecialAbilityBoromirHornRing` | то же | 532 NO-EFFECT: только тот же SpecialPowerModule-clause | то же |
| 3 | FactionImladris | `GondorBoromir_mod_ElrondsRat` | `Command_SpecialAbilityBoromirHorn` | то же | 2106 NO-EFFECT: тот же clause | то же |
| 4 | FactionMordor | `MordorNazgulMarshal` | `Command_SpecialAbilityScreechDunklerMarschall` | то же (`ModuleTag_HornStarter`) + SCREECH-тип команды (`0x89`) | 3469 NO-EFFECT: тот же clause | то же; **владелец 2026-11-18: в игре скрик оглушает врагов** (тот же пульс; эффект задаёт нуггет мода; см. ниже и тест 5) |
| 5 | FactionRohan | `RohanGamling_mod_neu` | `Command_SpecialAbilityHornGamling` | та же форма, но `GamlingHornNew` (по строке матрицы сработало: `GamlingHornNew on object 45914`) | 1315 PASS (после 46 кадров) | **PASS** — подтверждает, что путь вызывается и применяется, когда список непустой |
| 6 | FactionRohan | `GondorAragorn_SummonedHunters` | `Command_SpecialAbilityElendilSummoned` | `ModuleTag_ElendilStarter` (SpecialPowerModule, `AragornElendilAntiCategory` — тот же носитель с `Duration = 5000`) + `ModuleTag_ElendilUpdate` (ModelConditionSpecialAbilityUpdate) + `ModuleTag_EnableElendil` (UnpauseSpecialPowerUpgrade; способность заперта до 2 уровня) | 985 NO-EFFECT: только SpecialPowerModule-clause | то же, что 1–4 |
| 7 | FactionAngmar | `AngmarAgandaur` | `Command_SpecialAbilityAgandaurLighting` | `ModuleTag_VerheerenderAngriffSpecialPower` (SpecialPowerModule; список-носитель AntiCategory) + `ModuleTag_VerheerenderAngriffUpdate` (WeaponFireSpecialAbilityUpdate, `SpecialWeapon AgandaurLightingWeapon`) + `ModuleTag_VerheerenderAngriffUnpause` | 4006 NO-EFFECT: **WeaponFireSpecialAbilityUpdate**-clause («0 жертв, 0 урона»); SpecialPowerModule **не** в списке непройденных | **не этот порт**: падающее звено — оружейный модуль WeaponFireSpecialAbilityUpdate (семья порядка 30 NO-EFFECT в MATRIX_GAPS.md), не SpecialPowerModule-путь; причина по данным мода не установлена, нужен отдельный разбор оружия (`docs/ennorath/MATRIX_GAPS.md`, класс WeaponFireSpecialAbilityUpdate) |
| 8 | FactionElves | `DorwinionNaturmagier` | `Command_SpecialAbilityNaturmagierErntesegen` | `ModuleTag_ErntesegenPower` (SpecialPowerModule, `NaturmagierErntesegenModifier`: `PRODUCTION 125%`, 90 с; фильтр `ANY +ElvenMallornTree ALLIES`) | 1538 NO-EFFECT: `SpecialPowerModule …: no result` + `SpecialAbilityUpdate (SPECIAL_SCREECH …): no result; ` (вторая часть — Port 279) | **NO-TARGET** (принят владельцем 2026-11-18): дерево `ElvenMallornTree` в сцене свипа нет; в игре в Дорвинионе применяется |
| 9 | FactionRohan | `GasthausGhanBuriGhan` | `Command_SpecialAbilityGeheimeWege` | `ModuleTag_ElfCloakStarter` (SpecialPowerModule, `GhanSecretPathsLeadership`: `DAMAGE_MULT 125%`, 15 с; фильтр `NONE +DruedainKrieger +DruedainKriegerHorde +DruedainWeiserBanner ALLIES`) | 966 NO-EFFECT: `SpecialPowerModule …: no result` + `ToggleHiddenSpecialAbilityUpdate: no result` (вторая — Port 279) | **NO-TARGET** (принят владельцем 2026-11-18): друэданов в сцене свипа нет; в игре у Гхана применяется |

Общий шаблон семейства рогов (строки 1–4, 6): **это не баффы, и это не «пустая кнопка».** Кнопка запускает пульс парного `ModelConditionSpecialAbilityUpdate` по врагам в радиусе (RW 0x894701), и именно он — в игре работает способность. Видимый эффект задаёт нуггет мода (его конфигурация локально не проверяема: ритейл-INI отсутствует): у рогов и Арагорна это страх/паника (ритейл Арагорна проверен: две орды бегут, `test_emotion_ai.cpp`), у скрика Маршала — по владельцу 2026-11-18 оглушение (lock временного состояния, отказ команд; синтетика ниже, тест 5). Движок в обоих случаях одинаков: пульс по врагам, замок врагов — приказы не проходят. `SpecialPowerModule` с `UpdateModuleStartsAttack = Yes` делает второй внешний эффект: ставит чужой `AntiCategory` на тех, кто попадёт под скан, на время `Duration` списка-носителя (`RW 0x897859..0x8978BB` / порт `applyToVictim` + AntiCategory-блок в `applyToVictim`). Список-носитель `BoromirGondorHornAntiCategory` / `AragornElendilAntiCategory` ни одной `Modifier =` строки не имеет — и `AntiCategory`-строка **в самом модуле закомментирована**; следовательно:

1. даже выбранный сканом объект ничего в атрибутах не меняет (продукт по пустым записям = 1.0, RW 0x804CB3 не находит записей);
2. AntiCategory-блокировки тоже нет (комментарий в модуле);
3. `AttributeModifier` в модуле остался как реликт первоначального «horn of Gondor with the anti-fear»-замысла — мёртвый груз данных.

Матрица это и честно показывает: **непройденным остался только SpecialPowerModule-модификатор**; кнопка при этом `fired`, `ModelConditionSpecialAbilityUpdate` отработал (SPECIAL_POWER_n выставлен, парный пульс исполнен — в ритейле Aragorn's horn делает две орды паниковать и бежать; `engine/tests/test_emotion_ai.cpp`, «emotion AI retail: Aragorn's Horn of Elendil (GenerateTerror)…»). То есть кнопка в игре не «не срабатывает»; она срабатывает, но даёт эмоциональный эффект (у Арагорна и Элендила — страх; у скрика Маршала — оглушение, по владельцу 2026-11-18), а не рост урона.

## Что именно доказывают новые тесты (`engine/tests/test_port_278_horn.cpp`)

Синтетический INI (CombatTestUtil, без файлов игры); сценарии воспроизводят точные формы мода:

1. **Рог как его выдаёт мод** (пустой список, фильтра нет): приведение в бой → по врагам в `EmotionPulseRadius` проходит пульс, `EMOTION_TERROR` выставлен им (HitTracker, `emotionFirstSeen`), алли и сам кастер не тронуты; `SpecialPowerModule` при этом никого не выбрал (`victimsAffected() == 0`) — как и ритейл (RW 0x89693A/0x89693D: пустое `NONE`-правило).
2. **Вариант со `SPECIAL_POWER_2` и `GenerateUncontrollableFear`** (второе плечо синтетики: WhichSpecialPower 2, `EMOTION_UNCONTROLLABLE_FEAR`) — тот же пульс по врагам, `SPECIAL_POWER_2` на кастере на время распаковки; союзники спокойны. (Вариант синтетический: конкретный флаг Ennorath проверяется данными мода; строка матрицы, как и в рогах, не показывает других непройденных пунктов парного апдейта — либо он ожидал и выполнил, либо флагов не спрашивал.)
3. **Форма строки матрицы** (before/after Difference Probe — тот же `SweepEffectProbe`, что и у свипа): после каста `SPECIAL_POWER_1` выставлен, враг получил `EMOTION_TERROR`, и **единственная** непройденная строка — `SpecialPowerModule AttributeModifier Port278HornEmptyList: no result; ` — ровно та же `missing`-строка, которую показывает `MATRIX.tsv` для кнопок 1–4, 6. Ничто больше в цепочке нажатия не сломано.
4. **Список-носитель по своей сути**: если модуль **получает фильтр** и пустышка-список применяется, запись кладётся с `Duration = 5000` (5000 мс → 25 кадров, RW 0x80585F), `PRODUCTION`/`DAMAGE_MULT` при этом читаются 1.0 (записей `Modifier =` нет) и по истечении сроков снимается — то есть канал для AntiCategory-носителя работает, но атрибуты он не меняет и не может.
5. **Маршал (`Command_SpecialAbilityScreechDunklerMarschall`)**: тот же сценарий на объекте `MordorNazgulMarshal` с тем же комплектом модулей: пульс по врагам, `EMOTION_TERROR` выставлена им (HitTracker), союзник не тронут; враги в запертом временном состоянии — `allowedToRespondToCommand(CMD_FROM_PLAYER)` = false, приказы не проходят; `SpecialPowerModule` никого не выбирает (`victimsAffected() == 0`). Это и есть проверка «оглушения» по движку: механика рога одна на семейство, эффект задаёт нуггет мода.

Тесты закрепляют: пульс парного апдейта идёт по врагам в радиусе; пустышка-список заведомо не меняет атрибутов (у записи нет строк `Modifier =`); строки матрицы для рогов именно такие, и только такие, по устройству данных мода.

### 1–4, 6. GondorBoromir_mod / GondorBoromir_OneRing / GondorBoromir_mod_ElrondsRat / MordorNazgulMarshal / GondorAragorn_SummonedHunters — рога

`ModuleTag_HornStarter` / `ModuleTag_ElendilStarter` (все перечисленные блоки):

```
Behavior = SpecialPowerModule ModuleTag_HornStarter
  SpecialPowerTemplate = SpecialAbilityHornOfGondor02
  UpdateModuleStartsAttack = Yes
  AttributeModifier = BoromirGondorHornAntiCategory
  AttributeModifierRange = 100
End
```

- **`AttributeModifierAffects` в модуле отсутствует** → обе реализации дают «никто»: в порту `!d->m_attributeModifierAffects → false`; в ритейле `RW 0x89693A` / `RW 0x89693D` ставят `NONE` с пустыми масками, а матч правила `NONE` требует непустую include-маску (`RW 0x7638F6/0x7638FD`) → тоже никто.
- **`AntiCategory` в модуле закомментирован** → даже «эффект-категория» не ставится нигде.
- **`BoromirGondorHornAntiCategory` / `AragornElendilAntiCategory` содержат только `Duration = 5000`** — ни одной `Modifier =` строки: даже выбранный сканом объект не получил бы никакого изменения.
- Парный `ModelConditionSpecialAbilityUpdate` доставляет врагам эмоциональный пульс (синтетика выше; ритейл — паника двух псов-орд у Арагорна; для Маршала — оглушение, по владельцу 2026-11-18).

**Вердикт: это не баг порта и не баг свипа; это форма кнопки в данных мода.** Кнопка — рабочая: парный `ModelConditionSpecialAbilityUpdate` доставляет врагам в радиус эмоциональный пульс (RW 0x894701, `EmotionPulseRadius`) — у рогов это паника (как у ритейла Арагорна), у скрика Маршала — оглушение (по владельцу 2026-11-18; тот же пульс, другой нуггет мода); строки матрицы честно показывают ровно один непройденный пункт — modifier-clause пустышки. `SpecialPowerModule` тут мёртвый носитель (AntiCategory закомментирован, список без `Modifier =`): управлять страхом он не должен был и не может. Порт ничего править здесь не должен; усиливать кнопку эффектами — решение автора мода (раскомментировать `AntiCategory = …` и/или дать списку `Modifier =` и фильтр).

### 5. RohanGamling_mod_neu — `Command_SpecialAbilityHornGamling` — PASS (контрольный)

Та же форма; `GamlingHornNew` применяется (`GamlingHornNew on object 45914`) и `SPECIAL_POWER_1` выставлен — матрица: PASS. Подтверждает: цепочка нажатия, применяемый модификатор при непустом списке и пульс парного апдейта работают; проблема рогов ровно одна — пустышка в данных.

### 7. AngmarAgandaur — `Command_SpecialAbilityAgandaurLighting` (kdtlachen)

Строка матрицы: `WeaponFireSpecialAbilityUpdate SpecialWeapon AgandaurLightingWeapon: no result (…0 victims hit…)`; SpecialPowerModule-clause отсутствует — apply-путь сюда непричастен. Это класс из ~30 NO-EFFECT WeaponFireSpecialAbilityUpdate (`MATRIX_GAPS.md`). Причина по данным мода не установлена (нужны: INI-блок оружия `AgandaurLightingWeapon`, его warhead и сцена AngmarAgandaur в Colab); принадлежит отдельному разбору, не этому порту.

### 8. DorwinionNaturmagier — `Command_SpecialAbilityNaturmagierErntesegen`

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

- Фильтр `ANY +ElvenMallornTree ALLIES`: `ElvenMallornTree` — **не KindOf**, это имя шаблона → include-KindOf-маска пуста; правило `ANY` в шаге 6 отвечает «нет», кроме шаблона-тезки по эквивалентности имён (шаг 4, RW 0x73D5C2) — выбран может быть **только Mallorn-дерево**.
- Сцена свипа DorwinionNaturmagier: герой, его копия, GondorFighterHorde, GondorBarracks, структуры (у дома) — **ElvenMallornTree в сцене нет** → никто → `no result`. **`NO-TARGET` — правильный вердикт** (принят владельцем 2026-11-18, сцены не меняются). В игре (Дорвинион, деревья рядом) порт применит `PRODUCTION 125%` на 90 с — доказано синтетикой той же формы фильтра (`PORT-278*`).

### 9. GasthausGhanBuriGhan — `Command_SpecialAbilityGeheimeWege`

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

- Фильтр `NONE +<шаблоны>`: имена — шаблоны → маска пуста → по KindOf не проходит никто (RW 0x7638F6..0x7638FD); матчатся только названные друэданы (шаг 4). Снова `ALLIES`: только свои. **Друэданов в сцене свипа Рохана нет** → `NO-TARGET` (принят владельцем 2026-11-18). В игре (Друэданы при Гхане) порт применит `DAMAGE_MULT 125%` на 15 с (синтетика той же формы — `PORT-278*`).

## Почему это не баг порта

1. Синтетические тесты обеих форм фильтра применяют модификатор союзнику, не подходящему под фильтр — не применяют; вне отношений (враг) — не применяют; самому кастеру при `AffectsSelf No` — не применяют; по истечении `Duration` список уходит из пула, значение возвращается.
2. Опущенный фильтр и в порту, и в ритейле выбирает никого (выше).
3. Парный пульс страха/неконтролируемого страха в порту реализован и исполняется (`SpecialAbilityModules.cpp: emotionPulse`); ритейл — в `test_emotion_ai.cpp` (Арагорн, две орды, паника и бегство); механика семейства одна — у скрика Маршала тот же пульс, а «оглушение» (по владельцу 2026-11-18) выражено запертым временным состоянием, отказывающим в приказах.
4. Совпадение с ритейлом по всем точкам кадра проверено по бинарю линейным чтением (таблицы выше); расхождений не найдено. Единственное узкое известное отличие всего пути (`RW 0xC0F374`, общий флаг «вне играбельной области») учтено стопом S-920 и на исход не влияет.

## Тесты

`engine/tests/test_port_278.cpp` (синтетические, без игровых файлов):

- `ANY +INFANTRY ALLIES`: союзник в радиусе получает `DAMAGE_MULT 125%`; структура вне фильтра, враг и сам кастер (`AffectsSelf No`) — нет.
- `ANY +<шаблон> ALLIES` (форма Ернтесегена): союзное дерево получает `PRODUCTION 125%`; инфант, иная структура, враг-дерево, кастер — нет.
- `NONE +<шаблоны> ALLIES` (форма Гхана): именованные друэданы получают `DAMAGE_MULT 125%`; инфант, враг-друэдан, кастер — нет.
- Снятие по `Duration`: после 25 кадров список уходит, значение возвращается к 1.0.
- Опущенный `AttributeModifierAffects` (форма рога): никто в радиусе не получает.

`engine/tests/test_port_278_horn.cpp` (синтетические, без игровых файлов):

- Рог как его выдаёт мод (пустышка-список, фильтра нет): враги в `EmotionPulseRadius` получают `EMOTION_TERROR` (HitTracker), алли и кастер — нет; `SpecialPowerModule` не выбирает никого.
- Вариант `WhichSpecialPower = 2` + `GenerateUncontrollableFear` (второй плечо синтетики): `SPECIAL_POWER_2` на время распаковки; враги получают `EMOTION_UNCONTROLLABLE_FEAR`; алли спокойны.
- Форма строки матрицы: после каста `SPECIAL_POWER_1` выставлен и враг получил `EMOTION_TERROR`; непройденный пункт ровно один — `SpecialPowerModule AttributeModifier Port278HornEmptyList: no result; ` (та же строка `missing`, что и в матрице для кнопок 1–4, 6).
- Список-носитель: если модулю дать фильтр, пустышка-список лежит на 25 кадров (`Duration = 5000`), `DAMAGE_MULT`/`PRODUCTION` всё это время 1.0 и после снятия; уровень AntiCategory-носителя подтверждён каналом, а не атрибутами.
- **MordorNazgulMarshal (`Command_SpecialAbilityScreechDunklerMarschall`)**: тот же пустой носитель (`VictimCount`-скан никого; `victimsAffected() == 0`) и тот же пульс по врагам; враги в радиусе оказываются в запертом временном состоянии (синтетика: `AIState = IDLE` → `AI_BUSY`, locked, RW 0x662FC8) и `allowedToRespondToCommand(CMD_FROM_PLAYER)` отвечает «нет» (RW 0x667174) — то есть «оглушение» в терминах движка: пока нуггет действует, приказы не проходят. Механика рога в порту одна на всё семейство; видимый эффект — данные мода (оглушение — по владельцу 2026-11-18; ритейл-INI локально нет).

Откат правки невозможен (правки движка нет); тесты закрепляют правила бинарника, чтобы будущая регрессия этих путей была красной.

## Что решил владелец (2026-11-18)

1. Кнопки рогов и Элендила — **не баг и не «немая» кнопка**: они делают эффект через парный `ModelConditionSpecialAbilityUpdate` (RW 0x89455B / RW 0x894701; фича — страх/паника у рогов Арагорна и Элендила, **оглушение у скрика Маршала** — владелец 2026-11-18, в игре оглушает врагов; механика одна на семейство, эффект задаёт нуггет мода). `SpecialPowerModule` с пустышкой-списком и закомментированным `AntiCategory` — мёртвый груз данных; решение за автором мода, если нужен дополнительный эффект. Корректировка 2026-11-18: скрику Маршала посвящён отдельный синтетический кейс (тест 5) — запертое временное состояние отказывает в приказах; по-прежнему ни страха, ни баффа в данных мода не требуется, механика порта правки не требует.
2. Матрица: **NO-TARGET принят как вердикт** для Ернтесегена и GeheimeWege (дерево и друэданы в сцены не попадают); причины по каждой кнопке — `docs/ennorath/matrix_explain.d/PORT-278.md`.
3. kdtlachen (AngmarAgandaur) — не относится к `SpecialPowerModule`-пути; падающее звено — `WeaponFireSpecialAbilityUpdate` (свой класс, свой разбор; SpecialPowerModule-clause в строке матрицы отсутствует).
4. PR открывается черновиком сразу (`Closes #645`, `Depends on #622 (Port 251)`); проверяльщик черновики не берёт. Когда #622 окажется в `main`: вмёржить `origin/main`, `gh pr ready`, статус `review`.
5. `cbhunt` (`test_cbhunt.cpp:329-331`, падение в `*SpecialPower*`) — не относится к этой ветке и уже исправлен (тест пересажен на тип `0x1D`; правка придёт в `main` с исправлениями CI).
6. S-7475..S-7477 остались неиспользованными (новых стопов нет: ничего не осталось непортированным).

## Проверки

Прогнаны 2026-11-17/18 на ветке `arena/c10fffa7-openbfme-godot` (сборка `engine/build`, `-j2`; `openbfme_tests`):

- `openbfme_tests -tc="PORT-278*"` — **10/10 passed** (5 `test_port_278.cpp` + 5 `test_port_278_horn.cpp`, 270 assertions).
- `-tc="SPELL-2*"` — 12/12 passed; `-tc="*Banner*"` — 40/40 passed (1020 assertions).
- `-tc="HERO-1*"` — 0 кейсов (имя-маска ничему не соответствует; соседи героев покрыты `*Banner*` и `*SpecialPower*`).
- `-tc="*SpecialPower*"` — 50/51 passed; единственное падение пре-существующее (`cbhunt … RW 0x82D925`), см. выше.

Движок не изменён → `*.hashes` не двигались; `git diff --stat -- engine/src` пуст относительно базы ветки. Ветка на 1 коммит позади `origin/main` (`3d017a4`, apply-путь и `test_cbhunt.cpp` не затронуты), поэтому вердикты переносятся.

# PORT-276: диагностика детонации Saleme и мины

Fork issue 643, dashboard task 255; ветка
`arena/98de06b1-openbfme-godot`. Это **диагностический PR, не исправление
движка**. Штатный полный бинарник прошёл все `PORT-276*`: **3 случая,
418 проверок, 0 failed**, включая четыре Saleme на значениях мода.
**Результат — stop по зелёному data-equivalent baseline**, не новый порт.

## Граница вывода

Первоначальные два синтетических теста уже проходили без runtime-правки
на main `3260086f48221c29c2c02b12299f1af546fefb51`: 2 случая, 77 проверок.
Это исторический результат, а не проверка текущего main и не проверка
данных Saleme. Прежний вывод о работоспособности общего targetless-пути
не распространялся автоматически на четыре mod-оружия.

По уточнению владельца от 2026-11-17 теперь отдельно проверяются **поля и
значения Ennorath 1.9.8.3**. Это назначенный архив для данной сверки;
документы свежего main уже называют 1.9.8.4, но его данные этим fixture не
проверялись. Мина `IsengardDeployedExplosiveMine` с `ORDER` и одиночным PASS
в наблюдении владельца — помеха сцены, не доказанный дефект порта.
Её trigger — `IsegardDetonateMine`; отдельного нового data-equivalent теста
мины эта проверка четырёх Saleme не добавляет. В полученном снимке карт
строка мины ещё `NO-EFFECT`; это различие версий наблюдения, а не основание
менять verdict вручную.

Собственные функции `WeaponFireSpecialAbilityUpdate` принадлежат Local 76
(issue 574) и не изменены. `engine/src`, чужие тесты, `.hashes`, `perf1`,
`uber1`, `save4*`, `.github/` и сгенерированные карты задачей не меняются.
Нового состояния симуляции, `xfer` / `crc` / save нет. Матрица остаётся
красной; `matrix_explain.d/` не добавляется (PROCESS 15.15).

## Источники и полнота данных

Повторно получены и проверены в sandbox:

- `ennorath-ini-1.9.8.3.zip`: MD5 `c53b6dea885cab044f80e783565f41f3`;
- предоставленный RW `game.dat`: MD5 `daed3668006cd90f01c34e5a7da1901f`.

Хэш идентифицирует артефакт, но не заменяет независимую проверку его
происхождения: оговорка PLAN / S-001 остаётся. Чтение RW выполнено через
`tools/mod/rw.py <game.dat> dis <entry> <count>`; оригиналы и полные извлечения
лежат только в `/tmp` и ignored `workspace/`, не в Git.

Прочитаны **полные** определения, включая закомментированное, наследование
и зависимости (пути относительно `data/ini/` архива):

| Источник | Что сверено |
|---|---|
| `object/evilfaction/units/harad/salemetraps.ini:1..395` | Полный базовый `SalemePoisonTrap` и три ChildObject; KindOf, тело, броня, оба варианта смерти, OCL / WeaponFire / AutoAbility / camouflage. Последующие spawn-ping объекты `397..479` тоже прочитаны; это размещение ловушек, не их родитель. |
| `includes/weapon.inc:37573..37687` | Полные `SalemeDetonateFallen` и четыре death-оружия: все активные нагеты, радиусы, типы, фильтры и отсутствие upgrade gates. Закомментированный poison modifier не включён. |
| `_gamedata.inc:1355`, `83`, `1442`, `9649` | Весь `AFFECTED_BY_POISON_OBJECTFILTER`, `WEAK_HERO_HERORESIST=0.80`, `SHOCKWAVE_RESISTANCE_ALWAYS=9999`, `TRANSPORTSLOTCOUNT_UNIT=1`. |
| `armor.ini:3243`, `includes/attributemodifier.inc:11676` | `SalemeTrapsArmor`, полный `BlendfalleDebuff`. |
| `specialpower.ini:3061`, `includes/commandbutton.inc:43731..43769`, `includes/commandset.inc:18678..18688` | Targetless power, три кнопки с AutoAbility и их CommandSet; strong использует poison-кнопку. |
| `includes/objectcreationlist.inc:92`, `object/projectile.ini:24855`, `weapon.ini:21047` | Полная цепочка `OCL_SalemeDetonateRanged` → `SalemeDetonateRangedObj` → `ArrowVolleyPhaseInitialWeapon`: это вспомогательный FX-only выстрел, не вредящий нагет. |

В `engine/tests/test_port_276.cpp`, `kSaleme276Ini`, вручную составлен
синтетический INI с этими полями и значениями. Используется только читатель
движка; собственного парсера нет. Оставлены ChildObject и замена модулей по
ModuleTag, настоящие upgrade / object filters, броня, оба SlowDeath,
нулевой UnpackTime, OCL с helper-объектом, camouflage и StartsActive
AutoAbility с настоящим command set. Полный poison-макрос, включая именные
исключения, присутствует в тесте. Модовые модели, текстуры, звуки и содержимое
FX не копируются: FXList — пустые синтетические ресурсы с теми же именами;
проверка не утверждает паритет изображения или звука. Здоровье / броня
жертв и координаты — явно синтетические условия сцены.

### Поле мода — значение — что делает RW

| Поле мода | Значение в 1.9.8.3 | Что делает RW |
|---|---|---|
| `SpecialAbilityDetonate` | `SPECIAL_GENERAL_TARGETLESS`, ReloadTime `999999999` | Без объекта-цели trigger RW **0x8961F7**, ветка **0x8962D8..0x896313**, выбирает позицию владельца при нулевой сохранённой позиции и вызывает position wrapper 0x6CF3D2. |
| WeaponFire update | `SpecialWeapon=SalemeDetonateFallen`, `WhichSpecialWeapon=1`, `UnpackTime=0`, `MustFinishAbility=Yes`, `AwardXPForTriggering=0` | Это вход в существующую ability / trigger цепочку, а не прямой вызов урона из теста. В RW 0x896288..0x89629D проверяется готовность оружия; 0x8962A3..0x8962B1 записывает ID владельца. |
| Trigger weapon / timing | LeechRangeWeapon Yes, AttackRange `9999`, MeleeWeapon No; DelayBetweenShots / PreAttackDelay / FiringDuration `0`, PreAttackType PER_SHOT | Wrapper RW 0x6CF3D2 передаёт ignoreRanges и позицию; нет требования наличия противника. |
| `DamageDealtAtSelfPosition` | `Yes` | RW 0x6CC915, **0x6CCBF9..0x6CCC10**, подставляет позицию стреляющего и очищает объект-цель; **0x6CCEFA..0x6CCF17** вызывает позиционный slot нагета. |
| Trigger `RadiusDamageAffects` | **ALLIES SELF SUICIDE**, без ENEMIES | RW 0x90D77C проверяет self / отношения, а не обещает этим оружием вражеский урон. RW **0x90E28C**, **0x90E603..0x90E61A**, при source ID == victim ID и бите SUICIDE `0x10` устанавливает kill. |
| Trigger DamageNugget | Damage **1**, Radius **20**, DelayTime `0`, DamageType **SLASH**, FX SWORD_SLASH, DeathType **BURNED** | Это принудительный self-kill, не 1 HP урона врагу. RW 0x90E641..0x90E647 сохраняет тип смерти, по которому выбирается SlowDeath. |
| Trigger filters / upgrades | `SpecialObjectFilter`, `RequiredUpgradeNames`, `ForbiddenUpgradeNames` **отсутствуют** | Нет скрытого upgrade / object-filter gate в этом блоке. Сам механизм проверки фильтров остаётся в RW 0x90D77C; отсутствие поля не заменено выдуманным upgrade в тесте. |
| Общие KindOf / body | PRELOAD SELECTABLE CAN_CAST_REFLECTIONS SCORE **IMMOBILE NOT_AUTOACQUIRABLE MINE DEPLOYED_MINE**; ActiveBody MaxHealth **150**, MaxHealthDamaged **75** | SUICIDE в RW 0x90E603..0x90E61A форсирует убийство источника независимо от того, хватило ли обычной единицы SLASH на 150 HP. Trap не должен иметь CAN_ATTACK или normal WeaponSet для временного death-оружия. |
| `SalemeTrapsArmor` | DEFAULT `100%`, PIERCE `5%`, LOGICAL_FIRE / FLAME `50000%`, FROST `100%`, POISON `0%`, STRUCTURAL `10%` | Вражеский payload — отдельное оружие; иммунитет **самой ловушки** к POISON не делает пехоту иммунной. Self-kill всё равно задаётся SUICIDE. Эти коэффициенты сохранены в fixture. |
| BURNED SlowDeath poison / strong / blind | INITIAL `SalemeExplodeGiftfalleWeapon` / `...Strong` / `SalemeExplodeBlendfalleWeapon`; destruction **1500 / 1500 / 1000 ms**, DEATH_1 | RW 0x860E93, **0x8612CD..0x8612D1**, запускает INITIAL. RW **0x8609A8**, **0x860A7D..0x860A8C**, стреляет временным оружием в позицию ещё существующего умирающего владельца. |
| BURNED SlowDeath explosive | **FINAL** `SalemeExplodeExplosionsfalleWeapon`, DestructionDelay **1 ms**, DEATH_1 | RW **0x860B39**, **0x860E50..0x860E61**, сначала вызывает FINAL (phase 2 через 0x8609A8), затем destroy 0x62BBAB. Самоудаление не отменяет взрыв. |
| Не-BURNED смерть | `ALL -BURNED`, DestructionDelay **5000 ms**, **без Weapon**; explosive даёт DEATH_1 и другой sink / FX | Нельзя наблюдать обычное уничтожение ловушки и требовать тот же payload: нужный путь выбирается death-mask в RW 0x861712. В тесте BURNED приходит от действительного igniter, не вручную. |
| Poison DOTNugget | Damage **10 / 25**, AcceptDamageAdd **No**, Radius **80**, DelayTime `0`, POISON / POISON / NORMAL, interval **1000 ms**, duration **15000 ms** | RW **0x911407**, **0x911444..0x911477**, заполняет и регистрирует DOT-запись; **0x91147C..0x911485** наносит initial hit. RW **0x820EF0** затем тикает сохранённой записью, даже когда trap уже удалена. |
| Poison `SpecialObjectFilter` | Весь `AFFECTED_BY_POISON_OBJECTFILTER`: ALL минус MACHINE / STRUCTURE / SHIP и именной список, в том числе **BarrowWight** | RW 0x90D77C применяет нагетовый фильтр после relationship gate. В тесте отдельно представлены разрешённая пехота, исключённые KindOf и именное исключение, а не только «любой враг». |
| Death weapons `RadiusDamageAffects` | **ENEMIES NEUTRALS SELF** | Это уже вражеский payload; соседний союзник не является его целью. SELF не означает «только self». |
| Blind EmotionWeaponNugget | UNCONTROLLABLE_FEAR, Radius **50**, Duration **10 s**, filter `ANY +INFANTRY +CAVALRY +MONSTER -HERO -STRUCTURE ENEMIES` | RW **0x911707**, **0x911757..0x911764**, передаёт тип / длительность в 0x68F3BF → 0x68F3CB → 0x8B4EB1: **forced emotion**, не HP damage и не обычный requested-флаг. |
| Blind AttributeModifierNugget | BlendfalleDebuff, Radius **50**, FX SWORD_SLASH, **тот же filter** | RW **0x90EAF9** применяет ModifierList к прошедшей фильтр цели. HERO / STRUCTURE не надо использовать как единственное доказательство эффекта. |
| `BlendfalleDebuff` | Category DEBUFF; ARMOR **−20%**; SHROUD_CLEARING / VISION / RANGE **−100%**; Duration **10000 ms** | Это модификаторы, не ordinary damage. Тест наблюдает сам список, а не угадывает по потере здоровья. Значения и длительность загружены читателем движка. |
| Explosive DamageNugget | Damage **500**, Radius **80**, FLAME / FLAME / BURNED, DelayTime `0`, DamageScalar **5000% NONE +MINE** | RW 0x90E28C заполняет ordinary FLAME damage и скаляры цели; для синтетической пехоты без MINE и с 100%-бронёй — 500. Нельзя сверять это с 1 SLASH от trigger. |
| Остальные explosive nuggets | MetaImpact: HeroResist **0.80**, amount **60**, radius **80**, taper **0.75**, ZMult **1.20**; FireLogic INCREASE_BURN_RATE, radius **10**, damage **5** | Это отдельные нагеты общего dispatch 0x6CC915, а не дополнительные 5 HP обычного урона пехоте. В fixture сохранены; физика отбрасывания / огонь / FX не являются предметом утверждения этого теста. |
| Upgrade gates death-оружия | `RequiredUpgradeNames` / `ForbiddenUpgradeNames` **нет ни у одного активного нагета** | Не требуется выдавать модовую технологию, чтобы тест имел право ждать перечисленные эффекты. Пустота обоих списков проверяется после штатного чтения INI. |
| OCL power / helper | OCL_SalemeDetonateRanged, CREATE_AT_LOCATION; helper INERT IMMOBILE UNATTACKABLE NO_COLLIDE, ImmortalBody `1`, Deletion `4000 ms`, one-shot ArrowVolleyPhaseInitialWeapon | OCL-ветка RW 0x8C73EF отдельна от WeaponFire. Helper-оружие содержит **FireFX, но не damage nugget**; оно не подменяет SlowDeath payload. |

Прочие сохранённые значения базового объекта: VisionRange 100, транспортный
слот 1, Physics GravityMult 1, CrushableLevel 2, ShockwaveResistance 9999;
camouflage StartsActive, forbidden TAKING_DAMAGE / USING_ABILITY, detection
50, period 1000 ms; AutoAbility StartsActive, scan 80, Query **5** enemy
INFANTRY / CAVALRY с полным набором исключений исходного блока. В тесте
настоящая auto-кнопка действительно найдена, но подходящих врагов четыре:
автокаст не должен опередить единственный явный trigger. Это не доказательство
всех веток AutoAbility / camouflage, а исключение отсутствующей зависимости
из сравнения данных Saleme.

## Что делает RotWK: цепочка, а не один адрес

Ключевой путь: **0x8961F7 → 0x90E28C → 0x8609A8 / 0x860B39**.

| Entry, первая инструкция | Прочитанная ветка | Существующий порт |
|---|---|---|
| 0x8961F7, `push ebp` | 0x8962D8..0x896313: при отсутствии цели выбирается позиция владельца; call 0x6CF3D2 | `WeaponFireSpecialAbilityUpdate::triggerAbilityEffect`; не изменён. |
| 0x6CF3D2, `push ebp` | 0x6CF3D8..0x6CF3F0: null victim, позиция, ignoreRanges → 0x6CEF6D | `ObjectWeapons::fireExtraWeaponAt`. |
| 0x6CEF6D → 0x6CC915 | Ordinary position dispatch 0x6CF259..0x6CF27B; source position 0x6CCBF9..0x6CCC10; position slot нагета 0x6CCEFA..0x6CCF17 | `Weapon::privateFireWeapon`, `ObjectWeaponDelivery::fireWeaponTemplate`. Эти участки также читались в первоначальном разборе, не переносились задачей. |
| 0x90E28C, `push ebp` | 0x90E603..0x90E61A: source==victim + SUICIDE → kill; 0x90E641..0x90E647 сохраняет DeathType | `FillDamageInfo` в `Object/Damage.cpp`. |
| 0x90D77C, `push ebp` | 0x90D7CA..0x90D7F6: SELF; relationship и SpecialObjectFilter далее в этой функции; dying source не равен отсутствующему source | `shouldDeliver` в `Combat/WeaponDelivery.cpp`. |
| 0x8609A8, `mov eax, 0xB9EBFB` | 0x860A7D..0x860A8C: временный weapon с dying owner и его позицией → 0x6CF530 | `SlowDeathBehavior::doPhase`, `ObjectWeapons::createAndFireTempWeapon`. |
| 0x860B39, `push ebp` | 0x860E50 `push 2`; 0x860E55 call 0x8609A8; **только потом** 0x860E61 call destroy 0x62BBAB | `SlowDeathBehavior::update`: FINAL до удаления владельца. |
| 0x911407, `push ebp` | 0x911477 register DOT → 0x911485 initial damage | Существующий DOT delivery и `DOTManager`. |
| 0x820EF0, `mov eax, 0xB9AE31` | Tick вызывает attemptDamage с сохранённой записью, не новый выстрел источника | Существующий `DOTManager::update`. |

Ключевые entry повторно дизассемблированы при этом продолжении; новые порты
по ним не заявляются. BFME2 — только **исторический donor cross-check**,
не целевой oracle: Open-BFME-2 `7ff7192e924ce57a0d4e4c9a518151c247200c5a`,
DONOR BFME2 0x00492A72 (`WeaponFireSpecialAbilityUpdate.cpp`), 0x002CE76F /
0x002CE30A (weapon wrappers / dispatch), 0x00507D84 (`DamageNugget.cpp`),
0x0045D97A / 0x0045DB0B (`SlowDeathBehaviorUpdate.cpp`). Донор при этом
продолжении не получен заново; fresh branch evidence выше — из RW, в том
числе SUICIDE, где донор отличается.

## Проверка штатной сборкой

Первый прогон: main `278cd6c55a636031b6b40109aaa66e2ed7f3810b`, HEAD
`e4618c2c8ec30c5573c753110c7c267203252b31` плюс новый fixture в рабочем
дереве. Полная обычная Release / no-Godot / `-j1` цель завершила
**1892/1892, exit 0**; после окончания fixture — та же нормальная цель
(7 шагов, включая `test_port_276.cpp` и relink) и **3/418, exit 0**,
7168 остальных случаев исключены фильтром.

После первого fresh-main rebase: main
`a05445324550664e578f528abf1463363f25f097`, HEAD
`e38c646e06b5ae5ef28c69fa0d3bc45cb91b0bc1`; та же обычная цель
**469/469, exit 0**, затем **3/418, exit 0**, 7183 исключены фильтром.
В обоих прогонах один C++ blob
**`51097b0f1a01804c0b1452e01521d9ea7c4b32aa`**; runtime задачей не менялся.

Повторный fetch перед оформлением PR получил main
`c47a498402707d4c7cc63208e1d2b51a0c835f86`; выполнен rebase, HEAD
`ab81d8fcb3e745a04d7c459edaa8ca0a7a5163e6`, тот же C++ blob. Для этой базы
повторно выполнены configure, обычная полная цель **1211/1211, exit 0**,
затем весь назначенный фильтр: **3/418, exit 0**, 7200 остальных случаев
исключены фильтром. Это последняя проверенная кодовая база; последующие
изменения отчёта / night-watch не меняют код, INI fixture или ожидания.
Это не отдельный облегчённый runner:

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENBFME_BUILD_GODOT=OFF
cmake --build engine/build -j1 --target openbfme_tests
engine/build/openbfme_tests '-tc=PORT-276*' --no-colors
```

Фактический вывод последней проверки (main c47a4984, doctest 2.4.12):

```text
[doctest] test cases:   3 |   3 passed | 0 failed | 7200 skipped
[doctest] assertions: 418 | 418 passed | 0 failed |
[doctest] Status: SUCCESS!
PORT276_FINAL_BASE_EXIT=0
```

`7200 skipped` — остальные случаи полного бинарника, исключённые фильтром.
Внутри трёх `PORT-276*` нет skip и нет требования установки игры / мода.
Новый тест уже зелёный **без новой runtime-правки**; fix-dependent red→green
не получен и не имитировался.

В штатную цель включены:

1. Сохранённый первый тест: targetless OCL power с ненулевым unpack;
   BURNED self-kill запускает INITIAL и FINAL, ровно 37 damage врагу
   200→163, дальний враг и союзник целы; caster удалён, lastDamager — его ID.
2. Сохранённый соседний тест: без SUICIDE остаётся обычная рана 120→117,
   враг внутри radius igniter не повреждён, выстрел один, смерти нет.
3. Новый тест значений Saleme: цикл по четырём наследуемым traps с одним
   обычным power trigger. Проверяются initial damage **10 / 25 / 0 / 500**,
   повторные poison ticks после удаления trap, forced fear и модификатор
   blind без HP damage; отдельно дальняя цель, союзник, MACHINE / STRUCTURE,
   HERO и именное poison-исключение BarrowWight. Пустые upgrade gates
   проверяются у всех нагетов. Для врагов дана 100%-броня и 10000 HP; источник
   по-прежнему имеет **настоящие 150 HP и SalemeTrapsArmor**.

Полный набор тестов, настоящая установка игры / мода и five-button sweep
не запускались. `test_select.py --base origin/main` сообщает **FULL** из-за
нового подключения `engine/CMakeLists.txt` (нет test map); это не обойдённый
успешный полный прогон. Для разрешённого диагностического PR выполняется
назначенный `PORT-276*` на **полном бинарнике**; до land остаются правила
полного прогона у владельца. CMake предупредил об отсутствующих `lame` /
`mpg123` и MP3 fixtures; пять audio-тестов без них не проверены. CI вручную
не запускалось, чужие ожидания не переснимались.

## Предложение владельцу сцены матрицы

Это конкретные места исходника, а **не выполненная правка сцены и не
повторный E2E свип**:

1. `SweepEffectProbe.h`, `EffectProbe::inspectWeaponSpecial` / `expectedHits`:
   строить ожидания **death payload**, выбранного BURNED self-kill, а не
   требовать вражеский damage от allies-only `SalemeDetonateFallen`.
   Проверять actual `SlowDeathBehavior` и INITIAL / FINAL; не считать
   вспомогательный OCL FX-only объект боевым payload. Для blind наблюдать
   forced UNCONTROLLABLE_FEAR / BlendfalleDebuff, для poison — DOT и его
   именной / KindOf filter. Существующий probe умеет projectile warhead /
   OCL и разные типы nuggets, но в прочитанной weapon-ветке не проходит
   self-kill → SlowDeath как источник ожидаемого эффекта.
2. `test_hud_factb_units.cpp`, `refreshTargets` / `closerEnemy`: обычная цель
   стоит на `(+160,+40)`; приближение targetless делается для TERROR /
   EMOTION / LUA_EVENT / META_IMPACT, **не DAMAGE**. Даже применяемое там
   `(+80,+20)` дальше blind-radius 50. Разместить отдельную незамороженную
   enemy INFANTRY с hold fire на **(+30,0)** от trap, вторую — на **(+150,0)**
   как контроль; не использовать единственную HERO / STRUCTURE для blind
   или MACHINE / poison-immune имя для poison. Проверять членов horde,
   а не её ImmortalBody. Число врагов учитывать относительно AutoAbility
   Query=5, чтобы явный клик не смешался с более ранним автокастом.
3. Та же сцена, `holdersAlive` / `replaced`: отсутствие caster после
   выстрела сейчас уводит в поиск replacement и при его отсутствии в fail
   / continue **до** обычного сравнения эффекта. Для подтверждённой
   self-detonation дать закончить death-фазе, сравнить эффекты по заранее
   сохранённым ID жертв / caster, а не требовать замену погибшей trap.
   Не разыменовывать старый указатель caster. HitTracker уже может
   атрибутировать damage сохранённому ID; отсутствие живого источника
   не должно обнулять DOT-наблюдение.
4. Одиночный свип каждой из пяти кнопок на свежих целях, затем полная
   сцена — отдельная проверка этих изменений. Нельзя автоматически
   перекрасить матрицу или добавить `matrix_explain.d/` по одному этому
   разбору. Mine ORDER / isolated PASS не требует runtime-порта.

**INFERENCE:** эти различия могут объяснить сохраняющиеся Saleme
NO-EFFECT. Чтение сцены показывает конкретные недостатки её условий /
ожиданий, но причинность именно текущего полного прогона не воспроизведена
здесь. Зелёный data-equivalent тест исключает показанный набор полей как
воспроизведение дефекта targetless fire; он не объявляет пять живых кнопок
проверенными владельцем.

## Соседнее непортированное поведение

**S-7469 сужен до настоящего пропущенного вызова**, а не права собственности:
entry RW 0x8961F7, **0x89620C** загружает Object+0x248, **0x896217** пропускает
null, **0x896219..0x89621B** вызывает 0x8E302A с аргументом 1 перед trigger.
Entry 0x8E302A, ветка **0x8E3091..0x8E30B9**, сбрасывает continuous-fire bits
и накопители. В портовой trigger-функции вызова нет; это подтверждено новым
`rw.py dis` обеих функций и сравнением исходника, а не только старой
аннотацией. Условие PROCESS 5.7(а) выполнено, **(б) не выполнено**: сама
функция явно запрещена для PORT-276 и закреплена за Local 76. Реализация и
её тест не добавлялись. Это не доказанная причина Saleme NO-EFFECT.

Общий порядок жертв уже имеет S-321; tracker model — S-184; передача
SPECIAL_WEAPON членам horde — S-859. Они не закрыты и не дублируются
новыми scope-only стопами. **S-7470 удалён**: отсутствие новой fix-dependent
регрессии / upstream deliverable само по себе не непортированное поведение
с адресом RW. S-7471 не использован.

## Проверить владельцу в оригинале и порте

Одной партией через Cheat Mode (`docs/CHEAT_MODE.md`):

1. Схватка Ennorath, приостановить ИИ противника. Для изолированного клика
   дать ему **одного GondorFighter**, не horde: Query=5 иначе допускает
   автоподрыв ещё до нажатия. Hold fire, для blind **без freeze**. Вторую
   цель оставить дальше 150; саму trap не freeze — DISABLED_HELD блокирует
   ability. Не смешивать остаточный poison разных проверок. Затем можно
   повторить на GondorFighterHorde, отдельно записав, был ли автоподрыв до клика.
2. Своей `SalemePoisonTrap` / `Command_SalemeGiftfalleDetonate`: ready,
   цель в пределах 30, нажать один раз — BURNED self-kill, enemy poison
   после INITIAL; проверить несколько ticks после исчезновения trap.
3. Повторить `SalemePoisonTrapStrong` с той же кнопкой и свежей целью —
   более сильный poison, дальний контроль цел.
4. `SalemeBlindTrap` / `Command_SalemeBlendfalleDetonate`: не требовать HP
   damage; проверить fear / blind debuff на обычной infantry, не HERO.
5. `SalemeExplodeTrap` / `Command_SalemeExplosionsfalleDetonate`: FINAL
   blast ранит ближних врагов, trap исчезает, дальний контроль цел.
6. `IsengardDeployedExplosiveMine` / `Command_IsengardMineDetonate`:
   дождаться подготовки, взрыв FINAL; сверить одиночную сцену и сцену
   вместе с остальными объектами, не объявляя ORDER дефектом движка.

Это предлагаемые наблюдения по данным / RW, **не проведённая ручная
проверка**. Для видимого вердикта по PROCESS 15.15 нужен ответ владельца
из оригинала. Новой видимой, местной или общей runtime-правки нет.

## Карты, ожидания, upstream, «Осталось»

| Категория | Изменение PORT-276 |
|---|---|
| Runtime / INI readers / RW functions | Нет новых и нет изменений; тесты загружают значения через существующий читатель. |
| Тесты | Два исходных диагностических кейса сохранены; добавлена проверка значений четырёх Saleme. Свежий штатный бинарник: **3/3, 418/418, 0 failed, exit 0**. |
| Чужие ожидания, pins, hashes | Не менялись. Red→green от новой runtime-правки отсутствует и не имитируется. |
| MATRIX / MATRIX_GAPS / RW_FUNCTION_MAP / FIELD_GAPS | Не менялись; собственного свипа и переобозначения кнопок нет. |
| Стопы / журнал | S-7469 — только RW-подтверждённый пропущенный вызов; S-7470 удалён как gate bookkeeping, не поведение. Собственный night-watch содержит фактический результат проверки. |

Upstream PR и ветка `up/port-276` **не готовятся**: код нового поведения
RotWK не изменён, диагностика / документы без fix-dependent red→green
не являются upstream-кандидатом. Ready-строка PR_CANDIDATES и английский
upstream-коммит не создаются; разрешён именно fork PR с диагностикой.

**Осталось:** владельцу сцены проверить приведённое конкретное предложение;
разрешённый диагностический fork PR передаёт результаты, не исправление.
Общий targetless путь и значения Saleme не дали воспроизведения дефекта,
статус **stop**, не fix / новый порт. Полный suite, установка мода, пять
живых кнопок, оригинальные FX / звук и причинность полного свипа ещё не
проверены. Непроверенное не выдано за исправление.

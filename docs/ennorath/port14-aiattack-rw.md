# PORT-14: AIAttackState / victim block — разбор RotWK 2.01 (issue #70)

Бинарник: `workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f` (оговорка S-001: образ сообщества).
Все адреса, ветки, непосредственные вызовы, указатели vtable и строки ниже прочитаны
`tools/mod/rw.py ... dis|xref|str` по этому файлу. Имена, взятые из декомпиляции BFME2 или из
порта, помечены `INFERENCE`; адрес инструкции и смещение поля при этом доказаны RW.
Значений INI мода, байт мода и игровых архивов в этом файле нет.

## 1. Что за функция и что возвращает

`AIAttackState::update` — RW **0x7510EC .. 0x7515E1**. Это слот 6 vtable **0xC2A1D8**
(`0xC2A1D8 + 0x18 = 0x7510EC`); STOPS S-583 называет тот же адрес. Конструктор
RW **0x74CABC** ставит `0xC2A1D8` в `this + 0` (RW 0x74CAEA); деструктор
RW 0x74CB56 возвращает её же (RW 0x74CB5D). Соседние слоты: `+0x10` = onEnter
(RW 0x74CED6), `+0x14` = onExit (RW 0x74D25D), `+0x18` = update (RW 0x7510EC),
`+0x20` = RW 0x742DD0, `+0x24` = isActive (RW 0x740ECD), `+0x3C` = RW 0x74CB33.

Регистры в update: `edi = this` (AIAttackState), `ecx = [edi+0x18]` (главная
AIStateMachine), `esi = [ecx+0x14]` (Object-владелец), `ebx` и `[ebp-8]` — текущий
goal object главной машины, `[ebp-0xC]` — AI владельца (`[esi+0x260]`),
`[ebp-0x10]` — `lastCommandSource`, `[ebp-1]` — owner `testStatus(0x1C IS_MELEE_ATTACKING)`,
`[ebp-2]` — `relationship == ENEMIES`.

Возвраты (порт `engine/src/GameLogic/StateReturnType.h`: CONTINUE = 0,
SUCCESS = -1, FAILURE = -2):

| RW | eax | смысл |
|---|---|---|
| 0x751227: `push -2; pop eax` → 0x75122A | -2 | STATE_FAILURE |
| 0x751456: `or eax, -1` → 0x75122A | -1 | STATE_SUCCESS |
| 0x7515DA: `xor eax, eax` → 0x75122A | 0 | STATE_CONTINUE |
| 0x75122A | eax | общий эпилог: `pop edi/esi/ebx; leave; ret` |
| 0x7515D4: `jle 0x75122A` | <= 0 | результат внутренней машины без замены на CONTINUE |

Граница функции: RW 0x7515E1 уже начинает другую функцию (`push ecx; push ecx`).

## 2. Поля AIAttackState, прочитанные в этом разборе

| смещение | RW | что это |
|---|---|---|
| `+0x18` | 0x7510F7 | главная AIStateMachine; её `+0x14` — owner, `+0x20` — object id цели |
| `+0x24` | 0x7513BC, 0x7515B3, 0x7515C8 | внутренняя attack machine (Pursue/Approach/Aim/Fire/Wait) |
| `+0x28` | 0x7511DC | `AttackExitConditions*`; vslot 0 = `shouldExit(machine)` |
| `+0x2C` | 0x74D08C .. 0x74D092, 0x75132B | **Team\*** команды жертвы на входе в атаку. Порт хранит `ObjectID m_victimTeam`; это не тот же тип |
| `+0x30` | 0x74D0A0 .. 0x74D0A8 | `m_originalVictimPos` (Coord3D) |
| `+0x3C` | 0x751411 | `m_lockedWeapon` / `AsciiString`; 0x401E64 проверяет «пусто» |
| `+0x40` | 0x74CAFA | `m_follow` (аргумент конструктора `+0xC`) |
| `+0x41` | 0x74CB00, 0x751263 | `m_attackingObject` (аргумент `+0x10`); victim block работает только при 1 |
| `+0x42` | 0x74CB0F | `m_forceAttacking` (аргумент `+0x14`) |
| `+0x44` | 0x74D086 | object id жертвы на входе |
| `+0x48` | 0x74CF69 .. 0x74CF75, 0x7514A2 | байт `MeleeWeapon` текущего оружия на входе (RW 0x441B59) |
| `+0x49` | см. раздел 6 | `m_49` = «жертва была ENEMIES на входе в атаку» |
| `+0x4C` | 0x74CB12 | счётчик/тип машины, стартует с 3 |
| `+0x20` | 0x74CAF0 | второй vtable `0xC2A1C4` (NotifyWeaponFiredInterface) |

Статусы и KindOf из движковых таблиц (`engine/src/GameLogic/BitFlagNames.cpp`,
`CombatNames.h`): `0x01` CAN_ATTACK, `0x16` IS_ATTACKING, `0x1C` IS_MELEE_ATTACKING,
`0x26` HORDE_MEMBER, `0x32` FLEE_OFF_MAP, `0x3C` UNATTACKABLE; model condition
`0x81` = STONED (это `Object+0x10C`, а не status `Object+0x94`); KindOf 25 =
PROJECTILE, 94 = TREE, 109 = HORDE, 151 = ARMY_OF_DEAD, 7 = STRUCTURE.
`[Object+0x458] & 1` — DEAD.
`[Object+0x110]` биты `0x20/0x40/0x80` — model conditions 37/38/39:
ATTACKING, ATTACKING_STRUCTURE, ATTACKING_POSITION.

## 3. Полная карта ветвей и вызовов

### 3.1. Прелюдия RW 0x7510EC .. 0x7511DC (не переносится в PORT-14)

| RW | ветка / вызов |
|---|---|
| 0x7510F7 .. 0x751106 | `0x8DBACE` = `StateMachine::getGoalObject` (резолвит `[machine+0x20]` через 0x449681). Нет owner → 0x7511DC |
| 0x75111E .. 0x751129 | `0x68B678` = `Object::getControllingPlayer` (RW 0x68B678 → 0x79FD6F); игрок типа 0 (`[player+0x5C] == 0`) пропускает всю прелюдию |
| 0x75112F .. 0x751147 | есть goal → `&goal->position` (`goal+0x38`); нет goal → `0x664CB3` = `AIUpdateInterface::getAttackPosition` (RW 0x664CB3: только status 0x16 IS_ATTACKING и нет currentVictim, возвращает `AI machine + 0x24`) |
| 0x75114D .. 0x7511D2 | квадрат расстояния owner↔цель; `0x68E43B` = vision range; `max(vision, 600.0)` (RW 0xBF24F8 = 600.0); если `dist² <= max²` → `0x742DFC` |
| 0x742DFC | **INFERENCE**: сброс временного speed cap владельца. Тело: 0x715776 читает `[owner+0x1A8]` (группа), 0x76FD04 = `AIGroup::clearTemporarySpeedCaps`; без группы 0x68B31D = `curLocomotor`, затем `[locomotor+0x2C] = -1.0` (RW 0xBD19DC) |

### 3.2. Выходы до victim block: RW 0x7511DC .. 0x751263

| RW | ветка / вызов |
|---|---|
| 0x7511DC .. 0x7511EF | если `[edi+0x28]` есть, vslot 0 `AttackExitConditions::shouldExit(machine)` (RW 0x7511EB); true → 0x751456 = STATE_SUCCESS |
| 0x7511F5 .. 0x751203 | `0x46E918(0x81)` = `Object::testModelCondition(STONED)` (Object `+0x10C`); true → 0x751227 = STATE_FAILURE |
| 0x751205 .. 0x751224 | `0x44DDEC(0x3C)` = `Object::testStatus(UNATTACKABLE)` (Object `+0x94`); true → главная машина vslot `+0x38` `StateMachine::setGoalObject(nullptr)` (RW 0x741689 → 0x8DB8CB), затем 0x751227 = STATE_FAILURE |
| 0x75122F .. 0x751246 | AI vslot `+0x260` = `AIUpdateInterface::rva002697FC` (RW 0x668D8E; **INFERENCE**: имя из декомпиляции BFME2). true → 0x7515DA = STATE_CONTINUE (мood-ретarget уже сменил goal) |
| 0x75124C .. 0x751261 | `0x68B4A4` = `Object::isOutOfAmmo` (thunk: `add ecx,0x34C; jmp 0x6C8115`, 6 слотов оружия, 0x6CDCE7). Если ammo есть → 0x751263; если нет и template `+0x10B & 2` (KindOf PROJECTILE) сброшен → 0x751227 = STATE_FAILURE, иначе 0x751263 |

### 3.3. Victim block RW 0x751263 .. 0x7513D1

`0x751263 cmp byte [edi+0x41],0; je 0x7513D1` — атака позиции/area пропускает весь блок.

| RW | ветка / вызов |
|---|---|
| 0x75126D .. 0x75128E | victim = `[ebp-8]`: null, `[victim+0x458] & 1` (DEAD), или `0x44DDEC(0x32)` FLEE_OFF_MAP → **0x751438** |
| 0x751294 .. 0x7512A8 | `ebx = [esi+0x260]` AI владельца; AI vslot `+0x23C` = `getLastCommandSource` (RW 0x670313: `mov eax,[ecx+0x48]; ret`). Бит 2 = `CMD_FROM_AI` |
| 0x7512A4 .. 0x7512B8 | `0x44DDEC(0x1C)` owner IS_MELEE_ATTACKING → `[ebp-1]`; `0x68D7AB(victim)` = `Object::getRelationship` (RW 0x68D7AB → `Team::getRelationship` 0x7A3DAD) |
| 0x7512BD .. 0x7512C6 | `neg/sbb/inc` → `[ebp-2] = (relationship == ENEMIES)`; ENEMIES = 0, NEUTRAL = 1, ALLIES = 2. true → 0x751316 |
| 0x7512C8 .. 0x7512E7 | особый путь только при **не-ENEMIES**, `m_49 != 0`, `(lastCommandSource & 2) != 0`, `!owner IS_MELEE_ATTACKING` (`[ebp-1] == 0`), и у victim template `+0x113 & 0x40` (KindOf TREE) сброшен |
| 0x7512E9 .. 0x751308 | `0x6627F0(0)` = `AIUpdateInterface::friend_setGoalObject(nullptr)` (RW 0x6627F0: чинит флаг `machine+0x38`, зовёт machine vslot `+0x38`); `0x7A3E36` = `Team::getTeamTargetObject`; если victim == team target → `0x79FDEA(0)` = `Team::setTeamTargetObject(nullptr)` |
| 0x75130D .. 0x7513B7 | AI vslot `+0x240` = `notifyVictimIsDead` (RW 0x63F3BF = `ret`), затем 0x751227 = STATE_FAILURE |
| 0x751316 .. 0x751320 | normal path: `0x6682B1(victim)` = `AIUpdateInterface::setCurrentVictim` (RW 0x6682B1) |
| 0x751325 .. 0x75132E | victim Team\* `[victim+0x31C]` против `[edi+0x2C]`; равны → 0x7513BC |
| 0x751334 .. 0x75136C | если команда сменилась и AI есть: victim `testStatus(0x01 CAN_ATTACK)` установлен → сразу 0x75137B; иначе contain `[victim+0x258]` есть, contain vslot `+0x10` `isGarrisonable` (RW 0x8BD372 = true / 0x9188EB = false), contain vslot `+0x114(0)` `getContainCount` (RW 0x865CE3) == 0 → 0x75136E, остальные случаи → 0x75137B |
| 0x75136E .. 0x751385 | `0x68D7AB(victim)` снова: relationship == NEUTRAL(1) → 0x751387; relationship != ENEMIES(0) → 0x751387; иначе (== ENEMIES) → 0x7513BC |
| 0x751387 .. 0x7513B7 | failure: `0x6627F0(0)` чистит AI goal; `0x7A3E36` team target; если victim был им — `0x79FDEA(0)`; AI vslot `+0x240` `notifyVictimIsDead`; 0x751227 = STATE_FAILURE |
| 0x7513BC .. 0x7513CE | `0x8DBACE` = inner attack machine `getGoalObject`; если != victim → inner machine vslot `+0x38` `setGoalObject(victim)` (RW 0x741689 → 0x8DB8CB) |
| 0x7513D1 | переход к хвосту (и сюда же прыгает 0x751267, когда `+0x41 == 0`) |

### 3.4. Хвост RW 0x7513D1 .. 0x751488

| RW | ветка / вызов |
|---|---|
| 0x7513D1 .. 0x7513F3 | если owner containedBy `[esi+0x27C]` и у контейнера contain `[+0x258]` есть: contain vslot `+0xC0` = `OpenContain::isPassengerShotBlocked(owner, victim)` (RW 0x865B33); true → 0x751227 = STATE_FAILURE |
| 0x7513F9 .. 0x751402 | `0x742D63` = `AIAttackState::chooseWeapon` (RW 0x742D63 .. 0x742DAD; **INFERENCE**: имя из BFME2). Внутри: 0x8DBACE goal, AI vslot `+0x23C` lastCommandSource, `0x68B619` = `ObjectWeapons::chooseBestWeaponForTarget(goal, criteria=5, source)` (RW 0x68B619 → 0x6C8A4E), `0x68E197` = `ObjectWeapons::updateWeaponStatusConditions`. false → 0x751227 = STATE_FAILURE |
| 0x751408 .. 0x751414 | `0x68B58C(0)` = `Object::getCurrentWeapon(0)` (RW 0x68B58C) → `ebx` Weapon\* |
| 0x751411 .. 0x75141D | `0x401E64` = `AsciiString::isEmpty` на `[edi+0x3C]`; пусто → 0x751488 |
| 0x75141F .. 0x751463 | непустое имя: `ebx == 0` → 0x751227 = STATE_FAILURE; имя weapon template = `[weapon+4]+8` либо пустая строка RW 0xBD0C3F; `0x406585` = `AsciiString::compare` с `[edi+0x3C]`; равно → 0x751488 |
| 0x751470 .. 0x751482 | иначе `0x406585` сравнивает имя template владельца `[ownerTemplate+0x64]` со строкой RW 0xC2B138 `GondorTrebuchet`; не равно → 0x751227 = STATE_FAILURE, равно → 0x751488 |
| 0x751488 .. 0x751494 | `ebx == 0` или `[weapon+0x34] <= 0` (maxShotCount) → 0x751227 = STATE_FAILURE |

### 3.5. Хвост RW 0x751488 .. 0x7515DA

| RW | ветка / вызов |
|---|---|
| 0x75149A .. 0x7514A5 | `0x441B59` = `WeaponTemplate::isMeleeWeapon` (byte `+0x125`); сравнение с `[edi+0x48]` |
| 0x7514A7 .. 0x7514DE | если differs: state vslot `+0x14` = `AIAttackState::onExit(EXIT_NORMAL)` (RW 0x74D25D, arg 0); `0x6682B1(victim)` setCurrentVictim; `0x6627F0(victim)` friend_setGoalObject; главная машина vslot `+0x38` setGoalObject(victim); state vslot `+0x10` = `AIAttackState::onEnter` (RW 0x74CED6); вернуть результат onEnter через 0x75122A |
| 0x7514E3 .. 0x7514F7 | id текущего состояния внутренней машины; `0xE4` (228, **INFERENCE**: BFME2 WaitUntilFinishedFiring) или `0xF423F` при пустой машине |
| 0x7514FE .. 0x75159E | при id 0xE4 очистить биты 0x20/0x40/0x80 `[esi+0x110]`; иначе выставить ATTACKING (0x20), ATTACKING_STRUCTURE (0x40, если victim STRUCTURE), ATTACKING_POSITION (0x80, если `!m_attackingObject`). Каждое изменение зовёт `0x68B53C` = `Object::notifyModelConditionChanged` (RW 0x68B53C → drawable 0x679512 и AI vslot `+0x264` wakeUpNow 0x662552) |
| 0x75159E .. 0x7515B1 | `runState` флаг `[state+0x1C]`; false → 0x7515C8 |
| 0x7515B3 .. 0x7515C2 | machine vslot `+0x20` = `AIStateMachine::setState(machine[+0x1C])` (RW 0x7515B6 читает поле машины `+0x1C`; RW 0x751E3A → 0x8DBDEF → 0x8DBA13; **INFERENCE**: имя из BFME2); результат == -2 → 0x751227 = STATE_FAILURE |
| 0x7515C8 .. 0x7515CF | machine vslot `+0x10` = `AIStateMachine::updateStateMachine` (RW 0x751D34) |
| 0x7515D2 .. 0x7515D4 | `test eax,eax; jle 0x75122A`: <= 0 вернуть как есть |
| 0x7515DA .. 0x7515DC | `xor eax,eax; jmp 0x75122A`: положительный sleep → STATE_CONTINUE |

### 3.6. Victim-dead path RW 0x751438 (ключевое отличие от порта)

| RW | ветка / вызов |
|---|---|
| 0x751438 .. 0x751440 | AI владельца `[esi+0x260]`, AI vslot `+0x240` `notifyVictimIsDead` (RW 0x63F3BF = `ret`, т.е. no-op) |
| 0x751446 .. 0x751450 | owner template `+0x11A & 0x80` (KindOf ARMY_OF_DEAD). Если выставлен → 0x751456 `or eax,-1` = STATE_SUCCESS |
| 0x751450 | иначе `je 0x7515C8`: внутренняя attack machine vslot `+0x10` updateStateMachine; > 0 → 0x7515DA CONTINUE, <= 0 → эпилог с её результатом |

То есть RW **не** вызывает `a.replacementVictim()` и **не** завершает атаку сразу
`notifyVictimIsDead(); return STATE_SUCCESS`. Он сохраняет внутреннюю машину
(STOPS S-583: charge/melee state продолжает жить и перехватывает цель).
NIGHT_WATCH, строка PORT-1: у обычных юнитов/орд атака заканчивается раньше именно
из-за RW 0x751263 .. 0x751438; `FinishAttackOnceStarted` PORT-1 живёт в основном
для турелей.

## 4. Имена vslot'ов и доказательство override'ов

| vslot | RW pointer | имя / тело |
|---|---|---|
| AI `+0x23C` | 0x670313 | `getLastCommandSource`: `mov eax,[ecx+0x48]; ret`. Слот 143 BFME2 (**INFERENCE**: имя) |
| AI `+0x240` | 0x63F3BF | `notifyVictimIsDead`: однобайтовый `ret`. Слот 144 BFME2 (**INFERENCE**: имя) |
| AI `+0x204` | 0x8851E4 | `addTargeter`: `ret 8`. Слот 129 BFME2 (**INFERENCE**: имя); вызывается из 0x6682B1 при очистке victim (RW 0x6682EC) |
| AI `+0x260` | 0x668D8E | `rva002697FC`: mood-ретarget; RW 0x668D95 проверяет status 0x26 HORDE_MEMBER и RW 0x668DA5 template `+0x108 & 4` (KindOf 2 IMMOBILE), затем кадр по id, state 0x11, `getNextMoodTarget` и `friend_setGoalObject` (**INFERENCE**: имя/семанитика из BFME2) |
| AI `+0x264` | 0x662552 | `wakeUpNow` (scheduler wake), вызывается из 0x68B53C |
| machine `+0x10` | 0x751D34 | `updateStateMachine` (vtable 0xC2B330 / 0xC2DC88, `+0x10` = 0x751D34) |
| machine `+0x20` | 0x751E3A | `setState(stateID)` → 0x8DBDEF → 0x8DBA13 (**INFERENCE**: имя из BFME2) |
| machine `+0x38` | 0x741689 → 0x8DB8CB | `setGoalObject(Object\*)`: RW 0x8DB7E9 ставит `[machine+0x20] = object+0x74` и копирует позицию `object+0x38` в `[machine+0x24]` (0x8DB778). `0x8DBACE` читает тот же `+0x20` |
| contain `+0x10` | 0x9188EB / 0x8BD372 | `isGarrisonable`: false / true. GarrisonContain vtable 0xC5C698 `+0x10` = 0x8BD372, OpenContain 0xC59AF0 `+0x10` = 0x9188EB |
| contain `+0x114` | 0x865CE3 | `getContainCount(int)`; в RW 0x751364 аргумент 0 |
| contain `+0xC0` | 0x865B33 | `isPassengerShotBlocked(rider, victim)`; одинаков во всех прочитанных contain vtable |
| state `+0x10` | 0x74CED6 | `AIAttackState::onEnter` |
| state `+0x14` | 0x74D25D | `AIAttackState::onExit(StateExitType)` |
| exit-conditions `+0` | 0x7511EB | `AttackExitConditions::shouldExit(machine)` |
| main machine `+0x38` | 0x741689 | `StateMachine::setGoalObject`, в 0x751224 вызывается с nullptr |

**Override-proof для `+0x240`.** Поиск всех 4-байтных вхождений `0x670313`
(`getLastCommandSource`) как `vtable_base + 0x23C` дал 25 кандидатов. 13 из них
имеют подпись AI-таблицы `+0x30 = 0x851E97`, `+0x38 = 0x66C0B9`,
`+0x23C = 0x670313`: **0xC10590, 0xC2E1C8, 0xC5E388, 0xC62070, 0xC63230,
0xC634D0, 0xC63B60, 0xC66D48, 0xC67088, 0xC69A00, 0xC6A5B8, 0xC6A848,
0xC6ACA8**. У всех 13 `+0x240 = 0x63F3BF` (`ret`). Остальные 12 — не AI-таблицы
(другие `+0x30/+0x38`); их `+0x240` = `0x851E97` (`xor eax,eax; ret`) или
`0x49093C` (getter другого класса), т.е. тоже не тело-override. Прямые
`call [reg+0x240]` в .text: 0x4481B9, 0x5E719F, 0x749D6C, 0x74AA2D, 0x74ADB4,
0x74B02B, 0x74D0C3, 0x7513B1, 0x751440, 0x75211C, 0x7E5AF6. AI-вызовы —
0x74D0C3 (onEnter), 0x7513B1 (этот update), 0x751440 (dead path), 0x749D6C,
0x74AA2D, 0x74ADB4, 0x74B02B, 0x75211C (другие AI-состояния при пропаже цели);
0x4481B9 / 0x5E719F / 0x7E5AF6 — другие vtable/попадания в середину инструкции.
Вывода «есть переопределение с телом» нет ни для одного AI-объекта.

`0x8DBACE` и vslot `+0x38` доказаны парой: 0x8DBACE читает `[machine+0x20]`,
0x8DB7E9 пишет туда `object+0x74`, 0x8DB778 пишет позицию в `[machine+0x24]`.
Машинные vtable 0xC2B330 и 0xC2DC88 найдены по указателю 0x751D34 в `+0x10`;
у обоих `+0x20 = 0x751E3A`, `+0x38 = 0x741689`.

## 5. Прямые callee update (имена и адреса)

| RW callee | имя |
|---|---|
| 0x8DBACE | `StateMachine::getGoalObject` |
| 0x68B678 | `Object::getControllingPlayer` |
| 0x664CB3 | `AIUpdateInterface::getAttackPosition` |
| 0x68E43B | `Object::getVisionRange` |
| 0x742DFC | очистка temporary speed cap (**INFERENCE**) |
| 0x46E918 | `Object::testModelCondition(bit)` |
| 0x44DDEC | `Object::testStatus(bit)` |
| 0x668D8E | AI vslot 0x260 `rva002697FC` (**INFERENCE**) |
| 0x68B4A4 | `Object::isOutOfAmmo` → 0x6C8115 |
| 0x670313 | AI vslot 0x23C `getLastCommandSource` (**INFERENCE**) |
| 0x68D7AB | `Object::getRelationship` → 0x7A3DAD `Team::getRelationship` |
| 0x6627F0 | `AIUpdateInterface::friend_setGoalObject` (**INFERENCE**) |
| 0x7A3E36 | `Team::getTeamTargetObject` (валидирует и чистит stale id) |
| 0x79FDEA | `Team::setTeamTargetObject` |
| 0x63F3BF | AI vslot 0x240 `notifyVictimIsDead`, no-op (**INFERENCE**) |
| 0x6682B1 | `AIUpdateInterface::setCurrentVictim`; при null зовёт victim AI vslot 0x204 |
| 0x865B33 | contain vslot 0xC0 `isPassengerShotBlocked` |
| 0x8BD372 / 0x9188EB | contain vslot 0x10 `isGarrisonable` |
| 0x865CE3 | contain vslot 0x114 `getContainCount` |
| 0x742D63 | `AIAttackState::chooseWeapon` (**INFERENCE**), внутри 0x68B619 и 0x68E197 |
| 0x68B58C | `Object::getCurrentWeapon(0)` |
| 0x401E64 | `AsciiString::isEmpty` |
| 0x406585 | `AsciiString::compare` |
| 0x441B59 | `WeaponTemplate::isMeleeWeapon` |
| 0x68B53C | `Object::notifyModelConditionChanged` |
| 0x74CED6 / 0x74D25D | state vslot 0x10 onEnter / 0x14 onExit |
| 0x751E3A | machine vslot 0x20 `setState` (**INFERENCE**) |
| 0x751D34 | machine vslot 0x10 `updateStateMachine` |

## 6. `m_49`: все писатели и читатели AIAttackState

Флаг `state+0x49` принадлежит объектам с vtable **0xC2A1D8**. В её методах
прочитаны ровно четыре обращения:

| RW | что делает |
|---|---|
| 0x74CB0C | конструктор 0x74CABC: `mov byte [esi+0x49], al` при `al = 0` |
| 0x74D066 | onEnter: `mov byte [ebx+0x49], 0` перед веткой `m_attackingObject` |
| 0x74D0AC .. 0x74D0B9 | onEnter: `0x68D7AB(victim)` → `neg/sbb/inc` → `mov byte [ebx+0x49], al`. Итог: `m_49 = (owner.getRelationship(victim) == ENEMIES)` на момент входа в атаку |
| 0x7512C8 | update: `cmp byte [edi+0x49], 0; je 0x751316` — гейт особого не-ENEMIES пути |

0x74CB5D — деструктор, только возвращает vtable 0xC2A1D8. Прочие попадания
`[reg+0x49]` (0x741AC8 в конструкторе с vtable 0xC28560, 0x74DB16 в функции
0x74DADA, чей указатель лежит в таблице 0xC27458, 0x85E5B8 / 0x85F56E,
0x751F4D, 0x74DE18 и т.п.) — байты **других** классов и состояний; к
`AIAttackState::m_49` они не относятся (**INFERENCE**: классификация по
create → constructor → vtable → slot).

## 7. Ответ на вопрос «где RW делает `a.replacementVictim()`?»

Портовый `AIUpdateInterface::replacementVictim()`
(`engine/src/GameLogic/Module/AIUpdateCombat.cpp:290`) использует сохранённый
`m_targetHorde`: при смерти текущего члена орды он подставляет следующего живого
члена того же horde. В RW **0x751263 .. 0x751438 такого вызова нет**: dead path
0x751438 зовёт no-op `notifyVictimIsDead` и оставляет внутреннюю attack machine
(0x7515C8). Подмена/spawn следующего члена делается не здесь.

Механизм RW — `Object::adjustVictim` **0x68D30E** (в порту `AI::adjustVictim`,
`engine/src/GameLogic/AI/AIClosestEnemy.cpp:533`; BFME2 объект 0x0028CCB9,
**INFERENCE**: имя из декомпиляции). Тело 0x68D30E: поднимается по `containedBy`
`Object+0x27C` (0x68C866), берёт contain vslot `+0x7C` (horde interface),
считает радиус текущего оружия (0x6CA935) либо RW 0xBDF350, зовёт contain
vslot `+0x180` и `+0x48` (выбор ближайшего члена), при неудаче повторяет с нулевым
радиусом. Постоянного `m_targetHorde` в этом пути нет.

Все 13 xref'ов `0x68D30E` и их семантика:

| RW call site | что это |
|---|---|
| 0x74CFA3 | **onEnter атаки**: 0x74CF93 проверяет victim template `+0x115 & 0x20` (KindOf HORDE), затем `adjustVictim(owner, 1, 0)` и 0x74CFB2 `setCurrentVictim`. Это начальный выбор члена орды — RW-эквивалент инициализации `m_targetHorde` |
| 0x66890B / 0x668A12 | `AIUpdateInterface::getNextMoodTarget`: nemesis-цель и найденная ближайшая цель корректируются `adjustVictim` (**INFERENCE**: имя BFME2) |
| 0x700AB9 / 0x700F27 / 0x70116D / 0x7012C3 | тела поиска ближайшего врага `AI::findClosestEnemy` (RW 0x7006D7, обёртка 0x701443, 0x70146A); при `qualifiers & 2` и не `& 0x10` выигравший horde заменяется ближайшим живым членом (**INFERENCE**: семантика BFME2) |
| 0x7482BC | guard/retaliation target: найденный HORDE корректируется перед `setGoalObject` |
| 0x76BF83 / 0x76DC4F | выбор цели special power: HORDE корректируется |
| 0x898135 / 0x949F5B / 0x94A030 | другие поисковые цели (скрипт/способность), тот же признак HORDE |

STOPS S-583 отдельно называет перехват цели через `AI::findClosestEnemy` RW
0x701443 в charge/melee состоянии. Поэтому в PORT-14 удаляется только вызов
`a.replacementVictim()` из ветки `victimGone` в `AIAttackState::update`; само
состояние `m_targetHorde`, его xfer/hash и остальные портовые пути, где helper
уже используется, не трогаются. Дублирующий обработчик рядом с `chooseWeapon()`
или `notifyNewVictimChosen()` не добавляется: RW 0x7513F9 — это `chooseWeapon`,
а не новый victim handler.

## 8. Что осталось INFERENCE / не перенесено (кандидаты в STOPS)

- Прелюдия RW 0x7510EC .. 0x751263 (speed cap 0x742DFC, mood-ретarget 0x668D8E,
  STONED 0x81, out-of-ammo, exit conditions) в PORT-14 не портируется: задача —
  victim block 0x751263 .. 0x751438 и его возврат. Имя 0x742DFC — **INFERENCE**.
- Хвост RW 0x7513D1 .. 0x7515DA (garrison shot block, chooseWeapon, locked weapon
  name / `GondorTrebuchet`, model conditions, setState, updateStateMachine)
  прочитан для точного адреса возврата, но в PORT-14 не переносится; имена
  `setState`, `chooseWeapon`, `rva0034FB52Slot8` — **INFERENCE** из BFME2.
- `m_victimTeam` в RW — Team\* (`+0x2C`), в порту — `ObjectID`. Для точной ветки
  0x751325 .. 0x751385 нужно сравнивать команду жертвы с командой, сохранённой на
  входе, без нового постоянного sim-поля; если правка всё же вводит/меняет
  sim-состояние (`m_49`), в том же коммите обязательны `AIAttackSnapshot.cpp`
  (XferVersion 2 + gate `v.current >= 2`), вклад в crc/hash в `AIAttack.cpp` и
  покрытие `save4*`.
- `m_49` — новое sim-состояние для порта: Bool «жертва была врагом на входе».
  Его писатель — `onEnter`, читатель — update. Без xfer/hash/save4* порт нельзя
  считать готовым.
- `STATE_SUCCESS` для ARMY_OF_DEAD (0x751446 .. 0x751456) и «оставить внутреннюю
  машину» (0x7515C8) — точные RW ветви, но видимый результат в игре владельцем
  ещё не подтверждён; это честно указывается в PR.
- Строка «Осталось» в PR и строки NIGHT_WATCH / STOPS оформляются отдельным шагом.

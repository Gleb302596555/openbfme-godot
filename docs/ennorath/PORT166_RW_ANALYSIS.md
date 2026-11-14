# PORT-166: Erkenbrand's "Time to Strike" (`Command_ErkenbrandChargeAttack`) — RotWK 2.01 разбор

Issue #395 (P-64): по кнопке `FIRE_WEAPON` перезарядка слота стартует сразу по приказу, заряда нет, после этого герой едет медленно. Три вопроса:
когда слот `FIRE_WEAPON` начинает перезарядку; что делает заряд и почему игровой путь его пропускает; что оставляет его медленным.

Mod data (Ennorath 1.9.8.2): `Command_ErkenbrandChargeAttack` (commandbutton.inc:42936) — `FIRE_WEAPON`, `PRIMARY`, `NEED_TARGET_ENEMY_OBJECT`,
`EnableOnModelCondition = MOUNTED`; `ErkenbrandChargeWeapon` (weapon.inc:50938) — `AttackRange 300`, `DelayBetweenShots 40000`, `FiringDuration 0`,
`PER_SHOT`, `CanFireWhileCharging`; `RohanErkenbrand` (rohanerkenbrand.ini) — LocomotorSets `SET_NORMAL` TheodenHumanLocomotor
(`NORMAL_GOOD_HERO_SPEED` = 50), `SET_MOUNTED` HeroHorseLocomotor (`NORMAL_MOUNTED_HERO_SPEED` = 100), `SET_ENRAGED` HeroHorseEnragedLocomotor
(`FAST_MOUNTED_HERO_SPEED` = 120), набора `SET_PANIC` нет.

## Путь приказа (MSG_DO_WEAPON_AT_OBJECT, RW 0x77A34C .. 0x77A3C6)

`0x76FAB7` блокирует слот группы `LOCKED_TEMPORARILY` → `0x771F49` (groupIdle: по членам `0x5E821A` на машине ИИ, AI + 0x3CC = 1 при источнике 0) →
ещё раз блок слота → `0x77208A` `groupAttackObject(false, цель, maxShots, 0)` → `0x76FD04` `clearTemporarySpeedCaps`. Порт: `WeaponCommands::atObject`
(WeaponCommands.cpp). Клик мышью переводит GUICommand (ControlBarCommandProcessing.cpp:445 — press слота FIRE_WEAPON), клиентский транслятор шлёт
`MSG_DO_WEAPON_AT_OBJECT { slot, objectID, maxShots }` (раскладка аргументов — та, что читает диспетчер RW 0x77A34C / 0x77A3CB).

## Выбор машины атаки (RW 0x74CED6 .. 0x74D050)

`AIAttackState::onEnter`: `chooseWeapon` (RW 0x742D95) оставляет заблокированный слот; вид машины:
`0x74CFDB` статус `CONTESTING_BUILDING` (0x25) → вид 5; `0x74CFF1` `HORDE` → виды 6/7 (HordeAttackMachine); **`0x74D01B .. 0x74D02A`**:
у текущего оружия шаблон с `CanFireWhileCharging` (+ 0x82) → **вид 8, машина `0x74523F`** (`AttackFireWhileChargingMachine`, одно состояние
`AIChargeTargetState`, vtable `0xC29708`: onEnter `0x75491F`, update `0x7518D8`, onExit `0x74D892`); иначе melee (вид 2, `0x744B71`) или generic.
Дальше melee/ranged.

## Удар (RW 0x7518D8 .. 0x751A19)

Пока цель вне `AttackRange` (+ 0x38) — только подход. Удар (условие `0x7519A7 .. 0x7519D2`: 3D-дистанция < AttackRange, модуль угла цели <
max(`AcceptableAimDelta` + 0x34, 0.035), статус оружия `0x6CD142` == READY_TO_FIRE — `test eax,eax; jne`):

1. `0x7519DB` `runThrough` (`0x748348`): цель движения — точка в 50 за жертвой вдоль линии герой→жертва;
2. `0x7519E0 .. 0x7519F2` `CHARGING` on (Object + 0x11C бит 4 = модель-условие 0x84) + `0x68B53C`;
3. `0x7519FE` `chooseLocomotorSet(4)` — **`SET_PANIC`** (AI vslot 0x238(4));
4. `0x751A07` `fireAtOwnPosition` (`0x74D9CE`): статус `IS_FIRING_WEAPON` + `fireCurrentWeapon(nullptr, ownPos)` (`0x6920AD`) — **здесь стартует
   перезарядка слота** (выстрел фиксирует `whenWeCanFireAgain` = now + DelayBetweenShots).

До удара статус оружия `READY_TO_FIRE` (`Weapon::getStatus`, RW 0x6CD142): по приказу перезарядка **не** стартует. Если враг уже в 300 (AttackRange
модового оружия) — удар проходит в первый же кадр приказа, и игрок видит «перезарядку по приказу».

## Выход (RW 0x74D892 .. 0x74D8E4)

`CHARGING` off (`0x74D8B4`) → `0x74D8C8` `chooseLocomotorSet` (AI vslot 0x238, аргумент 0 = `SET_NORMAL`) → `0x68DF11(1)`
`releaseObjectWeaponLock(LOCKED_TEMPORARILY)` → выход из `AIMoveToState` (`0x748D8A`).

`chooseLocomotorSet` — RW `0x6680B2` (vslot 0x238 AIUpdateInterface; найден по vtable-слоту `0x228` = `0x66288A`, тело совпадает с донором BFME2
`AIUpdateInterfacePrivateCommands.cpp:2459`): `SET_NORMAL` → `SET_NORMAL_UPGRADED` при `+0x3B9`; тот же набор → true; `+0x3C9`
(`m_bfmeLocomotorSetLocked`) → false; успех — `+0x1F4` = набор, `+0x1F8` = Speed набора. Единственный писатель `+0x3C9` в game.dat — FlammableUpdate
RunToWater (`0x8909B3`); `+0x3B9` писателей не имеет.

## Скорость (RW 0x5E3F49 / 0x5E3EF7)

`speed = locomotorSetSpeed` (ai + 0x1F8, Speed текущего набора) × множитель; `isCharging` (`0x5E3EF7`): (`ChargeAvailable` шаблона и (атака 0x16 или
leash 0x1E)) либо (`Object + 0x251` — `hasChargeAttackAbility`, вычисляется один раз по модулю SpecialAbilityUpdate типа 0x2A — и `CHARGING` 0x84).

## Ответы на три вопроса

1. **Перезарядка стартует в ударе** — RW `0x751A07` → `0x6920AD`, не по приказу. В игре враг обычно уже в AttackRange 300, и удар проходит в первый
   кадр приказа.
2. **Заряд делает машина вида 8** (RW `0x74D01B .. 0x74D02A` → `0x74523F`): run-through + `CHARGING` + `SET_PANIC` + выстрел на своей позиции (урон
   наносит цепочка `FireWeaponUpdate` `ChargingModeTrigger`). Пропуск заряда — это не выбор вида машины (порт выбирает вид 8 по тому же признаку,
   `+0x82`), а удар, который либо уже выстрелил по приказу (враг в радиусе), либо не проходит условие `0x7519A7`.
3. **Медленным оставляет выход** — RW `0x74D8C8` возвращает набор локомотива. У ванильного пользователя заряда (Хаму́л, mordorkhamulnew.ini:
   `SET_NORMAL` FellBeastLocomotor + `SET_PANIC` FellBeastSwoopLocomotor) аргумент 0 и есть набор, с которого шёл заряд: после заряда скорость равна
   скорости до приказа. Герой на коне (`SET_MOUNTED`, ToggleMounted RW `0x8B1812` ставит 7) идёт с набора 7: возврат в `SET_NORMAL` (50 вместо 100)
   и есть «медленно после». Порт восстанавливает набор, который сместил удар (для всех, кто заряжается с `SET_NORMAL`, это та же команда).

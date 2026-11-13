# Port 168: troll eating heals, the held passenger is thrown (fork issue 397)

Facts read from `game.dat` (RotWK 2.01, md5 `daed3668…`) with `tools/mod/rw.py` and the pefile tables; donor BFME2 1.06 marked `DONOR`.

## 1. Why eating an orc does not heal

Ennorath data: `object/evilfaction/units/mordor/mountaintroll.ini` (MordorMountainTroll) — `SpecialAbilityUpdate ModuleTag_08` with
`GrabPassengerAnimAndDuration = AnimState:EATING AnimTime:3000 TriggerTime:1400` and **`GrabPassengerHealGainPercent = 50.0`**; the eat itself is the
`AutoPickUpUpdate AutoPickUpForTastyOrcs` → `EatObjectEntry = MyHealth:99% TargetHealth:100% Filter:NONE +ORC` → `doSpecialPowerAtObject(orc, 0x4000)`.
The transferable object (watcher) has the same field with `10.0`.

* Module data parse table RW 0xC561C0: name RW 0xC55BF8 `GrabPassengerHealGainPercent`, **parser RW 0x42ED6F**, user data 0, **offset + 0x30**.
* Parser RW 0x42ED6F: `RW 0x42DC9F` next token, `RW 0x42EAAD` scanReal, `RW 0x42ED89` `fstp [store]`, then `RW 0x42ED8E .. 0x42ED96`
  `fld st(0); fldz; fcomip; jbe` — a value below zero formats `RW 0xBD4330` `"invalid Real value %1.7f -- expected >= 0"`. **There is no `* 0.01`
  anywhere in the parser**: the INI number is stored as written. (Contrast: `RW 0x42ED00` parseReal, and the percent parser used by fields such as
  `Volume` which multiplies.) So `50.0` in the INI is **50.0**, not 0.5.
* Consumer RW 0x8532F9 .. 0x853331 (`SpecialAbilityUpdate`'s grab finish, the eat branch): body slot 0x14 (`ActiveBody` RW 0x8C1D75: health / max),
  `fmul [RW 0xBD88D8] = 100.0`, `fadd [module data + 0x30]` (the field above), `fstp` to a local, `fld 100.0 / fcomip` clamp to 100 (`RW 0x853317`),
  then body slot 0x58 `(percent, 0)` at `RW 0x853331`.
* Body RW 0x8C47DE (slot 0x58, second argument 0): a percent above 100 first scales the max health by `percent * [RW 0xBE5600] = 0.01`
  (`RW 0x8C4806`), then the health goes to `percent * 0.01 * [body + 0x1C]` through `internalChangeHealth` (`RW 0x8C48FC .. 0x8C4912`, slot 0x84).
* Port: `engine/src/GameLogic/Module/SpecialAbilityModules.cpp:246` parses the field with `INI::parsePercentToReal`, which stores
  `scanReal(token) * 0.01f` (`engine/src/Common/INI/INIMacro.cpp:249`) — the port therefore holds **0.5** where RotWK holds 50.0, and
  `grabFinish` adds 0.5 percentage points instead of 50: a 2400 hp troll regains 12 hp out of the 1200 the data asks for, invisible in the health bar.

## 2. Why the grabbed goblin is never thrown

Ennorath data (`object/evilfaction/units/wild/cavetroll.ini`, CaveTroll): `TransportContain ModuleTag_09` `TypeOneForWeaponSet = CLUB` /
`TypeTwoForWeaponSet = INFANTRY`, so a held goblin (INFANTRY) sets the container's `PASSENGER_TYPE_TWO` weapon-set flag (`RW 0x86A731`), which selects
`WeaponSet Conditions = PASSENGER_TYPE_TWO  Weapon = PRIMARY CaveTrollGoblinThrow`. That weapon (`weapon.ini:12317`) has
**`ProjectileFilterInContainer = NONE +INFANTRY`** and a `ProjectileNugget` with **no `ProjectileTemplateName`** — the projectile is the held unit itself.

* `Weapon` template field `+ 0x120` is `ProjectileFilterInContainer` (`DONOR` `WeaponStoreNewOverride.cpp:141`).
* RW 0x6CD298 (`Weapon::getRemainingAmmo`): the filter is non-empty (`RW 0x762977` on `template + 0x120`) → the owner (`RW 0x449681` of
  `[weapon + 8]`) → its contain (`RW 0x6CD2BF`, `[object + 0x258]`) is asked the count with the filter (`RW 0x6CD2D9`, contain slot 0x114 with the
  filter pushed). Without a container, or with an empty filter, the clip's status is read as before (`RW 0x6CD2E1 .. 0x6CD2FA`).
* RW 0x90FA48 (the projectile nugget's object-target deliver): `RW 0x90FA81 lea ecx, [template + 0x120]` / `RW 0x90FA87` the filter's empty test /
  `RW 0x90FA90` the source's contain `+ 0x258` → **`RW 0x90FAAA` the launch transform (Weapon RW 0x6CAB85) of the source, then `RW 0x90FAC3` contain
  slot 0x13C with the filter, whose answer replaces `[ebp + 8]`, the projectile object** (`RW 0x90FAC9 jmp 0x90FB3A`). That object then gets the
  producer (slot `RW 0x68B6A1`) and is launched through the first of its modules that answers the projectile interface (`RW 0x90FB4B`, vslot `+ 0x48`).
* Contain slot 0x13C = **RW 0x866367**: walk the contained list (`[container + 0x34]`), skip nulls, and with a filter `ObjectFilterMatch` RW 0x7640C1
  (rider, filter, the container's player `RW 0x68B678`); the first match is taken out through slot 0xA4 (`RW 0x8663AC`, removeFromContain) and returned.
  No match answers null.
* Port: `Weapon` already reads the filter (`engine/src/GameLogic/Object/WeaponState.cpp:113` `getRemainingAmmo`, `:149` / `:164` `getStatus`), but the
  host that answers it, `ObjectWeapons::Host::containedAmmo` / `ownerHasContainAmmo` (`engine/src/GameLogic/Combat/ObjectWeapons.cpp:136`), is a stub that
  always answers false — the weapon is therefore permanently OUT_OF_AMMO. And `ObjectProjectileLauncher::launch` has no container branch (stop S-360),
  so a nugget without `ProjectileTemplateName` returns without launching anything: the troll plays the swing and nothing leaves his hand.

## 3. What is not part of this task

* The held unit's pose (not following the hand) and the tree texture over the log are the visual part (Local 48).
* `AutoThrowObject` (`RW 0x8995FB`) is a different step of `AutoPickUpUpdate` (S-7160) and stays unported.

# PORT-2: `RecoveryTime` of the body modules (RotWK 2.01, `game.dat`, caveat S-001)

The field is in the body table (`Body` / `ActiveBody` and every subclass that parses it) and RotWK
2.01 **reads** it once, in `ActiveBody::attemptDamage` (RW 0x8C3FA3), and passes it to a
function that is empty. The port already parses the field and drops it, which is what retail does.
This document is the evidence; the code keeps the reading so the mechanism is the same shape as
retail and a test pins both facts (the read happens, and the value it feeds changes nothing).

## 1. The field

| what | address |
|---|---|
| `RecoveryTime` string | RW 0xC71D1C (referenced from exactly one place: the table row below) |
| body data table row | RW 0xC71DA8: `{ name 0xC71D1C, parser 0x73A429, userData 0, offset 0x24 }` |
| table | RW 0xC71D68; rows `MaxHealth` +0x08 ... `BurningDeathFX` +0x54; `RecoveryTime` is the 5th row, offset **0x24** |
| parser | RW 0x73A429: `scanUnsignedInt` (RW 0x42DC9F) then `scanDuration` (RW 0x42EA42) = `ceil(ms * 0.005)` frames (RW 0xD9F610 = 0.005f), stored as a 32-bit unsigned |
| module-data layout | `ActiveBodyModuleData` RW 0x8C373F: `MaxHealth` +0x08, `InitialHealth` +0x0C, `MaxHealthDamaged` +0x10, `MaxHealthReallyDamaged` +0x14, `DodgePercent` +0x18, `EnteringDamagedTransitionTime` +0x1C, `EnteringReallyDamagedTransitionTime` +0x20, **`RecoveryTime` +0x24**, `UseDefaultDamageSettings` +0x28, ... |

The same row is in the field tables of every body class that inherits `ActiveBody`'s table:
`DelayedDeathBody`, `RespawnBody`, `PorcupineFormationBodyModule`, `HighlanderBody`, `ImmortalBody`,
`StructureBody`, `HiveStructureBody` (the mod writes it in 142 `ActiveBody`, 12 `RespawnBody`,
3 `PorcupineFormationBodyModule` and 1 `DelayedDeathBody` definitions, plus 2 `DetachableRiderBody`
ones whose class the port does not have).

## 2. `ActiveBody::attemptDamage` (RW 0x8C3FA3, ends RW 0x8C47DB `ret 4`) — every branch

The function is 636 instructions. Slots on the stack frame, established once:

| slot | what | set at |
|---|---|---|
| `[ebp-0x28]` | `esi - 0x10` — the body module's base (the `BehaviorModule` subobject) | 0x8C3FBD |
| `[ebp-0x10]` | `edi` = the `Object` (interface + 0x44 of the body module, RW 0x8C3FC0) | 0x8C3FCE |
| `[ebp-0x24]` | `eax` = `[esi-0xC]` = the **module data** (`ModuleData` pointer of the module) | 0x8C3FD1 |
| `[ebp-0x18]` | the amount (float), after `AdjustDamage` | 0x8C4068 / 0x8C4086 |
| `[ebp-0x1C]` | the damage state before the health change (+0x30 of the body interface in RW terms) | 0x8C421F |
| `[ebp-0x20]` | the source `Object*` (`findObjectByID`) | 0x8C409B |
| `[ebp-0x2C]` | the horde helper (`ContainModuleInterface*` of the container) | 0x8C40E7 / 0x8C411F |
| `[ebp-0x30]` | the source object of the last damage record | 0x8C4312 / 0x8C432A |
| `[ebp-0x11]` | the "hit was clipped by the burning-death cap" byte | 0x8C40F7 |

| branch | address | what it does |
|---|---|---|
| 1 | 0x8C3FDA | the body's indestructible byte (+0xC7) set: **return**, nothing happens (no FX, no record) |
| 2 | 0x8C3FE7 | `Object::testModelCondition(0x14A = INVULNERABLE)` (RW 0x46E918): **return** |
| 3 | 0x8C3FFB | `Object::testStatus(0x3C = UNATTACKABLE)` (RW 0x44DDEC) and the damage type is not 8 (UNRESISTABLE): **return** |
| 4 | 0x8C403C | the output halves (`actualDamageDealt` D+0x70, `actualDamageClipped` D+0x74) zeroed, **before** the effectively-dead test (so a refused hit reports 0 / 0) |
| 5 | 0x8C4049 | `Object` + 0x458 bit 1 (effectively dead): **return** |
| 6 | 0x8C405D | `AdjustDamage` (RW 0x5D893C) with the cached armour and the current armour set → `[ebp-0x18]` |
| 7 | 0x8C406B | damage type 7 (HEALING): `attemptHealing` (RW 0x8C3A48 family, called through slot 0x... at 0x8C4071 jumps to 0x8C47BF) and **return** — healing does not reach anything below |
| 8 | 0x8C407C | not UNRESISTABLE: amount `*= body + 4` (the body damage scalar; `movss xmm0,[esi+4]`) |
| 9 | 0x8C408B | the source object (`findObjectByID`, RW 0x449681) and the BLOODTHIRSTY sacrifice test (RW 0x693A1A → 0x68C878, slot 0xC and slot 0x20) |
| 10 | 0x8C40E7 | the burning-death cap test (`[ebp-0x11] = 0`; the gate at 0x8C40F0 `comiss` against RW 0xC1B594 = 1.0f so a hit of at most 1.0 never caps, then the body-data byte +0x24 of the **body** at 0x8C40FD — the burning-death flag, unrelated to this field) |
| 11 | 0x8C4107 | the container's horde interface (object + 0x27C's contain module + 0x258, slot 0x7C) → `[ebp-0x2C]` |
| 12 | 0x8C4122 / 0x8C4147 | the two status / model-condition globals built once (RW 0xDE9B74 bits) |
| 13 | 0x8C4169 .. 0x8C4214 | the cap: object has an AI (+0x260), data byte +0x51 (BurningDeathBehavior), 3 statuses, 3 model conditions, damage type 6 or 0x17, no stunned physics, no container or a container with a horde interface → `min(health - 1.0, amount)`, and `[ebp-0x11] = 1` when the amount was reduced |
| 14 | 0x8C4218 | a `kill` flag takes the whole health; `[ebp-0x1C]` = the damage state before |
| 15 | 0x8C422E .. 0x8C423C | damage type 4 (SIEGE): `setCorrectDamageState`-adjacent call RW 0x8C3E70 with the amount (crush / siege state) |
| 16 | 0x8C4244 .. 0x8C4257 | `internalChangeHealth(-amount)` through the body's virtual slot 0x84 |
| 17 | **0x8C425D .. 0x8C4276** | **the `RecoveryTime` read**, see section 3 |
| 18 | 0x8C427B | the record: `body + 0x70 = amount`, `body + 0x74 = previous - current` |
| 19 | 0x8C4294 .. 0x8C4309 | the last-damage record (frame, source, mask, type) when the last record is older than two frames, else the "prefer an attacker" rule; then + 0xB6 cleared and the frame stored |
| 20 | 0x8C430F .. 0x8C4341 | the source's controlling player marked as an attacker of the victim's player (RW 0x6AAC05) |
| 21 | 0x8C4346 .. 0x8C4384 | the damage-state change: the modules' damage interface slot 0, the source's PillageModule (RW 0x68B8A3) |
| 22 | 0x8C4389 .. 0x8C44C1 | the damage modules' slot 2 (`onBodyDamageStateChange`) and the damage state sounds |
| 23 | 0x8C44F6 .. 0x8C4550 | the state's model conditions and structure rubble effects |
| 24 | 0x8C4582 .. 0x8C463E | the death: kill credit (RW 0x6955BC), the die modules, the horde containment test, the cheer |
| 25 | 0x8C46A9 .. 0x8C46EA | the attacker's experience for the clipped damage |
| 26 | 0x8C46F0 .. 0x8C4767 | `doDamageFX` and the repulsor status |
| 27 | 0x8C476C .. 0x8C47BD | the clipped hit's tail: leave the horde (slot 0x38), the locked temporary state (RW 0x66309C), AI command 0x53 (RW 0x8C3EC3(2)), `BurningDeathFX` (RW 0x4B1B5A) |
| 28 | 0x8C47BF .. 0x8C47CA | a HEALING hit that is not a `kill` and has a non-zero amount: the body's virtual slot 4 |
| 29 | 0x8C47DB | `ret 4` |

## 3. The `RecoveryTime` read (RW 0x8C425D .. 0x8C4276) and what it feeds

```
008C425D  mov  eax, [ebp - 0x24]      ; the module data saved at 0x8C3FD1
008C4260  mov  eax, [eax + 0x24]      ; RecoveryTime, the row at RW 0xC71DA8
008C4263  test eax, eax
008C4265  jbe  0x8c427b               ; 0 (or an unsigned "negative") skips the call
008C4267  mov  ecx, [0xde412c]        ; TheGameLogic
008C426D  mov  ecx, [ecx + 0x40]      ; TheGameLogic + 0x40 = the current logic frame
008C4270  add  ecx, eax               ; frame + RecoveryTime
008C4272  push ecx                    ; argument 1: the "recovery ends" frame
008C4273  mov  ecx, [ebp - 0x10]      ; ecx = the Object (this for the callee)
008C4276  call 0x9f3a3c               ; Object::<method>(frame + RecoveryTime)
008C427B  ...                         ; continues with the record (section 2, branch 18)
```

`RW 0x9F3A3C` is `C2 04 00` — `ret 4`, i.e. the image's shared empty function. It is the same address
the rest of this port already cites as "empty": `W3DBuffDraw::doDrawModule` slot 11
(`docs/STOPS.md` S-113, `engine/src/GameClient/BuffDraw.h`), `AptOnline::OnInitialized`
(`engine/src/GameClient/GUI/AptScreens/AptOnlineShell.cpp`), `AptLanLobby::OnInitialized`,
`SciencePurchasePointTracker` slot 4, `RepairSpecialPower::doSpecialPower` (slot 0x1C),
`DelayedDeathBody`'s own attemptDamage, `FireWeaponWhenDamagedBehavior` slots 1 / 12 / 15,
`GiantBirdAIUpdate` state `onExit`, `AIGroupOrders` member removal slot 0x18, `AIHarvest::xfer`.
The callee takes one stack argument (`ret 4`) and **does not touch it**: no global is written, no
field of the object is read, no virtual call is made. Whatever the deadline is, this build of
RotWK 2.01 computes it and throws it away.

Who else reads the field: the body data tables of the subclasses (`DelayedDeathBody` RW 0xC723FC and
RW 0xC72628, `RespawnBody` RW 0xC72430, `PorcupineFormationBodyModule` RW 0xC72E58's data tables),
and the parser that fills it. The only read of `[module data + 0x24]` in the image is the one above:
`ActiveBody::attemptDamage` loads the module data once, at RW 0x8C3FD1, from the body module's
`[esi - 0xC]`; every other body function that reads `[reg + 0x24]` reads a **body** offset (+0x24 of
the body is the damaged-health fraction, RW 0x8C3934, or another class's field), not the module data.
The subclass overrides that call the base (`PorcupineFormationBodyModule` RW 0x8C63C4 → RW 0x8C63E7,
`HighlanderBody` RW 0x8C49C7, `DelayedDeathBody` RW 0x8C57xx) go through the same code, so they read
the same field and feed it to the same empty function.

## 4. What this means for the port

* `ActiveBodyModuleData::m_recoveryTime` must keep being parsed (the row is in the table; a missing
  parser is a load error) with the row's own parser (`parseDurationUnsignedInt` = `ceil(ms * 0.005)`
  frames, the port's existing `INI::parseDurationUnsignedInt`, RW 0x73A429).
* `ActiveBody::attemptDamage` must read it at the same point: a non-zero value makes the game logic
  frame plus the value, handed to the body's empty hook. The hook is `BodyModuleInterface::onRecoveryScheduled(float)`-shaped in
  retail (`Object` + a thiscall with one stack argument); the port's `ActiveBody` (and through it every
  subclass) does what the callee does — nothing — but the deadline value is computed and passed, so the
  code path and its arithmetic are the ones RW has.
* No simulation state changes: there is no deadline to store, no reader, no hash, no save field. The
  frame arithmetic itself is `unsigned` wrap-around (`add ecx, eax` on 32 bits), and the call is skipped
  when the field is 0 (`jbe`), which a test can see through the body's counters.

## 5. INFERENCE and stops

* INFERENCE: the callee is the same `ret 4` stub as the other citations above; the image has one such
  stub (RW 0x9F3A3C) and the port relies on that identity, as `docs/STOPS.md` S-113 already does.
  If a clean 2.01 `game.dat` differed there (caveat S-001), the hook could do something; nothing in
  this image does.
* INFERENCE: the deadline is in logic frames (`TheGameLogic + 0x40` is the frame counter the same
  function uses for the last-damage record at RW 0x8C4294 .. 0x8C4309), not milliseconds.
* Not ported: nothing of this field's consumer exists to port. The recovery **hook name** is unknown
  (the `Object` method table slot is not needed: the call is a direct call to an empty function).
* `DetachableRiderBody` (2 mod definitions) is a class the port does not have at all (field audit
  status `missing-module`); it belongs to that class's lane, not this one.

## 6. Data (mod, not committed)

`RecoveryTime` appears in the mod's merged INI 158 times (142 `ActiveBody`, 12 `RespawnBody`,
3 `PorcupineFormationBodyModule`, 1 `DelayedDeathBody`; 2 more in `DetachableRiderBody`, a class the
port lacks), 27 more lines are commented out. The literal is `5000` (25 frames); the
`*_HEALTH_RECOVERY_TIME` macros resolve in the mod's `data/ini/_gamedata.inc` to **10000 (×77, 50
frames) and 5000 (×5, 25 frames)**. So every body of the mod that is hit computes a deadline 25 or 50
frames ahead and hands it to the empty callee.

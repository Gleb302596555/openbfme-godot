# PORT-115 — the archers' Barrage (`Command_ArcherBombard`): a horde's FIRE_WEAPON at a position (P-60)

Issue #299 (`docs/ennorath/PROBLEMS.md` P-60). Image: `workspace/RotWK/game.dat`, 11 346 944 bytes, md5
`daed3668006cd90f01c34e5a7da1901f` (caveat S-001). Every address below was read with
`python tools/mod/rw.py workspace/RotWK/game.dat dis <addr>`; the donor is the BFME2 1.06 decomp
(`Open-BFME-2`, `Code/`) and each donor fact is marked `DONOR` and confirmed in RW.

## 1. The Ennorath data (mod INI 1.9.8.2, vanilla overlaid by the mod)

* `Command_ArcherBombard` (`commandbutton.ini:1077`, `..Lindon` 1089, `..Imladris` 1102): `Command = FIRE_WEAPON`,
  `WeaponSlot = TERTIARY` ("Because of HordeAI, archer hordes need a different weapon for attacking a position"),
  `Options = NEED_TARGET_POS OK_FOR_MULTI_SELECT`, `RadiusCursorType = ScatterAreaRadiusCursor`. Command sets:
  `IsengardUrukCrossbowHordeCommandSet` (`commandset.ini:272`), `MordorArcherHordeCommandSet` (339),
  `MordorCorsairsOfUmbarHordeFireBombCommandSet` (386), `MordorHaradrimArcherCommandSet` (836),
  `GondorRangerHordeCommandSet` (1335) and two more (1354, 852).
* The horde object, e.g. `MordorArcherHorde` (`object/evilfaction/hordes/mordor/mordorhordes.ini:392`):
  `WeaponSet` PRIMARY `MordorArcherMissileHordeRangefinderEdain_New` (`includes/weapon.inc:33862`: `HordeAttackNugget`,
  no `NoVictimNeeded`), TERTIARY `MordorArcherMissileHordeRangefinderBombard_New` (`weapon.inc:33880`:
  `NoVictimNeeded`, `BombardType`, `ScatterRadius`, `MinimumAttackRange`, **an empty `HordeAttackNugget`** — no
  `LockWeaponSlot`), `AutoChooseSources = TERTIARY FROM_PLAYER FROM_SCRIPT` ("Not AI.").
  `Behavior = HordeContain`: `RanksToReleaseWhenAttacking = 1 2 3 4`.
* The member, e.g. `MordorArcher` (`object/evilfaction/units/mordor/mordorarcher.ini:644`): `WeaponSet` PRIMARY
  `MordorArcherBowEdain` (`weapon.inc:20477`, **no** `NoVictimNeeded`), TERTIARY `MordorArcherBowBombardEdain`
  (`weapon.inc:20542`, `NoVictimNeeded`, `BombardType`), `AutoChooseSources = PRIMARY FROM_PLAYER FROM_SCRIPT FROM_AI`
  and `TERTIARY FROM_PLAYER FROM_SCRIPT` ("Not from AI.").

The members' bombard bow is therefore never *auto-chosen* for an AI order, and the position order a horde gives its
members is `CMD_FROM_AI` (step 9). An explicit lock of the members' TERTIARY slot is what keeps them on the bombard
bow while they shoot the ground — which is what the button's own comment says.

## 2. RW: from the ground click to the members' volley

1. Client (already ported, lane FACT-A / S-1990): the button press sends MSG_DO_WEAPON (1037), the ground click of a
   `NEED_TARGET_POS` FIRE_WEAPON button sends **MSG_DO_WEAPON_AT_LOCATION (1038)** `{ int slot, location, int maxShots }`.
2. **RW 0x77A3CB**, the logic's handler of 1038: arg 0 the slot, arg 1 the location, arg 2 the shot limit;
   `push 1 / push slot / call 0x76FAB7` = `AIGroup::setWeaponLockForGroup(slot, LOCKED_TEMPORARILY)`;
   `test al, al / je 0x77CFE6` — **when no group member locked, nothing else runs**; otherwise the shared tail
   RW 0x77A337: `groupAttackPosition(&loc, maxShots, CMD_FROM_PLAYER)` and RW 0x76FD04 (every member's AI + 0x2C,
   the temporary speed cap, = -1.0). The `MSG_DO_WEAPON` form (RW 0x77A304) is the same with `pos = NULL`.
3. **RW 0x76FAB7 `AIGroup::setWeaponLockForGroup(slot, type)`**: over the member list (head at + 4, the object at
   node + 8): RW 0x68B58C(0) the member's current weapon, **`call 0x69121A` — `Object::setWeaponLock(slot, type)`**
   (RW 0x76FAE9) whose answer sets the returned "any locked" byte, then the shared-timer copy (the weapon template's
   byte + 0x16A, RW 0x6CD142 `computeStatus`, RW 0x6CA232 `cacheStatus`).
   DONOR BFME2 0x0036DD45 (`AIGroupAttackTeam.cpp:AIGroup::setWeaponLockForGroup`) — same member walk, same call.
4. **RW 0x69121A `Object::setWeaponLock(slot, type)`**: (a) the object's contain (+ 0x258) through **vtable slot
   0x168** with (slot, type) — RW 0x691220 .. 0x691232; (b) `setStatus(SWITCHED_WEAPONS 0x51, type == 2 && slot != 0)`
   (RW 0x62684D); (c) the object's own weapon set (+ 0x34C) through RW 0x6C97F9 — whose answer is the function's.
5. **RW 0x8656D6 — contain slot 0x168** (OpenContain's, and HordeContain's through it): the passenger walk
   (contain vslot 0x110) with the callback **RW 0x8656A7**, which pushes (slot, type) and calls
   **`Object::setWeaponLock` RW 0x69121A on every passenger** (recursive), OR-ing a "some passenger refused" byte;
   slot 0x168 returns "none refused", which RW 0x69121A ignores. **This is the horde's members receiving the
   TERTIARY lock of the Barrage button.**
6. **RW 0x77229A `AIGroup::groupAttackPosition(pos, maxShots, cmdSource)`**: per member — `pos == NULL` takes the
   member's own position (+ 0x38); the member's contain (+ 0x258) vslot 0xB4 (`isPassengerAllowedToFire`) and, when it
   holds, vslot 0x118 (the passenger list) with each passenger's RW 0x68B5FA(ATTACK_NEW_TARGET 0, 0, &pos, cmdSource)
   and, on the answers 2 / 3, its AI's **RW 0x6961F1 `aiAttackPosition`**; then the SpawnBehaviorInterface
   (RW 0x68C3A3) vslot 0x10; then the member's own AI (+ 0x260) command interface (+ 0x20) **RW 0x6961F1**.
   A free-standing horde's HordeContain answers slot 0xB4 through **RW 0x86C713**: the slot of the horde's *container*,
   false when the horde is not contained — so for the Barrage only the horde object's own AI is ordered here.
   DONOR BFME2 `AIGroupAttackTeam.cpp:AIGroup::groupAttackPosition` — same shape (its `allowedToFire` passenger branch).
7. **RW 0x6961F1 `AICommandInterface::aiAttackPosition(&pos, maxShots, cmdSource)`**: builds the parms (command type
   0xE, the position, the limit, the source; ctor RW 0x7536DD) and calls the command sink's vslot 0 — for a horde
   **`HordeAIUpdate::aiDoCommand` RW 0x89E169**, whose hand-off list contains 0xE (the members' orders are ended
   through the contain's slot 0x14, RW 0x87594C, unless the horde stands in a container) — then the base command,
   **`privateAttackPosition` RW 0x66DE02**: the RAMPAGE gate (the weapon set flags bit 8, RW 0x68BE7D) for a PLAYER
   order, the off-map gate (Object + 0x458 bit 3), `ContinueAttackRange` (the weapon template + 0x148 > 0 → a victim
   search RW 0xA39090 with the filters RW 0xC0F374 / 0xC0F368, found → RW 0x66C536, else the limit becomes 1),
   `setState(9)` RW 0x66E02D, the goal position RW 0x6654AD, the goal object cleared RW 0x66E038, the shot limit on
   the current weapon (+ 0x34 = maxShots, + 0x20 = 0) RW 0x66E04B and RW 0x66B271(&pos) for CMD_FROM_AI /
   CMD_FROM_PLAYER.
8. State 9 is `AIAttackState` not attacking an object: the weapon is chosen through RW 0x742D95 → **RW 0x6C8A4E
   `chooseBestWeaponForTarget`, where a locked weapon stays** (RW 0x6C8A9E .. 0x6C8AA7), so the horde keeps its locked
   TERTIARY bombard rangefinder. A HORDE's sub machine is the horde attack machine (approach 0xC9 → fire 0xCA → wait
   0xCB; DONOR B1 `Rva001812B0AIHordeMachineCtor.cpp`; the fire state's onEnter **RW 0x74C258** and update
   **RW 0x74C4A7**): the horde fires its current weapon at the goal position when it has no victim.
9. That weapon's **`HordeAttackNugget` with no victim** takes the position form (**RW 0x911AC9**): the source's
   contain (+ 0x258) gives the horde interface (contain vslot 0x7C) and calls its **slot 0 = RW 0x875550**: unless the
   melee attack holds, every member of a copy of the member list (RW 0x865598) with an AI (+ 0x260) and a rank named in
   `RanksToReleaseWhenAttacking` (module data + 0x1B8) gets **`aiAttackPosition(pos, 0x7FFFFFFF, CMD_FROM_AI)`**
   (RW 0x6961F1). DONOR BFME2 `HordeContain::rva00472329` (`HordeContainIface11CSlots.cpp`) — same shape: the module
   data's + 0x1B8 map, the member's rank record + 0x188, `CMD_FROM_AI`, 0x7FFFFFFF shots.
10. Every released member now runs its own state 9 and its own `chooseBestWeaponForTarget` — with `CMD_FROM_AI`, which
    the mod's `AutoChooseSources` excludes for TERTIARY. **The lock of step 5 is what keeps the member on its bombard
    bow** (RW 0x6C8A4E: a locked weapon stays); without a lock the set is free to answer with another slot. The member
    shoots the ground until the horde's own shot limit (step 7, RW 0x66E04B) ends the horde's state or another order
    comes.
11. The release half: **RW 0x76FB64 `AIGroup::releaseWeaponLockForGroup(type)`** → per member **RW 0x68DF11
    `Object::releaseWeaponLock(type)`**: a destroyed object (+ 0x94 bit 0) returns at once; the contain (+ 0x258)
    through **vtable slot 0x16C** (the passengers' release, the twin of 0x168); then the object's own weapon set
    RW 0x6C98E6.

## 3. Where the port departs from RW

`AIGroup::setWeaponLockForGroup` (`engine/src/GameLogic/AI/AIGroup.cpp`, RW 0x76FAB7) does not call
`Object::setWeaponLock` (RW 0x69121A). It inlines step 4's (b) the `SWITCHED_WEAPONS` status and (c) the weapon set's
lock (`ObjectWeapons::setWeaponLock`, RW 0x6C97F9) and skips (a) **the contain slot 0x168 pass-through**;
`AIGroup::releaseWeaponLockForGroup` (RW 0x76FB64) calls the weapon set's release (RW 0x6C98E6) instead of
`Object::releaseWeaponLock` (RW 0x68DF11), skipping **the contain slot 0x16C pass-through** and the destroyed-object
gate. RW 0x76FAB7 / 0x76FB64 call RW 0x69121A / 0x68DF11 directly (RW 0x76FAE9, RW 0x76FB78), so both halves of the
group form are missing the same step.

Step 4 itself is already ported as `ObjectWeapons::setObjectWeaponLock` (RW 0x69121A, lane ARCHER-1 r2 — the
`HordeAttackNugget`'s `LockWeaponSlot`, the command button's three forms and the create modules all use it), so the
group form was the only caller that took the inner weapon-set function instead; the port's own stop line
(`AIGroup::forceAttackStopLine`, S-2480) names the pass-through as not ported.

Consequence for P-60: the Ennorath bombard horde's *members* never receive the button's TERTIARY lock and the lock is
never released on them again either. The behaviour is pinned by `PORT-115` (`engine/tests/test_port_115.cpp`).

## 4. What the run of `PORT-115` verified (and what it did not)

* Without the fix of §3, 3 of the 4 `PORT-115` cases fail (19 assertions): the members' TERTIARY lock
  (`isCurWeaponLocked` / `curSlot`) and the release half. The fourth case — a single archer, no contain — passes both
  ways, as it must.
* **The volley itself fired before the fix too.** RW 0x6C8A4E (`ObjectWeapons::chooseBestWeaponForTarget`, lane
  DECOMP-1) picks, *with no victim*, the first slot that is not OUT_OF_AMMO, not a MeleeWeapon and has
  `NoVictimNeeded` — the `AutoChooseSources` mask is not consulted in that branch (the mask is the with-victim branch,
  RW 0x6C831C). The members' bombard bow (`MordorArcherMissileBowBombard_New`, `NoVictimNeeded = Yes`,
  `BombardType = Yes`) is such a slot, so an unlocked member shoots it at the position as well. What the lock changes
  is that the choice stays: RW 0x6C8A4E returns at once on a locked set, so a victim that appears mid-Barrage (auto
  acquire) cannot switch the member to a bow whose `AutoChooseSources` for TERTIARY is "Not from AI"
  (`mordorarcher.ini:648-649`) — and the release half frees them again afterwards.
* The horde's *own* temporary lock is short-lived by design: the horde machine's `HORDE_WAIT` state
  (`HordeAIUpdateCombat.cpp:983`, `AIWaitUntilFinishedFiringState`) releases it on exit — RW 0x742D42:
  `[ecx+0x18] -> [eax+0x14]` (the object), `push 1` (LOCKED_TEMPORARILY), `call 0x68DF11`. RW 0x68DF11 also walks
  the contain (slot 0x16C), so in RW that exit frees the members as well; the port's exit (`AIAttack.cpp:1069`, lane
  WEAPON-1) calls the inner weapon-set release only. That is the rest of S-2480's pass-through, not this task's break,
  and it is left untouched here (`PORT-115` looks at the horde's lock on the approach frame, before its first shot).
* The rig's horde carries `UseCrushAttack = No` like every archer horde of the mod (`mordorhordes.ini:448`, "Archer
  hordes cannot crush or Bombard will not work"): RW 0x74F21A returns SUCCESS on it (template + 0x60D), while the
  default yes needs a goal object and the horde machine's squish state fails without one. With the field absent the
  Barrage order dies in the squish state — that is the mod's own comment about the button, and the port parses the
  field (`SquishCollide.cpp:209`).
* **Not reproduced:** the owner's in-game symptom (the button does nothing at all). In the rig, faithful to the mod
  data, the click already produced the members' volley before the fix. The first step the rig cannot cover is the
  client's target click, which S-1982 records as not read from RotWK: `GUICommandTranslator.cpp:157-201` builds
  MSG_DO_WEAPON_AT_LOCATION from ZH's translator (INFERENCE), and the scatter radius cursor of
  `RadiusCursorType = ScatterAreaRadiusCursor` is not part of that branch. That is FACT-A's lane, not this task's
  mechanism.

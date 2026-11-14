# PORT-160 (issue #378): EVA on capture / being captured / lost, and the camp-destroyed EVA — RW analysis

RotWK 2.01 `game.dat` (md5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001). Addresses are virtual (image base 0x400000).

## What was read, and from where

The sandbox of this session has no access to the game files: only GitHub / npm / pypi are reachable, the Drive folder `for-arena`
cannot be downloaded (`drive.google.com` and `www.googleapis.com` do not answer) and `workspace/RotWK/game.dat` does not exist. **No
new instruction of RotWK was read here** with `tools/mod/rw.py dis`. Every RW address below is therefore taken from the evidence this
repository already holds, and each one is named with the session that read it:

* `docs/ennorath/arena-triage/batch-10.md` rows 5, 6 and 7 — read 2026-11-09 straight from `game.dat` by the field audit: the three
  `CastleMemberBehavior` events, their data offsets, their parser, and the whole reader chain down to `TheEva`;
* `engine/src/Common/Audio/EvaEvents.cpp` — the 22 predefined event names at RW 0xBF2168 and the record defaults at RW 0x5DDDF5,
  written by lane AUDIO-2 from `rw.py dis`;
* `engine/src/GameLogic/Module/SpecialAbilityModules.cpp` — the capture's RW range 0x8544F2 .. 0x854772 read instruction by
  instruction by lane HERO-2 (its comments cite each step). The port says there that "the radar / EVA / drawable calls are the
  client's" and does not carry their addresses;
* `docs/STOPS.md` S-951 / S-1225 — the port's own record that the camp EVA (RW 0x799440) is missing;
* the BFME2 1.06 decompilation (`/tmp/open-bfme-2`, byte-matched to BFME2's `game.dat`) as **DONOR** only.

Anything not in those sources is marked INFERENCE below; nothing is guessed from a name.

## 1. The camp-destroyed EVA (the P-91 half that is fully proven)

| step | what it is |
|---|---|
| data 0xC30A68, +0x08, parser RW 0x797F69 | `CampDestroyedOwnerEvaEvent` (default 9) |
| data 0xC30A78, +0x0C, parser RW 0x797F69 | `CampDestroyedAllyEvaEvent` (default 0xA) |
| data 0xC30A88, +0x10, parser RW 0x797F69 | `CampDestroyedAttackerEvaEvent` (default 8) |
| RW 0x6972C4 (the button's abandon) / RW 0x77C2B7 (the dispatcher's pack) -> RW 0x79CB47 | the castle's abandon |
| RW 0x79CB5C | the abandon calls RW 0x799440 (a direct call site: not dead code) |
| RW 0x79951F / 0x799560 / 0x7995C2 | the reader of +0x08 (owner) / +0x0C (ally) / +0x10 (attacker), chosen against the local player |
| RW 0x7995DB -> RW 0x5DD9EE | the chosen event reported to `TheEva` at the castle |

The donor has the same three fields with the same defaults (`Behavior/BehaviorModuleDataBuildFieldParse.cpp:380-382`,
`CastleMemberBehavior.cpp:103`), so the mechanism is BFME2's too.

Port: `CastleBehavior::abandon()` carried the comment "RW 0x799440: the camp-destroyed EVA of the local player (client)" and did
nothing; S-951 and `CastleBehavior::stopLines()` listed RW 0x799440 as not ported. That was the divergence. The port now reports the
chosen event from `reportCampDestroyedEva()` at the castle through `AudioApi::reportEva` (RW 0x5DD9EE), before the state changes as
RW 0x79CB5C does.

## 2. The capture EVA ("building captured", "the enemy is capturing our building", "building lost")

Port path: `SpecialAbilityUpdate::startPreparation()` (RW 0x853D31 .. 0x853ED0, the `0x1D` / `0x1A` capture types) and
`SpecialAbilityUpdate::captureTarget()` (RW 0x8544F2 .. 0x854772, lane HERO-2). Both say the client calls (radar / EVA / drawable)
are not carried.

DONOR: the same two places report to `TheEva`, only for a **locally controlled target**:

* `Object/Update/SpecialAbilityUpdate.cpp:1347` (type 0x1D) and `:1364` (type 0x1A), the preparation:
  `if (target->isLocallyControlled()) TheEva->reportEvaEvent((EvaEventID)0x10, &target->m_position, 0);` — 0x10 = 16 =
  `BuildingBeingStolen`;
* `Object/Update/SpecialAbilityUpdateTriggerAbilityEffect.cpp:784`, the capture itself, before the team changes:
  `if (target->isLocallyControlled()) TheEva->reportEvaEvent(EVA_CAPTURED /* = 17 */, &target->m_cachedPos, 0);` — 17 =
  `BuildingStolen`.

The ids are the indices of the predefined name table: `DefaultEvaEvent` 0 .. `WallsBeingClimbed` 15, `BuildingBeingStolen` 16,
`BuildingStolen` 17, `WorldMustBattle` 18 .. (`EvaEvents.cpp`, RW 0xBF2168; the donor's `GameClient/EvaMessageName.cpp:88-91` has the
same names in the same order). The port's Eva is name based, so the port reports the **names**.

INFERENCE (stated, not verified against RotWK 2.01): that RotWK kept the donor's two report sites and its `isLocallyControlled` guard
verbatim. The capture body is a function RotWK did change around (the 0x1A recharge RW 0x854764 is RotWK's), and no session with the
binary read the report instructions inside RW 0x853D31 .. 0x853ED0 / 0x8544F2 .. 0x854772. Throttling is not in the code: it is the
record's `TimeBetweenEventsMS` (RW 0x5DDDF5, default 20000), which the port's `Eva` already applies.

## 3. What the port does now (one mechanism each, no second path)

1. `CastleBehavior::reportCampDestroyedEva()` (called first by `abandon()`): the owner's event when the castle's player is the local
   player, the ally's when the local player is its ally, else the attacker's; at the castle's position; through `AudioApi::reportEva`.
   The names come from the castle's own `CastleMemberBehavior` data. A camp of no player (a neutral flag) reports nothing: there is no
   player to be related to.
2. `SpecialAbilityUpdate::startPreparation()` reports `BuildingBeingStolen` when the preparation starts on a locally controlled target
   (both capture enums, 0x1D and 0x1A) and `captureTarget()` reports `BuildingStolen` when the capture lands on one.

## 3a. How the tests see it (`PORT-160*`)

The tests read the result out of TheEva's own records — the index of RW 0xBF2168 the report resolved to and the position the report carried
(`Eva::lastPlayedPosition`), plus `Eva::counters().reported` for "was anything reported at all" — so a passing test means the port reported
*that* record *at that object*, not merely that some sound was requested. The audio manager and a simulated device are installed because
TheEva belongs to the audio owner (RW 0x5DD9EE is the audio thread's), the camp flag unpacks its castle so `abandon()` runs on a real castle,
and the capture half drives `SpecialAbilityUpdate` through `initiateIntentToDoSpecialPower` on a combat world (the locomotor store, the map and
the AI its approach needs).

Evidence that the tests hold the fix (revert check in this session): with `CastleModules.cpp` / `CastleModules.h` / `SpecialAbilityModules.cpp`
taken back to `dcea2b2` the same run fails — `3 test cases: 1 passed, 2 failed; 129 assertions: 10 failed` (the camp's three events and both
capture reports, one assertion each) — and with the fix `3 passed / 129 passed`.

## 4. Open (needs the owner or a session with `game.dat`)

* the instruction addresses of the two capture reports inside RW 0x853D31 .. 0x853ED0 and RW 0x8544F2 .. 0x854772 — `rw.py xref` of
  RW 0x5DD9EE restricted to those ranges — to turn the INFERENCE above into a fact (stop S-7326);
* whether the `target->isLocallyControlled()` guard survived into 2.01 unchanged;
* `CastleMemberBehavior/StoreUpgradePrice` (data +0x18) stays unread by RotWK (batch-10 row 8) and unported.

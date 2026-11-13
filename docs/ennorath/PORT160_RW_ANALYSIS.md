# PORT-160 (issue #378): EVA on capture / being captured / lost, and the camp-destroyed EVA — RW analysis

RotWK 2.01 `game.dat` (md5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001). Addresses are virtual (image base 0x400000).

## What was read, and from where

This sandbox has no access to the game files: only GitHub / npm / pypi are reachable, the Drive folder `for-arena` cannot be
downloaded (`drive.google.com`, `www.googleapis.com` unreachable) and `workspace/RotWK/game.dat` does not exist. No new
instruction of RotWK was read here with `tools/mod/rw.py dis`. Every RW address below is therefore taken from **the evidence this
repository already holds**, and each one is named with the session that read it:

* `docs/ennorath/arena-triage/batch-10.md` rows 5, 6, 7 — read 2026-11-09 directly from `game.dat` (the audit had the binary): the
  three `CastleMemberBehavior` events, their data offsets, the parser and the whole reader chain down to `TheEva`;
* `engine/src/Common/Audio/EvaEvents.cpp` — the 22 predefined event names at RW 0xBF2168 and the record defaults at RW 0x5DDDF5,
  written from `rw.py dis` by lane AUDIO-2;
* `engine/src/GameLogic/Module/SpecialAbilityModules.cpp` — the capture's RW range 0x8544F2 .. 0x854772, read instruction by
  instruction by lane HERO-2 (its comments cite each step); the port notes there that the "radar / EVA / drawable calls are the
  client's" but does not carry their addresses;
* `docs/STOPS.md` S-951 / S-1225 — the port's own record of what is missing (the camp EVA RW 0x799440 is listed as not ported);
* the BFME2 1.06 decompilation (`/tmp/open-bfme-2`, byte-matched to BFME2's `game.dat`) as the **DONOR** only.

Anything not in those sources is marked INFERENCE below; nothing is guessed from a name.

## 1. The camp-destroyed EVA (CastleMemberBehavior, the P-91 half)

RW chain, all read in batch-10 (rows 5-7):

| step | what |
|---|---|
| data 0xC30A68 / +0x08, parser RW 0x797F69 | `CampDestroyedOwnerEvaEvent` (default 9) |
| data 0xC30A78 / +0x0C, parser RW 0x797F69 | `CampDestroyedAllyEvaEvent` (default 0xA) |
| data 0xC30A88 / +0x10, parser RW 0x797F69 | `CampDestroyedAttackerEvaEvent` (default 8) |
| RW 0x6972C4 (the button's abandon) / RW 0x77C2B7 (the dispatcher's pack) -> RW 0x79CB47 | the castle's abandon |
| RW 0x79CB5C | the abandon calls RW 0x799440 (not dead code: a direct call site) |
| RW 0x799440 -> RW 0x79951F / 0x799560 / 0x7995C2 -> RW 0x7995DB -> RW 0x5DD9EE | the owner / ally / attacker event chosen against the local player and reported to `TheEva` |

The donor has the same three fields with the same defaults (`Behavior/BehaviorModuleDataBuildFieldParse.cpp:380-382`,
`CastleMemberBehavior.cpp:103`), so the mechanism is BFME2's too; the defaults are the port's (`CastleModules.h`).

Port: `CastleBehavior::abandon()` (`engine/src/GameLogic/Module/CastleModules.cpp`) carries the comment "RW 0x799440: the
camp-destroyed EVA of the local player (client)" and does nothing; S-951 and `CastleBehavior::stopLines()` list RW 0x799440 as not
ported. This is the divergence: the abandon runs, the report is missing.

## 2. The capture EVA ("building captured", "the enemy is capturing our building", "building lost")

Port path: `SpecialAbilityUpdate::startPreparation()` (RW 0x853D31 .. 0x853ED0, the `0x1D` / `0x1A` capture types) and
`SpecialAbilityUpdate::captureTarget()` (RW 0x8544F2 .. 0x854772). Both carry their RW addresses per instruction (lane HERO-2),
and both say the client calls (radar / EVA / drawable) are not carried.

DONOR: the same two places report to `TheEva`, and only for a **locally controlled target**:

* `Object/Update/SpecialAbilityUpdate.cpp:1347` (type 0x1D) and `:1364` (type 0x1A), the preparation:
  `if (target->isLocallyControlled()) TheEva->reportEvaEvent((EvaEventID)0x10, &target->m_position, 0);` — 0x10 = 16 =
  `BuildingBeingStolen`;
* `Object/Update/SpecialAbilityUpdateTriggerAbilityEffect.cpp:784`, the capture itself, before the team changes:
  `if (target->isLocallyControlled()) TheEva->reportEvaEvent(EVA_CAPTURED /* = 17 */, &target->m_cachedPos, 0);` — 17 =
  `BuildingStolen`.

The ids are the indices of the predefined name table: `DefaultEvaEvent` 0 .. `WallsBeingClimbed` 15, `BuildingBeingStolen` 16,
`BuildingStolen` 17, `WorldMustBattle` 18 .. (`EvaEvents.cpp`, RW 0xBF2168; the donor's `GameClient/EvaMessageName.cpp:88-91` has the
same names in the same order). The port's Eva is name based, so the port must report the **names**.

INFERENCE (stated, not verified in RotWK 2.01): that RotWK kept the donor's two report sites and the `isLocallyControlled` guard
verbatim. The capture body is one of the functions RotWK did change around (the 0x1A recharge RW 0x854764 is RotWK's), and the exact
instruction addresses of the two reports inside 0x853D31 .. 0x853ED0 / 0x8544F2 .. 0x854772 were not read by any session that had
the binary. Throttling is not in the code: it is `Eva.ini`'s `TimeBetweenEventsMS` of the records (RW 0x5DDDF5 default 20000), which
the port's `Eva` already applies.

## 3. What the port must do (one mechanism each, no second path)

1. `CastleBehavior::abandon()` reports the camp-destroyed event exactly as RW 0x799440 chooses it: the owner's event when the castle's
   player is the local player, the ally's when the local player is its ally, else the attacker's; at the castle's position; through
   `AudioApi::reportEva` (RW 0x5DD9EE). The three names come from the module data (defaults 9 / 0xA / 8 in the Eva table).
2. The capture reports `BuildingBeingStolen` when the preparation starts on a locally controlled target and `BuildingStolen` when the
   capture lands on one.

## 4. Open (needs the owner or a session with `game.dat`)

* the instruction addresses of the two capture reports inside RW 0x853D31 .. 0x853ED0 and RW 0x8544F2 .. 0x854772 (a `rw.py xref` of
  RW 0x5DD9EE restricted to those ranges), to upgrade the INFERENCE above to a fact;
* whether the `target->isLocallyControlled()` guard survived into 2.01 unchanged.

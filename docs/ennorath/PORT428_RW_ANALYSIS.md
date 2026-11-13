# Port 428 / P-111: the fell beast circles over a victim at a supply pile (RW analysis)

Issue #428. `game.dat` = RotWK 2.01, md5 `daed3668…`, caveat S-001. Every address below was
read with `tools/mod/rw.py workspace/RotWK/game.dat dis|xref`. BFME2 1.06 (`/tmp/open-bfme-2`,
`Code/`) is a **donor only**: it gives the shape of the algorithm, RotWK is the source of truth.

## 1. The data chain (mod first, PROCESS 15.11)

- Ennorath `data/ini/object/neutral/supplyrocks.ini`: `Object SupplyRocksPile01` … `SupplyRocksPile*`.
  `KindOf = STRUCTURE IMMOBILE SELECTABLE SUPPLY_SOURCE_ON_PREVIEW IGNORED_IN_GUI NO_COLLIDE
  SUPPLY_SOURCE`; `Geometry = BOX`, `GeometryName = Geom_Orig`, `GeometryMajorRadius = 35.0`,
  `GeometryMinorRadius = 25.0`, `GeometryHeight = 15`. A **low, wide** static box (top at z=15).
  Four `GeometryContactPoint … Repair` rows belong to the pile itself.
- Ennorath `data/ini/object/custom/phoenix.ini`: the `GiantBirdAIUpdate` block of the flyers
  (`SpecialContactPoints = Swoop`, `FollowThroughDistance`, `AttackPriority`, …). `MordorFellBeast`
  is the child object in `data/ini/object/evilfaction/units/mordor/fellbeast.ini`; its LocomotorSet
  uses `FellBeastLocomotor`. The same module drives Gwaihir, Drogoth, Smaug and the phoenix, so a fix
  belongs in the shared aerial path, not in one template.
- The victim is an infantry unit standing inside the pile's 2D footprint. The bird's swoop goal is
  the victim's contact point, i.e. a point on/near the ground inside that same footprint.

## 2. The attack / approach chain in RotWK

### 2.1 The strike (`AIGiantBirdAttackState`, enter RW 0x76B668) — already ported, not touched

- RW 0x76B6D6: the victim is found by AI `+0x584` through `0x449681`; dead (`Object +0x458` bit 0
  at 0x76B6F2) → failure.
- RW 0x76B6FF → `call 0x667C76` (`findNearestLabeledContactPointOnTarget`, `preferred = false`);
  RW 0x76B715…0x76B73F on false → `call 0x690BD2` (`getWorldspaceBestContactPoint`, seeded,
  `0x37B`). This is the contact point / mode 1 (`RW 0xAD3187` switch inside `RW 0xAD30E0`) that
  **Port 164 (#387) owns**; issue #428 explicitly says not to touch it.
- RW 0x76B744…0x76B79E: `range = min(3 * speed, 1500)`-style clamp to 8.5 (`RW 0xC2E190`), then
  `0x5E3BE4` (3D squared distance) against `range²`; beyond it → `STATE_FAILURE` (RW 0x76B796),
  the swoop restarts. So the strike needs the bird within a few tens of units of the contact point.
- RW 0x76B79E…0x76B8B3: the grab branch; RW 0x76B8BD…0x76BA05: the normal fire (`0x69036C`,
  `0x69200A`) and `noteStrike`. The port's `AIGiantBirdAttackState::onEnter`
  (`engine/src/GameLogic/Module/GiantBirdAIUpdate.cpp`) matches these branches.

### 2.2 The approach (`AIGiantBirdSwoopState`, enter RW 0x76DB0E, update RW 0x76DF1D)

- The swoop builds the aerial path once on entry (`computeAerialPath`, the single `call` xref of
  RW 0x778413 is RW 0x769ECA in the bird's `buildPath`), then every frame RW 0x76DF1D:
  - RW 0x76DFC4 `call 0x76E154` (`refreshTarget`);
  - RW 0x76DFDD level swoops use `checkAhead` (`0x76A80A`), otherwise RW 0x76E03E
    `call 0x76A933` → `steer`; `steer` re-builds the path when the path is blocked
    (`checkAhead` → 1: raise z by 5, `MASK_A_LOWER_START`, RW 0xBDAE58; → 2: sink by 1,
    RW 0xBD1908);
  - RW 0x76E046: an invalid spline ends the swoop; RW 0x76E0C0…: arrival test.
- **Consequence**: if every sampled point of the path is reported blocked, `checkAhead` returns 1
  on every frame, `steer` rebuilds the path every frame and the bird circles above the victim. That
  is P-111. There is no separate "give up" branch in the swoop for this case: `computeAerialPath`
  returns true (RW 0x778C66) except when `snap` is set and the flyer is not in the logic
  (RW 0x77848B); the give-up paths that do exist are the follow-through's 41-frame limit
  (RW 0x76BB6A) and the attack state's distance gate above.

### 2.3 The pathfinder's answer for an air locomotor (`computeAerialPath`, RW 0x778413)

- RW 0x778477…0x7784D8: with `snap`, `0x6FE456` (`Pathfinder::adjustDestination`, the AI's
  surfaces) and `0x68B3BD` reserve the goal; z is preserved.
- RW 0x778535…0x778600: the behind point, the flyer, the goal, the terminal point beyond it;
  `MASK_*` bits choose the lowered start / end / direct variants (RW 0xDE7908…0xDE7918).
- RW 0x778747 `call 0x77824E` probes the terminal point. When it is blocked,
  RW 0x778759…0x77891E searches the circle around the goal (atan2 `0x441BF4`, `sin`/`cos`
  `0xA3CF90` / `0xA3CF84`, steps of 0.2 `RW 0xBDAD78`, up to pi `RW 0xBDD388`, the free point
  turned 0.4 further `RW 0xBE0DE4`): the first free point becomes the terminal, the second is
  appended before the goal (RW 0x778920…0x77892E). With **no** free point RW 0x77891E falls through
  to 0x778934: the original (blocked) terminal is kept, `goal` and `terminal` are appended
  (0x778968 / 0x778972). The port's `computeAerialPath` already reproduces this search.
- RW 0x778989…: the "collision test for terrain and objects" walk (half the bounding sphere radius,
  at most 200 steps, five corrective points).

### 2.4 The blocked-run test — **where the port departs**

- `pathClear` RW 0x778176: the point `d` along the spline, z lowered by `clearance/2`
  (`RW 0xC2ECBC`), must be above the terrain and clear of `separation`.
- `probe` RW 0x77824E: five ground samples (centre at z − h/2, four corners at
  z − h/2 * 0.777), then `separation` adds the object overlaps; 1 when anything intrudes, 2
  without an AI, else 0.
- `separation` RW 0x777F9F — read instruction by instruction:
  - RW 0x777FD2 `call [eax+0x184]` → `[eax+0x584]` is the partner (the victim, ignored);
  - RW 0x777FF7 `call 0x67C5F3` builds `PartitionFilterWouldCollide` from `[esi+0xa8]`
    (**the flyer's own GeometryInfo**), the probed `pos`, the angle `[esi+0x44]` and the flag `1`;
  - RW 0x77801D `call 0xa39340` is the partition range query (`FROM_BOUNDINGSPHERE_3D`);
  - RW 0x77803D self skip, RW 0x77804A partner skip, RW 0x77805C `test ch,3` = INFANTRY (bit 8)
    / CAVALRY (bit 9) skip, RW 0x778065 `test byte [eax+0x115],0x20` = HORDE (bit 109) skip,
    RW 0x778072 `test cl,4` = IMMOBILE (bit 2) skips the 1.2-radius distance gate;
  - RW 0x7780A0…0x778144 adds `2*r_other + r_self - distance` to the push and clears `clear`.
    Bit indices from `BitFlagNames.cpp` (`TheKindOfNames`, RW 0xDA0E68); the port already has all
    four of these KindOf skips.
- `PartitionFilterWouldCollide::allow` RW 0x66139C (the filter's vtable RW 0xC112C8): it passes
  the filter's geometry/position/angle and **the candidate object's** `+0xa8` geometry, `+0x38`
  position and `+0x44` angle to `call 0xad2ce0` (RW 0x6613C2), then
  `sub al,[esi+0x1c]; neg al; sbb eax,eax; inc eax` — the object passes only when the **3D shape
  overlap** equals the desired flag. RW 0xAD2CE0 is the height-aware shape test: a BOX of height 15
  at z=0 does not overlap a flyer whose shape starts above z=15, regardless of their x/y footprints.
- DONOR BFME2 0x00375C28 (`Code/GameEngine/Source/GameLogic/AI/AerialPathfinder.cpp:184`,
  `AerialPathfinder::rva00375C28`) shows the same loop and its filter; DONOR BFME2
  `Code/GameEngine/Source/Common/GapBodiesGE07.cpp:146` `Rva00261603Filter::allow` calls
  `GeometryInfo::CollidesWith` — the 3D overlap — not a 2D footprint test. RotWK's own instructions
  above (0x66139C → 0xAD2CE0) confirm the donor is still what RotWK does.

### 2.5 The port

`engine/src/GameLogic/AI/AerialPathfinder.cpp`, `separation()`: the would-collide filter is
`BuildPlacement::footprintsOf` + `footprintsCollide`, a **2D** footprint test (recorded as stop
S-4041 / S-1020). It ignores z, so the low supply pile is reported as an obstacle at *every*
altitude the bird flies at. `probe` → `pathClear` → `checkAhead` therefore return "blocked" for the
whole swoop; `steer` rebuilds the path each frame and the bird never reaches the contact point,
never satisfies the attack state's distance gate (RW 0x76B79E) and never strikes. This explains
why the symptom appears only when the victim stands inside/next to a wide static object's footprint.

RW 0xAD2CE0 **is already ported** (`NotifyCrushModules::geometriesOverlap`,
`engine/src/GameLogic/Module/NotifyCrushModules.cpp`, lane SIEGE-1 / FLANK-1) with the height
rules of RW 0xAD2CE0, so the first departure can be fixed in the shared mechanism, once, for all
GIANT_BIRD flyers and all low obstacles. No new simulation state, no new path, no new handler:
the existing `separation` filter is changed to ask the 3D overlap.

## 3. What stays unported / not touched

- The contact point's mode 1 at RW 0xAD3187 (Port 164, #387) and the port's S-1281 fallback: not
  touched by #428.
- The no-fly zones (`AerialPathfindNoFlyZone`, RW 0x777E54 answers 0) and the x87 / CRT parity of
  the terminal circle search (S-081): S-4041, unchanged.
- RW's separation uses the object's own `GeometryInfo` (`Object +0xa8`); the port's
  `Object::geometryShapes()` is its equivalent (lane MODCOMPAT-MOD1).
- RW 0xA39340's exact partition iteration order and the `GeometryInfo::CollidesWith` shape-height
  asymmetry are ported through `NotifyCrushModules::geometriesOverlap`; their remaining parity
  limits are S-081 / S-1020 and are named in `docs/STOPS.md`.

# Port 16: supply-source conditions (deferred)

Issue #72 (deferred; see `docs/STOPS.md` `S-7227`–`S-7230` and Local 30 / issue
#246). Static reads below are from RotWK 2.01 `game.dat` with
`tools/mod/rw.py dis`. This is an analysis note, **not** an implementation on
`main`: no engine or test changes from this task are merged into `main` (draft
isolated helpers and synthetic tests remain in branch history at
`arena/cbb122b1-openbfme-godot` commit `831e0a47` for reference only), no game
or mod files are included, and the port continues to report both
`SUPPLY_SOURCE_SAFE` (105) and `SUPPLY_SOURCE_ATTACKED` (106) as unported.

## Dispatch and player AI identity (`initFromDict` / `setPlayerType`)

* Dispatcher RW 0x7EB7CD indexes its jump table with ordinal minus 5. Ordinal
  105 (`SUPPLY_SOURCE_SAFE`) goes through RW 0x7ECB46 to RW 0x7E5640; ordinal
  106 (`SUPPLY_SOURCE_ATTACKED`) through RW 0x7ECB6D to RW 0x7E56C1. Both
  handlers iterate matching players until one answers true (RW 0x7E5681–0x7E56BD
  and RW 0x7E56D9–0x7E5705).
* RW 0x6AA6CD reads `Player +0x2FC`; a null pointer returns true for SAFE,
  otherwise it jumps to RW 0x8F7573. RW 0x6AA6E1 returns false for ATTACKED
  without that pointer, otherwise jumps to RW 0x8F5ABD.
* `Player +0x2FC` is **not** the manager-owned `AISkirmishPlayer` created by
  RW 0x8EDF09 → RW 0x6C7C27. Instead, `Player::setPlayerType` (RW 0x6AA450,
  allocation block RW 0x6AA45E–0x6AA4ED) allocates `Player +0x2FC` via
  RW 0x8F7F2B (`AIPlayer`, vtable 0xC79D98) or RW 0x8F3DF3
  (`AISkirmishPlayer`, which calls RW 0x8F7F2B and replaces the vtable with
  0xC79788 at RW 0x8F3E1B). Checking `SkirmishAIManager::findAI()` alone
  therefore does not establish whether `Player +0x2FC` exists or which vtable
  it carries (`S-7228`).
* Class selection in `Player::setPlayerType` also reads `AIData`: field row
  0xC1CC80 names `ForceSkirmishAI`, parser RW 0x42E558, offset `+0x65`; the
  `AIData` constructor clears it at RW 0x6FFD49. RW 0x6AA47D–0x6AA498 creates a
  `Player +0x2FC` AI when `type == PLAYER_COMPUTER` (1), or when its `skirmish`
  argument is true and `[0xDE4938]+0x96C` is nonzero (parsed from
  `SkirmishAIData` via RW 0x6A9456–0x6A9467; row 0xC13CC8 is
  `MakeAllSkirmishSidesAIControlled`, parser RW 0x42E558, data offset `+0x95C`,
  already parsed by the port's `SkirmishAIStore`). RW 0x6AA49A–0x6AA4AA then
  selects `AISkirmishPlayer` (RW 0x8F3DF3) if the `skirmish` argument or
  `AIData +0x65` (`ForceSkirmishAI`) is nonzero, otherwise base `AIPlayer`
  (RW 0x8F7F2B). On `main`, `ForceSkirmishAI` is not consumed and
  `Player::setPlayerType` stores only `PlayerType` and a boolean flag.
* `Player::initFromDict` (RW 0x6B08D6–0x6B0A26) determines both arguments to
  `setPlayerType` (called at RW 0x6B09FA / RW 0x6B0A20):
  * RW 0x6B08D6–0x6B0905 initializes both local flags (`skirmishMatch = 0`,
    `noMatch = 0`) and skips the side search when the player is an observer or
    the `playerIsSkirmish` key is unset/false.
  * Otherwise RW 0x6B090B–0x6B0971 iterates the saved map-side records stored on
    `SidesList` at `+0x7C0` / `+0x7C4` (getter RW 0x6AA31E). These are **not**
    the lobby slots: RW 0x7319EC–0x731A25 copies the map sides removed during
    skirmish setup (WorldBuilder `Skirmish<Side>` entries on retail MP maps)
    into a temporary array and RW 0x731D72–0x731D94 stores that array and count
    at `SidesList +0x7C0` / `+0x7C4` (still unported on `main`, `S-271`).
  * For each saved record, RW resolves its `PlayerTemplate` and compares
    `PlayerTemplate::Side` (`+0x18`) with the initialized `Player::m_side`
    (`+0x58`, copied at RW 0x6B051E–0x6B052D). A match sets `skirmishMatch = 1`
    (passed as `skirmish = true` at RW 0x6B0A0E, even for human players);
    finding no matching saved side sets `noMatch = 1` at RW 0x6B0997, which at
    RW 0x6B09F5–0x6B0A26 forces `setPlayerType(PLAYER_HUMAN, false)` even when
    `playerIsHuman` is false.
  * On `main`, `PlayerList::newGame` (`engine/src/Common/PlayerList.cpp:172`)
    instead passes `!human && skirmish`, and synthetic skirmish test maps
    (`P16TestUtil.h`, `test_start_random.cpp`, `test_logic_players.cpp`) carry
    no `Skirmish<Side>` map sides, so enabling `noMatch -> PLAYER_HUMAN`
    without coordinated fixture and `Player` changes would flip AI slots to
    `PLAYER_HUMAN` (`S-7228`).

## SAFE: source selection, enemy bounds, base center, and visibility

* **Per-condition frame cache (`+0x44` / `+0x48`) and `GenericCopy`:**
  RW 0x7E564F–0x7E566C reuses `Condition +0x44` when `frame <= +0x48` and the
  cached int is `-1` or `1`; otherwise it writes `frame + [0xD9F608]` (`5`
  frames, 1 second) to `+0x48`, evaluates, and stores `-1` for false or `1` for
  true (RW 0x7E56AB / 0x7E56B6). Constructor RW 0x7B3216–0x7B3219 zeroes both
  fields, so frame 0 still evaluates (`+0x44 == 0` misses `-1` / `1`). For
  per-team generic scripts, RW 0x7A1759 duplicates the script via RW 0x7B7F2B →
  0x7B7E23, cloning the condition list via RW 0x7B76E1 → 0x7B655B (constructor
  RW 0x7B651B zeroes the clone's `+0x44` and `+0x48`). Because the port's
  `GenericCopy` shares the source `Script` / `ScriptCondition` definitions, any
  runtime cache must key per script instance and participate in `crc` / `xfer`.
* **Top-level `SAFE` flow (`RW 0x8F7573`):** RW 0x8F757A calls the supply-source
  chooser RW 0x8F6B5A with `minResources` (condition parameter 1). When RW
  0x8F6B5A returns null, SAFE returns `true` (RW 0x8F7581–0x8F7585); otherwise
  RW 0x8F7587–0x8F7590 passes the chosen object's position and `ThingTemplate`
  to `isSupplySourceSafe` (RW 0x8F5220).
* **`acquireEnemy` / `getAiEnemy` (`RW 0x8F47EB` → `RW 0x8F457D`, `S-7227`):**
  RW 0x8F6B87 invokes vslot `+0x30` on `this` (`Player +0x2FC`). Both Player AI
  vtables (`AIPlayer` 0xC79D98 and `AISkirmishPlayer` 0xC79788) end at slot
  `+0x68` (RW 0x8F994F; RTTI/type strings start at `+0x6C`). Base slot `+0x30`
  (0xC79DC8) is RW 0x851E97 (`return nullptr`); derived slot `+0x30` (0xC797B8)
  is `AISkirmishPlayer::getAiEnemy` (RW 0x8F47EB), the exact method also called
  by `Player::getCurrentEnemy` (RW 0x6AA5F5). RW 0x8F47F3–0x8F4814 checks
  `frame >= +0x9C`, writes `frame + [0xD9F608] * 5` (25 frames) to `+0x9C`
  first, calls `acquireEnemy` (RW 0x8F457D–0x8F47EA), and returns `+0xA0`.
  `acquireEnemy` walks `Player +0x34C` (`TeamPrototype` → `Team` → members) for
  `hasAnyUnits` (RW 0x6ABD44 → 0x7A07D8), `hasAnyBuildFacility` (RW 0x6ABE2C →
  0x7A1129, testing `ThingTemplate +0x5F6`), `hasAnyObjects(0)` (RW 0x6ABDF7 →
  0x7A0956) and `getPlayerStructureBounds` (RW 0x8F6511), measures 2D SSE
  squared distance from `AIPlayer +0x38` (`m_baseCenter`), and adjusts scores
  across other skirmish AIs via `Player::getCurrentEnemy` (`+250000.0f` if
  already targeted, `-625.0f` if targeting the caller, clamped at `0.0f`). On
  `main`, `engine/src/GameLogic/SkirmishAI/AITacticalAI.cpp:3395–3555` (#68 Do
  not touch) keeps a separate manager-side enemy cache (`enemyIndex` /
  `enemyRefreshFrame`) using `ai.build.startPosition`, world-object scans and
  `skirmishAI().findAI(j)` (`S-7227`).
* **`getPlayerStructureBounds` (`RW 0x8F6511`):** When an enemy player is
  returned, RW 0x8F6B91–0x8F6BC8 calls RW 0x8F6511 and computes the 2D midpoint
  `(hi + lo) * 0.5f` in scalar SSE. RW 0x8F6511 initializes its min/max output
  bounds to `{0.0f, 0.0f}`, walks the player's `TeamPrototype` list (`+0x34C`),
  each prototype's `Team` instances and their member objects, and expands the
  2D X/Y box over every member whose `ThingTemplate` has `STRUCTURE` (`+0x108`
  bit 7, without filtering out dead, destroyed, or under-construction members;
  RW 0x8F6517–0x8F655D, 0x8F6585–0x8F660A).
* **Unreferenced `computeCenterAndRadiusOfBase` (`RW 0x8F6186`):**
  `Player::getBaseCenter` (RW 0x6AA64F–0x6AA66E) reads `Player +0x2FC`, copies
  the three floats at `+0x38..+0x40` (`m_baseCenter`) to the caller's output,
  and returns byte `+0x44` (`m_baseCenterValid`); without `Player +0x2FC` it
  returns `false` without writing the output. `AIPlayer`'s constructor
  (RW 0x8F7F2B) zeroes `+0x38..+0x40` and clears `+0x44`; `newMap` (vslot
  `+0x18`, called from RW 0x6B191B) is RW 0x63F3BF (`ret`) in both `AIPlayer`
  and `AISkirmishPlayer` vtables; and `computeCenterAndRadiusOfBase`
  (RW 0x8F6186, the only code in `game.dat` that computes and writes
  `+0x38..+0x44`) has **zero** direct (`E8` / `E9`) or 32-bit pointer references
  anywhere in any PE section of RotWK 2.01 `game.dat`. Consequently, in every
  new game `+0x38..+0x40` remains `{0.0f, 0.0f, 0.0f}` (`+0x44 = false`), and
  can become nonzero only when loaded from a save via `AIPlayer::xfer`
  (RW 0x8F599C–0x8F59A2). Adjacent wrapper RW 0x6AA671 dispatches AI vslot
  `+0x38` (RW 0x8F78EB in both vtables), which handles object IDs and never
  touches `+0x38..+0x44`.
* **Source chooser loop (`RW 0x8F6B5A` .. `RW 0x8F6DE6`):**
  1. Walks the world object list (`TheGameLogic` first/next object, RW
     0x8F6BD2–0x8F6C73) and accepts only candidates whose `ThingTemplate` has
     both `STRUCTURE` and `SUPPLY_SOURCE`, whose `SupplyWarehouseDockUpdate`
     exists (constructor RW 0x8AB4C0–0x8AB4F1 sets `+0x88` `m_boxesStored` from
     `StartingBoxes`; RW 0x8AB64E–0x8AB675 can recompute it by dividing by
     `GlobalData +0xA5C` `ValuePerSupplyBox`), whose low 32-bit signed `imul`
     `boxesStored * ValuePerSupplyBox` is at least the current threshold
     (RW 0x8F6C57–0x8F6C5D), and whose team relationship to the AI player is
     nonzero (`!= ENEMIES`, RW 0x6ADBEB).
  2. Rejects any candidate that already has the player's own non-`OFF_MAP`
     `SUPPLY_GATHERING_CENTER` nearby (RW 0x8F6C97–0x8F6D36): calls
     `PartitionManager::getClosestObject` (RW 0xA39090 → 0xA3BDB0) from the
     candidate within `boundingCircleRadius + 200.0f` (`0xBE4170`), using
     distance type 1 (`FROM_CENTER_3D`, RW 0xA3A7D0; tie guard `>` at
     RW 0xA3C0D2–0xA3C0E6 and max-distance guard at RW 0xA3BF21–0xA3BF2B) and
     the three-filter chain `0xBD88B0` → RW 0x660DB2 (`KindOf`
     `SUPPLY_GATHERING_CENTER`), `0xC10E38` → RW 0x66107F
     (`controllingPlayer == player`), and `0xC10E2C` → RW 0x660EB1
     (`Object +0x458` bit 3 `OFF_MAP` clear).
  3. Measures squared 2D distance from `AIPlayer +0x38` in scalar SSE binary32
     (`dy*dy + dx*dx`, RW 0x8F6D3B–0x8F6D5E). When an enemy exists, RW
     0x8F6D65–0x8F6D94 computes the squared 2D distance to the enemy structure
     bounds midpoint and compares `0.4 * ownDistSq > 0.6f * enemyDistSq` on the
     x87 stack without intermediate float stores: `0.4` at `0xC79A80` is
     binary64, `0.6f` at `0xBDAD70` is binary32 (`0.6000000238418579`), `ja`
     does not reject unordered operands, and MSVCR71 FP setup RW 0x440809–
     0x440825 (`_controlfp` mask `0x30300`, value `0x20000`) puts x87 in PC24
     (24-bit significand rounding with wide exponent). Among surviving
     candidates, strict `<` keeps the first candidate on equal distance.
  4. If a pass finds no candidate, RW 0x8F6DC1–0x8F6DCF halves the signed
     threshold toward zero (`cdq; sub eax, edx; sar eax, 1`) and repeats only
     while the new threshold is strictly greater than `100`.
* **Safety scan (`RW 0x8F5220`) and the 16 `Contain` vtables (`S-7229`):**
  * RW 0x8F524C–0x8F526D queries radius `ThingTemplate +0xB0` (bounding circle)
    plus `AIData +0x84` (`SupplyCenterSafeRadius`, field row 0xC1CD80, default
    `250.0f` at RW 0x6FFC04 / 0x6FFC7D) using distance type 3
    (`FROM_BOUNDINGSPHERE_3D`, RW 0xA3AEB0–0xA3AF3D) and filters excluding
    `DOZER` / `HARVESTER`, insignificant buildings (RW 0x660F1A),
    effectively-dead objects (RW 0x660E71, `Object +0x458` bit 0, not
    `OBJECT_STATUS_DESTROYED`), non-enemies (RW 0x66110A, `ENEMIES` to the
    player's default team), and invisible objects (`PartitionFilterIsVisible`,
    vtable 0xC0F19C → RW 0x660D71); the result is inverted (`found == nullptr`,
    RW 0x8F5330–0x8F5334).
  * `PartitionFilterIsVisible` (RW 0x660D71) calls
    `Object::getShroudVisibilityStatusForPlayer` (`RW 0x694CCC(obj, player, 0)`),
    and when that hides a stealthed object still accepts it if
    `player->getPlayerType() == PLAYER_COMPUTER` and
    `TheInvisibilityManager->isFiring(obj)` (RW 0x660D89–0x660DA3 → RW 0x8ED15F).
  * Audit of all **16 `ContainModuleInterface` vtables** across the 17
    registered `Contain` modules (`mask: 95` in
    `engine/data/rotwk-201/module-registry.json`) for the calls made inside
    `RW 0x694CCC`:
    * **6 `GarrisonContain`-family classes** (`table 0xC095F0`) —
      `GarrisonContain` (create `0x64B740`), `HordeGarrisonContain`
      (`0x64B77B`), `TunnelContain` (`0x64B9D2`), `SlaughterHordeContain`
      (`0x64BD1F`), `CitadelSlaughterHordeContain` (`0x64BDAE`), and
      `ProductionQueueHordeContain` (`0x64BE3D`) — install **RW 0x87B392** in
      `ContainModuleInterface` slot `+0x4C` (`getApparentControllingPlayer`,
      called at RW 0x694DFB) and `return nullptr` in slots `+0x78` / `+0x7C`.
      RW 0x87B392 returns `originalTeam->controllingPlayer` (`+0xDC`) when
      `GarrisonContain +0x9BD` (`m_hideGarrisonedStateFromNonallies`) is set and
      the observer is not allied to the garrisoning controller; in RW
      0x694DE8–0x694E82, an all-stealth garrison whose first rider is neither
      detector-revealed (RW 0x693BF2) nor `DETECTED` (status `0x11`) hides the
      container iff `observingPlayer->getRelationship(apparentPlayer->defaultTeam) == ENEMIES`.
    * **10 non-garrison `ContainModuleInterface` vtables** (covering the 11
      remaining `Contain` registrations: `OpenContain` `0x64B3D2`,
      `CaveContain` `0x64B47C`, `HealContain` `0x64B4B7`, `TransportContain`
      `0x64B546`, `HordeTransportContain` `0x64B6B1`, `SiegeEngineContain`
      `0x64B899`, `HordeSiegeEngineContain` `0x64B928`, `RiderChangeContain`
      `0x64BA11`, `HordeContain` `0x64B5D5` + `HorseHordeContain` `0x64B664`
      sharing `0xC5B480`, and `AODHordeContain` `0x64B80A`) install `return
      nullptr` (RW 0x46C6BE / 0x851E97) in slot `+0x4C`; in slots `+0x78` /
      `+0x7C` only `HordeContain` (`0xC5B480` → RW 0x872A6F) returns non-null.
    * On `main`, `GarrisonContain +0x9BD` remains unported (`S-1100`), while
      `EvaDamageNotice.cpp` (`S-7090`), `AIClosestEnemy.cpp` (`S-1022`), and
      `EmotionModules.cpp` (`S-326`) each carry partial local copies of
      `RW 0x660D71` / `0x694CCC` (`S-7229`).

## ATTACKED: shared helper state and last-damage record

* `AIPlayer` constructor RW 0x8F7F91–0x8F7F9A zeroes timer `+0x70` and object
  ID `+0x74`. In `RW 0x8F5ABD`:
  * At frame 0, RW 0x8F5ACB–0x8F5ADC sets `+0x70 = 10` and returns `false`.
  * At frames `> 0`, RW 0x8F5ADE–0x8F5B01 clears `+0x74 = INVALID_ID`, returns
    `false` when `frame < +0x70`, and returns `false` when
    `Player +0x374 + 10 < frame` (`m_lastAttackedByFrame`); otherwise it writes
    `+0x70 = frame + 10` before scanning.
  * RW 0x8F5B04–0x8F5BC1 walks `Player +0x34C` team prototypes, their teams and
    members. A member with `KindOf` `SUPPLY_GATHERING_CENTER`, `DOZER`, or
    `HARVESTER` (RW 0x8F5B53–0x8F5B69) matches when its `BodyModuleInterface`
    slot `+0x40` (`getLastDamageInfo`) is non-null, record byte `+0x78`
    (`DamageInfoOutput::m_noEffect`) is `0`, and slot `+0x44`
    (`getLastDamageTimestamp`) satisfies `timestamp + 10 > frame` (strict, RW
    0x8F5B6B–0x8F5B93). On match RW 0x8F5BCE–0x8F5BD7 stores the member's
    `ObjectID` at `AIPlayer +0x74` and returns `true`.
  * `AIPlayer::xfer` (RW 0x8F5756) serializes `+0x74` via RW 0x707887
    (RW 0x8F5786–0x8F578B), `+0x38..+0x40` at RW 0x8F599C–0x8F59A2, and (for
    snapshot version `>= 2`) timer `+0x70` at RW 0x8F5A22–0x8F5A31. A second
    caller at RW 0x8F7441 clears `+0x70` (RW 0x8F744B), calls RW 0x8F5ABD,
    resolves `+0x74` on `true` (RW 0x8F7458–0x8F746A), and falls back to
    RW 0x8F6B5A on `false` (RW 0x8F746C–0x8F7476), so `+0x70` / `+0x74` belong
    on the shared Player AI rather than a condition-private copy (`S-7230`).
* Body vtable audit for slots `+0x40` / `+0x44`: `InactiveBody` (create RW
  0x651113 → ctor RW 0x8C1A06 → vtable 0xC718C8) has slot `+0x40` = RW 0x851E97
  (`return nullptr`); `SymbioticStructuresBody` (vtable 0xC72200) forwards
  slot `+0x40` (RW 0x8C4FD7) and slot `+0x44` (RW 0x8C5100) to the linked
  symbiote's body (or returns `nullptr` / `-1` when absent).
* **`ActiveBody::attemptDamage` last-damage record selection (tracked in
  Local 30, issue #246):** RW 0x8C4294–0x8C42F5 does not overwrite the saved
  damage record on every hit: when the saved frame equals the current or
  previous frame (unsigned comparison, so initial `-1` matches at frame `0`),
  the existing record is kept unless the incoming damage has a live source and
  either the previous source no longer resolves or the incoming source has
  `INFANTRY`, `CAVALRY`, or one of the five significant-building `KindOf` bits
  tested by RW 0x68DAE6 (`FS_POWER`, `FS_FACTORY`, `FS_BASE_DEFENSE`,
  `FS_TECHNOLOGY`, `FS_CASH_PRODUCER`); on replacement RW 0x7441E0 copies the
  full `DamageInfo` including `DamageInfoOutput::m_noEffect` (`+0x78` at RW
  0x744200–0x744203). Because this mechanism is shared across retaliation
  (Local 20), damage FX (FX-4), attacker marking (CAMP-1, RW 0x8C430F–0x8C4341)
  and Port 16, the owner split it out into **Local 30 (#246)**.

## Deferred status

Port 16 (#72) remains open and deferred (`S-7230`) until the shared
prerequisites land:

1. `S-7227` — unified `Player +0x2FC` enemy cache (`RW 0x8F47EB` / `0x8F457D`)
   vs `SkirmishAI/AITacticalAI.cpp`.
2. `S-7228` — `Player::initFromDict` saved-side matching (`RW 0x6B08D6–0x6B0A26`,
   `S-271`) and `Player::setPlayerType` `Player +0x2FC` allocation (`RW 0x6AA450`).
3. `S-7229` — shared `PartitionFilterIsVisible` (`RW 0x660D71` → `RW 0x694CCC`),
   `GarrisonContain +0x9BD` (`S-1100`), and consolidation of `S-7090`, `S-1022`,
   `S-326`.
4. **Local 30 (#246)** — `ActiveBody::attemptDamage` last-damage record
   retention/replacement (`RW 0x8C4294–0x8C42F5`).

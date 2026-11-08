# Port 16: supply-source conditions (work in progress)

Issue #72. Static reads below are from RotWK 2.01 with `tools/mod/rw.py dis`.
This is an analysis note, **not** an implementation or a complete specification.
No game or mod files are included. The port still reports both conditions as
unported.

## Dispatch and player AI identity

* Dispatcher RW 0x7EB7CD indexes its jump table with ordinal minus 5. Ordinal
  105 goes through RW 0x7ECB46 to RW 0x7E5640; ordinal 106 through RW
  0x7ECB6D to RW 0x7E56C1. The two handlers iterate matching players until
  one answers true (RW 0x7E5681–0x7E56BD and 0x7E56D9–0x7E5705).
* RW 0x6AA6CD reads Player +0x2FC; a null pointer returns true for SAFE,
  otherwise it jumps to RW 0x8F7573. RW 0x6AA6E1 returns false for ATTACKED
  without that pointer, otherwise jumps to RW 0x8F5ABD.
* This pointer is **not** the `AISkirmishPlayer` created by RW 0x8EDF09 →
  RW 0x6C7C27 (which belongs to the skirmish manager's group). Player type
  setup RW 0x6AA45E–0x6AA4ED instead allocates the object at +0x2FC via
  RW 0x8F7F2B or RW 0x8F3DF3. The former assigns vtable 0xC79D98; the latter
  calls the former then replaces the vtable with 0xC79788 (RW 0x8F3E1B).
  A test for the existence of a manager `AISkirmishPlayer` alone therefore
  does not establish whether this player has the AI queried here. How this
  identity is represented in the port remains to be resolved.
* The class selection also depends on AIData: field row 0xC1CC80 names
  `ForceSkirmishAI`, parser RW 0x42E558, offset +0x65; the constructor clears
  it at RW 0x6FFD49. RW 0x6AA49A–0x6AA4AA selects the derived constructor
  if the caller's flag or this byte is nonzero, otherwise the base constructor.
  In the port `Player::setPlayerType` stores only type and a flag. A
  reconstruction from `isSkirmishAI()` alone misses the AIData byte's branch.
  The owner allowed the additional parsing change; the parser and setting
  are now present, but the Player AI class selection still does not consume
  the setting.
* Player type setter RW 0x6AA47D–0x6AA498 creates an AI when type is 1, or
  when its skirmish argument is true and `[0xDE4938]+0x96C` is true. RW
  0x6A9456–0x6A9467 parses SkirmishAIData into that object at +0x10; table
  row 0xC13CC8 is `MakeAllSkirmishSidesAIControlled`, parser RW 0x42E558,
  data offset +0x95C. This field is already parsed by the port's
  `SkirmishAIStore`. The type argument and the data flag must be read at the
  setter's time, not inferred later from manager `findAI()`; its object can
  be absent even while Player +0x2FC exists. Correction to the earlier
  analysis: the second argument at RW 0x6B09FA / 0x6B0A20 is NOT simply the
  side's `playerIsSkirmish` value. RW 0x6B08D6–0x6B0905 initializes it to
  zero and skips the check for an observer or an unset/false skirmish key.
  Otherwise RW 0x6B090B–0x6B0971 iterates the saved side records of the
  SidesList at +0x7C0/+0x7C4 (RW 0x6AA31E), resolves each saved record's
  faction template and compares its Side (+0x18) with the initialized player
  Side (+0x58, copied at RW 0x6B051E–0x6B052D). Correction: these are NOT
  the lobby slots. RW 0x7319EC–0x731A25 copies the map sides it removes into
  a temporary array and RW 0x731D72–0x731D94 stores that array and count at
  SidesList +0x7C0/+0x7C4. A Side match sets the argument to 1 at RW
  0x6B0A0E; no match sets a separate byte at 0x6B0997, which affects the
  chosen player type at 0x6B09F5–0x6B0A26. The argument can be passed for
  human players as well. `PlayerList::newGame` currently stores
  `!human && skirmish`, and `SkirmishSides::build` now retains the removed
  records' faction names in a separate list; other saved-side data remains
  S-271. Replacing that expression with the raw dictionary value would miss
  the observer and Side matching. The saved faction list is not consumed yet;
  the setter call's exact lifecycle must be integrated before using either flag.

## SAFE: selection and scan

* RW 0x7E564F–0x7E566C reuses condition +0x44 when the frame is at most
  +0x48 and the cached value is -1 or 1; otherwise it stores the current
  frame plus `[0xD9F608]` at +0x48 and reevaluates. It stores -1 for false
  and 1 for true (RW 0x7E56AB / 0x7E56B6). The condition constructor clears
  both fields at RW 0x7B3216–0x7B3219, so the first evaluation is not a cache
  hit even at frame zero. The generic team-script copy is distinct: RW
  0x7A1759 calls script duplicate RW 0x7B7F2B → 0x7B7E23; RW 0x7B7EBA–
  0x7B7ECA duplicates the condition list via RW 0x7B76E1 → 0x7B655B, whose
  constructor RW 0x7B651B clears the new condition's +0x44 and +0x48. Thus
  a copy's cache starts empty, not shared with or copied from the source.
  The port's `GenericCopy` instead references the source `Script` and its
  `ScriptCondition`, so a mutable cache on the definition alone would be
  incorrect. Any runtime cache design must distinguish each copy and retain
  its identity through hash and xfer.
* RW 0x8F757A asks RW 0x8F6B5A for a source. No candidate returns true
  (RW 0x8F7581–0x8F7585); a candidate supplies its position and template to
  RW 0x8F5220 (RW 0x8F7587–0x8F7590).
* The virtual call at RW 0x8F6B87 is on the AI itself, not on Player. The
  base vtable slot +0x30 at 0xC79DC8 points to RW 0x851E97, which returns
  null; the derived vtable slot at 0xC797B8 points to RW 0x8F47EB, the
  cached enemy player. RW 0x8F47F3–0x8F4814 refreshes at frame +
  `[0xD9F608] * 5` before returning its cached value. The port has an enemy
  helper in `AITacticalAI`, but its state is attached to the separate
  manager AI; equivalence for both +0x2FC implementations is not established.
* When an enemy is returned, RW 0x8F6B91–0x8F6BC8 calls RW 0x8F6511 and
  computes the midpoint of the resulting bounds. RW 0x8F6511 walks the
  enemy's team prototypes and teams and bounds STRUCTURE member positions;
  its initial bounds are zero (RW 0x8F6517–0x8F655D, 0x8F6585–0x8F660A).
  The existing `AITacticalAI.cpp` helper walks the world and filters by
  controlling player instead; those are different traversal criteria, so its
  equivalence for this call is not established.
* RW 0x8F6BD2–0x8F6C73 walks the world object list and rejects candidates
  unless their template has STRUCTURE and SUPPLY_SOURCE, their
  `SupplyWarehouseDockUpdate` exists, its +0x88 multiplied by GlobalData
  +0xA5C is at least the supplied threshold, and RW 0x6ADBEB on the source's
  team returns nonzero. The module constructor RW 0x8AB4C0–0x8AB4F1 sets
  +0x88 from the module data's StartingBoxes; RW 0x8AB64E–0x8AB675 can set
  it again from a value divided by GlobalData +0xA5C. The port exposes
  `boxesStored()` and `valuePerSupplyBox`; RW 0x8F6C57 uses the low 32 bits
  of `imul`, and RW 0x8F6C5A–0x8F6C5D compares them as signed integers.
  Subsequent source updates still need checking.
* RW 0x8F6C97–0x8F6D36 queries within the candidate's bounding-circle radius
  plus the 200.0 constant at 0xBE4170. The query uses distance type 1 and a
  chain whose vtables 0xBD88B0, 0xC10E38 and 0xC10E2C resolve to RW
  0x660DB2, 0x66107F and 0x660EB1 respectively. They test KindOf
  SUPPLY_GATHERING_CENTER, equality of controlling player, and Object
  +0x458 bit 3 clear. A found object rejects this source. Both this query
  and the safety query below call closest-object RW 0xA39090 → 0xA3BDB0.
  RW 0xA3C0D2–0xA3C0E6 rejects a distance greater than the current squared
  bound, not an equal distance. Type 1 is 3D center distance (RW 0xA3A7D0);
  type 3 uses the other object's 3D bounding sphere and geometry center
  height (RW 0xA3AEB0–0xA3AF3D). The result also depends on the spatial
  index and the query's maximum-distance guard (RW 0xA3BF21–0xA3BF2B), so
  it must not be replaced with a simple unbounded object loop.
* Player getter RW 0x6AA64F–0x6AA66E reads the separate Player AI pointer
  at +0x2FC, copies its three position floats from +0x38 to the caller's
  output, and returns its +0x44 byte; with no AI it returns false without
  copying. This getter does not explain when the position is updated. The
  adjacent Player wrapper RW 0x6AA671 dispatches AI vslot +0x38 (both Player
  AI vtables point to RW 0x8F78EB), which handles object IDs and does not set
  that position; it must not be mistaken for a position setter.
* RW 0x8F6D38–0x8F6DA9 compares squared 2D distance from the AI's +0x38
  position, and when an enemy exists rejects a candidate if
  `0.4 * ownDistanceSquared > 0.6 * enemyBoundsMidpointDistanceSquared`.
  This shorthand is not enough for bit-exact reproduction: RW 0x8F6D3B–
  0x8F6D5E computes the own distance with scalar SSE float operations
  (subtract, square each, then add y² + x²). RW 0x8F6D65–0x8F6D94 computes
  the enemy difference, squares, sum and comparison in x87; its 0.4 operand
  at 0xC79A80 is a binary64 constant, but its 0.6 operand at 0xBDAD70 is
  binary32 (approximately 0.6000000238418579). No intermediate `fstp` to
  float occurs in that comparison. `ja` also does not reject unordered
  comparisons. The game's FP setup RW 0x440809–0x440825 calls MSVCR71
  `_fpreset`, `_statusfp`, `_controlfp` with mask 0x30300 and value bits
  0x20000: the project models this as x87 precision control 24. Each x87
  arithmetic operation therefore rounds at PC24 while retaining the wide
  exponent; plain host long-double arithmetic would not reproduce it.
  The first candidate survives an equal-distance tie. RW 0x8F6DC1–0x8F6DCF
  halves the signed threshold toward zero and repeats only while the new
  value exceeds 100.
* RW 0x8F524C–0x8F526D adds template +0xB0 and AIData +0x84. The query
  uses distance type 3 and exclusion filters for DOZER and HARVESTER,
  the insignificant-building predicate RW 0x660F1A, visibility predicate
  RW 0x660D71, not-DESTROYED predicate RW 0x660E71 and relationship-to-team
  predicate RW 0x66110A. The query result is inverted at RW 0x8F5330–0x8F5334.
  The visibility predicate has a special branch for a COMPUTER player and
  a firing object (RW 0x660D89–0x660DA3); simply rejecting every stealthed
  object would not reproduce that branch.

## ATTACKED: separate state and recorded damage

* RW 0x8F7F91–0x8F7F9A initializes the +0x70 timer and +0x74 object ID to
  zero. RW 0x8F5ACB–0x8F5B01 sets the timer to 10 and returns false at
  frame 0. At later frames it clears the ID, returns false before the timer,
  and also returns false if Player +0x374 plus 10 is older than the current
  frame. Otherwise it sets the timer to frame +10 before scanning.
* RW 0x8F5B04–0x8F5BC1 walks the player's team prototypes, their teams and
  members. A SUPPLY_GATHERING_CENTER, DOZER or HARVESTER (RW 0x8F5B53–
  0x8F5B69) must have a body whose slot +0x40 returns a record, with record
  byte +0x78 clear, and whose slot +0x44 plus 10 is strictly greater than
  the frame (RW 0x8F5B6B–0x8F5B93). On success RW 0x8F5BCE–0x8F5BD7 stores
  the member's ID in AI +0x74 and returns true; otherwise it returns false.
  Base Player AI xfer RW 0x8F5756 serializes position +0x38 at RW 0x8F599C–
  0x8F59A2 and, for snapshot version at least 2, timer +0x70 at RW 0x8F5A22–
  0x8F5A31. Correction: RW 0x8F5786–0x8F578B also transfers object ID +0x74
  via RW 0x707887. The routine's RW 0x8F5767–0x8F576C returns early for CRC;
  the port must nevertheless hash state that can affect its simulation.
* The same helper has another caller: RW 0x8F744B clears AI +0x70, RW
  0x8F744F calls RW 0x8F5ABD, and RW 0x8F7458–0x8F746A resolves AI +0x74
  if it returned true. On failure it calls the source chooser RW 0x8F6B5A
  (RW 0x8F746C–0x8F7476). The caller belongs to an action outside this
  issue; a correct implementation must share the helper and its state, not
  add a separate condition-only copy. The port now has an isolated
  `PlayerSupplyAttack` helper with the timer, selected ID, team/member scan,
  hash and xfer; it is not attached to Player AI or called by either path yet.
  Its synthetic tests cover the frame-zero branch, throttling, recent damage,
  no-effect and null records, newest member first, the strict ten-frame
  boundary, and a snapshot round-trip; these are not condition acceptance tests.
* Record +0x78 is `DamageInfoOutput::m_noEffect`: constructor RW
  0x66365E clears it and copy RW 0x744200–0x744203 copies it. ActiveBody
  uses that copy at RW 0x8C42F1–0x8C42F5. The port now stores `m_noEffect`
  with the selected fields of the last damage record; synthetic tests are
  written but have not yet been run.
  Whether this byte can be true on any body path that the condition can scan
  has not been proved. RW 0x8C403F–0x8C4044 clears output floats +0x70 and
  +0x74 but does not clear +0x78 at those instructions; on the recording
  branch the copy includes it. This is not by itself proof of a reachable
  true byte. InactiveBody is not evidence that the scan sees a true record:
  create RW 0x651113 calls constructor RW 0x8C1A06, which installs body table
  0xC718C8 at RW 0x8C1A38; its slot +0x40 (0xC71908) is RW 0x851E97,
  which returns null, although its damage slot RW 0x8C1A75 can set the input
  output byte. SymbioticStructuresBody's body table 0xC72200 instead has
  slot +0x40 RW 0x8C4FD7 and +0x44 RW 0x8C5100, which forward to the
  resolved symbiote's body or return null / -1 when it is absent. The port
  now forwards both last-record accessors and the damage-frame slot
  from `SymbioticStructuresBody` to the linked body's interface. The owner
  permitted changes to ActiveBody and SymbioticStructuresBody for this
  mechanism; these changes have not yet been run in tests.
* The record is not overwritten for every hit: RW 0x8C4294–0x8C42F5 writes
  immediately if the saved frame is neither the current frame nor the previous
  frame (unsigned wrap matters at frame 0). Otherwise it resolves the previous
  and incoming source IDs. No incoming source keeps the old record; a missing
  previous source or incoming INFANTRY / CAVALRY replaces it; otherwise RW
  0x68DAE6 allows the incoming source when it has FS_POWER, FS_FACTORY,
  FS_BASE_DEFENSE, FS_TECHNOLOGY or FS_CASH_PRODUCER. On a copy RW
  0x8C42F5 copies the entire DamageInfo, resets the clearable-attacker flag,
  and stores the current frame. The port formerly overwrote its partial
  record on every damaging hit; its existing fields and the new byte now
  follow the same replacement decision. Runtime verification is pending.
  The native ActiveBody snapshot was bumped to version 3 and rejects older
  versions, which did not record `m_noEffect` and cannot reconstruct it.

## Outstanding before code

Resolve the two +0x2FC AI variants and their lifetime in the port, use the
now-parsed `ForceSkirmishAI`, implement and test the PC24 comparison and
confirm record-byte possibilities across body implementations, and design the condition cache
and player AI timer/ID with hash and xfer. Confirm all relevant callers and
tests before claiming parity. Existing manager AI state is not proof of the
separate player AI's presence or behavior. `PORT-16` prerequisite tests exist
but have not yet run; no implementation of the two conditions is claimed.

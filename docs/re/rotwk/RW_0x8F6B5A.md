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

## SAFE: selection and scan

* RW 0x7E564F–0x7E566C reuses condition +0x44 when the frame is at most
  +0x48 and the cached value is -1 or 1; otherwise it stores the current
  frame plus `[0xD9F608]` at +0x48 and reevaluates. It stores -1 for false
  and 1 for true (RW 0x7E56AB / 0x7E56B6). This per-condition simulation
  state is not present in the port's `ScriptCondition`.
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
* RW 0x8F6BD2–0x8F6C73 walks the world object list and rejects candidates
  unless their template has STRUCTURE and SUPPLY_SOURCE, their
  `SupplyWarehouseDockUpdate` exists, its +0x88 multiplied by GlobalData
  +0xA5C is at least the supplied threshold, and RW 0x6ADBEB on the source's
  team returns nonzero. The module constructor RW 0x8AB4C0–0x8AB4F1 sets
  +0x88 from the module data's StartingBoxes; RW 0x8AB64E–0x8AB675 can set
  it again from a value divided by GlobalData +0xA5C. The port exposes
  `boxesStored()` and `valuePerSupplyBox`; signed multiplication/overflow
  and subsequent source updates still need checking.
* RW 0x8F6C97–0x8F6D36 queries within the candidate's bounding-circle radius
  plus the 200.0 constant at 0xBE4170. The query uses distance type 1 and a
  chain whose vtables 0xBD88B0, 0xC10E38 and 0xC10E2C resolve to RW
  0x660DB2, 0x66107F and 0x660EB1 respectively. They test KindOf
  SUPPLY_GATHERING_CENTER, equality of controlling player, and Object
  +0x458 bit 3 clear. A found object rejects this source. The exact boundary
  and partition iteration need to be verified before implementation.
* RW 0x8F6D38–0x8F6DA9 compares squared 2D distance from the AI's +0x38
  position, and when an enemy exists rejects a candidate if
  `0.4 * ownDistanceSquared > 0.6 * enemyBoundsMidpointDistanceSquared`.
  The first candidate survives an equal-distance tie. RW 0x8F6DC1–0x8F6DCF
  halves the signed threshold toward zero and repeats only while the new
  value exceeds 100. Exact x87 rounding remains to be checked.
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
* The same helper has another caller: RW 0x8F744B clears AI +0x70, RW
  0x8F744F calls RW 0x8F5ABD, and RW 0x8F7458–0x8F746A resolves AI +0x74
  if it returned true. On failure it calls the source chooser RW 0x8F6B5A
  (RW 0x8F746C–0x8F7476). The caller belongs to an action outside this
  issue; a correct implementation must share the helper and its state, not
  add a separate condition-only copy.
* Record +0x78 is `DamageInfoOutput::m_noEffect`: constructor RW
  0x66365E clears it and copy RW 0x744200–0x744203 copies it. ActiveBody
  uses that copy at RW 0x8C42F1–0x8C42F5. The port's `ActiveBody` stores
  only selected fields of the last damage record, not `m_noEffect`.
  Whether this byte can be true on every body path that the condition can
  scan has not been proved. ActiveBody is explicitly out of scope for #72;
  do not silently treat the missing field as false or modify that module.

## Outstanding before code

Resolve the two +0x2FC AI variants and their lifetime in the port, exact
partition distance boundaries and x87 behavior, record-byte possibilities
across body implementations, and a design for the condition cache and player
AI timer/ID with hash and xfer. Confirm all relevant callers and tests before
claiming parity. Existing manager AI state is not proof of the separate
player AI's presence or behavior. No PORT-16 test or behavioral change is
claimed by this note.

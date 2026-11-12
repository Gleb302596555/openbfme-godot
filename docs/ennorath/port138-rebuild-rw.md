# Port 138 / issue #340 — rebuilding destroyed structures

Research checkpoint, 2026-11-12. **Analysis is in progress; no runtime fix or
acceptance claim is made by this document.** The supplied RotWK 2.01 image was
checked with md5 `daed3668006cd90f01c34e5a7da1901f`. Addresses below are offsets
in that image, read using `tools/mod/rw.py dis`, with caveat S-001. The merged
INI archive was inspected outside Git; no INI or extracted values are included.

## Two distinct mechanisms, not two implementations of one action

The owner's goblin example must not be diagnosed from the name of the visual
state alone. The merged Ennorath data routes the Wild economic structures
through `RebuildHoleExposeDie` and a replacement hole object. Their hole uses
`RebuildHoleBehavior`, including the Wild cave, fissure, mine shaft, spider pit
and treasure trove. The separate `GoblinCampKeep` uses the camp/castle
`StructureCollapseUpdate` / `RubbleRiseUpdate` path. Other factions' economic
structures and the neutral lairs also use the hole path. A change to camp-rise
timing alone therefore would not repair the reported Wild-building mechanism.

The merged data has the five hole-behavior instances described in the issue;
the four named-worker instances use a nonzero worker-respawn delay and nonzero
hole regeneration, while the neutral instance has no named worker and zero
regeneration. These are data facts, not a license to invent a delay in
`RubbleRiseUpdate`.

## Camp/castle path already present

Port 64's class chain and full update analysis remain the starting evidence:
`docs/ennorath/port64-rubblerise-rw.md`. In this session the existing port was
checked against that path, not duplicated:

- `StructureCollapseUpdate` clears the construction/RUBBLE flags and sets
  POST_RUBBLE at the bottom of the collapse (RW 0x8A8001; existing port path in
  `StructureModules.cpp`).
- `RubbleRiseUpdate::update` begins at RW 0x8A85D0. RW 0x8A85EE selects waiting
  state; RW 0x8A8603 tests model-condition bit 59 (POST_RUBBLE). INITIAL and
  BURST are issued at RW 0x8A8614 and RW 0x8A8734, before the first movement;
  there is no read of Min/MaxRubbleRiseDelay here.
- The rise moves the object and finishes by clearing POST_RUBBLE at RW
  0x8A8932 and setting POST_COLLAPSE at RW 0x8A8949. The port already implements
  this sequence, with state, crc and xfer. This module is not the hole worker's
  construction-percentage controller.

## Hole path: RW instructions checked so far

The existing port names RW 0x8868C2 for `RebuildHoleBehavior::update`. Its
linear disassembly was read in this session:

| RW | Behavior |
|---|---|
| 0x8868D7..0x886927 | Resolve saved worker and reconstructing-object IDs; a missing object restarts the worker-respawn process (call 0x8865F7). |
| 0x88692A..0x886943 | With no worker, decrement the positive wait counter; only the transition to zero enters creation. This is a wait, not an immediate rise on cessation of damage. |
| 0x886949..0x886956 | Test the hole data's worker-name string; empty selects the direct-build branch. |
| 0x886994 | Empty-worker branch calls the build assistant through slot +0x38. |
| 0x8869A3..0x8869D3 | If the result has GettingBuiltBehavior, set initial-health percent to 1, force completion, and start construction. |
| 0x8869D6..0x8869F7 | Set model conditions 0x44/0x45 and flush the drawable's conditions. This is not a POST_RUBBLE rise. |
| 0x886A01..0x886A42 | Named-worker branch resolves the worker template and creates that worker; it does not itself call the direct-build branch. |

The port already contains the corresponding wait-counter gate and the two
creation branches in `RebuildHoleModules.cpp`. The next comparison must follow
the named worker's construction command and the direct builder's actual
implementation through construction percentage and model selection. A getter
or a percentage field alone does not establish what the UI draws.

The existing UI path is `DrawableIconUI.cpp`: it draws the construction caption
only for an object under construction, not SOLD, with construction percentage
not equal to -1 (existing RW references 0x677CF9 / 0x679AE8). The investigation
must establish which construction path fails to supply those existing inputs;
adding a second percentage display or resetting a camp timer would be a patch
around the symptom.

## Next steps and acceptance boundary

1. Finish the RW worker/direct-build chain, including the remaining hole-update
   branches, and compare every step with the port. Check the existing S-5305
   stop concerning non-dozer `buildObjectNow` before deciding whether this is a
   local correction or an out-of-scope shared construction mechanism.
2. Only then change the first differing step in its existing path. If a shared
   mechanism beyond this issue is the cause, stop for the owner's decision as
   required by PROCESS 15.3; do not substitute a new rebuild controller.
3. Add `engine/tests/test_port_138.cpp` in `engine/cmake/Port138.cmake`, with a
   red-on-baseline regression and neighboring-behavior coverage, including the
   existing construction-caption inputs and ordered body/model transitions.
4. Run the full `openbfme_tests` target with `PORT-138*`, add NIGHT_WATCH (and
   STOPS if needed), and open the PR. None of these acceptance steps is claimed
   complete at this checkpoint.

No simulation state, xfer format, hashes, existing tests, or other tasks' files
have been changed. The Linux test-only build was started; its final result is
not yet available. The cloud environment has no installed game, so the visual
check remains for the owner.

## Checkpoint 2: the shared-builder stop and the remaining hole branches

The tail mention of S-5305 was checked against its actual row: S-5305 is the
unported `Object::doCommandButton` cases, not a missing `buildObjectNow` helper.
`Construction::buildObjectNow` is already ported (RW 0x797796; the direct hole
branch calls it at RW 0x886994). It is therefore not a reason to stop this task.

The rest of the hole update was read linearly from `/tmp` disassembly of RW
0x8868C2:

- RW 0x886AAD calls the worker AI's slot +0x1F8 with rebuild=1, build=0.
  `WorkerAIUpdate::construct` at RW 0x8AD6A4 writes construction percent 0 at
  RW 0x8AD86F, sets health to 1 at RW 0x8AD881..0x8AD88E, clears condition
  0x43 and sets 0x44/0x45 at RW 0x8AD894..0x8AD8B5, then queues the BUILD task
  at RW 0x8AD8C6. The port's `constructForRebuild` has these steps.
- RW 0x886B1E..0x886B72 records the replacement, transfers the paid price,
  stops spawning when the spawner was inactive, and hides/masks the hole.
- RW 0x886B77..0x886BD5 heals only the hole while below max health, by
  `(HoleHealthRegen / logicFPS) * maxHealth`; this is independent of the
  worker-respawn counter. It is not a second trigger for reconstruction.
- RW 0x886BDC..0x886C40 removes the hole when the replacement is canceled or
  finished, with the GettingBuiltBehavior/effectively-dead exception.
- The worker's actual progress is `DozerAIUpdate::workOnBuild`, RW 0x88DE43:
  RW 0x88DE89..0x88DE99 adds `100 / calcTimeToBuild` to Object+0x288;
  RW 0x88DEA8..0x88DEB5 adds maxHealth/frames to the body. Completion at
  RW 0x88DEE0 ff clears the construction statuses/conditions and writes -1.

The merged Wild economic definitions use a named worker in GettingBuiltBehavior
and DestroyObjectWhenDone in their collapse blocks; RebuildWhenDead is not set
there. Thus the old corpse's collapse/removal, the hole countdown, and the new
object's worker-driven construction are three consecutive parts of one route,
not a self-repair triggered by the end of incoming damage. The next checkpoint
exercises that whole route synthetically, including the caption inputs, before
choosing the first differing runtime step. No runtime change is claimed yet.

## Checkpoint 3: the WorkerAI/DozerAI inheritance and the remaining comparison

The earlier working note treated `WorkerAIUpdate` and `DozerAIUpdate` as
alternative, unrelated worker classes. That was wrong: the port declares
`WorkerAIUpdate : public DozerAIUpdate, ...` (`WorkerAIUpdate.h:60`), and its
update calls the inherited `dozerMachineStep()` (`WorkerAIUpdate.cpp:541`).
Consequently the `dynamic_cast<DozerAIUpdate *>` in
`GettingBuiltBehavior::spawnWorkerOrStart` also accepts a WorkerAIUpdate;
S-650 is not evidence that Ennorath's named workers cannot build. The hole's
named-worker branch already calls `constructForRebuild` (RW 0x8AD6A4), whose
BUILD task is processed by the inherited dozer action machine and
`workOnBuild` (RW 0x88DE43). This removes a false stop candidate, not a
confirmed root cause.

The first executable comparison is now the committed synthetic route in
`engine/tests/test_port_138.cpp`: death -> hole -> wait -> worker -> staged
construction -> existing caption helper -> completion and hole removal, with
the direct-build and camp-rise neighbours. Its translation unit passes a
standalone GCC syntax check using the actual Ninja compile flags. The full
test-target build and the PORT-138 run are still in progress; no red/green or
runtime-fix claim is made here.

The Drive connector lists as enabled but both attempted downloads returned
`auth_error` (the app is not connected). The previously read RW instruction
notes above remain the evidence already obtained; new disassembly and a fresh
merged-INI audit require the owner to reconnect Drive or supply the ignored
`workspace/RotWK/game.dat` and the INI archive again. No game or mod data was
added to Git.

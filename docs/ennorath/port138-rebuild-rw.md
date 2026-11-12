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

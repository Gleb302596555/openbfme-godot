# PORT-103 — FlammableUpdate / BurningSoundName (issue 263)

## Analysis (2026-11-10)

Target: RotWK 2.01, game.dat MD5 daed3668006cd90f01c34e5a7da1901f.
All instruction addresses below were read with `tools/mod/rw.py ... dis`.
This document contains analysis, not a claim of completed implementation/tests.

### Identity and reachable reader

- Field row RW 0xC62E70: name 0xC62DD4, parser 0x73B217, offset 0x18,
  in FlammableUpdate's table 0xC62E30. Data constructor RW 0x890BBB:
  RW 0x890BE6 clears +0x18.
- Create RW 0x64CD09 allocates 0x50 bytes and calls constructor 0x88FCD8
  at RW 0x64CD30. Constructor RW 0x88FD29..0x88FD3D installs primary
  vtable 0xC62C04, update interface 0xC62BF8 (+0x10), damage interface
  0xC62BEC (+0x20); RW 0x88FD0C initializes audio handle +0x34 to zero.
- Update slot 0 = RW 0x890738; damage slot 0 = RW 0x8904B8;
  healing slot 1 = RW 0x89015F. Primary deleting destructor slot 0 =
  RW 0x88FF53; RW 0x88FF56 calls destructor 0x88FDAD.
- Existing ignition RW 0x8901AB has a NORMAL-only gate at RW
  0x8901BE..0x8901C2. After setting AFLAME status/model condition,
  RW 0x890201 calls 0x88FE48. This is the reachable field reader, not a
  detached getter. Only one direct caller was returned by `rw.py xref`.

### Start (RW 0x88FE48)

RW 0x88FE5B..0x88FE6E reads object ID at object +0x74, takes module data
+0x18 and constructs a local audio event via RW 0x6DB5B3. That constructor
stores the supplied ID at event +0x34 (RW 0x6DB5F3..0x6DB5F8) and, for a
nonzero ID, sets event positioning mode 2 (RW 0x6DB600): object attachment,
not a one-time world position. RW 0x88FE86 calls TheAudio slot 0x64
(addAudioEvent); RW 0x88FE93 stores its result at module +0x34.
RW 0x88FE96 destroys the local event. The module does not force a loop
flag or poll/restart the sound: looping belongs to the named audio event.
There is no name/empty-string branch in this function; absent/NoSound is
handled by the audio layer. An already AFLAME module never reaches it.

### Stop and neighbours

- Stop helper RW 0x88FBCA reads handle +0x34, tests it at RW 0x88FBD0,
  skips if zero (RW 0x88FBD2), otherwise calls TheAudio slot 0x6C with
  the handle (RW 0x88FBDC..0x88FBDD), then clears it (RW 0x88FBE0).
- Quench RW 0x88FF6F calls it at RW 0x8900C7, before clearing AFLAME
  at RW 0x8900CC..0x8900E8. Burned model/status decisions precede the
  helper and must remain unchanged. A BURNED status is not itself the
  sound-stop trigger: update RW 0x890AB3..0x890AD9 sets BURNED, then
  separately checks aflame-end (RW 0x890ADE..0x890AE8) and calls quench
  at RW 0x890AED only when that deadline is reached.
- Healing reaches the same quench at RW 0x89019B only on replenishing
  FlameDamageLimit (compare RW 0x890192, branch RW 0x890196).
- Destructor RW 0x88FDAD calls helper at RW 0x88FDDD. `rw.py xref
  0x88FBCA` returns exactly the quench and destructor calls.

### Donor cross-check (not the source of RW facts)

DONOR BFME2 0x0048C7BF (`Code/GameEngine/Source/GameLogic/Object/Update/FlammableUpdate.cpp`,
`rva0048C7BF`) constructs the object-bound event from data +0x18 and
stores the audio handle at +0x34. DONOR BFME2 0x0048C55E (same file,
`stopBurningSound`) removes a nonzero handle and resets it.
`FlammableUpdateDtor.cpp` calls this helper in the destructor and in
DONOR BFME2 0x0048C8F9 (quench). These mechanisms are independently
confirmed by the RW instructions above; no donor-only branch is proposed.

### Port plan and scope

The port currently parses the name, but the ignition/quench explicitly
report sound start/stop as unported. Use the existing AudioApi held-sound
queue with a dedicated flammable slot; do not touch FXEvents or
GodotFXPlayer. Keep handles entirely on the audio-owner side (as existing
held sounds do), not in simulation hashes/snapshots. Stop on both quench
and module destruction. No change to ignition gates, damage, healing,
burned timing, spreading, model conditions, or parser behaviour.

Synthetic PORT-103 tests will exercise a real FlammableUpdate object and
an installed synthetic AudioManager: one attached request per ignition,
no repeated start, stop on aflame-end/healing/destruction, no sound for
absent/NoSound; BURNED before aflame-end must not stop the sound.

INFERENCE: none used to change behaviour yet.
Осталось: реализация, PORT-103 tests on the full openbfme_tests target,
NIGHT_WATCH, final rebase, PR and owner listening check.

## Implementation checkpoint

- `c12d31e`: dedicated HELD_FLAMMABLE_LOOP in the existing AudioApi;
  ignition, quench, destructor wired. No new simulation member/hash/layout.
- `7e90cab`: six synthetic test cases in `engine/tests/test_port_103.cpp`,
  via `engine/cmake/Port103.cmake` in the full openbfme_tests target.
- RW xfer slot 3 at 0xC62C10 = 0x88FBEE; RW 0x88FC76..0x88FC7B
  passes handle +0x34 to TheAudio slot 0x160. Native load currently
  reconstructs from FS_AFLAME; this mapping still needs verification against
  the audio handle xfer implementation before review (do not claim parity
  from the caller alone).
- Full build running; a follow-up process reconfigures and builds the same
  full target, then runs exactly `-tc="PORT-103*"`, logging to
  `/tmp/port103-build.log` and `/tmp/port103-tests.log`.

Осталось: full build/test results, fix any compile/assertion failures,
verify native audio xfer semantics (or remove unsupported inference),
red/green run on full target, mod data check, final rebase and PR.

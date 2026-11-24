# Port 161 / P-89: burning terrain audio — investigation

Status: implemented and verified on the full `openbfme_tests` target (Release,
Godot OFF, Ninja -j1). PORT-161: 8 cases / 153 assertions passed. The negative
control fails without the producer notifications, and the normal binary passes
again afterward. Audible in-game acceptance remains with the owner.

**Reading this log:** the investigation and parked/build-pending sections are
historical checkpoints. The final verification section below supersedes their
status; Port 290 has landed and is not an outstanding dependency.

## Sources

RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f. All RW instruction ranges below were read using `tools/mod/rw.py ... dis`.
Ennorath INI archive supplied through the owner's Drive was inspected locally; no game data is included here.

DONOR BFME2 0x00285B91 / 0x00285BEC (`Code/GameEngine/Source/Common/Rva00285BECDtor.cpp`): burning-cell construction/copy registers a LargeGroupAudio member; destruction unregisters it.
DONOR BFME2 0x002856CF (`Code/GameEngine/Source/Common/Rva002856CF.cpp`): delayed member update.
DONOR BFME2 0x002862C9 (`Code/GameEngine/Source/GameLogic/LargeGroupAudio/LargeGroupAudioKeyMapBfmeAddKey.cpp`): the cell's key map.
DONOR BFME2 0x00287FAD (`Code/GameEngine/Source/GameLogic/Map/FireLogicSystem.cpp`): grid update.
These donor facts were used to locate the RW functions, not as authority for a code change.

## Confirmed RW map

| Situation | Instructions read / result | Current port |
|---|---|---|
| Construct burning-cell key | RW 0x6862D2..0x6862F3: store integer world x/y at +8/+C, set +10 to -1, call 0x685E40. Set vtables 0xC11830 / 0xC11820. | `FireLogicSystem::m_cellsOnFire` is a set of coordinate pairs; no audio registration. |
| Copy into set node | RW 0x686381..0x686397: copy coordinates, set +10 to -1, call 0x685E40. | Set insertion has no audio side effect. |
| Register | RW 0x685E43..0x685E5D: only if +10 == -1, call LargeGroupAudio add (0x60D5FF), store current logic frame at +10. | No corresponding event from fire grid. |
| Destroy temporary or resident key | RW 0x68632F..0x686336 calls 0x685E62; RW 0x685E65..0x685E77: if +10 != -1, call LargeGroupAudio remove (0x60D633), reset +10 to -1. | Set erase/clear has no audio side effect. |
| Audio key | RW 0x686A60 jumps to 0x6869D8; RW 0x686A13..0x686A47 builds the BurningCell key once using 0x7EEB0C. | No fire-grid producer of this key. |
| Member interface | Vtable 0xC11830 slots 0/1 -> RW 0x6863F6 (integer coordinates converted to float); slots 2/3 -> 0x685E2F; slots 4/5 -> 0x685E35; slot 6 -> 0x685E3B (weight 1); slot 7 -> 0x686A60; slots 8/9 -> 0x9188EB; slot 10 -> 0x9F9F1A (load +10). | Existing event type represents object members, keyed by ObjectID in LiveGameAudio. |
| Decayed heat | RW 0x6887AE..0x6887FC: if not newly lit, construct lookup key and erase found resident; destroy lookup temporary. | Erases coordinate pair, silently to audio. |
| Newly lit cell | RW 0x688836..0x688887: clear newly-lit bit; if at MaxCellsBurning, clear heat and skip; otherwise construct temporary, insert key and destroy temporary. | Inserts coordinate pair, silently to audio. |
| Fuel exhausted | RW 0x6888AA..0x688923: construct lookup key, erase resident when found, clear heat/set burnt flag and destroy lookup temporary. | Erases coordinate pair, silently to audio. |
| Tree consumption | RW 0x688939..0x688963: subtract consumed fuel; walk tree nodes, call TerrainLogic 0x67F4C2 with consumed amount. | Existing `TerrainTrees::burnTreeLumber`; separate from audio membership. |
| Spread ignites another cell | RW 0x688C0A..0x688C5B: positive neighbour heat and no current heat; capacity gate; construct/insert/destroy key. | Inserts coordinate pair, silently to audio. |
| End of ten-row-slice cycle | RW 0x688CCF..0x688D02: increment accumulator; at ten reset to zero and call 0x685E07 for each resident key. | Accumulator wraps, but no audio member update. |
| Delayed update | RW 0x685E0A..0x685E2A: only when member +10 == -1 and LargeGroupAudio +3C >= -1, call 0x60D5CB and stamp logic frame. | Missing for burning cells. |
| Global audio gate | RW 0x60D5CB..0x60D664: update/add/remove skip if disabled (+38) or current frame equals gate (+3C); otherwise iterate maps. | Existing `LargeGroupAudioLink::notify` provides this gate. |

Ennorath `data/ini/largegroupaudio.ini` uses the BurningCell key in the small, medium and large fire maps, selecting their ambient loops. This is a terrain-cell mechanism, not one sound per map tree. Ground burning without a tree is also affected. Do not add a parallel FlammableUpdate sound to optimized trees.

## First differing step and remaining work

The burning-cell set loses the original member lifecycle: pairs replace objects whose construction and destruction notify LargeGroupAudio. The existing `FlammableUpdate` audio path (Port 103) does not repair this.

Before implementation, finish reading the member flag/stealth getters and the map add/update/remove consumers, plus fire-grid area-operation and snapshot paths. Important: RW creates temporary keys during lookups, not just resident keys. Their add/remove behaviour must be understood with the map consumers before simplifying it to resident membership. Do not blindly emit one add per `std::set::insert`.

The client currently indexes LargeGroupAudio members by ObjectID (`LiveGameAudio::LgaRuntime`); burning cells are not objects. A safe fix must preserve separate identities, including coexistence with a real object's equal numeric ID, and must cover every insertion/erase/clear/load path. Any new simulation bookkeeping requires xfer/hash in the same code commit. Existing object audio behaviour must remain unchanged.

Tests to add in `test_port_161.cpp` / `Port161.cmake`: synthetic INI and real audio consumer; burning terrain registered by tree fuel, audible loop start and stop, duplicate-request count, neighbouring nonburning/object member preserved, clear/extinguish/load lifecycle. Expectations must follow the remaining RW consumer analysis, not this proposed outline.

Build: full `openbfme_tests` target configured Release, Godot OFF, Ninja -j1. Initial attempt lacked cmake; cmake and ninja installed in .venv, build restarted. No completed run yet.

## Consumer investigation: scope stop

The follow-up reads establish that this is not just an absent sound name.
The exact retail consumer is a shared, incompletely ported mechanism already
tracked by S-248. No engine patch is submitted at this checkpoint.

DONOR BFME2 0x005697F7 / 0x0056979A
(`Code/GameEngine/Source/Common/LargeGroupAudioSoundKeyPair.cpp`,
`rva005697F7`, `rva0056979A`, `unregisterSubject`): add/remove consume a
position and weight, not an ObjectID. They update four grids, or retain/remove
a pending position/weight record before grids exist. The remove function's
exact donor address is not established here; its name/body, not an inferred
address, was used to locate the RW remove implementation.

Confirmed against RW instructions, read with rw.py:

- RW 0x7EF032..0x7EF09B: map add checks current model/status masks and,
  when configured, stealth; each qualifying Sound block receives 0x971326.
- RW 0x7EF0A1..0x7EF11E: remove checks stored model/status/stealth and
  requires the member's stored frame to be strictly greater than the global
  gate before invoking 0x971392.
- RW 0x7EEF0D..0x7EF006: update checks both states; both qualify and equal
  positions means no call; otherwise selects move/add/remove. In particular,
  a stored frame at/before the gate means that the old state was not counted.
- RW 0x971326..0x97138F: test member keys (slot 7), get position (slot 1),
  get weight (slot 6), add weight to +0x64, then either 0x9712C9 (existing
  grids) or construct/insert a position/weight record through 0x970326 and
  0x970E81.
- RW 0x9712D7..0x97131A: walk FOUR grid pointers starting at +0x2C;
  find the grid cell with 0x9D76E3, add its weight with 0x9D7DCD and process
  the affected cell with 0x971121. This is not a single floor(x / Size) map.
- RW 0x971392..0x97142E: matching keys, stored position (slot 0), weight
  (slot 6), subtract total weight; when grids exist, visit the same four
  grids, subtract cell weight (0x9D7D5B), process affected cells.
- RW 0x971430..0x971466: without grids, look up the position/weight record
  at +0x20 with 0x9704EA and erase a found record with 0x47E273. RW
  0x970326..0x97033F stores two coordinates and a word weight. The temporary
  BurningCell registrations therefore have real value-based consumers; one
  cannot model them as objects with INVALID_ID.
- RW 0x9D7DF6..0x9D7EBD reads neighbouring grid-cell weights; RW
  0x9D7F98..0x9D7FDB adjusts the preferred audio x/y using their sums.
  DONOR BFME2 0x005C8876
  (`Code/GameEngine/Source/GameLogic/LargeGroupAudio/LargeGroupAudioGridCell.cpp`,
  `getPreferredAudioPosition`) was read first for this function. Only the
  instruction ranges above have been confirmed here; the rest is not claimed
  as a complete RW port.
- RW 0x9188EB returns false for burning-cell stealth. RW
  0x686412..0x686449 is its xfer (secondary base): transfer version, coordinate
  pair and registration-frame field. The registration frame is not safe to
  dismiss as unsaved client bookkeeping in a future implementation.

Port comparison:

- `FireLogicSystem.cpp` inserts/erases coordinate pairs in the update,
  extinguish/fuel operations and snapshot load, and clears them on reset.
  None of those entries implements the RW LargeGroupAudio subject interface.
- `LargeGroupAudioEvent` / `LiveGameAudio::LgaRuntime` identify members only
  by ObjectID; two non-object cells sent as INVALID_ID would overwrite each
  other. Fabricating object IDs is not the retail mechanism either.
- `LiveGameAudio::largeGroupUpdate` expressly labels its grid/threshold
  rules INFERENCE (S-248). It builds one floor-based grid from members and
  positions sounds at weighted member centres, rather than using the four
  retail grids and neighbouring-cell position calculation read above.

The affected shared consumers include object LargeGroupAudioUpdate members
as well as burning ground: all Sound blocks passed through these functions,
including the mod's army and water-wading loop maps, use the same runtime.
The explicit forbidden-area files (FX player, voice responses, other ports)
have not been changed.

Owner decision needed: authorize a separate shared LargeGroupAudio/S-248
runtime task (four grids, position/weight subjects, thresholds/positions and
persistence), followed by BurningCell wiring here, or explicitly expand
Port 161 to cover that shared mechanism. Adding a direct sound per tree,
an INVALID_ID member, or a render-frame scan would be a substitute, not the
requested port. No verdict that RW lacks a reader is being made: the reader
chain is positively identified above.

No PORT-161 test has been added and no successful test run is claimed. The
full test-target build was started before this scope blocker was established;
this report is a stop, not a review submission. Existing S-248 already tracks
the unported shared runtime; no unrelated stop row or hash pin was changed.

After establishing the scope stop, the background full-target build was deliberately stopped (204/1717); no test executable or doctest result was produced.

## Dependency hand-off: Local 60 became Port 290

The owner's comment on fork issue 457
(https://github.com/Gleb302596555/openbfme-godot/issues/457#issuecomment-6987908004)
closes the local task because its work moved to Arena as Port 290, fork issue
678. This is a task hand-off, not evidence of the runtime landing in main.

On this resumption, `origin/main` at c239a326 still has the ObjectID-only
`LargeGroupAudioEvent` and the S-248 INFERENCE grid in
`LiveGameAudio::largeGroupUpdate`; fork issue 678 is open. Port 161 therefore
continues to wait for Port 290's runtime to land. The existing owner restriction
remains: do not implement the shared runtime on this branch. After that landing,
connect the burning-cell register/unregister/delayed-update calls and run the
PORT-161 tests. No engine behaviour or test expectation changed in this update.

## Resumed after Port 290 landing

Port 290 is merged by 40fd43f0b (fork PR 743). Its value-based consumer
accepts BurningCell notifications without a fabricated object identity.
The former scope stop above is historical, not a remaining blocker.

Implementation now connects every existing set insertion, erase, clear and
snapshot replacement to the shared link. Temporary-key constructor/destructor
notifications are preserved, as are the resident node's copied constructor and
stored registration frame. RW 0x685E43 stamps even a globally gated add;
RW 0x685E65 skips only frame -1. The cycle-end delayed update is gated by
RW 0x685E0A/16, not an unconditional periodic re-add. No shared audio runtime
or FX-player code is changed.

The native fire snapshot advances to version 3 to save the audio registration
frames (RW 0x686441); versions 1/2 explicitly migrate their previously absent
audio registration to -1 for delayed update. Frames are presentation-only
bookkeeping and have no simulation reader: fire CRC remains unchanged.
On load, constructor contributions whose saved frames fail the map gate are
removed before the deferred update can register them. This adapts the existing
OpenBFME snapshot layout rather than claiming byte compatibility with retail.

Tests and the completed full-target build are still pending at this checkpoint.

Checkpoint 4713a5074: six synthetic PORT-161 cases and Port161.cmake are
committed. GCC -fsyntax-only passes both the new test source and the changed
FireLogicSystem.cpp with the full target's generated compiler flags. Full
openbfme_tests compilation is running single-threaded; no executed-test pass
or negative-control result is claimed yet. The initial build was interrupted
after 135/1913 steps to regenerate with the new test lane, then resumed from
its object cache. CMake warns about unavailable lame/mpg123 fixtures (five
unrelated MP3 cases); neither requested test filter uses them.


## Continuation: verification automation

The test source now also covers the optimized-tree fuel registration input
(RW 0x683F80 -> 0x686A82), without creating an object for that tree, and
mixed membership: extinguishing one cell preserves another cell and an
object contribution. Eight PORT-161 cases total; their syntax check passes.

The full Release target is still compiling with -j1, not replaced by a small
standalone test executable. A session-only verifier waits for that build,
then incrementally builds the final source state and runs PORT-161 plus
PORT-290/fire-logic neighbors. It prepares the negative control in ignored
workspace copies: disable only BurningCell's notification body, compile a
private replacement object, replace that member in a private core archive,
and relink the full test binary to a separate path. It must fail the burning
cell registration regression, then the unmodified binary must pass PORT-161
again. Tracked source, normal archive, normal binary and hash pins are not
modified by this control. Execution results remain pending; this describes
the scheduled verification, not a claim it already succeeded.


## Final verification

Verified source state: `07d27cd30738f3059fb8de962010f94254e94d7e`
(last engine/test edit: `0e126a497`). Full target build, not a reduced harness:

```
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENBFME_BUILD_GODOT=OFF
cmake --build engine/build -j1 --target openbfme_tests
engine/build/openbfme_tests -tc="PORT-161*"
[doctest] test cases:   8 |   8 passed | 0 failed | 7256 skipped
[doctest] assertions: 153 | 153 passed | 0 failed |
[doctest] Status: SUCCESS!
```

The skipped count here is the test-name filter, not a claim of a full-suite
run. The full suite and installed-game scenarios were not run. Neighbor checks:

| Filter | Passed cases | Passed assertions |
|---|---:|---:|
| `PORT-290*,fire logic:*` | 14 | 164 |
| `audio4 LGA:*` | 2 | 44 |
| `save4d weather*` | 1 | 34 |

Negative control: a separate source copy returns before
`notifyBurningCell` sends anything. It replaces only that object in a private
core archive, then relinks the full test binary to a separate path. The case
`PORT-161 burning grid cell registers anonymous audio and extinguishes without a ghost`
fails at `REQUIRE(b.events.size() == 3)`: actual 0; 1 case failed,
9 assertions passed / 1 failed. The unchanged normal binary then passes
PORT-161 again: 8 cases / 153 assertions. No tracked sources, hash pins,
normal object files, normal archive or normal binary were changed for this
negative control. Session logs are under ignored
`workspace/port161-verification/`.

### Behavior / evidence / regression map

| Behavior | RW evidence | Synthetic test in test_port_161.cpp |
|---|---|---|
| Temporary construction, resident copy, temporary destruction; coordinate/weight/key payload | 0x6862B6 / 0x686355 / 0x686311, vtable 0xC11830, 0x685E3B / 0x686A60 | burning grid cell registers anonymous audio and extinguishes without a ghost |
| Newly-lit cancellation does not construct a resident or emit audio | 0x68715E..0x6871AB / 0x6887AE | quiet fuel and pre-residency extinguishing do not produce burning audio |
| Clear destroys residents without lookup temporaries | 0x686D3C -> 0x686311 -> 0x685E65 | reset unregisters each resident once without a temporary lookup |
| Registration frame persists; simulation CRC unaffected by audio bookkeeping | cell frame 0x686441; native v3 adaptation, not a retail-file claim | saved registration frame survives replacement and keeps simulation CRC |
| Only frame -1 retries at tenth slice; once stamped, no duplicate retry | 0x688CCF..0x688D02 -> 0x685E0A..0x685E2A | old snapshot registers only on tenth slice and does not update twice |
| Global gate refuses notification, but constructor still stamps frame | 0x60D5FF / 0x60D633, 0x685E55..0x685E5D | gate frame drops constructor notifications without periodic re-add |
| Tree template fuel enters the fire grid, not a new tree object | 0x683F80 -> 0x686A82 | tree fuel registers cell audio without creating a tree object |
| Removing one value-based cell contribution preserves other cell/object contributions | 0x971326 / 0x971392 | extinguishing a cell preserves another cell and an object contribution |

INFERENCE / native adaptation: pre-v3 OpenBFME saves have no registration
frame. They explicitly migrate to -1 for RW's delayed registration; v3 stores
the frame after the coordinate list. These audio-only fields are saved but
not added to simulation CRC: no simulation reader consumes them. No existing
test expectation or hash pin was changed. Shared runtime tie order (S-7512)
and ducking / Living World / crowds (remaining S-248) are unchanged, outside
this producer-only follow-up. No new shared-runtime substitute is introduced.

### Owner check (not executed here)

In Ennorath, select the Cardolan scout and use its second ability on a forest.
Listen near the burning cells: the fire-group crackle should accompany the
existing ground fire / tree sinking. Extinguish or wait for the fire to finish:
no lingering loop. Check a quiet forest stays quiet, then repeat around
save/load and a map restart. Also check existing object/army LGA sounds per
the audio entries in VISUAL_REGRESSIONS.md. The synthetic tests verify the
producer and consumer weights, not that installed samples are audible.

No upstream PR: this is a pure audio/display change without before/after
in-game snapshots, so the upstream acceptance gate is not met.


Integration check: after opening PR #875, the advanced main had one textual
CMake conflict: Port 301 and Port 161 both appended their test variable after
Port 290. The Port 161 include/test entry was relocated beside Play2 (the
requested lane model), without importing unrelated main changes or changing
engine/test code. A merge-tree check against main 5a910338c must be clean;
the full target and both regression filters are rebuilt/rerun after this
CMake-only relocation before handing off the PR.

## PR #875 rework: batch 2026-11-23a build return

The return on f55e52b1 supplied only a truncated warning tail, not a compiler
error. The complete batch log was recovered read-only from the owner's Drive:
`openbfme-runs/batch/2026-11-23a/build-tip.log`, file
`1XLeFQr5z0kp2GLw37Ggn9XieFLgWOuTo`, 460175 bytes, SHA-256
`cec7a9ba234eef552a18e0ee61dde8ec2aa762ea1698946db8a23d4263d125dd`.
It identifies batch source 3e8b4e81984fe5a1c4e65030182b03762f2be09a.
Its last completed compile step is 1343/1447, followed by:

```
ninja: build stopped: interrupted by user.
```

There is no `error:`, `fatal error:` or `FAILED:` diagnostic in that log.
This establishes an interrupted build, not a diagnosed C++ compilation
failure. Who/what sent the interruption is not established. The batch script
classifies any nonzero build exit as red and, without an `error` match,
returns only the log tail (`batch_bot.py::build`); that explains why the PR
return contains unrelated TestWallData warnings rather than the interruption
line. The original batch did not reach linking or its tests. This does not
claim to have recovered the individual bisect-point logs, which were not
present in that Drive folder.

Fresh main db85d5794472c977e0db56086e8f4ede2fc39e5d was merged into the
same task branch (merge 966b5f7b8), without conflicts. The stale, shallow
sandbox snapshot was preserved in a stash before restoring the branch's
remote history; it was not applied over the merged tree. The new main's
FireLogicSystem changes (Local 136 citations and Local 140 INI parser) are
retained unchanged. No speculative gameplay/compiler workaround, other
agent's function edit, CI change, or pin remeasurement is made to conceal
an externally interrupted build. RW behavior and its evidence remain as
above: this rework changes integration, not BurningCell semantics.

At this checkpoint a clean Release/-j1 full-target rebuild after the merge
is running. The previous f55e52b1 test results above are historical; they
are not presented as results for the new merged head. A new PR head is
pushed for the checker's requested rebuild as well.

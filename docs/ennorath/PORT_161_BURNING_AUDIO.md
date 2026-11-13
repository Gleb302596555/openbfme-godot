# Port 161 / P-89: burning terrain audio — investigation

Status: analysis checkpoint, not an implemented fix. No claim of passing PORT-161 tests.

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

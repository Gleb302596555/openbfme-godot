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

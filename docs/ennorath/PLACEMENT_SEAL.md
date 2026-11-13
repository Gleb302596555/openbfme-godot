# Building placement seal / bib (Port 170, P-106)

Status: **blocked on an original-game reproduction**. No engine behaviour was changed. The reported rule "red off tainted ground, white on tainted ground" is not the colour rule found in the RotWK 2.01 placement update. PROCESS.md 15.13 requires the owner to resolve that disagreement before a visible behaviour is changed.

## Sources read

- `workspace/RotWK/game.dat`, RotWK 2.01, MD5 `daed3668006cd90f01c34e5a7da1901f`; every RW address below was read linearly with `tools/mod/rw.py ... dis`.
- DONOR BFME2 0x002A21E1 (`Code/GameEngine/Source/GameClient/InGameUIHandleBuildPlacements.cpp`) and 0x0039361E's neighbourhood in `Code/GameEngine/Source/Common/System/BuildAssistant.cpp`; donor only, not used as a substitute for RW instructions.
- Ennorath 1.9.8.2 merged INI, read outside Git. The named Isengard building templates were checked for active `KindOf` tokens and `TerrainResourceBehavior`; no mod bytes or value excerpts are committed.

## Mechanism map

| Situation | RotWK 2.01 | Current port |
|---|---|---|
| Client frame parity | RW 0x6A3932..0x6A393F reads the client frame through vslot 0x7C and enters the judgement loop only when bit 0 is set. Even frames do not clear the previous drawable tint/bib. | `PlaceEventTranslator::handleBuildPlacements` and `PlacementIcons::update` use the same odd-frame gate and retain the last answers on even/busy frames. |
| Ordinary legal site, code 0 | RW 0x6A3A6B branches to 0x6A3AE2; with no failed castle plot, 0x6A3B35 passes null to `Drawable::colorTint` (0x6757BB), then removes the faction bib through TerrainVisual vslot 0x34. | `PlacementIcons::TINT_NONE`, `BIB_NONE`; Godot applies a zero tint and hides the bib. |
| Ordinary refusal, code other than 0/9 | RW 0x6A3A6F/0x6A3A72 sends the code to 0x6A3ADB, which passes `IllegalBuildColor` at 0xDA0B28 to the tint path at 0x6A3B37; TerrainVisual vslot 0x30 receives highlight=1. | `TINT_ILLEGAL`, `BIB_HIGHLIGHT` (`TBRedBib.tga`). |
| Code 9 | RW 0x6A3A74..0x6A3AB7 sets RGB (0,0,0.5), replaces blue with the constant at 0xC1348C when client-frame bit 2 is set, and calls 0x6757BB. RW 0x6A3ABC..0x6A3AD6 adds the non-highlighted bib. This is a blue lighting tint, not a taint-manager query. | `TINT_CODE9`, blue 0.5/1.7 on the same frame bit, `BIB_NORMAL` (`TBBib.tga`). |
| Where code 9 comes from | The legality body read from RW 0x79681A through 0x796D34 reaches its final special case at 0x796C66: template byte +0x11B bit 0x10 (KindOf 156, WALL_HUB). RW 0x796C77 finds a module, 0x796C9C gets the builder's player, 0x796CAB collects that player's points. The callback at 0x6B00CE..0x6B00F2 filters objects by template +0x10A bit 2 (also WALL_HUB) and appends their positions. RW 0x796CB5..0x796CEF compares the 3D distance with module +0x30; a point in range returns 0 at 0x796D24, otherwise 9 is returned at 0x796D0C. | `BuildPlacement::isLocationLegalToBuild` has no corresponding final WALL_HUB/range pass. This is a real missing retail path, but it is a wall-hub range rule, not a `TaintManager` colour rule. Adding it would change wall-hub placement and needs its own evidence/test scope; it was not added under this issue. |
| Tainted ground | No call to the taint manager or taint-cell colour decision occurs in the placement colour loop or in the full legality body above. The special final return is the WALL_HUB distance test. | The placement paths likewise do not query `TaintManager`. The terrain/radar taint presentation is a separate mechanism (`refresh_taint`, S-5261). |

The indirect legality calls in the placement loop are RW 0x6A39C7 and 0x6A3A02 (`TheBuildAssistant` vslot 0x44). The port's corresponding body is `GameLogic/BuildPlacement.cpp`; its presentation consumer is `GameClient/PlacementIcons.cpp`, drawn by `GodotDevice/GodotPlacementGhost.cpp`.

## Ennorath cross-check

`IsengardArmory`, `IsengardSiegeWorks`, `IsengardUrukPit`, `IsengardWargPit` and `IsengardLumberMill` do not have an active WALL_HUB token in the merged mod data (the token is commented out where it appears). Some furnace variants do add WALL_HUB, while other furnace variants do not. None of these facts establishes a taint-based seal colour: the RW code-9 path reads owned WALL_HUB object positions, not the defiled-ground mask.

The `BUILD_PLACEMENT_CURSOR` states of the named buildings use a model/decal presentation; the faction bib is a separate quad. "Seal" must be identified as one of these before changing its colour or draw order.

## Why this is a stop, not a patch

1. The existing port already preserves tint/bib across even frames; a stationary cursor does not acquire a second colour decision from the frame-parity gate.
2. The only retail code-9 colour branch found is the WALL_HUB range branch, and the owner's expected red/white-by-taint rule does not match its instructions.
3. Implementing the owner's remembered rule directly, or treating code 9 as "tainted ground", would be an INFERENCE that changes visible behaviour and is forbidden by PROCESS.md 15.1/15.13.
4. The missing WALL_HUB range pass is shared construction behaviour. It was not patched locally in the ghost renderer, and no substitute taint-colour path was added.

Owner decision needed: identify the exact building/template, map and visible element ("seal": faction bib or `excirclemarker` decal), and confirm the red/white expectation against original RotWK 2.01 with that reproduction. With that evidence the next session can trace the responsible path and write a failing `PORT-170*` test before changing it.

## Verification state

No `PORT-170*` test exists yet because no RW-supported expected taint colour has been established. The full `openbfme_tests` target was configured/building in this sandbox; no completed test result is claimed. No PR and no upstream PR were opened.

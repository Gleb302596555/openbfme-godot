# Port 49 — W3DModelDraw/AlphaCameraFadeInnerRadius: RW read

Issue #153. Target: the community-modified RotWK 2.01 image used by the Ennorath handoff (caveat S-001). Addresses below are virtual addresses of that image. Every branch was read linearly with `tools/mod/rw.py ... dis`; a displacement scan was used only to find candidates, never as evidence of absence.

## 1. Parse and defaults

The registered module name is `W3DScriptedModelDraw` (string RW `0xBDBBB0`). The module create RW `0x464092` allocates `0x2E8` and calls the constructor RW `0x4C04A3`; the data create RW `0x4640CD` allocates `0x188` and calls RW `0x4C85E9`. The data constructor stores the data vtable RW `0xBE0D40` (`0x4C860D`) and the parser RW `0x4C893A` pushes the shared field table RW `0xBE1320` (`0x4C8940`).

| field | field-table row | parser | data offset | constructor default |
|---|---:|---:|---:|---|
| `AlphaCameraFadeOuterRadius` | `0xBE1610` | `0x42ED1C` (`parsePositiveNonZeroReal`) | `+0x144` | `0.0` (`0x4C86F7`) |
| `AlphaCameraFadeInnerRadius` | `0xBE1620` | `0x42ED1C` | `+0x148` | `0.0` (`0x4C86FF`) |
| `AlphaCameraAtInnerRadius` | `0xBE1630` | `0x42EEFA` (`parsePercentToReal`) | `+0x14C` | `1.0` from `0xBD1908`, stored at `0x4C8722` |

`0x42ED1C` stores the scanned real and then refuses a value `<= 0` (the message string is RW `0xBD4304`). `0x42EEFA` calls `0x42EB18`, which scans the token and multiplies by the float at `0xBE5600` (`0.01`); it does not clamp the percentage. The port already has these three rows in `W3DModelDraw.cpp` (`parsePositiveNonZeroReal`, `parsePositiveNonZeroReal`, `parsePercentToReal`) and the same constructor defaults.

## 2. Reachability of the consumer

Class chain: create `0x464092` -> constructor `0x4C04A3` stores the primary vtable `0xBDFFE8` (`0x4C04D3`) and the secondary tables `+0xC = 0xBE3B78`, `+0x10 = 0xBE3B70` (`0x4C04D7` / `0x4C04DE`). Primary slot `0x2C` (table word `0xBE0014`) is `0x4C72BE`.

`Drawable::draw` RW `0x67C482` builds the drawn transform (`0x67C1FB`), walks the drawable's draw-module vector at `+0x14C`, and calls primary slot `0x2C` at `0x67C50C`. `0x4C72BE` calls the frame gate `0x4BFAF4` at `0x4C73C7`. `0x4BFAF4` compares the client frame global `0xDC7580` with draw `+0xB8` and jumps to `0x4BF560` at `0x4BFB01` when they differ. `0x4BF560` requires the render object pointer `+0x50` (`0x4BF56C`) and the current animation-state pointer `+0x18` (`0x4BF575`), then calls the fade consumer `0x4B52C8` at `0x4BF586`, passing the module data (`[draw+4]`) as the argument. The data constructor inserts `<DefaultEmptyIdleAnimationState>` (port mirror: `W3DModelDraw.cpp`, RW constructor `0x4C85E9` / `0x4C87DD`), so a model draw with only a `DefaultModelConditionState` still has `+0x18` and reaches the consumer. The secondary slots `0x24/0x28/0x2C/0x30/0x34` (`0x4C0B15/0x4C0B38/0x4C0B88/0x4C0BC0/...`) call the same gate; the primary draw path is sufficient proof.

The enable gate draw `+0x28D` is set to `1` by the constructor (`0x4C0708`), written by the setter `0x4783D1` (primary slot `0xE8`, table word `0xBE00D0`), and transferred by the xfer block `0x4C5997`. No caller of the setter was located in the read draw path; INFERENCE: the constructor value is what the fade consumer sees in normal play.

## 3. Consumer `0x4B52C8 .. 0x4B5494`

`esi` = draw module, `edi` = module data. The routine first clears render byte `+0xBC` (`0x4B52D4`).

| branch | instructions | effect |
|---|---|---|
| disabled | `0x4B52DB..0x4B52E2` | draw `+0x28D == 0` -> return, no opacity write |
| at-inner gate | `0x4B52E8..0x4B52FB` | `1.0 <= data+0x14C` -> return, no opacity write. The gate is the at-inner alpha, not the outer radius |
| camera / drawable position | `0x4B5301..0x4B5325` | camera XY from global `0xDE447C`, vslot `0x11C`; drawable position from `0x676711(draw+8)` |
| squared XY distance | `0x4B532A..0x4B5366` | `(cx-dx)^2 + (cy-dy)^2`; Z is not read. `0x4B5347` reads outer `+0x144`; `jb 0x4B53AB` only when `dist2 < outer^2` |
| outside / at outer | `0x4B5368..0x4B53A6` | `0x670AF4(drawable,1)`, render vslot `0x194(0)`, render vslot `0x5C(1.0)`; if static-sort `+0x158 >= 0`, render `+0x98 = 0` |
| inside / at inner | `0x4B53AB..0x4B53C7` | reads inner `+0x148`; `inner^2 >= dist2` -> alpha = at-inner `+0x14C` |
| between radii | `0x4B53C9..0x4B5401` | `dist = sqrt(dist2)`; `span = outer-inner`; `alpha = ((span-(dist-inner))*atInner + (dist-inner)) / span`. At the inner boundary this is at-inner; at the outer boundary it is `1.0` |
| effects-LOD override | `0x4B5405..0x4B5433` | global `0xDE3B84` (TheGameLODManager, see the port's `GameLODManager.h` and `docs/OPTIONS.md`); if non-null and `+0x1778 <= 1` (EffectsLOD VeryLow/Low), takes `0x670AF4(drawable,0)`, render vslot `0x194(1)`, then the outer tail's opacity `1.0` |
| alpha > 0 | `0x4B5438..0x4B544D` | compares with float `0xC1B594` (`0.0`); `0x670AF4(drawable,1)`, render vslot `0x194(0)` |
| alpha <= 0 | `0x4B544F..0x4B5456` | `0x670AF4(drawable,0)`, render vslot `0x194(1)` |
| opacity write | `0x4B5458..0x4B5471` | render vslot `0x5C(alpha)` |
| fading marker / static sort | `0x4B5474..0x4B548B` | render byte `+0xBC = 1`; if data `+0x158 >= 0`, render `+0x98 = data+0x158` |

Camera position: global `0xDE447C` is stored at `0x69EF6B` from the view interface; its vslot `0x11C` (W3DView table `0xBDD490`, word `0xBDD5AC`) is `0x487668`, which calls `0x53B370`. `0x53B370` reads the camera transform translation (`+0x24`, `+0x34`, `+0x44`) into the returned vector. This is the camera eye, not the look-at target. The port's equivalent is `TacticalCamera::eye()` / the displayed eye.

Drawable position: `0x676711` returns the drawable's interpolated drawn position (the same gather/interpolation family as `0x6765B9`, see `RenderInterpolation.h`). The port's equivalent at the draw call is the drawable's synced `Thing::getPosition()`.

Helper `0x670AF4` writes drawable byte `+0x441` to its argument and, when `+0x450` is non-null, calls `0x764358` with it. The port already documents `+0x441` as the render visible byte (`Drawable.h`, RW `0x5FA2CE` / `0x4D12FA` readers). INFERENCE from the consumer's complementary arguments: render vslot `0x194` is the hidden/visibility toggle (argument `1` when the visible byte is `0`, argument `0` when it is `1`) and vslot `0x5C` is the opacity setter. The slot bodies were not decoded for this issue; the fade itself is proven by the consumer instructions above.

## 4. Port gap and scope

Before the fix the port parsed all three fields but did not apply the consumer:

* `W3DScriptedModelDraw::initializeState` reported S-096 for `AlphaCameraFade*` / `BirthFadeTime` / `StaticSortLevelWhileFading`.
* `Drawable::advanceAnimation` / `advanceAnimationLocal` called `W3DScriptedModelDraw::advance` only for entries marked `animated`; a static model draw with camera-fade fields never ran the draw-time consumer.
* The Godot model view read `Drawable::drawOpacity()` (projectile / phantom / stealth) but not the draw module's `frame().opacity` / hidden fade, so even an applied draw-module opacity would not reach the instance.

The fix belongs in that existing path: run the camera-distance consumer for model draws (animated or not), write the existing draw-module opacity/hidden state, and let the Godot model view consume it. `BirthFadeTime` and `StaticSortLevelWhileFading` remain S-096; the static-sort write is a render-sort field, not the alpha mechanism. The effects-LOD override is part of the consumer and uses the port's existing EffectsLOD state. This is client presentation state: it does not enter the simulation hash and needs no xfer block.

## 5. Port integration and checks

The fix runs `W3DScriptedModelDraw::updateCameraFade` from `Drawable::advanceCameraFade`, called by both existing animation entry points before the track step (RW call order `0x4BF586`). It writes the existing `m_opacity` / `m_hiddenByFade`; the Godot model view multiplies that opacity into the instance and removes the instance while hidden. The same existing frame opacity also carries the per-animation FadeBeginFrame/FadeEndFrame ramp (RW 0x4B5497), so that ramp now reaches the renderer too. The view cache remembers the hidden state so leaving it rebuilds the instance. `HudDevice::applyCamera` supplies the final displayed eye, including the existing CAMANIM-1 mode-4 override, converted back to SAGE axes; `GameWorld` supplies the existing EffectsLOD. No camera behavior, simulation state, hash or xfer block is added.

`engine/tests/test_port_49.cpp` holds the synthetic `PORT-49*` cases; `engine/cmake/Port49.cmake` lists them for `openbfme_tests`. The cloud check is the owner-mandated test-only configure (`-DOPENBFME_BUILD_GODOT=OFF`), `-j1` build and `-tc="PORT-49*"` run. That option excludes `GodotGameWorld.cpp` and `GodotInGameHud.cpp` from this build (engine/CMakeLists.txt gates them behind `OPENBFME_BUILD_GODOT`); their compile and the visible Ennorath check remain for the owner's station. The red-before run removes only the two `advanceCameraFade();` call sites, so the new test still compiles and `PORT-49 path` exercises the old animated-only gap. The neighboring filter `draw data*` selects no case in this tree (doctest filters test names, not source file names); `mapobj draw data*` is run separately. The file itself is also checked with `-sf=*test_w3d_draw_data.cpp`.

The final cloud build was made at `01cecd52` (product commit `fc419afe`, rebase base `547e37bd`), Debian 12 / GCC 12.2, Release, Ninja, `-j1`, Godot OFF, with no install/corpus environment variables. `PORT-49*`: 6/6 cases and 53/53 assertions. Neighbors: `draw runtime*` 30/30 (362 assertions), `draw data*` 0 selected, `LOCAL-14*` 7/7 (87), `mapobj*` 35/35 (913), `mapobj draw data*` 6/6 (458), and the draw-data source-file filter 16/16 (846). Every process exited 0. One LOCAL-14 Ennorath case and three mapobj retail cases printed `SKIP:` and returned without their data checks; doctest counts those early returns as passed, so they are not claimed as retail verification. The red run before restoring the two call sites failed exactly the normal and local static-path assertions (`1 == 0.625`): 5/6 cases, 51/53 assertions. The broad/nightly filters and Godot smoke test were not run in this cloud lane (PROCESS section 7б).

Ennorath 1.9.8.2 examples read from the mod INI (not committed): `BlueMtnCol` (`data/ini/object/civilian/bluemountains.ini`, object at line 1810) uses outer 200 / inner 100 / at-inner 10%; `BlueMtnMountain` (same file, line 6128) uses 500 / 250 / 0%; `EreborMountainForCastle` (`data/ini/object/goodfaction/structures/dwarven/campsandcastles.ini:986`) uses 600 / 200 / 10% and also sets `StaticSortLevelWhileFading = 2` (the sort write stays S-096). The issue's "99 draws at 10%" is not uniform: the comment-excluded audit count is 99 uses of each camera row, with at-inner values 15% (37), 0% (30), 10% (21), 00% (10), 5% (1).

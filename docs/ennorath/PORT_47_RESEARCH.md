# PORT-47 / issue #151 — research checkpoint (2026-11-08)

Status: **reader implementation in progress, unblocked by Local 21 (#161)**.
The historical scope stop below was resolved by main b03e82c8. The owner
authorized continuing #151 on `Drawable::recolorRenderObject`.
All instruction addresses below were read with `tools/mod/rw.py ... dis` against
the supplied RotWK 2.01 executable. No retail bytes or mod INI are included.

## Historical port before Local 21 / reader fix

`SubObjectsUpgradeModuleData::m_excludeSubobjects` is parsed by
`UpgradeModuleClasses.cpp` but has no runtime consumer.
`SubObjectsUpgrade::processUpgrade` sends the texture swaps through
`ObjectClientHooks::upgradeTextures`; its RecolorHouse branch only increments
`houseRecolorsNotPorted`. There is not an existing implementation to reuse.
The texture event currently travels through `ClientEventRecorder`,
`DrawableManager`, `Drawable::setUpgradeTextures` and the model draw path.

House colour currently reaches the device through the drawable palette and
`GodotGameWorld.cpp`; `GodotW3DMaterial.cpp` applies it to HOUSECOLOR meshes.
Neither path currently consumes this module's exclusion list. Merely adding an
event or changing the diagnostic counter would not implement the visual effect.

## Proven module and dispatch chain

- Create at RW `0x64FE80`: allocation size 0x24, call to constructor at
  `0x64FEA7 -> 0x8B8EE4`.
- Constructor `0x8B8EF4..0x8B8F08` installs primary vtable `0xC6E830`,
  secondary interface at this+0x10 with vtable `0xC6E7E8`.
- That interface's slot +0x28 (table word `0xC6E810`) is `0x8B91AD`.
  Its body adjusts this by -0x10 at `0x8B91B7`, consistent with the
  constructor, rather than relying on code proximity to identify the class.
- `0x8B91C1/0x8B91C3`: failed prerequisite helper skips the upgrade body.
- `0x8B91D8/0x8B91DA`: null drawable skips draw work.
- `0x8B9205..0x8B9215`: hide list at data+0x144.
- `0x8B9229..0x8B9236`: show list at data+0x138.
- `0x8B923B..0x8B9247`: combine hide/show return booleans;
  `0x8B9255..0x8B9260` refreshes after a visibility change.
- `0x8B9250 -> 0x8B911E`: texture swap helper, mode zero.
  `0x8B912F..0x8B913E` returns false for an empty data+0x15C list.
  `0x8B915F..0x8B918C` iterates draw modules, gets the interface at
  primary vslot +0xA8 and invokes its texture-swap slot +0x84.
- **Both gates apply**: `0x8B9265/0x8B9269` skip recolour if the texture
  helper returned false; `0x8B926B/0x8B9272` skip if RecolorHouse is false.
- `0x8B9274` adds 0x150 to module data; `0x8B927A..0x8B927D` passes this
  vector to drawable method `0x67273A`.
- `0x8B9282..0x8B9284` calls the existing custom-animation helper.

## Drawable and model-draw reader

`0x672743/0x67274A` gates on drawable byte +0x3AA (house colour enabled).
`0x67274C..0x672781` contains the campaign gate: `0x5FF924` is the OR
of `0x626355` (campaign context) and `0x6253FD` (Living World context).
Outside that context it proceeds; inside it requires an object and a KindOf
match against BASE_SITE (0x78) or ALWAYS_SHOW_HOUSE_COLOR (0xB5).
`0x70C548` passes the template KindOf mask to `0x661359` (any-bit test).
`0x672783..0x6727A8` traverses draw modules, gets their +0xA8 interface,
and calls interface vslot +0x78 with the exclusion vector.

Correction to the first checkpoint: `0x4641D4 -> 0x4C97BC` is the
**Quadruped** variant, not the scripted class. That variant installs primary
vtable `0xBE1B98` and the interface at this+0x0C, vtable `0xBE1AD0`
(`0x4C97CC..0x4C97D9`). Primary slot +0xA8 is `0x4C9323`, whose instructions
return this+0x0C. Interface slot +0x78 (`0xBE1B48`) is **`0x4B8701`**.
The scripted create is `0x464092`: `0x4640BC -> 0x4C04A3`.
Its constructor installs primary `0xBDFFE8` (`0x4C04D1`) and the interface
at this+0x0C, `0xBE3B78` (`0x4C04D7`). Its primary +0xA8 also resolves
to `0x4C9323`; the interface +0x78 (`0xBE3BF0`) also resolves to
`0x4B8701`, and +0x84 (`0xBE3BFC`) to `0x4C1068`. Thus both the
scripted class and the inspected variant reach the same reader.

`0x4B870A..0x4B8732` requires the owning drawable, object, the result of
`0x68B678`, and the render object. It fetches colour from result+0x2A0.
`0x68B678` reads the team at object+0x31C and tails into `0x79FD6F`,
which reads its controlling-player record. This is the **day player colour**,
not the drawable override/night colour and not a custom hero palette:
`0x68B6F5` reads the same controlling-player+0x2A0, whereas `0x68B71F`
reads +0x2A4 for night; neither override nor night branch occurs in `0x4B8701`.

**Important non-obvious behaviour:** `0x4B8734..0x4B874F` tests vector
begin/end, then reads only the **first element**, not a loop over all names.
An empty vector passes null. A nonempty vector passes that first string
(empty-string storage if necessary). `0x4B8750..0x4B8771` constructs kind-1
colour parameters and invokes the render object's vslot +0x1F8.
Do not implement an all-elements exclusion loop without contrary RW evidence.

## Render-object endpoint inspected

Mesh constructor `0x548F40` installs vtable `0xBE9610` at `0x548F6F`;
slot +0x1F8 is `0x54BDE0` (word `0xBE9808`).
At `0x54BE08..0x54BE43`, a non-null exclusion string is compared to the
portion of the mesh name **after its first dot**, if a dot exists:
`0x54BE27` calls imported `strchr`, `0x54BE38` calls imported `_strcmpi`,
and equality jumps to the return path `0x54C6B8`. A name without a dot does
not take this skip. Thus the observed endpoint really excludes a matching
sub-object from recolouring; it does not recolour only the named object.

`0x59B1A0..0x59B1F1` was also inspected: it iterates child render objects,
forwarding both arguments through their +0x1F8 slot at `0x59B1D4`.
It is installed as vslot +0x1F8 in table `0xBEC7F0`; the inspected
constructor body writes that table at `0x59F025`. The exact hierarchy
composition selected by every model/LOD is not claimed here.

## Historical blocker, resolved by Local 21: render-object replacement (S-090 / S-5015)

The requested reader is an operation on the **current render object**, not a
persistent negative house-colour flag. An excluded mesh returns before changing
its material (`0x54BE43 -> 0x54C6B8`); it keeps whatever colours/textures it
already has. Setting its palette to zero would be an unsupported behaviour.

The preceding texture operation uses the same shared lifecycle as all model
state changes:

- Scripted interface slot +0x84 is `0x4C1068`. After replacing its upgrade
  texture vector (`0x4C1072 -> 0x4BFFE6`), it calls primary slot +0x100,
  `0x4C451D`, with **force = 1** (`0x4C107F`).
- The shared `0x4C451D` checks the current state, auxiliary change flag and
  force at `0x4C45A7..0x4C45B5`. Its later reuse path also requires both
  states' texture vectors at state+0x64/+0x68 to be empty
  (`0x4C45DD..0x4C4602`, `0x4C4630..0x4C4637`), plus the other compared
  inputs. `0x4C4648..0x4C464B` explicitly rejects reuse for force = 1.
- Reuse calls `0x4C2F4E` on the existing render object at `0x4C466F`.
  The other path invokes teardown `0x4B2C18` at `0x4C46B6`, then builds the
  render object, for example through `0x4C4CA8 -> 0x536310`.
- The asset variant builder at `0x536A40` handles texture replacements
  (`0x536B0E -> 0x53697A`) and then ordinary palette application
  (`0x536B2E`, render vslot +0x1F4). The separate upgrade recolour vslot
  +0x1F8 runs **after** this model-state/texture operation.

The port does not represent that same lifecycle:

- `W3DScriptedModelDraw::setUpgradeTextureSwaps` replaces the vector and calls
  `rebuildTextureSwaps`, with no forced model-state operation.
- `applyModelState` recreates only when the resolved model name or turret
  bones differ (or there is no previous state). Its own source explicitly
  labels the RotWK `0x4C451D` implementation untranscribed (S-090).
- `GodotGameWorld::refresh_view` chooses a cached model from the resolved name,
  common palette and merged swaps. `GodotW3DMaterial::Create` applies those
  swaps and recolours from that palette while building every material.
  There is no representation of the material state left untouched by the
  exclusion, or the forced recreation boundary on the existing path.
- The merge with RandomTexture and ModelConditionState Texture is explicitly
  INFERENCE in `rebuildTextureSwaps`; S-5015 already records this common gap.

**Scope decision requested:** assign/approve the common render-object
replacement and per-mesh material-state work (S-090 / S-5015), coordinated with
Ports 48–50, before implementing this reader. Changing that lifecycle also
affects temporary/permanent sub-object visibility, RetainSubObjects, model
states, upgrade texture lists, RandomTexture and custom palettes. Those effects
must not be silently patched only for SubObjectsUpgrade or guessed from the
current port. INFERENCE: the complete set of visual regressions is not yet
measured; no new runtime behaviour is proposed or implemented.

## Historical remaining work at the scope stop

Owner decision on the scope above; then finish the lifecycle analysis, implement
one existing end-to-end path, obtain a mod reproduction scenario, add synthetic
PORT-47 tests/lane with red/green evidence and neighbouring behaviour, and open
the requested PR. No PORT-47 test exists, no test expectation was changed, and
no acceptance or visual verification is claimed. Game/mod data were not committed.

## Historical build checkpoint at the scope stop

The sandbox initially lacked CMake/Ninja; installed them inside ignored `.venv`
from PyPI along with capstone/pefile. Tests-only Release CMake configuration
succeeded. It reports missing MP3 fixture generators (lame/mpg123), unrelated
to the planned PORT-47 filter. The single-job build was restarted after rebasing onto origin/main
(29814b9d); it is not a completed build or passing test run. Build stopped when
the scope dependency was identified, rather than spending the full initial
build on a documentation-only stop.

## Reader integration after Local 21

Re-read with rw.py dis: 0x8B9265..0x8B9284, 0x67273A..0x6727AD,
0x4B8701..0x4B8771 and 0x54BE08..0x54BE43. Implement only their live reader:

- Keep the existing texture event first. Only a nonempty UpgradeTexture with
  RecolorHouse emits the subsequent recolour operation (0x8B9269 / 0x8B9272).
- Preserve drawable enabled/campaign gates (0x67274A / 0x672781). The existing
  campaign carryover pointer represents the linear-campaign context; Living
  World uses gameKind == 3 and livingWorld (0x6253FD).
- Require a controlling player (0x4B8727); snapshot kind 1 and its day colour,
  independently of night/custom palettes (0x4B872C / 0x4B8759).
- Snapshot only the first name, or empty for no exclusion (0x4B873C..0x4B874F).
  Deliver once to Drawable::recolorRenderObject after upgradeTextures. Its
  Local 21 implementation already owns all per-mesh and replacement state.
- No simulation state, new parser, renderer path or saved-state layout is
  introduced. Events are presentation-only, following ClientEventRecorder's
  existing ordered owner-thread transport. Existing client reload limitations
  (S-4190: no module loadPostProcess) are not expanded into a new save system.

The synthetic tests must exercise an actual parsed SubObjectsUpgrade through
ClientEventRecorder and DrawableManager, assert exactly one ordered recolour,
first-name-only/case-insensitive matching and retained base palette, plus no
recolour for the negative gates and reset on texture removal. No other module
fields or Local 21's implementation are to be changed.

## Current verification checkpoint

The reader is implemented in `SubObjectsUpgrade::upgradeImplementation` →
`ObjectClientHooks::recolorHouse` → the existing ordered recorder/manager path
→ `Drawable::recolorRenderObject`. The renderer and Local 21 lifecycle were
not changed. New client events are transient, not simulation state.

| Changed behaviour | RW evidence | Synthetic test in `test_port_47.cpp` |
|---|---|---|
| Texture then explicit recolour, once | 0x8B9265/69, 0x8B926B/72, 0x8B927D | upgrade recolours after texture replacement |
| Controller day colour, kind 1; not night or custom | 0x4B8727/2C, 0x4B8759 | same case, different custom/base/night and explicit colours |
| First exclusion only; empty means no exclusion | 0x4B873C..0x4B874F | same case and empty exclusion case |
| Excluded mesh retains base colour; match after first dot, case-insensitive | 0x54BE24..0x54BE43 | same case, Blade / Armor / Extra.Blade and a dotless mesh |
| House-enabled and campaign KindOf gates | 0x67274A, 0x672760/81 | no recolour case, ten input scenarios, including no controller |
| Subsequent texture removal resets overrides through existing lifecycle | 0x4C107F, 0x4C4648/4B, 0x4B2C18 | same case, removal assertion |

Campaign-helper instructions were re-read at 0x5FF924, 0x626355 and 0x6253FD.
INFERENCE (port representation, not RW branch semantics): the port's optional
`campaignCarryover()` represents the current linear campaign pointer. Living
World uses the existing gameKind/livingWorld fields. No new campaign-state
plumbing was introduced; campaign startup/load outside these existing fields
has not been verified here.

All changed translation units, including the synthetic test, compile in the
sandbox. Runtime assertions now have focused red/green evidence on the pre-rebase
base (see below). The standard full executable and rebased-base verification
remain pending the tests-only Release single-job build.
`tools/test_select.py --base origin/main` selects FULL (no test map; CMake
registration cannot be traced). Game-dependent skips will not count as passes.

Visible verification must be done by the owner, not claimed from these tests.
The supplied Ennorath archive was checked beyond the field occurrence:
`LAGondorFighter` has the field, but its `GondorFighterCommandSet` has no active
purchase buttons in this mod. It is NOT a valid normal-click reproduction.

Use the verified playable route instead:

1. Start an Ennorath Angmar skirmish with Cheat Mode and a bright player colour;
   pause the opponent's AI, give resources, keep instant building enabled.
2. Spawn `AngmarDarkDunedainHorde` and `AngmarForgeWorks` through Cheat Mode
   (named spawn API if not in the filtered UI list; see `docs/CHEAT_MODE.md`).
3. Level the forge through its normal building-upgrade buttons. Research
   `Command_PurchaseTechnologyAngmarDarkIronBlades` (level-2 prerequisite) and
   `Command_PurchaseTechnologyAngmarDarkIronArmor` (level-3 prerequisite).
4. Select the horde and purchase `Command_PurchaseUpgradeAngmarDarkIronBlades`
   first, then `Command_PurchaseUpgradeAngmarDarkIronArmor`. Its active command
   set includes both. Compare the visible blade before/after the armour upgrade:
   armour is recoloured with the player's day colour, the excluded blade keeps
   its preceding material colour. Repeat on a night map to distinguish the
   explicit day recolour from the ordinary night palette.
5. Include related house/custom-palette entries of `VISUAL_REGRESSIONS.md` in
   the owner's next visual check batch; this branch does not claim them passed.

Sources checked in the extracted archive: `object/evilfaction/units/angmar/
angmardarkdunedain.ini` (`Armor_Upgrade` and the blade exclusion),
`object/evilfaction/hordes/angmar/angmarhordes.ini` (horde command sets),
`commandset.ini` and `commandbutton.ini` (active purchases and prerequisites).
This verifies a data path, not a completed visual test. Game assets/INI were
not committed; the extracted archive remains under /tmp only.

## Focused runtime evidence before final rebase

Base `8b8af54f`, reader/test commit `aa9351ec`: linked the existing
`test_port_47.cpp` object with the original `test_main.cpp` and built engine
libraries into a temporary focused runner, without other test objects. No
retail files or installation environment are required.

```
[doctest] test cases:   3 |   3 passed | 0 failed | 0 skipped
[doctest] assertions: 114 | 114 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Red control: copied the core archive under /tmp and replaced only the
`UpgradeModuleClasses.cpp` object, changing the one `recolorHouse` call back to
its former diagnostic increment. The identical test object then reports
3 failed cases, 8 failed assertions (exit 1). Tracked sources, the main archive
and the normal executable were not modified by this control.

The first focused attempt exposed fixture errors, not accepted engine changes:
(1) a dotless child mesh is resolved from its own asset file, not the HLOD's
container file; (2) passing a null team to object construction gives the neutral
player's default team. The fixture now supplies the separate synthetic mesh
file and an explicit ownerless team, with a null-controller assertion. Both
expectations remain as required by RW 0x54BE32 and 0x4B8727.

After this evidence, fetched `origin/main` (`bc66c72f`) and rebased successfully;
only appended NIGHT_WATCH rows conflicted, both sides retained. A new single-job
build and repeat focused/standard checks are queued on that base. The result
above is NOT claimed as a completed test of the rebased branch. Standard
`openbfme_tests -tc="PORT-47*"`, neighbours and FULL are still pending.

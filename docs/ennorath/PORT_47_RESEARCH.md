# PORT-47 / issue #151 — research checkpoint (2026-11-08)

Status: **in progress; no engine change, no acceptance claim**. This is a
checkpoint, not the completed analysis/implementation requested by issue #151.
All instruction addresses below were read with `tools/mod/rw.py ... dis` against
the supplied RotWK 2.01 executable. No retail bytes or mod INI are included.

## Current port

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
`0x67274C..0x672781` contains a further global/object gate; its semantics
are **not yet established** and must not be guessed.
`0x672783..0x6727A8` traverses draw modules, gets their +0xA8 interface,
and calls interface vslot +0x78 with the exclusion vector.

The scripted model draw create call `0x4641D4 -> 0x4C97BC` installs primary
vtable `0xBE1B98` and the interface at this+0x0C, vtable `0xBE1AD0`
(`0x4C97CC..0x4C97D9`). Primary slot +0xA8 is `0x4C9323`, whose instructions
return this+0x0C. Interface slot +0x78 (`0xBE1B48`) is **`0x4B8701`**.
This links the reader to the class, not just to a neighbouring function.

`0x4B870A..0x4B8732` requires the owning drawable, object, the result of
`0x68B678`, and the render object. It fetches colour from result+0x2A0.
The identity/meaning of that colour source still needs verification.

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
Its class/vtable provenance is still unproven; do not yet claim this proves
which model hierarchy levels the live upgrade reaches.

## Remaining before implementation

1. Prove the drawable's global/object gate and colour-source identity.
2. Complete render hierarchy provenance and review the remaining mesh branches
   relevant to this call; verify base model-draw dispatch as well as scripted.
3. Read mod INI (not yet downloaded) to select an owner reproduction scenario.
4. Finish the analysis commit; implement the existing event/draw path end to
   end, including cache invalidation and any required save state, without
   disturbing other SubObjectsUpgrade fields or concurrent model-draw work.
5. Add the new Port47 CMake lane and synthetic `PORT-47*` tests, demonstrate
   red without the fix, then green, plus a neighbouring-behaviour test.
6. Add NIGHT_WATCH (and STOPS if needed), rebase, and open the requested PR.

## Build checkpoint

The sandbox initially lacked CMake/Ninja; installed them inside ignored `.venv`
from PyPI along with capstone/pefile. Tests-only Release CMake configuration
succeeded. It reports missing MP3 fixture generators (lame/mpg123), unrelated
to the planned PORT-47 filter. The single-job initial build was started;
no `PORT-47` tests exist yet and no test run is claimed.

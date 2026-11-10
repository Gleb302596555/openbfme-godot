# PORT-119 / P-56: two-in-one heroes stand inside each other

Issue #303, problem P-56 (owner, 2026-11-08): heroes made of two models ("two in one") stand with both figures in one
place instead of side by side; they should stand as in RotWK 2.01.

## 1. What the mod's data actually is

The issue's "Known" block guessed the twins were a horde whose members stand on rank slots. The data says otherwise
(Ennorath 1.9.8.2, `data/ini/object/goodfaction/units/imladris/imladriszwillinge.ini`):

| line | what it is |
|---|---|
| 2 | `Object ImladrisZwillingeEdain` — the two-figure hero. It has **no** `HordeContain` (the whole file has none). |
| 10 .. 23 | `Draw = W3DScriptedModelDraw ModuleTag_DRAWShagratEmmiter`: `DefaultModelConditionState Model = euella_emmitr` — a rig. No `Skeleton` line. |
| 25 .. | `Draw = W3DScriptedModelDraw ModuleTag_HeroShagrat`: `AttachToBoneInAnotherModule = FIREBONE01`, `Model = euElladanN`, `Skeleton = EUHaldir_SKL` — the first visible figure. |
| 1232 .. 1236 | `Draw = W3DScriptedModelDraw ModuleTag_DRAWGorbagEmmiter`: `Model = euelro_emmitr` — the second rig. No `Skeleton` line. |
| 1237 .. 1241 | `Draw = W3DScriptedModelDraw ModuleTag_HeroGorbag`: `AttachToBoneInAnotherModule = FIREBONE02`, `Model = euElrohirN`, `Skeleton = RULegolas_SKL` — the second visible figure. |

So the two figures are **four draw modules of one object**: two rigs (the mod's `*_emmitr` models, which carry the
bones) and the two figures attached to those rigs' bones. Nothing stands on a horde slot; the placement is exactly
`AttachToBoneInAnotherModule`. A whole-mod scan of the mod's INI finds the same pattern on one more object:
`GorbagShagratUnited` (`FIREBONE01` + `FIREBONE02`); the twins and that object are the mod's only two-figure heroes,
and the ChildObjects that inherit from them (`ImladrisZwillingeEdain_Summoned`, `_GraueSchar`, `_forMM`,
`_forMM_Summoned`, `ImladrisZwillingeEdain_Schmiede`) inherit the four draws.

## 2. What RotWK does with it (RW 2.01)

`W3DScriptedModelDraw::adjustTransformMtx` **RW 0x4B686D** builds the module's model matrix and then:

| RW address | what it does |
|---|---|
| 0x4B6878 .. 0x4B68C2 | the *parent drawable* branch (`[this+0xA4]` drawable, `[this+0xB4]` bone name, `call 0x672B5B`); not this object's case |
| 0x4B6A6F .. 0x4B6A72 | `eax = [this+4]` (module data), `ebx = eax + 0x40` = `m_attachToDrawableBone` |
| 0x4B6A75 .. 0x4B6A81 | `AsciiString::isEmpty` (`call 0x401E64`); empty -> no attach, the object matrix stays |
| 0x4B6A95 .. 0x4B6AA2 | `ecx = [this+8]` (the drawable) and `call 0x672B5B(name, &mtx)`; false -> 0x4B6B2C (object matrix) |
| 0x4B6AAA .. 0x4B6B1C | the bone's 12 floats **replace** the module's matrix |
| 0x4B6B21 .. 0x4B6B26 | `call [eax+0x104]` = the attached-transform callback, only on a successful lookup (base `ret 4`, RW 0x9F3A3C) |

`Drawable::0x672B5B` (the bone lookup) walks the drawable's **module list** (`[drawable+0x14C]`), asks each module's
current model for the bone (`0x4B2F19`: `[module+0x44]` model -> `[vtable+0xC8]` find bone by name ->
`[vtable+0xCC]` bone transform), and returns on the first module that has it. A module whose data names no
`Skeleton` still answers: the lookup is on the module's model instance, whose hierarchy comes from the model file's
HLOD — the mod's rigs (`euella_emmitr`, `euelro_emmitr`) are exactly that shape.

The port has this mechanism (lane LOCAL-29, issue #218): `Drawable::entryTransform`
(`engine/src/GameClient/Drawable.cpp`) replaces the entry matrix by `Drawable::boneTransform` and calls
`W3DScriptedModelDraw::adjustAttachedTransform` (slot +0x104), and `GodotGameWorld::refresh_view`
(`engine/src/GodotDevice/GodotGameWorld.cpp`) hands that matrix to the instancer for every model entry.

## 3. Result

`engine/tests/test_port_119.cpp` (lane file `engine/cmake/Port119.cmake`) holds the behaviour for the twins' exact
shape: four draw modules of one object, two rigs naming only a `Model` (no `Skeleton` line), two figures carrying
`AttachToBoneInAnotherModule`, and the two figures' transforms 10 apart, side by side, with the object's position
between them; a figure whose bone is not there keeps the object's transform.

Result of the run (`engine/build/openbfme_tests -tc="PORT-119*"`): 4 test cases, 63 assertions, all pass, no game files
mounted. The tests pass on `main` as it stands: **the mechanism P-56 needs is already in the port** (lane LOCAL-29, issue
#218, `docs/STOPS.md` row S-7232, `docs/ennorath/NIGHT_WATCH.md` row "Local 29"), which landed 2026-11-10 and has not been
checked in game yet ("в игре ждёт"). So the issue's first instruction applies: do not build a second path.

What was different before LOCAL-29: `Drawable::entryTransform` did not exist, every draw module's model was placed on the
object's matrix, so both figures of the twins stood in the same point. After LOCAL-29 the second and fourth module are
placed on the rigs' bones (`FIREBONE01`, `FIREBONE02`), which is what RotWK does at RW 0x4B6A6F .. 0x4B6B26.

Check the port's own side of the shape (all four held by the tests):
* `W3DScriptedModelDrawModuleData::buildFieldParse` -> `m_attachToDrawableBone` (lower-cased, RW's `parseLowercasedString`
  at data + 0x40);
* `Drawable::addEntry` copies it to `DrawEntry::attachToBone` for every model module (four `Draw =` lines -> four entries,
  INI order kept);
* `Drawable::entryTransform` / `Drawable::boneTransform` replace the entry matrix by the rig's bone in the rig's current
  pose, including a rig whose data names no `Skeleton` (the mod's `*_emmitr` models: the hierarchy comes from the model
  file's HLOD);
* `GodotGameWorld::refresh_view` hands that per-entry matrix to the instancer on every path (new instance, fast path, slow
  path).

If the owner still sees the two figures in one point after this lands, the remaining suspects are outside the transform
code, and this is the order to look at them:

1. the rig models do not load in the port's asset mount (`euella_emmitr` / `euelro_emmitr`): `MapObjectReport` /
   `DrawableManager` report `model ... is not available`; without the rig there is no bone, and RotWK itself would keep
   the figure on the object's matrix (RW 0x4B6AA4 -> 0x4B6B2C);
2. the rig model has no `FIREBONE01` / `FIREBONE02` bone (`W3DScriptedModelDraw::errors()` reports `bone ... not found`
   for the module's own bones only);
3. both figures resolving to the same bone: `Drawable::boneTransform` takes the first module that has the bone, exactly
   like RW 0x672B5B, so this would be data.

## 4. INFERENCE

* Whether the two figures of RotWK's own `GorbagShagratUnited` (the vanilla hero the mod copies: `shagrat_emmiter` /
   `gorbag_emmiter`, `GeometryOffset = X:0 Y:-9.5`) stand on the same rig bones is not established here: the rig models
   are art, not INI. INFERENCE that the mod's twins use the same trick with its own `*_emmitr` rigs.
* "The two figures stand side by side" is thus RW's attach rule plus the rigs' bone positions; the port reproduces the
  rule (tests), the bone positions come from the mod's models.


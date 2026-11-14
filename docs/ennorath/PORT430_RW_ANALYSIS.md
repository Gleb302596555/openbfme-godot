# Port 430: a model texture named `None` (P-109)

Fork issue #430 (upstream `Farms — P-109`, owner label Port 177), task 153. Read on the owner's
RotWK 2.01 `game.dat`, MD5 `daed3668006cd90f01c34e5a7da1901f`, with `tools/mod/rw.py dis|str|xref`
and raw immediate sweeps (`pefile` + `struct.unpack` over `.text`, any 4-byte little-endian
value equal to the target, opcode-agnostic). `DONOR BFME2 1.06` facts are marked
`DONOR BFME2 0x…`, mechanical conclusions drawn from them `INFERENCE`.

Symptom the owner reported: the console prints

```
W3DInstancer: EUSKLADN_A.GBFARM_BIB: texture not found: None
W3DInstancer: ANGPORTER.BUCKET: texture not found: None
```

The third model of P-109, `CAH_skull`, takes its texture from an FX material parameter
(`NormalMapped.fx`, held by #410 in Local 54) and shows the same message for the same reason.

## 1. Where the name comes from

Two places in the W3D data name a texture, and both hand the string to the texture loader
unchanged:

| source | port | RW |
|---|---|---|
| mesh chunk `W3D_CHUNK_TEXTURE_NAME` (0x32) | `MeshModelClass::read_textures`, `Libraries/WWVegas/WW3D2/meshmdlio.cpp:318-350` | reader reached from the mesh chunk walk; the name is copied verbatim, no rewriting |
| FX material string property (`W3D_CHUNK_SHADER_MATERIAL_PROPERTY`, type 1) | `MeshModelClass::read_shader_materials`, `meshmdlio.cpp:431` | `FXShader::Parameter::Load_W3D`, DONOR BFME2 0x0015115C |

The INI-side pair parse (`Texture = A B`, `W3DModelDraw::parseTexturePair`, port
`GameEngineDevice/.../W3DModelDraw.cpp:252`, RW 0x4C2EEC) is a replacement table and not the
source of the string; the models of the three symptoms carry it in their W3D file.

## 2. What retail does with the name

### 2.1 The loader: a name without a dot is never a file

`RW 0x530D29` is the constructor of the compiled-texture load (the port's
`Resolve_W3D_Texture` / `Resolve_Packed_W3D_Texture`):

1. `RW 0x530D40` copies the name as written.
2. `RW 0x530D45` calls `strrchr(name, '.')`.
3. With a dot, four candidates are written after it, each flagged as a candidate at `+0x10`
   (file) with the format byte `+0xD = 1`:
   `.dds` `RW 0x530D61` (string `RW 0xBDC7BC`), `.tga` `RW 0x530D82` (string `RW 0xBDC7EC`),
   `.jpg` `RW 0x530DA3` (string `RW 0xBDC7B4`), `.png` `RW 0x530DC4` (string `RW 0xBDC7C4`,
   the alpha stream at `+0x1C`).
4. Without a dot, `RW 0x530D59` is `test edi, edi` / `je 0x530DE1`: the function returns with
   both file pointers (`+0x10`, `+0x1C`) still null and no candidate at all.

So `None` — no dot — never becomes a file name: no `None.dds`, `None.tga`, `None.jpg`,
`None.png`, no `art\compiledtextures\no\none.*`, no lookup of any kind.

### 2.2 What the null pointers produce: the 1 × 1 magenta surface

The texture creation `RW 0x53193E` (its only caller is `RW 0x531C04`, reached from
`RW 0x531BF9`) is the branch that builds a texture without file bytes:

- `RW 0x531958` tests the file pointer; null goes to `RW 0x531B02`.
- `RW 0x531B07` .. `RW 0x531B44` create a surface of format 0x15 whose single texel is
  pushed as `0xFFFF00FF` (`RW 0x531B44`), i.e. opaque magenta, and the texture object is
  returned as a normal, valid texture.

This is the whole of retail's behaviour for the name. The mesh draws. One magenta texel.

### 2.3 Retail prints nothing

`game.dat` 2.01 contains **no** "texture not found" string: every string of the image was
scanned (both the ASCII and the 2-byte string tables), and the closest candidate,
`"Unable to open %s"` (`RW 0xBEAC50`), is the file system's own log line — its single
reference is `RW 0x575D79`, inside the file-open path (`RW 0x575D40` region), which no
texture path calls. There is no message anywhere along `RW 0x531BF9` → `RW 0x53193E` →
`RW 0x531B07`.

Consequently the port's line `texture not found: None` is not a retail behaviour at all. It
is the port's own diagnostic for "the resolver found no file", printed for a name retail never
tries to resolve.

### 2.4 `None` is the engine's own word for "absent" — elsewhere

`AsciiString::isNone`, `RW 0x437720`:

```
push 0xBD3BF8        ; "None"
call 0x4372C0        ; the _stricmp body (case-insensitive)
neg / sbb / inc      ; -> bool
```

String `RW 0xBD3BF8` has exactly 27 references in the image, all `push` immediates, and they
sit at `0x4B62xx`, `0x4C21xx`, `0x4C31xx`, `0x4C7Bxx`, `0x4C82xx`, `0x61Dxxx`, `0x620xxx`,
`0x73Bxxx`, `0x80Exxx`, `0x8CFxxx`, `0x9C1Cxx`. **None of them is in the texture loader
(`0x53xxxx`) or in the mesh reader (`0x18xxxx`).** Their callers discard the tested value —
they are the draw module's bone / sub-object lookups and the INI parsers, matching DONOR
BFME2 `W3DModelDraw.cpp` (`findSingleBone`, `findSingleSubObj`, `parseShowHideSubObject`),
where `isNone()` means "this field is not set, skip it".

So the engine does have a `None` convention, but it lives on the game side (a name that means
"no such bone / sub-object / replacement"), not in the texture path. A texture named `None`
is not a special case there either: it is simply a name with no extension.

### 2.5 The FX path ends in the same loader

`TextureAsset::Get` `RW 0x532875` (DONOR BFME2 0x00132D89): it tests the name for NULL only
(`RW 0x53288A`, `RW 0x53291E`), and on a miss constructs the asset (`RW 0x5328CB`, ctor
`RW 0x532813`) and registers it (`Add_Prototype` `RW 0x5328EF`). There is no `"None"` branch.
DONOR BFME2 `FXShaderSetup::UpdateParameterList` 0x0015265E binds a type-1 parameter with
`TextureAsset::Get(name, 0, 0)`, and `MissingFactory::PreLoad` 0x00131CA5 rebuilds the impl
from `+0x30 / +0x34` only — no name test. So an FX parameter `None` takes exactly the same
route and produces exactly the same silent magenta.

## 3. The divergence and the fix

The port searched for a file for every name, including `None`, and reported the miss. The
mechanism fix is one flag on the shared resolver, mirroring `RW 0x530D59`:

- `TextureResolution::Probed` (`Libraries/WWVegas/WW3D2/textureloader.h`), true by default.
- `W3D_Texture_Name_Is_Probed(name)` — true when the name has a dot, false otherwise.
- `Resolve_W3D_Texture` returns an unprobed, unfound resolution for a name without a dot,
  building no candidate; `Resolve_Packed_W3D_Texture` returns it before the packed-JPEG
  attempt (retail's `RW 0x530D59` path tries nothing at all).
- The two consumers stop reporting when `Probed` is false and keep drawing retail's magenta:
  `W3DModelBuilder::loadTexture` (`GodotW3DModelBuilder.cpp`, the `report["errors"]` line) and
  `GodotW3DMaterial::Load_Texture` (`GodotW3DMaterial.cpp`, the guarded error branch). Both
  already fell back to the 1 × 1 `W3D_MISSING_TEXTURE_COLOR` surface, which is `RW 0x53193E`.
- Nothing else changes: a name **with** an extension that resolves to nothing is still a
  miss, still reported, still magenta.

No second path and no silent default was added: the "none" condition is the one retail has
(the absence of a dot), evaluated in the one loader every texture name passes through.

## 4. Tests

`engine/tests/test_port_430.cpp`, wired by `engine/cmake/Port430.cmake`
(`OPENBFME_PORT430_TESTS`), five cases:

| case | what it pins | fails before the fix |
|---|---|---|
| `a name without an extension is the name retail's loader never opens` | `W3D_Texture_Name_Is_Probed` over `none.tga` / `None` / `NONE` / `""` | yes (the function does not exist) |
| `` `None` is never looked up, and no file of that name is used `` | the file system is asked nothing for `None`, even with `none.dds` / `none.tga` present; a real name still walks `.dds` / `.tga` / `.jpg` | yes |
| `an ordinary missing texture still walks every candidate …` | neighbours: `CUCrow.tga` still tries the four paths and is a real miss | no (guard) |
| `a mesh whose TEXTURE_NAME is `None` parses with the name kept` | the mesh reader does not rewrite the data; the loader is what decides | no (guard) |
| `the FX parameter `None` of CAH_skull reaches the loader as a name it does not open` | FX material property read (type 1) and the same resolution; the sibling `AWSkull01.tga` keeps its own lookup | yes |

Synthetic W3D only (`engine/tests/W3dSynth.h`): the tests pass with no game and no mod data.

## 5. Stops

None added: the behaviour is ported, not left out. The one remaining piece of P-109,
`CAH_skull`'s FX parameter list, is #410's (`Local 54`) — the flag here covers it, and the FX
material's own parsing was already ported.

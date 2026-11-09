# PORT-74 — `W3DLaserDraw` in RotWK 2.01 (issue #206)

All addresses are static addresses of the retail `game.dat` used for the port (md5 `daed3668…`), read with
`python tools/mod/rw.py workspace/RotWK/game.dat …`. BFME2 1.06 (`Open-BFME-2`) is the readable donor and is
quoted only as DONOR.

## 1. The class chain (create → constructor → vtable → slots)

| what | address | how it was reached |
|---|---|---|
| module name string `"W3DLaserDraw"` | RW 0xBDBB8C | `rw.py str W3DLaserDraw`; referenced at RW 0x464BC3 (the module-factory registration) and RW 0x4C92FE / 0x4C931E (the cached name key / `getModuleNameKey`) |
| module-data factory (`friend_newModuleData`) | RW 0x464159 | `rw.py x 0x4C8989` → the immediate at RW 0x464191 |
| module-data object size | 0x74 | RW 0x464165 `push 0x74` before `operator new` RW 0x42F6E0 |
| module-data constructor | RW 0x4C917B | RW 0x46417A `call 0x4C917B` |
| module-data vtable | RW 0xBE18B8 | RW 0x4C9190 `mov dword ptr [esi], 0xBE18B8` |
| `buildFieldParse` | RW 0x4C8989 | RW 0x464190 `push 0x4C8989` → `INI::initFromINIMultiProc` |
| field table | RW 0xBE1798 | RW 0x4C898F `push 0xBE1798`, then `MultiIniFieldParse::add` RW 0x42B8D7 with base offset 0 |
| Envelope (OrnamentData) initialiser | RW 0x4C894B | RW 0x4C91A0 (module data) and RW 0x4C94C3 (draw module copy) |
| draw-module constructor | RW 0x4C9440‑ish, vtable writes at RW 0x4C9483 / 0x4C9489 | `mov dword ptr [ebx], 0xBE1938` |
| draw-module vtable | RW 0xBE1938 | same |
| `LaserDrawInterface` vtable (at +0x0C) | RW 0xBE1934 | RW 0x4C9489 |
| `doDrawModule` | RW 0x4C8A6C (slot +0x2C of RW 0xBE1938) | the only slot that reads the `"LaserUpdate"` name key RW 0xBE18A8 and the module data |
| `getModuleNameKey` | RW 0x4C931D / cached form RW 0x4C92D8 | slot +0x08 / +0x10 |
| `loadPostProcess` | RW 0x4C89E2 (sets `+0x24` = 1 after the base hook) | slot +0x04 |
| `xfer` | RW 0x4C899A | slot +0x0C |
| the LaserUpdate client update | found at runtime by name: RW 0x5487EC (name → key) with `"LaserUpdate"` RW 0xBE18A8, then RW 0x6747C2 (drawable → client update by key) | RW 0x4C8AA1..0x4C8ABC |

## 2. Module data (RW 0x4C917B, size 0x74) — 16 rows, RW 0xBE1798

| # | field | offset | parse proc | default (constructor RW 0x4C917B) |
|---|---|---|---|---|
| 0 | `NumBeams` | +0x20 | `parseUnsignedInt` RW 0x42ECB2 | 1 (RW 0x4C91B5) |
| 1 | `InnerBeamWidth` | +0x10 | `parseReal` RW 0x42ED00 | 0.0 (RW 0x4C91BB `xorps`) |
| 2 | `OuterBeamWidth` | +0x14 | `parseReal` | 1.0 (RW 0x4C91C0, `0xBD1908`) |
| 3 | `InnerColor` | +0x08 | `parseRGBColor` RW 0x42F13E | 0xFFFFFFFF (RW 0x4C917F `or dword [esi+8], -1`) |
| 4 | `OuterColor` | +0x0C | `parseRGBColor` | 0xFFFFFFFF (RW 0x4C9183) |
| 5 | `MaxIntensityLifetime` | +0x24 | RW 0x73A429 | 0 (RW 0x4C91C5) |
| 6 | `FadeLifetime` | +0x28 | RW 0x73A429 | 0 (RW 0x4C91C8) |
| 7 | `Texture` | +0x2C | `parseAsciiString` RW 0x42E59E (appended to a vector, ctor RW 0x478CBE) | empty list |
| 8 | `ScrollRate` | +0x18 | `parseReal` | 0.0 (RW 0x4C91CB) |
| 9 | `Tile` | +0x1C | `parseBool` RW 0x42E558 | false (RW 0x4C91D0) |
| 10 | `Segments` | +0x38 | `parseUnsignedInt` | 1 (RW 0x4C91B8) |
| 11 | `ArcHeight` | +0x3C | `parseReal` | 0.0 (RW 0x4C91D3) |
| 12 | `SegmentOverlapRatio` | +0x40 | `parseReal` | 0.0 |
| 13 | `TilingScalar` | +0x44 | `parseReal` | 1.0 |
| 14 | `FanWidth` | +0x48 | `parseReal` | 0.0 |
| 15 | `Envelope` | +0x4C..+0x73 | RW 0x73A74B (`parseOrnament`) | RW 0x4C894B: three opacities = 1.0 (`0xBD1908`), initial delay / attack / decay = 1, counter +0x0C = 0, state +0x24 = 0 |

Table order and offsets are the binary's own (RW 0xBE1798, 16 rows + the `{0,0}` terminator); the mod uses 13 of
them (no `MaxIntensityLifetime`, `FadeLifetime`, `ArcHeight`).

## 3. Draw module object (RW 0x4C9483, vtable RW 0xBE1938)

| member | offset | initial value |
|---|---|---|
| `m_textures` (vector of the loaded textures) | +0x10 | empty (RW 0x4C9490) |
| `m_textureAspectRatio` | +0x20 | 1.0 (RW 0x4C94A3) |
| `m_selfDirty` | +0x24 | true (RW 0x4C94A8) |
| `m_at28` (the texture index of this frame) | +0x28 | 0 (RW 0x4C94AC) |
| `m_at2C` (the current envelope opacity) | +0x2C | 1.0 (RW 0x4C94AF) |
| `m_at30` (the fan width, copied from module data +0x48) | +0x30 | `[moduleData + 0x48]` (RW 0x4C94B4/0x4C94C0) |
| `m_envelope` (a 0x28 copy of module data +0x4C) | +0x34 | `rep movsd` of 10 dwords from `moduleData + 0x4C` (RW 0x4C94CB..0x4C94D1) |

## 4. `doDrawModule` RW 0x4C8A6C — the branches read so far

1. RW 0x4C8A97: the `"LaserUpdate"` name key (RW 0x5487EC, cached at TheNameKeyGenerator RW 0xDD90E4), then
   RW 0x4C8ABC `call 0x6747C2` (drawable → client update by key). No update → return (RW 0x4C8AC8).
2. RW 0x4C8ACE: if neither the update's byte +0x24 nor the draw's `m_selfDirty` (+0x24) is set, return
   (RW 0x4C8AD8 → 0x4C90DD). Both are then cleared (RW 0x4C8AE2 / 0x4C8AE6).
3. The update's positions: start = `[update+0x0C], [+0x10], [+0x14]`, end = `[+0x18], [+0x1C], [+0x20]`
   (RW 0x4C8B10..0x4C8B29).
4. `Segments` `[data+0x38] <= 0` → nothing to draw (RW 0x4C8AEA). The arc branch is taken only when
   `ArcHeight [data+0x3C] > 0` **and** `Segments > 1` (RW 0x4C8AF5 `comiss` with RW 0xC1B594, RW 0x4C8B07).
5. The segment split (RW 0x4C8BAD..0x4C8BE4): the segment length is
   `(end - start) / Segments` (`fild [data+0x38]`, `fmul 0.5` … per axis), the overlap ratio `[data+0x40]` is
   added to the first segment's fraction and subtracted from the last one's (RW 0x4C8BE6 / 0x4C8BF4).
6. The arc branch adds `cos(dist / halfLength * PI/2) * ArcHeight` to the z of each segment end (RW 0x4C8CCF
   `call 0x403111` (cos), RW 0x4C8CD4/0x4C8CD9 `fmul [ebp-0x10]` (the segment length) / `fmul 0xBD89D0` (PI/2),
   RW 0x4C8CF1 `fmul [edi+0x3C]` (ArcHeight)).
7. The ground skim: TheTerrainLogic RW 0xDE4690, slot +0x18 = `getGroundHeight`, and the constant RW 0xBD889C
   (2.0) added to it, `MAX(z, 2.0 + groundHeight)` for both ends (RW 0x4C8D4E..0x4C8DAD).
8. Still to read: the per-beam loop (width from `InnerBeamWidth`/`OuterBeamWidth` and the update's width scale,
   the colour interpolation, `Set_Texture_Tile_Factor`, `Set_Color`, `Set_Texture`, and the envelope update) —
   RW 0x4C8DB3 onwards.

## 5. The beam loop (RW 0x4C8DB3 .. 0x4C9102), the draw module object and what is not in the image

Per segment (RW 0x4C8AF5), after the arc / no-arc point pair of section 4:

1. RW 0x4C8E7F / 0x4C8E9F: `data+0x08` (InnerColor) and `data+0x0C` (OuterColor), one packed `GameMakeColor` word each, are unpacked
   by RW 0x6D3AD5 into red / green / blue / alpha floats.
2. RW 0x4C8EB1: `i` runs from `NumBeams - 1` down to 0; `index = segment * NumBeams + i` (RW 0x4C8EBF).
3. Width (RW 0x4C8EC7 / 0x4C8EE0): one beam takes `InnerBeamWidth * widthScale` (the update's `+0x3C`); more beams take
   `InnerBeamWidth + i / (NumBeams - 1) * (OuterBeamWidth - InnerBeamWidth)`, times the width scale.
4. Tiling (RW 0x4C8F14): only when `m_textures[0]` exists and `Tile` is set; `tileFactor = TilingScalar / width * m_textureAspectRatio * length`
   (RW 0x4C8F73 .. 0x4C8F84), handed to the line at RW 0x4C8F95 (RW 0x55DCE0).
5. RW 0x4C8F9A: `Set_Width(width, m_at30)` (RW 0x55DBE0) — `m_at30` is the draw module's copy of the module data's `FanWidth`
   (RW 0x4C94B4 / 0x4C94C0). RW 0x4C8FBB: `Set_Points(2, laserPoints)` (RW 0x55E9F0).
6. Colour (RW 0x4C8FC8 / 0x4C9002): one beam takes `Inner * innerAlpha`; more take
   `Inner + i / (NumBeams - 1) * (Outer - Inner) * innerAlpha` per component (the inner alpha scales the whole mix).
7. RW 0x4C9055: the envelope update RW 0x740659 runs **once per beam**, with `&m_at2C` as both the input and the output; the colour is
   then multiplied by `m_at2C` (RW 0x4C9062 .. 0x4C9093) and set at RW 0x4C90A8 (RW 0x55DC20).
8. RW 0x4C90AD: `Set_Texture(&m_textures[m_at28])` (RW 0x55DB80) — the texture of this whole draw.
9. RW 0x4C90DD .. 0x4C90F0: after the segments, `m_at28` advances by one and wraps at the texture count (the `(finish - start) / 4` of the
   vector at `+0x14`). `Segments == 0` skips the drawing but still advances it (RW 0x4C8AEF -> 0x4C90DD).

The envelope itself is an OrnamentData at module data `+0x4C` (0x28 bytes): `InitialOpacity` +0x00, `PeakOpacity` +0x04,
`SustainOpacity` +0x08, the counter +0x0C, `InitialDelay` +0x10, `AttackTime` +0x14, `DecayTime` +0x18, `SustainTime` +0x1C,
`ReleaseTime` +0x20, the state +0x24 (RW 0x73A74B; the times are milliseconds stored as `floor(value * 0.005)` with RW 0xD9F610,
i.e. 5 frames per second of INI time). RW 0x740659 is the five-state machine; RW 0x4C894B is the initialiser the module data constructor
calls. `MaxIntensityLifetime` / `FadeLifetime` (RW 0x73A429, same millisecond scale) are parsed and — like the rest of the class — read
nowhere in doDrawModule.

The draw module object (constructor RW 0x4C9483, vtable RW 0xBE1938): `+0x10` the line array, `+0x14` the texture vector, `+0x20` the
aspect ratio (1.0), `+0x24` the "self dirty" byte (1, set again by `loadPostProcess` RW 0x4C89E2), `+0x28` the texture index,
`+0x2C` the envelope opacity (1.0), `+0x30` the fan width (module data `+0x48`), `+0x34` the envelope copy.

NOT IN THE PORT, therefore not in this issue: the SegmentedLineClass render objects (created at RW 0x4C94D3 .. 0x4C958E with the
additive shader, `Set_UV_Offset_Rate(0, ScrollRate)`, `TILED_TEXTURE_MAP` and `Add_Render_Object` on `W3DDisplay::m_3DScene`
RW 0xDE1B34) and the `LaserUpdate` client update that supplies the endpoints. See `docs/STOPS.md` S-7197.

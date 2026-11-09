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

## 5. Open (to read before the port)

- RW 0x4C8DB3 .. 0x4C90F0: the beam loop proper (colours, tiling, texture advance) and the envelope call.
- RW 0x4C8A6C's tail: the textures are loaded in `loadPostProcess`/`doDrawModule` when `m_textures` is empty
  (the vector at +0x10) and the aspect ratio is taken from the first texture.
- The LaserUpdate object itself (start / end / width scale) is a separate module (audit `missing-module`), out of
  this issue's port: the port takes start / end / width scale as the arguments of the geometry, as the issue says.

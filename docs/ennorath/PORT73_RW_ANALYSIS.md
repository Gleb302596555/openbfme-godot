# PORT-73 (issue #205): W3DTreeDraw's shrub push aside — RW analysis

RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`. All addresses are virtual (image base 0x400000), read with
`tools/mod/rw.py dis / xref / str` and raw byte scans. BFME2 1.06 (`/tmp/open-bfme-2`, byte-matched decomp) is the donor and is
marked DONOR; every step below was confirmed in RotWK itself.

## The five fields (field table RW 0xBE2E80, 24 rows; row = name, parser, userData, offset)

| row | name | parser | offset |
| --- | --- | --- | --- |
| RW 0xBE2EA0 | `MoveOutwardTime` | RW 0x73A429 (duration in logic frames) | +0x10 |
| RW 0xBE2EB0 | `MoveInwardTime` | RW 0x73A429 | +0x14 |
| RW 0xBE2EC0 | `MoveOutwardDistanceFactor` | RW 0x42ED00 (real) | +0x18 |
| RW 0xBE2ED0 | `DarkeningFactor` | RW 0x42ED00 | +0x1C |
| RW 0xBE2F20 | `DoTopple` | RW 0x42E558 (bool) | +0x3D |

## The class chain (create -> constructor -> vtable -> slot), the shrub buffer

* `TheTerrainVisual` = RW 0xDE4AC8, vtable RW 0xBDDFC8. Slot 0x6C = RW 0x491977 (`mov ecx,[ecx+0x14]; jmp`) -> BaseHeightMap
  RW 0x467819 -> `jmp 0x4E70F5` with the object, i.e. `BaseHeightMap + 0x3854` is the shrub buffer (`+ 0x3850` is the tree buffer,
  its per-frame update RW 0x4684FB -> RW 0x4ED508).
* W3DShrubBuffer's own identity strings: "W3DShrubBuffer" RW 0xBE4CA8, referenced by the assert helper RW 0x4E95D0; the constructor
  RW 0x4E95DE and the fields of the donor's layout (records 0xA0 from +0x1958, count +0x4FB58, types 0x5C from +0x4FB90, partition
  shorts +0x5C0, world bounds +0x1948..+0x1954, push-changed byte +0x4FB5D, frames per logic frame +0x5BC).
* BaseHeightMap's forwarders into the buffer, matched in order: addShrub RW 0x467A2C -> RW 0x4E8B8F (`ret 0x2C` = 11 arguments, the
  donor's addShrub signature), unitMoved RW 0x467819 -> RW 0x4E70F5, draw RW 0x4E8B8F's caller chain RW 0x4E9511 -> RW 0x4E8B8F.

## The logic -> client call

RW 0x6858BE (`TerrainTrees::unitMoved`, from Object's cell change RW 0x69264D for INFANTRY / CAVALRY / MONSTER / MACHINE):

```
0058D0  call dword ptr [eax+0x6c]        ; TheTerrainVisual slot 0x6C with the object — FIRST, before every test below
0058D7  test byte ptr [eax+0x108], 4     ; IMMOBILE -> out
0058E2  call 0x695070                    ; crusher level; above 1, or MACHINE (+0x109 bit 3) -> the tree topple RW 0x6854FD
```

So the push aside runs for every object whose cell changed, and the client repeats the IMMOBILE test itself.

## W3DShrubBuffer::unitMoved RW 0x4E70F5 (283 bytes, `ret 4`)

* RW 0x4E70FB `mov eax,[0xDE4364]`; RW 0x4E7100 `cmp byte [eax+0x1C],0` (GlobalData `ShowProps`, gamedata.ini says Yes) and
  RW 0x4E710D `cmp byte [eax+0x1D],0` (GlobalData `PushAsideShrubs`, row RW 0xBFF610, offset +0x1D, parser RW 0x42E558) -> out.
* RW 0x4E711D `test byte [template+0x108],4` -> IMMOBILE out; RW 0x4E713C `addss` of RW 0xBE4AD4 = 7.0 onto the major radius
  (+0xB8); bounds +0x1948..+0x1954; `49.9f` RW 0xBE4AD0 through floor (0xBD0580) and ceil (0xBD0588) give the cell range, table +0x5C0.
* the walk RW 0x4E7323..0x4E73D1: type (record +0x40, i.e. 0x1998) below 0 goes, index outside the count goes, 3D squared distance
  (`(dz*dz + dy*dy) + dx*dx`) against r*r, then the type's data `[type*0x5C + this + 0x4FB90]` + 0x10 must be above 0 (RW 0x4E73AE),
  then RW 0x4E73CC `call 0x4E6C74` with (drawableID +0x58, the unit's position, `getUnitDirectionVector2D()` RW 0x70B9E0, the unit's
  id +0x74).

## The push RW 0x4E6C74 (283 bytes, `ret 0x10`)

* RW 0x4E6C80 GlobalData +0x1C; count +0x4FB58; per record (0xA0): drawableID +0x58, push aside +0x5C, rate +0x60, sin +0x64,
  cos +0x68, source +0x6C, last frame +0x70, type +0x40.
* the same source within three logic frames is skipped (RW 0x4E6CC7 `sub; cmp eax,3; jb`; the frame is TheGameLogic + 0x40 through
  RW 0xDE412C); a record already bent is skipped (RW 0x4E6CD7, the `ucomiss`/`test ah,0x44`/`jp` idiom for "not zero").
* the direction: RW 0x4E6CF4..0x4E6D0E computes `dir.x * (tree.y - unit.y) - dir.y * (tree.x - unit.x)`; above 0 gives cos = -dir.y,
  sin = dir.x (RW 0x4E6D1A / 0x4E6D26), else cos = dir.y, sin = -dir.x (RW 0x4E6D2B / 0x4E6D31).
* the rate RW 0x4E6D41..0x4E6D6D: `1.0f` (RW 0xBD1908) divided by `(float)MoveOutwardTime * [this+0x5BC]` (the frames per logic frame,
  30 / 5 = 6, written at RW 0x4E5F94 from RW 0xDE4324 + 0x38).

## addShrub RW 0x4E8B8F: which records can be pushed

```
004E8C66  cmp dword ptr [ebx+0x10], 0    ; MoveOutwardTime above 0 -> the record joins the partition
004E8C6A  ja 0x4E8C88
004E8C6C  cmp byte ptr [ebx+0x3D], 0     ; DoTopple -> also joins it
004E8C70  jne 0x4E8C88
004E8C7E  or dword ptr [eax+esi+0x19CC], -1   ; else the record's partition link is -1 (never walked)
```

DONOR BFME2 0xE91CE has the same two tests (`framesToMoveOutward > 0 || doTopple`). DoTopple therefore only decides whether the walk
can find the record; the walk itself still needs `MoveOutwardTime` above 0 (RW 0x4E73AE), so a `DoTopple = Yes` tree with
`MoveOutwardTime = 0` (the mod's trees) is never bent.

## The per-frame step RW 0x4E8519 (in the draw RW 0x4E8490 .. 0x4E85A1)

```
004E8519  movss xmm0,[edi+0x19B8]   ; +0x60 the rate; zero -> skip (the ucomiss/jnp idiom)
004E852D  addss xmm0,[edi+0x19B4]   ; +0x5C push aside += rate
004E8535  comiss xmm0,[0xBD1908]    ; >= 1.0f -> the rate becomes -1.0 / (MoveInwardTime * 6)  (RW 0x4E8550..0x4E856F,
                                    ;   fild [type data +0x14], fmul [this+0x5BC], fdivr qword [0xBDFC10] = double 1.0)
004E8577  comiss xmm1,xmm0 / jb     ; else <= 0 -> rate 0 and push aside 0 (RW 0x4E857C / 0x4E8584)
```

The loop's other filters are the client's (type, visible +0x44, topple state +0x84, shroud +0x94, pause); they never change the
arithmetic. The step happens once per drawn frame, the port steps it at its 1/30 s update (lane SPELL-3's S-930).

## The bend RW 0x4E7B25 / RW 0x4E7B4D (also RW 0x4E7F73 / RW 0x4E7F9B in the partial rebuild RW 0x4E7D26)

```
004E7AEC  movss xmm4,[edi+0x19B4]   ; push aside
004E7AF4  comiss xmm4, 0 / jbe      ; only a bent record
004E7B1D  movss xmm4,[edi+0x19C0]   ; cos  (+0x68)
004E7B25  mulss xmm4,[edi+0x19B4]   ; * push aside
004E7B2D  mulss xmm4,[eax+0x18]     ; * MoveOutwardDistanceFactor (the type's data)
004E7B32  mulss xmm4,[edx]          ; * the untransformed vertex z
004E7B36  addss xmm4,xmm0           ; x
004E7B3D  movss xmm4,[edi+0x19BC]   ; sin (+0x64) ... the same product into y
```

So the model leans away from the walker and stands back up along the same line; the drawn vertex offset is
`vertex z * push aside * (cos, sin) * MoveOutwardDistanceFactor`.

## DarkeningFactor (+0x1C): no reader in RotWK 2.01

The functions that read a shrub type's data read only +0x10 (RW 0x4E73AE, RW 0x4E6D4E), +0x14 (RW 0x4E8550) and +0x18
(RW 0x4E7B2D, RW 0x4E7B4D, RW 0x4E7F7B, RW 0x4E7F9B). A raw byte scan of `.text` for every float access with displacement
0x1C (`F3 0F 10 / 59 / 5C / 58` with modrm 0x40..0x47, `D9 /r` and `D8 /r` forms, 556 hits) has no hit in the shrub or tree
buffers' range 0x4A0000..0x500000; the only hits there are Matrix3D reads (RW 0x4E86DC..0x4E86F5, RW 0x4EC539..0x4EC552, the
render object's transform). DONOR BFME2 agrees: `m_darkening` is stored by the constructor and read by nothing. The port keeps
the field parsed and applies nothing.

## The port

* `GameClient/TreeTopple.{h,cpp}` (the lane's existing single tree path): the record gains the bend (push aside, rate, sin, cos,
  source, frame), `unitMoved` (RW 0x4E70F5), `pushOne` (RW 0x4E6C74), `advancePushAside` (RW 0x4E8519) and the shear in
  `placement` (RW 0x4E7B25 / RW 0x4E7B4D).
* `GameLogic/Map/TerrainTrees.{h,cpp}`: slot 0x6C is now sent (`Event::PUSH_ASIDE`, RW 0x6858D0) before the crusher tests.
* `GodotDevice/GodotGameWorld.cpp`: the event reaches `TreeTopple::unitMoved`; the draw passes the instance scale so the shear is
  the drawn offset of RW 0x4E7B2D.

## Not ported (docs/STOPS.md)

* the two GlobalData gates of RW 0x4E7100 / RW 0x4E7111 (the port has no GameData block of TheWritableGlobalData; gamedata.ini
  sets `ShowProps = Yes` and never sets `PushAsideShrubs`, whose constructor default is not proven in this session);
* the partition itself (the port scans its records linearly), the visible / shroud / pause filters of the draw loop and the vertex
  buffers (the port shears the instance's basis);
* `DarkeningFactor` (no reader, above).

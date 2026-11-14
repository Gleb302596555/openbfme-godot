# PORT-159 / P-83: the mumak and the dwarven battle wagon have no shadow on Ultra (issue #377)

Owner symptom (2026-11-11): with the Ultra setting the mumak (`HaradMumakil` and its
variants) and the dwarven battle wagon (`DwarvenBattleWagon` and its variants) cast no
shadow. Both templates say, in the mod's data (vanilla lines, unchanged by Ennorath):

```
Shadow = SHADOW_VOLUME_NON_SELF_2 ; volumetric shadow that doesn't cast onto all objects using SHADOW_VOLUME_NON_SELF_1
ShadowSizeX = 36 ; clamp the angle so shadow isn't as long.
```

`SHADOW_VOLUME_NON_SELF_2` is entry 8 of the shadow-type name list, i.e. bit 0x100 (the
list read out of `RW 0xD99EB0`: 0 SHADOW_DECAL, 1 SHADOW_VOLUME, 2 SHADOW_VOLUME_NEW,
3 SHADOW_DYNAMIC_PROJECTION, 4 SHADOW_DIRECTIONAL_PROJECTION, 5 SHADOW_ALPHA_DECAL,
6 SHADOW_ADDITIVE_DECAL, 7/8/9 SHADOW_VOLUME_NON_SELF_1/2/3 (0x80/0x100/0x200),
10/11 ALPHA/ADDITIVE_DECAL_DYNAMIC, 12 SHADOW_MERGE_DECAL, 13 SHADOW_DECAL_TERRAIN_CLAIM,
14 SHADOW_VOLUME_OR_DECAL, 15 SHADOW_VOLUME_NEW_OR_DECAL, 16 SHADOW_SUBTRACT_DECAL_DYNAMIC).

## What RotWK does with the type word

The type word (template + 0x5F2 / condition state) reaches the shadow managers through
`W3DShadowManager::addShadow` (RW 0x49A2EB, the three-argument switch; the structure is
`DONOR BFME2 0x9A8D3 (Shadow/W3DShadowManagerAddShadow.cpp)`, and RW re-reads the same):

* RW 0x49A302: `esi = shadowInfo->m_type` (ShadowTypeInfo + 8), default 2 when there is no info.
* RW 0x49A317..0x49A355: types 0x4000 / 0x8000 resolved first: `GlobalData + 0x60`
  (`UseShadowVolumes`, written by the static LOD apply RW 0x601D34 / 0x601D59) set ->
  0x4000 becomes 2, 0x8000 becomes 4; clear -> 1.
* RW 0x49A358..0x49A380: types **2, 0x80, 0x100, 0x200** go to
  `W3DVolumetricShadowManager::addShadow` (RW 0x4F24A8, the stencil-volume manager).
* RW 0x49A39A: type 4 goes to `W3DVolumetricShadowManagerV2` (RW 0x507CAA).
* RW 0x49A3B2..0x49A3D8: types 1, 0x20, 0x40, 0x400, 0x800, 0x1000 and 0x10000 go to
  `W3DProjectedShadowManager::addShadow` (RW 0x50BF6A, the decal manager the port calls
  `ObjectShadow::resolveDecalShadow` / `buildDrawableDecalShadow`).
* RW 0x49A3DA: any other type (0, 0x8, 0x10, 0x2000 — and the two "or" composites, which
  were rewritten above) returns NULL: no shadow object at all.

So `SHADOW_VOLUME_NON_SELF_1/2/3` (0x80 / 0x100 / 0x200) are **volume shadows**: they are
handed to the volumetric manager exactly like `SHADOW_VOLUME` (0x2). The three "non self"
variants differ only in the stencil group they write (RW's occluded-object pass groups
them, `RTS3DScene::flushOccludedObjectsIntoStencil` — `DONOR BFME2 0x93841
(W3DDevice/GameClient/RTS3DSceneFlushOccludedObjectsIntoStencil.cpp:290, :354)` masks the
template's shadow word with 0x380 and shifts it to a stencil bit), never in *whether* a
shadow is made.

The volumetric manager's own gate (RW 0x4F24A8): `WW3D::Has_Stencil` (RW 0x51C760),
a non-null render object, `GlobalData + 0x60` (`UseShadowVolumes`), a name on the render
object, a geometry from the shadow-geometry cache, then `W3DVolumetricShadow` (allocation
size 0x4904, RW 0x4F2520) with the render object at + 0x70, the length scale from
`tan(ShadowSunAngle * 0.017453)` (RW 0x4F2577), the extrusion padding unless the drawable
is KINDOF_IMMOBILE (RW 0x4F25B4 tests byte + 0x108 bit 2) and the type word stored at
+ 0x34 (RW 0x4F25A7) — the stencil-group word the occluded pass reads back.

The static LOD apply (RW 0x601C62) writes the three shadow flags into GlobalData:
`+0x60` = UseShadowVolumes (RW 0x601D34 from the level record + 0xC), `+0x61` =
UseShadowDecals (RW 0x601D40 from + 0xD), `+0x62` = UseShadowMapping (RW 0x601D4C from
+ 0xE); when `+0x62` is set it clears `+0x60` and `+0x61` (RW 0x601D59 / 0x601D61).

## Where the port departs

The port's shadow-map caster test, `ObjectShadow::castsMap` (`GameClient/ObjectShadow.h`),
knows only `MapMask = 0x2 | 0x4 | 0x4000 | 0x8000` (SHADOW_VOLUME, SHADOW_VOLUME_NEW and the
two "or" composites) plus, with shadow mapping on, the decal mask. A template whose type
word is 0x80 / 0x100 / 0x200 — the three volume-shadow variants every large creature of the
game uses — therefore fails the test, `cast_shadow` is set false on its instances, and on
Ultra (where the decal path is off as well: `GameLODManager::applyStaticLODLevel` clears
UseShadowVolumes and UseShadowDecals when UseShadowMapping is set, RW 0x601D59 / 0x601D61)
the object has no shadow at all. That is the first differing step.

Other objects of the mod with the same word (so the same bug, fixed by the same change):
`WildBeuteWagen` (beute wagen), `MordorTroll`/`CaveTroll`/`MountainGiant`/`FellBeast`,
`Gwahir`/`ManweEagles`/`ImladrisGwaihir`, the Ents (`EntsInfantry`, `Treebeard`,
`LothlorienFlinkBaum`, `LothlorienBaumBart`), `RohanRadagast`, `WildDrummerOrc`, the
nature rocks (`NatureRocks`, type SHADOW_VOLUME_NON_SELF_1), the cinematic objects and the
mod's own additions in `modobjects.ini` / `cinematicobjects.ini`.

## The change

`ObjectShadow::MapMask` gains the three non-self volume bits (0x80 | 0x100 | 0x200), i.e.
the port's caster test now agrees with RW 0x49A361..0x49A37E (types 0x80/0x100/0x200 ->
volumetric manager), exactly as it already agreed for 0x2. One mask, one place: every
caster decision of the client (map build, per-frame refresh, decal builder) reads it.

Rejected alternative (INFERENCE, would contradict RW): treating 0x100 as a decal. RW
0x49A3B2 does not list 0x100 among the projected-manager types, and
`docs/OBJECT_SHADOWS.md` records that retail draws no decal blob for these units; the
stencil volume's look is what the Godot shadow map stands in for (S-3304).

## Not covered here

The stencil grouping itself (which receiver excludes which NON_SELF_1/2/3 group) is not
ported — S-3304 already records that the port draws one Godot directional shadow map per
caster instead of retail's stencil volumes, and `RTS3DSceneOcclusion.h` says the same of
the occluded pass. The three bits select the same map; the group stays presentation.

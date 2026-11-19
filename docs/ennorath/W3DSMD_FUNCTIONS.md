# W3DScriptedModelDraw / W3DHordeModelDraw: the 79 uncited own functions (Local 89, #715)

The function map (`RW_FUNCTION_MAP.tsv`, 2026-11-19) listed 73 `W3DScriptedModelDraw` and 6 `W3DHordeModelDraw` own functions at depth 0
with `status = none`. They are the slots of three RotWK vtables:

- **the draw module vtable**: RW 0xBDFFE8 (W3DScriptedModelDraw) and RW 0xBDC810 (W3DHordeModelDraw). 68 slots, and the slot index
  equals the donor's numbering (`DrawModule.h`, `W3DHordeModelDrawCtor.cpp`, BFME2 1.06);
- **the ObjectDrawInterface at draw + 0xC**: RW 0xBE3B78, shared by every model draw (W3DQuadrupedDraw's copy is RW 0xBE1AD0). The
  functions get `this` = draw + 0xC. Slots 0-2 follow ZH's `ObjectDrawInterface`, and the rest are RotWK's own.

Found with `tools/mod/rw.py dis` and a data scan for the vtable entries on `game.dat` (md5 daed3668…).

Status: **a** = ported, the citation is added at the port's code. **b** = ported differently: the port does it elsewhere, and the
citation goes there. **c** = missing: ported in this task, or left in the stop named.

## Draw module vtable (RW 0xBDFFE8; the horde draw's RW 0xBDC810 shares every slot not listed under it)

| slot | RW | what it is | status / port |
|---|---|---|---|
| 0 | 0x4C7D11 | scalar deleting destructor (body RW 0x4C70C3) | b: `~W3DScriptedModelDraw` (C++ owns the members) |
| 1 | 0x4B32FD | loadPostProcess: the frame stamp (+0xB8) from the client frame (RW 0xDC7580), and the script selectable (+0x214 >= 0) re-applied through the interface's setSelectable (slot 24) | c: the port does not save client draws (S-7615) |
| 2 | 0x4C0911 | the class name, "W3DScriptedModelDraw" (RW 0xBDBBB0) | b: the module registry's name (`W3DModelDraw.cpp` module table) |
| 4 | 0x4C08CC | getModuleNameKey (NameKey of RW 0xBDBBB0, cached) | b: the module registry |
| 6 | 0x4C091D | onDrawableBoundToObject: the first model state (RW 0x4B4379) and animation state (RW 0x4B4443), setModelState (slot 64), select (RW 0x4BF15E), then the object's attachments | b: the constructor and the first `setModelConditionFlags` (`W3DScriptedModelDraw.cpp`) |
| 7 | 0x4B7378 | preloadAssets: with a render object (+0x50), RW 0x4B583C | b: Godot loads models and textures on first use |
| 12 | 0x4B26F9 | setShadowsEnabled: +0x4A = on, and the shadow (+0x58) is visible when on and the draw is not hidden (+0x4B) | b: `Drawable::setShadowsEnabled` (RW 0x672D28); the renderer reads it per module |
| 13 | 0x4B26E4 | releaseShadows: the shadow (+0x58) released and cleared | b: `ObjectShadow` (the shadow goes with the instance) |
| 15 | 0x4B8803 | donor rva000B90A3: the render object's attached-bone transforms after a model change (RW 0x4B686D height, RW 0x6765B9 drawable position) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 17 | 0x4B26E0 | get the byte +0x4C | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 18 | 0x7B2EFE | set the byte +0x4D | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 19 | 0x4B95AD | **setTerrainDecal by texture name**: the old decal (+0x5C) released; a non-empty name makes a decal shadow (TheW3DShadowManager RW 0xDD1D14 vslot 0xC) of that texture, sized by the template's ShadowSizeX / Y / OffsetX / Y (+0x4F8 .. +0x504), type 0x20; its +5 from +0x49, visible when shadows are on and the draw is not hidden | **c: ported in this task** (`W3DTerrainDecal.h`) |
| 20 | 0x4B94BC | **setTerrainDecal(type)**: the same with the texture of the type (RW 0xD99F40: 0 TERRAIN_DECAL_DEMORALIZED_OBSOLETE, 1 EXHorde, 2 EXHorde_UP, 3 EXHordeB, 4 EXHordeB_UP, 5 EXJunkCrate; 6 is the custom texture of slot 19, its table entry a filler); type >= 7 (TERRAIN_DECAL_NONE) only removes | **c: ported in this task** |
| 21 | 0x4B2A1B | **setTerrainDecalSize(x, y)**: the decal's size (+0x58 / +0x5C of the decal) | **c: ported in this task** |
| 22 | 0x4B2A3B | **setTerrainDecalOpacity(f)**: RadiusDecal opacity int(f * 255) (RW 0x73228B) | **c: ported in this task** |
| 23 | 0x4B48E0 | radius decal from a RadiusDecalTemplate (+0x1D0), coloured by the object's indicator colour (RW 0x6916CD) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 24-34 | 0x4B2A5D, 0x4B2A68, 0x4B2A89, 0x4B2B36, 0x4B2B41, 0x4B2B92, 0x4B2BA7, 0x4B2BBF, 0x4B2BDC, 0x4B2C0D | the two radius decals' setters (+0x1D0 / +0x1D4 and +0x218 / +0x21C / +0x228: clear, position, colour, opacity, template, update) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 36 | 0x4C6D93 | donor rva000C7699: walks the draw's attached list (RW 0x4BF529 per entry) and refreshes it | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 37 | 0x4B30C0 | isVisible: the render object exists and is really visible (vslot 0x180) | b: Godot visibility |
| 38 | 0x4B7748 | reactToTransformChange: the render object's transform from the drawable (RW 0x676711), the height adjustment (RW 0x4B686D), the attached and particle bones | b: the device sets the instance transform each frame |
| 49 | 0x98255B | getRenderObject (+0x50) | b: the device's instance |
| 50 | 0x4B5A01 | a height: with the object's KINDOF bit (+0x108 & 0x40) its bounding radius * k (RW 0xAD1920), else 2 * (z - terrain height + radius) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 51 | 0x4B6789 | GetPortraitImage: the model state's PortraitImageName (+0x60) as a mapped image, cached (+0x2D9 / +0x2DC) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 52 | 0x4B9460 | getButtonImage: the model state's ButtonImageName (+0x5C), cached (+0x2E0 / +0x2E4) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 53-55 | 0x4B5809, 0x4B5814, 0x4B582B | the ramp mesh name override (+0x260, per index +0x264): set, set one, "has one" | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 56 | 0x4BACA2 | builds a model or animation name with an index suffix (two AsciiString::format, RW 0x437A90) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 57 | 0x4B3350 | track 0 to its last frame: prevFrame = frame, frame = frames - 1 | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 58 / 59 | 0x4783D1 / 0x478374 | set the bytes +0x28D / +0x28E (the horde draw's flags) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 60 | 0x47836C | forwards to the module data's vslot 0x78 | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |

W3DHordeModelDraw's own slots: 0 RW 0x478A50 (destructor, b), 1 RW 0x4CA6F0 (loadPostProcess = slot 1 of the base, c S-7615),
2 RW 0x478423 (name "W3DHordeModelDraw", b), 3 RW 0x478461 (**xfer**: the base xfer RW 0x4C52FE, then the bool at +0x2EC unless loading;
c, S-7615), 4 RW 0x4783DE (getModuleNameKey, b), 6 RW 0x47A043 (onDrawableBoundToObject: the horde's LOD row, then the base RW 0x4C091D, b).

## ObjectDrawInterface (RW 0xBE3B78, `this` = draw + 0xC)

| slot | RW | what it is | status / port |
|---|---|---|---|
| 0 | 0x4B2DA8 | clientOnly_getRenderObjInfo: position, bounding sphere radius, transform | b: `Drawable` bounds in the device |
| 1 | 0x4B2E3F | clientOnly_getRenderObjBoundBox (RW 0x4B22FB, 0x4B2374) | b: as slot 0 |
| 2 | 0x4B7664 | clientOnly_getRenderObjBoneTransform: the bone by name (vslot 0xC8), or identity when the model lacks it | b: `W3DModelDrawBones` |
| 4 | 0x4B4B23 | getCurrentBonePositions (a 16 x 0x30 static table at RW 0xDCFD50) | b: `W3DModelDrawBones` (S-097) |
| 10 | 0x4C0B38 | the name of track i's animation (i < 3, after RW 0x4BFAF4) | b: `W3DDrawTrack::clipName` |
| 11 | 0x4C0B88 | track i's animation handle (vslot 0xC) | b: `AnimationSoundClientBehavior` (the track's clip) |
| 12 | 0x4C0BC0 | track i's frame and frame count | b: `W3DDrawTrack::frame` |
| 13 | 0x4B2A03 | the render object's +0x9C (via the draw's getRenderObject) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 14 | 0x4BF0B5 | the frame count and rate of the animation a condition set would play (LOD-aware, RW 0x4BEF4A) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 16 / 19 | 0x4BC171 / 0x4BB86C | the draw's +0xA8 table: look up (RW 0x472958) / insert (RW 0x6B4E57, RW 0x47E273) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 17 / 18 | 0x4B86BA / 0x4B30DC | +0xA4 = value, +0xA0 = 0, the name at +0xB4 / +0xA0 = 1 | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 22 / 23 | 0x4B4FFD / 0x4B4FBC | per weapon slot of the current model state (flag +0xF4 bit 3, entries of 12 bytes at +0xAC): the frames / 60 | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 25 / 26 | 0x4B300D / 0x4B3049 | the animation loop duration in milliseconds (RW 0x4B2808), and its overload | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 28 | 0x4B304E | for an HLOD render object (class id 0x19) the sub-object visibility pass | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 32 | 0x478381 | a 16-byte record copied into draw + 0x28 with the bit masks 7 / 0x3FFFFFF8 / 0x40000000 | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 35 | 0x4C436D | a named state lookup and reapply (RW 0x535C5A, RW 0x6D32E4) | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 36-43 | 0x4BA427, 0x4BA4FF, 0x4BA5BB, 0x4BA693, 0x4B7A59, 0x4B7B07, 0x4BB76A, 0x4BB7EB | the wall and ramp mesh queries: the sub-object by name (the override +0x260 or the module data's RampMesh1 / RampMesh2 / WallBoundsMesh / RaisedWallMesh, +0x10C .. +0x118), its mesh, then RW 0x4B7BA0 / 0x4B8149 | b: `DrawableWallMeshes` |
| 44 | 0x4B7A22 | the template's entry i (+0x11C list, 8 bytes each) to RW 0x88FCBB | open, S-7616: the behaviour is read; the port's counterpart is not proven yet |
| 45 / 46 | 0x4BF4D9 / 0x4BF501 | select the animation state named by the argument, from the transition table (RW 0x4B44F3) / the state table (RW 0x4B4514), at once (RW 0x4BF15E) | b: `select` |
| 48 | 0x4C0E53 | the current animation's name and its frame window | b: `AnimationSoundClientBehavior::frameWindows` |

## What this task ports, and what it leaves

- **Ported:** the terrain decal (slots 19-22 and the visibility rule of 12 / 13), and `Drawable::setTerrainDecal` (RW 0x672B96: stores
  the type at drawable + 0xA8 and passes it to the first draw module), its by-name variant (RW 0x672BBB: a non-empty texture name
  gives type 6, the custom texture, an empty one 7, none; then the module's slot 19). Callers in RotWK: HordeContain RW 0x870D4E (the horde's EXHorde / EXHorde_UP decal on its
  members, and their size by RW 0x67093E; Port 292, #684), the crate RW 0x8889B3 (type 5), Drawable RW 0x675CD3 (type 7 = none).
- **S-7615:** the client draw's save (loadPostProcess, the horde draw's xfer of +0x2EC): the port has no client snapshot.
- **S-7616:** the rows marked open: RotWK's behaviour is read above, but the port's counterpart is not proven, so they keep `none` in the map.
- The `b` rows are cited in `W3DScriptedModelDraw.h` (the block "Local 89"), where the port's counterpart is named.

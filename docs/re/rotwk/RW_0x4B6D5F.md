# RW 0x4B6D5F — a ParticleSysBone system follows its animated bone (issue #154, lane PORT-50)

RotWK 2.01 (`workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, the S-001
provenance caveat applies). Port: `engine/src/GameClient/ParticleBoneFollow.{h,cpp}` and
`LiveFX::createSystems` / `LiveFX::followAnimatedBones` (`engine/src/GameClient/LiveFX.cpp`), tests
`engine/tests/test_port_50.cpp`. No data of the mod is reproduced here: the addresses and the
instructions only.

Every instruction quoted below was decoded from the image in the same session (`tools/mod/rw.py
dis`, capstone 5, image base `0x400000`).

## The chain that reaches the walk

| What | Address |
|---|---|
| the per-draw pipeline (`W3DModelDraw::doDrawModule`) | RW 0x4C72BE (vslot 11 of the draw tables RW 0xBDC810 / 0xBDFFE8 / 0xBE1B98 / 0xBE1F20 / 0xBE3C40) |
| the flag of the module data (`ParticlesAttachedToAnimatedBones`) | data `+ 0x108`, INI row of the draw table RW 0xBE1320, parse via `INI::parseBool` |
| the gate | RW 0x4C7795 `cmp byte [edi + 0x108], 0; je 0x4C77A8` (`edi` = the module data, `[ebx + 4]`) |
| the call | RW 0x4C779E `lea ecx, [ebx + 0xC]; mov eax, [ecx]; push 0; call [eax + 0x6C]` (argument 0 = per frame) |
| interface slot 27 of the draw module's `+0xC` object | vtable RW 0xBE3B78 slot 27 = **RW 0x4B6D5F** |
| the other caller (end of the module) | RW 0x4C6D73 `lea ecx, [ebx + 0xC]; push 1; call [eax + 0x6C]` then RW 0x4C5CC5 |

The write side of the list is the creation: the ParticleSysBone block of RW 0x4C6514 fills the
record and pushes a node. `RW 0x4B6CDD` copies the record `{AsciiString bone +0, dword boneIndex +4,
dword +8, byte followBone +0x10}`; `RW 0x4C6B49` stores the same byte into the new system's `+0x1A7`
(`mov byte [eax + 0x1a7], cl`); RW 0x4C6BAD .. 0x4C6C39 build the node (the system's id at `+8`, the
record pointer at `+0xC`) and push it to the module's list at `+0x64` (the head/tail pair the walk
reads at its object `+0x58`).

## The walk, instruction by instruction

`esi` is the draw module's `+0xC` object, `[esi + 0x58]` the list, `[esi + 0x44]` the render object,
`[esi - 8]` the module data, `[esi - 4]` the drawable, `[ebp + 8]` the call's argument.

```
4B6D5F  ...                        ; early out unless [esi+8], [esi+0x44], [esi-4] and the list are set (0x4B6F01)
4B6DA1  mov ebx, [ebp - 0x1c]      ; the node
4B6DA4  mov ecx, [0xde3744]        ; TheParticleSystemManager
4B6DAA  lea edi, [ebx + 8]
4B6DAD  push [edi]                 ; the node's system id
4B6DAF  lea eax, [ebp - 0x18]
4B6DB2  push eax
4B6DB3  call 0x5f530a              ; findParticleSystemByID(&handle, id)
4B6DBF  test eax, eax
4B6DC1  jne  0x4b6dcc              ; a system that is gone: the node stays, the walk goes on (0x4B6EE4)
4B6DCC  cmp  byte [eax + 0x1a7], 0 ; the record's FollowBone
4B6DD3  jne  0x4b6ddf
4B6DD5  cmp  byte [ebp + 8], 0
4B6DD9  jne  0x4b6ef7              ; FollowBone = No and argument != 0: the walk ends here
4B6DDF  mov  eax, [esi - 8]        ; the module data
4B6DE2  cmp  byte [eax + 0x12a], 0 ; ParticleBonesCheckDrawable
4B6DE9  mov  ebx, [ebx + 0xc]      ; ebx = the record
4B6DEC  je   0x4b6e57
```

By name (the flag set):

```
4B6DF2  call 0x4b6cdd              ; copy the record
4B6E02  add  eax, 8                ; the bone's AsciiString ("RootTransform" when null)
4B6E0C  mov  ecx, [esi - 4]        ; the drawable
4B6E14  call 0x672b5b              ; the bone transform by name -> [ebp - 0x60], al = found
4B6E19  test al, al
4B6E1B  je   0x4b6e49              ; not found: keep the system's last transform
4B6E2F  call 0x5f2f55              ; setLocalTransform
4B6E40  push 1
4B6E44  call 0x5f32a7              ; setSkipParentXfrm(1)
```

RW 0x672B5B walks the drawable's sub-object pointer array `[drawable + 0x14C]` (`+4` stride,
null-terminated) and asks each sub-object for the bone name through its vtable slot `+0x10`
(`RW 0x672AEE` is that helper's head); the result is the sub-object's transform for the bone.

By index (the flag clear):

```
4B6E57  test ebx, ebx
4B6E59  je   0x4b6ed2              ; a record whose bone index is 0: nothing to do
4B6E5B  mov  ecx, [esi + 0x44]     ; the render object
4B6E61  call [eax + 0xd4]          ; the bone's visibility for the index ebx
4B6E6C  je   0x4b6e80
4B6E79  call 0x5f2ede              ; visible: start (the system's +0x1A3 = 0)
4B6E8B  call 0x5f2ee6              ; not visible: stop (+0x1A3 = 1)
4B6E96  lea  edx, [ebp - 0x90]
4B6E9D  call [eax + 0xcc]          ; Get_Bone_Transform(ebx) -> [ebp - 0x90]
4B6EB8  call 0x5f2f55              ; setLocalTransform
4B6EC9  push 1
4B6ECD  call 0x5f32a7              ; setSkipParentXfrm(1)
```

The two tiny leaf functions were read to fix the meaning of the pair: RW 0x5F2EDE is
`mov byte [ecx + 0x1a3], 0; ret` and RW 0x5F2EE6 is `mov byte [ecx + 0x1a3], 1; ret` - the
system's `m_isStopped` (`ParticleSys.h`, the same field `ParticleSystem::update` tests before it
emits).

## What that means for the port

1. **The gate is the module data, not the record.** RW 0x4C7795 reads `+0x108` of the same data the
   INI row `ParticlesAttachedToAnimatedBones` writes, once per draw. A module without it never calls
   the walk, so its systems stay where the creation put them. The port keeps that shape
   (`ParticleBoneFollow::wantsFollow`, one `Pose` per draw entry with its `follow` flag).
2. **FollowBone does not gate the per-frame follow.** The per-frame call passes 0, and 0x4B6DCC /
   0x4B6DD5 only skip a FollowBone = No record when the argument is non-zero - which is the
   end-of-draw call at RW 0x4C6D78. Both callers are therefore in the port: argument 0 from the
   per-frame pass, argument 1 available for the ending pass.
3. **The transform is the render object's bone transform and the parent is skipped.** Both branches
   call `setSkipParentXfrm(1)`, and `ParticleSystem::update` (RW 0x5FA2CE, the `m_skipParentXfrm`
   test) then takes the local transform as the world one; the creation's position + Z rotation has no
   such call and is still multiplied by the drawable. The port therefore hands the walk a pose that
   already carries the drawable's instance scale and multiplies the drawable's world transform in
   (retail's render-object transform times the pivot), which is the same value the old
   `LiveFX::boneWorldMatrix` path produces for a still model.
4. **A bone index of 0 is skipped on the by-index branch** (0x4B6E57). `HTreeClass::Get_Bone_Index`
   answers 0 for "not found", so a record whose creation-time index is 0 (a bone the model lacks) is
   never moved - the port keeps that, and the by-name branch ignores the index instead, as RW does.
5. **A missing bone or an undefined frame keeps the last transform** (0x4B6E1B jumps past both
   calls). The port counts such a frame in `Counters::missed` and writes nothing.

## INFERENCE (reported in the PR and in stop S-682 / S-7191)

* the pose is the draw's track 0 at its frame, evaluated by `PristinePose` (the transcription of
  `HTreeClass::Anim_Pose`, RW 0x562BB0 / raw 0x563A80 - the evaluator the renderer and the pick path
  call). The RW pose request 0x4B33FB blends track 1 in while an `AnimationBlendTime` countdown runs;
  that arm is not ported here.
* the render object's slot `+0xD4` is not identified. It is asked for the record's index and drives
  exactly the stop/start pair, so it is taken as the pivot's visibility in the pose
  (`HTreePose::IsVisible`, the animation's visibility channel).
* the two branches resolve to the same pose in this port; what stays different is what each reads
  from the record (name re-resolved per frame vs the creation-time index).
* the walk runs once per client frame from `LiveFX::updateAttachedSystems` (the port's point after the
  drawables' poses were synced) instead of from each draw's `doDrawModule`, so a drawable that is not
  drawn this frame still moves its systems.

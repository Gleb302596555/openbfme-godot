# Port 28 — Weapon/RotatingTurret: turret aim vector (RW analysis)

Issue #100. Source of truth: RotWK 2.01 `game.dat` (md5 daed3668…), read with `tools/mod/rw.py` + a
recursive-descent disassembler (linear `dis` desyncs inside this function). Donor: BFME2 1.06 decompile
(`/tmp/open-bfme-2`), `Code/GameEngine/Source/GameLogic/Object/WeaponProjectileLaunch.cpp`.

## 1. The field

| fact | address |
|---|---|
| WeaponTemplate row | RW 0xC17198 |
| parser (INI field table) | RW 0x42E558, member **+0x16C** (`RotatingTurret`, `parseBool`) |
| getter `mov al,[ecx+0x16c]; ret` | RW 0x6C9C50 (pointer in the field-getter table RW 0xC7B568) |
| accessor through the weapon: `mov eax,[ecx+4]; mov al,[eax+0x16c]; ret` | RW 0x6C9C6B |
| default write | RW 0x6CE18B; copies RW 0x6CE7AD / 0x6CE7B3 |

Ennorath parses it in 1 weapon definition (`engine/tests/data/weapon1/…` reproduces the parse); the port parsed
it at `engine/src/GameLogic/Object/Weapon.cpp:467` into `WeaponTemplate::m_rotatingTurret` and never read it.

## 2. The enclosing function: `Weapon::calcProjectileLaunchPosition` (RW 0x6CAB85)

Entry RW 0x6CAB85 (`push ebp` / `lea ebp,[esp-0x64]` / `sub esp,0xFC`), 565 instructions, ends RW 0x6CB48F
`ret`. `__cdecl`, 5 arguments: `(const Object *launcher, WeaponSlotType wslot, Int specificBarrelToUse,
Matrix3D &worldTransform, Coord3D &worldPos)` — arg1 `[ebp+0x6c]`, arg2 `[ebp+0x70]`, arg3 `[ebp+0x74]`,
arg4 `[ebp+0x78]`, arg5 `[ebp+0x7c]`. Callers: RW 0x90FAAA, RW 0x90FAD7 (the object- and position-target
branches of `Weapon::positionProjectileForLaunch`, ported as `BezierProjectileBehavior::positionForLaunch`)
and RW 0x85E9F2.

Body (only the parts that matter here):

| RW | what |
|---|---|
| 0x6CAC0F | `push 0x3A` + RW 0x44DDEC on the launcher: status INSIDE_GARRISON (garrison branch, ported, stop S-360 counters) |
| 0x6CAC90..0x6CACB3 | the launcher's own AI (+0x260) |
| 0x6CAC74 / 0x6CACA5 | RW 0x66246B on the AI: `getWhichTurretForWeaponSlot(wslot, &turretAngle, &turretPitch)` → `edi` = turret index, `[ebp+0x18]` = turretAngle, `[ebp+8]` = turretPitch; `-1` = `TURRET_INVALID` |
| 0x6CACB3 | RW 0x70E013 `Object::getDrawable` |
| 0x6CACCE | RW 0x6756A1 `Drawable::getProjectileLaunchOffset(wslot, barrel, &attachTransform, tur, &turretRotPos, &turretPitchPos)`: `attachTransform` = `[ebp+0x2c]`, turretRotPos = `[ebp+0x20]`, turretPitchPos = `[ebp+0xc]` |
| 0x6CACDA..0x6CAD3A | no drawable / no answer: identity and both turret positions zero (already ported) |
| **0x6CAD3F..0x6CAD68** | `WeaponSet::getWeaponInWeaponSlot(launcher + 0x34C, wslot)` (RW 0x6C81B0) → `[eax+4]` template, `cmp byte ptr [eax+0x16C],0`, `cmp edi,-1`: any false → RW 0x6CB440 |
| **0x6CAD6E..0x6CB43B** | the turret composition: the pitch adjustment (RW 0xA3CF84 / RW 0xA3CF90 = CRT `sin` / `cos` of `-turretPitch`), then the turn adjustment (same pair of calls on `turretAngle`), both composed into the attach transform at `[ebp+0x2c]` |
| 0x6CB440..0x6CB45C | RW 0x70BCE7 `Thing::convertBonePosToWorldPos(host-or-launcher, 0, &attachTransform, 0, arg4)` — the matrix the projectile starts with |
| 0x6CB461..0x6CB487 | `arg5` = `(arg4[0][3], arg4[1][3], arg4[2][3])` — `movsd` x3 copies 12 bytes from `arg4+0xc / +0x1c / +0x2c` |

So the field gates exactly one block, and that block changes **the transform the projectile is created with**
(the logic's launch position and basis), not a drawable pose. Consumer = logic → this issue is a port.

## 3. What the gated block computes

DONOR BFME2 `WeaponProjectileLaunch.cpp` (retail 0x002C9E48) is the same function without the gate; the RW
block matches it instruction for instruction (`fchs` of `turretPitch` at RW 0x6CAD71/0x6CAD79, the two
`sin`/`cos` pairs RW 0x6CADCB+0x6CADD9 and RW 0x6CAF21+0x6CAF2F, the `turretPitchPos` reads
`[ebp+0xc..0x14]` then `turretRotPos` `[ebp+0x20..0x28]`, the 3x3 products at RW 0x6CB000..0x6CB43B):

```c
if (slotWeapon && slotWeapon->getTemplate()->isRotatingTurret() && tur != TURRET_INVALID)
{
    Matrix3D turnAdjustment(1);
    Matrix3D pitchAdjustment(1);
    // rotate about a point: move it to 0,0, rotate, move back; pre-rotate keeps the first twist
    // from screwing the angle of the second pitch
    pitchAdjustment.Translate(turretPitchPos);
    pitchAdjustment.In_Place_Pre_Rotate_Y(-turretPitch);
    pitchAdjustment.Translate(-turretPitchPos);
    turnAdjustment.Translate(turretRotPos);
    turnAdjustment.In_Place_Pre_Rotate_Z(turretAngle);
    turnAdjustment.Translate(-turretRotPos);
    Matrix3D originalTransform = attachTransform;
    attachTransform.mul(turnAdjustment, pitchAdjustment);
    attachTransform.postMul(originalTransform);
}
```

I.e. with the flag set, the launch bone transform is turned around the turret's rotation point by the
turret's yaw and pitched around its pitch point by the turret's pitch: the projectile leaves along the
turret's aim direction instead of along the body's. Without the flag (or with no turret for the slot) retail
uses the pristine bone transform — what the port does today.

## 4. State of the port before this change

* `WeaponTemplate::m_rotatingTurret` — parsed at `engine/src/GameLogic/Object/Weapon.cpp:467`, never read.
* `BezierProjectileBehavior::calcLaunchTransform` (`engine/src/GameLogic/Module/ProjectileModules.cpp:303`)
  is RW 0x6CAB85 without the turret block: it takes the bone transform from `ProjectileLaunchOffsets` (the
  RW 0x6756A1 stand-in, `engine/src/GameClient/DrawableLaunchBones.h`), composes it with the launcher's
  transform (RW 0x70BCE7) and returns position + basis.
* The turret for the slot (RW 0x66246B) was assumed absent ("every slot answers TURRET_INVALID"), written in
  the comment at `ProjectileModules.cpp:298` and recorded as stop S-360 / S-1103.
* TurretAI itself exists (`engine/src/GameLogic/AI/TurretAI.cpp`, lane GARRISON-2) with `m_angle` / `m_pitch`; the
  launch composition is listed as not ported in `TurretAI.h`.

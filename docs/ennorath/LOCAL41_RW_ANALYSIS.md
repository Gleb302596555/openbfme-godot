# Local 41 — RW analysis: weapon aim at the victim's contact point

## RotWK 2.01 `game.dat` (md5 `daed3668…`), read via `tools/mod/rw.py` (`python -I`)

## 1. `WeaponTemplate::adjustTargetPositionOnVictim(source, victim, flag)` — RW 0x6CB85A (RET 0x10)

```
RW 0x6CB85A  prologue; out = (0,0,0)
RW 0x6CB87B  if victim == 0 -> return (0,0,0)  [RW 0x6CB889..0x6CB893]
RW 0x6CB88E  if PreferredTargetBone (+0x90) answers on victim (RW 0x6CB81A) -> return bone world pos  [RW 0x6CB89C..0x6CB8B7]
RW 0x6CB8BC  if flag == 0 -> plain LOS path (position + offset, clamped for castle structures)  [RW 0x6CB8F8..0x6CB983]
RW 0x6CB8C2  if flag != 0:
    seed = GetGameLogicRandomValue(0, 12345678, "Weapon.cpp", 0x6D5)   [RW 0x6CB8C2..0x6CB8D3]
    victim->getWorldspaceBestContactPoint(&pos, &source->m_position, nullptr, 3, seed, false)  [RW 0x6CB8EC]
    return pos with z clamped vs GlobalData+0x11C4 and +0xA8 offset  [RW 0x6CB908..0x6CB96D]
```

- **Donor BFME2 0x2CAB3A** (`Code/GameEngine/Source/GameLogic/Object/WeaponTemplateTargetPositions.cpp`): identical structure — PreferredTargetBone helper (`Rva002CAAFA::rva002CAAFA`, `getSingleLogicalBonePosition`), flag==1 draws `GetGameLogicRandomValue(0,12345678,"Weapon.cpp",0x6D5)` then `getWorldspaceBestContactPoint(out, &source->m_position, 0, 3, seed, false)`; flag==0 copies victim position + offset (LOS path, used by `CombatLineOfSight`).
- **Flag semantics**: flag==1 = weapon aim (contact point, preference 3, seed drawn). flag==0 = line-of-sight aim (victim position + offset, no random draw). The port's `CombatLineOfSight::victimLineOfSightPoint` is the flag==0 path — it does NOT use the contact point.

## 2. `Object::getWorldspaceBestContactPoint(out, callerPos, label, pref, seed, ignoreCollision)` — RW 0x690BD2

```
RW 0x690BD2  transform callerPos into object-local space (inverse of object transform, RW 0xB26C00)
RW 0x690D20  GeometryInfo::getBestContactPoint(&local, &localCaller, label, pref, seed, ignoreCollision)  [RW 0xAD30E0]
RW 0x690D4E  if body->getDamageState() == BODY_RUBBLE -> out.z = getMaxHeightAbovePosition()
RW 0x690D6E  transform local point back to world space; return
```

- **Donor BFME2 0x2904DC** (`Code/GameEngine/Source/GameLogic/Object/ObjectWorldspaceBestContactPoint.cpp`): same — inverse transform, `m_geometryInfo.getBestContactPoint`, BODY_RUBBLE -> z = max height, transform back.

## 3. `GeometryInfo::getBestContactPoint(out, callerPos, label, preference, seed, ignoreCollision)` — RW 0xAD30E0

Start: `*out = m_innermostContactPoint` (+0x50), which is precomputed by RW 0xAD25B0 as `(0, 0, height * 0.67f)` where `height = getMaxHeightAbovePosition()` (RW 0xAD1920). `+0x44` (preferredPosition) = (0,0,0).

```
switch (preference):
case 0:     nearest contact point whose label == `label`, distance to callerPos, overlap test unless ignoreCollision
case 1:     out = preferredPosition ((0,0,0)), clamp z <= height
case 3:     if count >= 2:
                idx = seed % (count - 1)
                blend = ((seed >> 8) & 0xFF) / 255.0f
                out = contacts[idx] * blend + contacts[idx+1] * (1 - blend)
            (count < 2: out stays at defaultPosition = (0, 0, height * 0.67f))
            clamp out.z <= height
            if !ignoreCollision && !CollidesWith(sphere radius 2 at *out against this geometry at origin):
                out = centre of first ENABLED shape: (offset.x, offset.y, offset.z + height * 0.5)
            clamp out.z <= height
            clamp out.z >= height * 0.1f
            return true
```

- **Donor BFME2 0x6BEF80** (`Code/GameEngine/Source/Common/System/GeometryInfoCollidesWith.cpp`): byte-for-byte the same algorithm (`index=seed%(count-1)`, `blendFactor=((seed>>8)&255)/255.0f`, fallback to first enabled shape centre, z clamps to `[height*0.1, height]`).

## 4. `WeaponTemplate::getSingleLogicalBonePosition` / PreferredTargetBone helper — RW 0x6CB81A

```
RW 0x6CB81A  StringBase at +0x90 (m_preferredTargetBone); if non-empty:
    victim->getSingleLogicalBonePosition(bone, &pos, 0)  (RW 0x68C5AF -> 0x672A73) -> true/false
```

- **Donor BFME2**: `Rva002CAAFA::rva002CAAFA` — same. The port already parses `m_preferredTargetBone` (+0x90, `Weapon.cpp:438`) and implements `preferredBoneAim` (`WeaponDelivery.cpp:1028`).

## 5. Retail callers of RW 0x6CB85A (`xref 0x6CB85A`)

| RW address | Function | Flag | Port place | Preference |
|---|---|---|---|---|
| 0x6CC37B | `WeaponTemplate::calcScatterTargetPos` wrapper (base aim for scatter) | 1 | `ObjectWeaponDelivery::scatterTargetPos` -> `fireFXAimPosition` (STRUCTURE victims) | 3 |
| 0x6CC3EA | `WeaponTemplate::calcScatterTargetPos` (STRUCTURE victim aim point) | 1 | same | 3 |
| 0x6CCAC7 | `Weapon::privateFireWeapon` fire-FX aim query | 1 | `ObjectWeaponDelivery::fireFXAimPosition` | 3 |
| 0x85EC51 | `BezierProjectileBehavior::projectileFireAtObjectOrPosition` launch aim | 1 | `BezierProjectileBehavior::aimPosition` | 3 |
| 0x85F46A | `BezierProjectileBehavior::update` homing-step aim | 1 | `BezierProjectileBehavior::aimPosition` | 3 |

Note: `DamageFieldNugget` (RW 0x90F534, port `damageFieldTo`) calls `adjustTargetPositionOnVictim(out, source=0, victim, 1)`. Retail reads `source+0x38` with a null source → crashes; retail data only delivers this nugget without a victim. The port reports an error instead (already handled, `WeaponDelivery.cpp:1411`).

## 6. Retail callers of RW 0x690BD2 (`xref 0x690BD2`)

| RW address | Function | Preference | Port place |
|---|---|---|---|
| 0x6CB8EC | `WeaponTemplate::adjustTargetPositionOnVictim` (flag 1) | 3 | `aimPosition` / `fireFXAimPosition` |
| 0x667D96 | `AIUpdateInterface::findNearestLabeledContactPointOnTarget` | 0 | `AIUpdateInterface::findNearestLabeledContactPointOnTarget` (`AIUpdateCombat.cpp:230`) |
| 0x945871 | `ControlBarRadialMenu` (label "Menu") | 0 | `ControlBarRadialMenu.cpp:143` |
| 0x74C5FF / 0x75253D / 0x76B73F / 0x76DE4C / 0x76E230 | AI attack approach/aim states (various preferences) | various | AI attack path (not weapon aim; out of scope for this task) |
| 0x852110 / 0x853AD2 | AI attack target computations | various | AI attack path (out of scope) |

## 7. Port's current state (before this task)

- `ObjectGeometry::getBestContactPoint` implements **preference 0 only** (nearest labeled point, overlap probe unless `preferred`=ignoreCollision). No seed, no preference 3.
- `ObjectGeometry::worldspaceBestContactPoint` calls it with `preferred` bool (maps to ignoreCollision), no preference, no seed.
- `BezierProjectileBehavior::aimPosition` (`ProjectileModules.cpp:419`): preferred bone, else `getValue(0,12345678,1749)` drawn **and discarded**, then geometry centre `z = pos.z + height*0.5` (S-362 approximation).
- `ObjectWeaponDelivery::fireFXAimPosition` (`WeaponDelivery.cpp:2299`): same — seed drawn and discarded (`(void)seed`), geometry centre (S-362).
- `scatterTargetPos` (`WeaponDelivery.cpp:2230`): for STRUCTURE victims calls `fireFXAimPosition` (inherits the fix).
- `CombatLineOfSight::victimLineOfSightPoint` (`CombatLineOfSight.cpp:110`): flag==0 LOS path (position + centreZ), NOT preference 3 — unchanged.

## 8. What to port (one mechanism, in the geometry)

1. `ObjectGeometry::getBestContactPoint`: add `int preference, int seed`; implement preference 3 (seed blend, count<2 → `(0,0,height*0.67f)`, overlap fallback to first active shape centre, z clamp `[height*0.1, height]`). Preference 0 stays as today.
2. `ObjectGeometry::worldspaceBestContactPoint`: add `int preference, int seed`; pass through.
3. `BezierProjectileBehavior::aimPosition`: after preferredBoneAim fails, draw `seed = getValue(0,12345678,kWeaponCpp,1749)` and call `worldspaceBestContactPoint(victim, shooterPos, "", 3, seed, false, out)`.
4. `ObjectWeaponDelivery::fireFXAimPosition`: same — draw seed, preference 3 contact point (no `(void)seed` discard).
5. Update all existing callers of `worldspaceBestContactPoint`/`getBestContactPoint` to the new signature (preference 0, seed 0, ignoreCollision=false).
6. Remove the S-362 geometry-centre aim from `aimPosition` and `fireFXAimPosition`.

No per-kind or per-weapon branches; every caller on the list above uses the same path.

## 9. Edition statement

RotWK only (the contact-point preference-3 aim is the 2.01 behaviour; BFME2's donor has the same function but this task is the RotWK port). BFME2 donor cited as reference only.

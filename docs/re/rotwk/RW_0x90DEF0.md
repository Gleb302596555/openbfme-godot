# RW 0x90DEF0 — the area shape of a damage nugget (issue #122, lane PORT-37)

RotWK 2.01 (`workspace/RotWK/game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, the S-001
provenance caveat applies). Port: `engine/src/GameLogic/Combat/WeaponDelivery.cpp`
(`damageAreaDistance` / `radiusDamage`, lane PORT-37) and
`engine/src/GameLogic/Object/PartitionManager.{h,cpp}` (`withinRange`, `distanceTo`). No data
of the mod is reproduced here.

This note was written with the image at hand: every instruction quoted below was decoded from
it in the same session (`tools/mod/rw.py dis`, capstone 5, image base `0x400000`), and the
table contents were read from the image's `.rdata`.

## Where the field and the slot are

| What | Address |
|---|---|
| field-table row `CylinderAOE` (damage nugget) | RW 0xC7B100 |
| parser | RW 0x42E558 |
| member | nugget `+ 0x1C0` (byte) |
| the function | RW 0x90DEF0, slot 6 of the nugget's vtable (tables RW 0xC7AE90 / RW 0xC7BB00) |
| neighbours read by the same function | `+0x150` Radius, `+0x154` MinRadius, `+0x158` DamageArc, `+0x160` DamageMaxHeight, `+0x164` DamageMaxHeightAboveTerrain |
| the partition | ThePartitionManager RW 0xDE4354, range query RW 0xA39340 |
| the distance table | RW 0xDBDAF8 |
| constants | RW 0xBD1908 = 1.0f, RW 0xBD83D8 = 10.0f, RW 0xBDD388 = pi |

## The branch, instruction by instruction

The head of the function decides which distance the range query gets. `esi` is the nugget,
`[ebp+8]` is the delivery info (its `+8` is the source's id), `[ebp+0xC]` is the query's
callback context.

```
90DF00  movss  xmm0, dword ptr [0xbd1908]      ; 1.0f
90DF0C  movss  xmm1, dword ptr [esi + 0x150]   ; Radius
90DF14  comiss xmm0, xmm1
90DF18  movss  dword ptr [ebp - 0x20], xmm1    ; the radius the query gets = max(1.0f, Radius)
90DF1D  jbe    0x90df24
90DF1F  movss  dword ptr [ebp - 0x20], xmm0
90DF24  mov    eax, dword ptr [ebp + 8]
90DF27  push   dword ptr [eax + 8]             ; the source's ObjectID
90DF2A  mov    ecx, dword ptr [0xde412c]       ; TheGameLogic
90DF30  call   0x449681                        ; object by id -> eax (the source object)
90DF35  cmp    byte ptr [esi + 0x1c0], 0       ; CylinderAOE
90DF3C  push   3
90DF3E  mov    dword ptr [ebp - 0x30], eax     ; the source is kept for the height caps
90DF41  pop    edi                             ; default: mode 3
90DF42  je     0x90df49
90DF44  xor    edi, edi
90DF46  inc    edi                             ; CylinderAOE != 0: mode 1
90DF47  jmp    0x90df78
90DF49  movss  xmm0, dword ptr [ebp - 0x20]    ; the clamped radius
90DF4E  comiss xmm0, dword ptr [0xbd83d8]      ; 10.0f
90DF55  jb     0x90df78                        ; under 10: keep mode 3
90DF57  mov    ecx, dword ptr [eax + 4]        ; the source's template
90DF5A  test   byte ptr [ecx + 0x11d], 8       ; KindOf BIG_MONSTER
90DF61  je     0x90df75                        ; not one: mode 4
90DF63  test   byte ptr [ecx + 0x10a], 0x20    ; KindOf TRANSPORT
90DF6A  je     0x90df75                        ; not one: mode 4
90DF6C  cmp    dword ptr [eax + 0x258], 0      ; the source's contain
90DF73  jne    0x90df78                        ; a monster transport with a contain: keep mode 3
90DF75  push   4
90DF77  pop    edi                             ; otherwise: mode 4
90DF78  push   0x36                            ; filter: exclude KindOf bit 54 (UNATTACKABLE)
90DF7A  push   0x59                            ; exclude KindOf bit 89 (INERT)
90DF7C  xor    ebx, ebx                        ; exclude KindOf bit 0 (OBSTACLE)
90DF87  push   eax
90DF88  push   0xde49e4                        ; the default GeometryInfo the filter needs
90DF93  call   0x445139                        ; PartitionFilter
90DF98  fld    dword ptr [ebp - 0x20]
90DFA3  push   edi                             ; <- the mode: the distance type of the query
90DFA8  mov    ecx, dword ptr [0xde4354]       ; ThePartitionManager
90DFAE  fstp   dword ptr [esp]                 ; <- the clamped radius
90DFB9  call   0xa39340                        ; iterateObjectsInRange
```

So there are exactly three outcomes for the area's shape:

| condition (in this order) | mode |
|---|---|
| `CylinderAOE` (`nugget + 0x1C0`) is not 0 | **1** |
| radius (already `max(1.0f, Radius)`) `>= 10.0f` **and** the source is not (`BIG_MONSTER` **and** `TRANSPORT` **and** has a contain) | **4** |
| otherwise (including every source that is a monster transport with a contain, and every radius under 10) | **3** |

`KindOf` is a bitset at template `+0x108` in the port's view, so `[ecx+0x11D]` bit 3 is bit
171 and `[ecx+0x10A]` bit 5 is bit 21; the two names are the field's own (`BIG_MONSTER`,
`TRANSPORT`). The base `0x108` is the one `AttachUpdate.h` records (`HORDE` = `+0x115` bit 5);
it is not re-derived here — **INFERENCE**.

## The distance table RW 0xDBDAF8

Five dwords, read from the image:

| mode | function | what it computes (per the port's reading of each function, lane MODCOMPAT-MOD1) |
|---|---|---|
| 0 | RW 0xA3A7A0 | `dx^2 + dy^2` of the centres |
| 1 | RW 0xA3AE50 | `(sqrt(dx^2 + dy^2) - GeometryInfo + 0x10)^2`, negated when the difference is negative: the **2D bounding circle**, z ignored |
| 2 | RW 0xA3A7D0 | `dx^2 + dy^2 + dz^2` |
| 3 | RW 0xA3AEB0 | the same in 3D: `dz +` the geometry's centre height (`GeometryInfo + 0x20`, RW 0xB4E370), minus the bounding sphere (`GeometryInfo + 0x14`) |
| 4 | RW 0xA3C7D0 | the object's own shapes against a static sphere of the query radius (RW 0xAD2CE0) |

`RW 0xA3C4E0` (the query's worker) clamps any type outside 0 .. 4 to 0 before it indexes the
table (RW 0xA3C50F .. 0xA3C52F), and the per-entry test rejects only "greater" (RW 0xA3A8AB), so
a NaN passes.

**The port's `DistanceCalculationType` keeps ZH's numbering, which is not the table's order:**
port 1 is the 3D centre (RW mode 2) and port 2 is the 2D bounding circle (RW mode 1). The
RotWK modes are therefore mapped by meaning, not by number: mode 1 -> port
`FROM_BOUNDINGSPHERE_2D`, mode 3 -> `FROM_BOUNDINGSPHERE_3D`, mode 4 ->
`FROM_GEOMETRY_OVERLAP`, all of which the port's `PartitionManager` already implemented.

## What the shape means

* Mode 1 measures the horizontal distance between the source's position and the victim's
  position less the victim's bounding circle: **height does not matter at all**. The two caps
  of the query's victim loop (`+0x160` DamageMaxHeight, `+0x164` DamageMaxHeightAboveTerrain,
  both relative to the source's position) are the only height limits a cylinder has — that is
  the whole of `CylinderAOE`'s effect.
* Mode 3 is the 3D bounding sphere, measured as (3D distance to the victim's geometry centre)
  minus the victim's bounding sphere: a victim above or below the centre is reached only while
  its own sphere reaches the query sphere.
* Mode 4 is the exact test: the victim's shapes against the sphere, no bounding approximation.
  It is the only mode where the shape's own offsets (a weapon rack on top of a building, a
  body leaning over an edge) matter.

## What the port does now

`damageAreaDistance` returns the port's enum from the three cases above; `radiusDamage` gives
each candidate to `PartitionManager::withinRange`, which is the query's own per-entry test
(RW 0xA3C4E0 / table RW 0xDBDAF8) for one object the caller already holds — the port still
walks the object list itself (S-321: RW's hit order is the partition's, and the port does not
reproduce it), so the area is RW's while the order is not. Before lane PORT-37 the port tested
every nugget with the 2D bounding circle and did not read `+0x1C0` at all.

For mode 4 the walk primes each candidate with the 2D bounding circle first (RW's own query
cannot walk a cell that the centre ± radius rectangle does not cover): every shape lies inside
that circle, so a farther object cannot overlap, and the shape test runs only for the few that
can.

## Open

* The `KindOf` base `0x108` (INFERENCE, above).
* A delivery whose source object is gone: retail dereferences the source at RW 0x90DF57 /
  RW 0x90DF6C with no null test. The port treats that as "the conditions do not hold" (mode
  4). Nothing observable depends on the choice — the nugget's slot 1 (RW 0x90E855) refuses
  every victim when the source is gone.

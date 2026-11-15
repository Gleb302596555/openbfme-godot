# PORT-216 — mumak projectile contact: investigation checkpoint

Scope: fork issue 506, dashboard task 192. This is not a completed port or a
claim that the owner's in-game report is fixed. No simulation code changed.

## Reference and method

RotWK 2.01 `game.dat` was obtained through the owner's Drive connector and
verified as MD5 `daed3668006cd90f01c34e5a7da1901f`. It is ignored under
`workspace/RotWK/`. Ennorath 1.9.8.3 was obtained separately and extracted
outside Git. No game data or disassembly dump belongs in this report.

Read the BFME2 donor first, then disassembled the RotWK functions with
`tools/mod/rw.py dis`, from their entry points. Donor addresses below are
RVAs, as recorded in the donor source headers, not RotWK addresses:

- DONOR BFME2 RVA 0x002CAB3A
  (`Code/GameEngine/Source/GameLogic/Object/WeaponTemplateTargetPositions.cpp`).
- DONOR BFME2 RVA 0x002904DC
  (`Code/GameEngine/Source/GameLogic/Object/ObjectWorldspaceBestContactPoint.cpp`).
- DONOR BFME2 RVA 0x006BEF80
  (`Code/GameEngine/Source/Common/System/GeometryInfoCollidesWith.cpp`).

The direct call chain is RW 0x6CB85A -> RW 0x690BD2 -> RW 0xAD30E0.
These are not module-vtable identifications. The Bezier module's factory,
constructor and interface-slot chain still needs to be verified before
making any claim about its collision path.

## Verified aim branches

| Situation | RotWK instructions | Existing port / conclusion |
|---|---|---|
| Null victim | RW 0x6CB86A..0x6CB889, return at 0x6CB983..0x6CB98B | Zero result; not evidence of a mumak-specific rule. |
| Preferred target bone answers | RW 0x6CB81A..0x6CB857; call at 0x6CB893, early result at 0x6CB89C | Bone first, no seed draw. Both existing aim callers already ask the preferred bone first. |
| Flag 1, no preferred bone | RW 0x6CB8BC..0x6CB8EC | Draw the Weapon.cpp:1749 seed, then call the victim's worldspace contact query with preference 3. No STRUCTURE, BIG_MONSTER, geometry-size or bone-list gate here. |
| Flag 0 | RW 0x6CB8F8..0x6CB96D | Victim position plus geometry offset; the STRUCTURE test and castle LOS clamp belong to this branch, not the flag-1 contact query. |
| Worldspace query | RW 0x690C49..0x690D20 | Transform caller into victim space; query GeometryInfo at Object +0xA8. |
| Failed geometry query / rubble / world transform | RW 0x690D25..0x690DFA | Failure returns false; body slot +0x24 returning 3 replaces local z with maximum geometry height; transform the resulting point back into worldspace. |
| Preference 3, at least two contact rows | RW 0xAD31A4..0xAD3276 | Blend adjacent rows selected by the seed. The existing `ObjectGeometry::getBestContactPoint` has this branch. |
| Preference 3, fewer rows | RW 0xAD3172..0xAD3184, 0xAD31AF..0xAD31B2 | Keep the stored innermost point. RW 0xAD25B0..0xAD260F initializes it at local x/y zero and 0.67 times geometry height. |
| Preference 3, collision and clamps | RW 0xAD3278..0xAD333D | Clamp to maximum height; unless collision is skipped, test the point and fall back to the first active shape's centre; apply the lower height clamp. |
| Neighbour preference 1 | RW 0xAD3340..0xAD3379 | Stored preferred point, clamped to maximum geometry height. Its initializer also updates it at RW 0xAD25E6..0xAD2607; its full initialization history is not yet traced. |
| Neighbour preference 0 / unknown preference | RW 0xAD337C..0xAD3517 | Nearest matching label passing the optional collision test, then height clamps; no match or unknown preference returns false. |

## What this rules out, and what remains

The port already calls the common query for units as well as structures:
`ObjectWeaponDelivery::fireFXAimPosition` in `WeaponDelivery.cpp`,
`BezierProjectileBehavior::aimPosition` in `ProjectileModules.cpp`, and
`ObjectGeometry::worldspaceBestContactPoint` in `ObjectGeometry.cpp`.
Adding a separate mumak/large-unit aim gate would contradict the inspected
flag-1 branch. This does **not** establish that projectile impact on a
mumak's surface works in the port.

Checked the mod's HaradMumakilForOliphant definition and its HaradMumakil /
HaradMumakilForMordor children: the candidate contact-point rows in that
definition are commented out. No numerical mod values are copied here.

Next: trace Bezier launch/update/collision and its caller to distinguish the
flight's aim point from the actual surface impact. The existing port has
`BezierProjectileBehavior::backOff` and
`CombatState::processProjectileContacts`; they must be compared with RW,
not replaced with another path. A shared-collision-manager cause beyond
this task must be reported before changing it.

## Validation status

The full `openbfme_tests` Release / Godot-OFF build was started with `-j1`.
No completed build or test result is claimed yet. No `PORT-216` tests have
been added. No pins, existing tests, workflows or engine code changed.

Remaining: finish root-cause analysis, implement only RW-proven behaviour,
add the synthetic `PORT-216` lane, prove red/green on the full test binary,
check `PORT-436*`, then NIGHT_WATCH and the review PR.

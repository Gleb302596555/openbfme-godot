# PORT-216 — mumak projectile contact: investigation and scope stop

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
These direct calls are not module-vtable identifications. The Bezier chain
was subsequently checked: the module registry names factory RW 0x64AE35;
its call at RW 0x64AE5F reaches constructor RW 0x85F899. The constructor
installs the primary table RW 0xC58604, update table RW 0xC585F8 at +0x10,
projectile table RW 0xC585E0 at +0x20 and collide table RW 0xC585C8 at
+0x24 (stores RW 0x85F8D3..0x85F8EE). Primary slot 13 is fire at RW
0x85EBEE; update slot 0 is RW 0x85F28A. Collide slot 0, RW 0x85E52B,
adjusts this by -4 and calls projectile-interface slots +0x10 (can collide,
RW 0x85EF6A) and +0x0C (handle collision, RW 0x85FDB7).

DONOR BFME2 RVA 0x0024B148
(`Code/GameEngine/Source/GameLogic/Object/Behavior/BezierProjectileBehaviorFriendNew.cpp`)
and RVA 0x0045C850 / 0x0045C226 / 0x0045BF06 / 0x0045CD6E
(`Code/GameEngine/Source/GameLogic/Object/Behavior/BezierProjectileBehavior.cpp`)
provided the factory, layout and branch-reading guide, not RW authority.

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

## What this rules out

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

## Bezier and collision findings

The update function was disassembled linearly from RW 0x85F28A:

| Situation | RW branch | Observation |
|---|---|---|
| Missing data/object or exhausted path | 0x85F29B..0x85F2D8 | Sleep forever, or dispatch end-of-path collision with a null object. |
| Prelanding state/emotion | 0x85F2DD..0x85F42B | Model flag and optional nearby-object emotion; no new aim selector. |
| Homing target still exists | 0x85F42B..0x85F55F | RW 0x85F46A calls the same weapon aim with flag 1, limits the end-point adjustment and rebuilds the path. |
| Step and drawable lookahead | 0x85F565..0x85F71D | Set the path position/orientation and next position. |
| Explicit victim collision shortcut | 0x85F71E..0x85F78B | Tests the length of the victim's **world-space** position plus half-height against radius plus speed; the projectile position is not subtracted. The existing port preserves this unusual rule. It is not a closest-surface projection. |
| Layer crossing | 0x85F78E..0x85F838 | Terrain/layer handling, outside the proposed aim correction. |
| Advance / return | 0x85F83A..0x85F858 | Advance the path step and return the sleep cost. |

Fire chooses the same flag-1 aim at RW 0x85EC44..0x85EC51 and copies it
into the flight end at RW 0x85ED86..0x85ED92. The launch finishes with two
updates and the transform record between them (RW 0x85EF34..0x85EF5E).

Object registration RW 0x68E31F..0x68E43A rejects `NO_COLLIDE` at
RW 0x68E3C7..0x68E401 before collision-manager registration RW 0x68E411.
The donor comparison is DONOR BFME2 RVA 0x0028DCC4
(`Code/GameEngine/Source/GameLogic/Object/ObjectRva0028DCC4.cpp`).
The checked mod's ordinary GondorArcher projectile inherits the
collision-manager opt-out from its reskin parent. Therefore simply enabling
surface collisions for these arrows would change their configured behaviour.

**Not established:** that the reported near-side impact should be obtained
by changing the clean RotWK weapon-aim or collision branches. The owner must
compare the stated scenario in original Ennorath before a visible departure
from these branches is proposed (PROCESS 15.13). This is not a verdict that
some field has no reader, nor a claim that P-127 is already fixed.

## Adjacent verified gap: stored GeometryInfo contact state (S-7387)

The aim cache is not just a function of the final shapes:

- RW 0xAD2B60..0xAD2BB0 parses a height, refreshes bounds at 0xAD2BA1,
  then calls the point initializer at 0xAD2BA7.
- RW 0xAD25B0..0xAD260F stores the innermost point at GeometryInfo +0x50
  and can raise the preferred point at +0x44.
- RW 0xAD1770..0xAD17AF parses an offset into the last shape **without**
  invoking that initializer.
- RW 0xAD3D30..0xAD3E04 appends a contact row, updates the highest-point
  cache, then calls the same initializer at 0xAD3DEF.
- RW 0xAD3172..0xAD3184 reads the stored innermost point before the mode
  switch. In mode 3, fewer than two rows leave that stored value in place
  (RW 0xAD31AF..0xAD31B2), before collision and height clamps.

DONOR BFME2 RVA 0x006BE450 (`Code/GameEngine/Source/Common/Rva0087EBB0Max.cpp`,
identified by its caller in `GeometryParseDimensions.cpp`) and
`Code/GameEngine/Source/Common/System/GeometryParseActiveAndOffset.cpp` /
`GeometryParseDimensions.cpp` were read as the guide before the RW parser
bodies were inspected.

In the port, `ObjectGeometry::shapesOf` replays geometry rows into final
shapes, while `getBestContactPoint` mode 3 recomputes the innermost point
from their current height. `worldspaceBestContactPoint` combines the
object's own shapes with its original template's contact rows.
`Object::geometryTemplate`, `setGeometryInfoFrom`, active-shape/height
changes and their existing snapshot representation are additional consumers
that a faithful stored-state solution must cover.

A synthetic follow-up discriminator (not mod data): a box of height 40
with a later vertical offset 12 keeps the earlier innermost z, approximately
26.8; moving that offset before the height refresh yields approximately
34.84. The port currently reconstructs both from final height 52. A large
enough box keeps both probe points inside, so the collision fallback does
not conceal this difference. These are prospective RW-based expectations,
**not results of an executed test**.

This gap is recorded separately, not declared the proven cause of the
near-side/centre complaint. It needs the shared geometry state and its
copy/update/snapshot contract, rather than a replacement formula in one
weapon. The nearby mode-1 initializer also conflicts with the existing
`PORT-164 giant bird: a victim without contact points answers its own origin`
expectation; that foreign test is unchanged. The adjacent-work exception
fails its shared-mechanism condition (b); completing both point caches would
also conflict with the protected neighbour expectation (c).

## Validation and remaining work

No engine code, foreign test, pin or workflow changed. No `PORT-216` tests
have been added; no red/green claim or completed full-binary run is made.
The full Release / Godot-OFF `openbfme_tests` build was started with `-j1`;
its completion is not yet established. The current branch was rebased onto
origin/main `467bc033` before this report update. No review PR is offered.

Remaining: owner comparison of the actual ordinary-arrow impact in original
Ennorath (GondorArcher against HaradMumakil, side-on, no passengers), then
confirm the P-127 correction's scope. S-7387 records the separate shared
GeometryInfo work and the synthetic regression needed for it. Once a
RW-proven correction is identified, add the `PORT-216` lane and run its
red/green check on the full binary plus `PORT-436*` before submitting review.

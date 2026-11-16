# PORT-216 — stored projectile contact points on compound geometry

Scope: fork issue 506, dashboard task 192. The stored weapon-contact-point
correction and eight synthetic tests are implemented; full-runner validation
is still pending. This is **not** a completed port or a claim that the
owner's near-side visual report is fixed. The owner has already confirmed
the original-game near-side stop; that question is resolved.

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
| Neighbour preference 1 | RW 0xAD3340..0xAD3379 | Stored preferred point, clamped to maximum geometry height. The constructor RW 0x4504A4..0x4504AE and initializer RW 0xAD25E6..0xAD2607 also initialize/raise it; this independent preferred-point gap remains S-7387. |
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

**Not established:** a new nearest-surface aim or collision rule. The owner
has confirmed the near-side stop in original Ennorath. The correction below
restores the existing RW point's parse-time history; it does not depart from
those branches. Whether this accounts for the complete visible complaint
remains an in-game acceptance check, not a no-reader verdict.

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

Before the correction, `ObjectGeometry::shapesOf` replayed geometry rows
into final shapes, while `getBestContactPoint` mode 3 recomputed the
innermost point from their current height. `worldspaceBestContactPoint`
combined the object's own shapes with its original template's contact rows.
`Object::geometryTemplate`, `setGeometryInfoFrom`, active-shape/height
changes and their existing snapshot representation are additional consumers
that a faithful stored-state solution must cover.

The synthetic INI-order regression (not mod data): a box of height 40
with a later vertical offset 12 keeps the earlier innermost z, approximately
26.8; moving that offset before the height refresh yields approximately
34.84. The old port reconstructed both from final height 52. A large
enough box keeps both probe points inside, so the collision fallback does
not conceal this difference. These are the added test's RW-based expectations,
**not yet results of an executed test**.

This gap is recorded separately, not declared the proven cause of the
near-side/centre complaint. It needs the shared geometry state and its
copy/update/snapshot contract, rather than a replacement formula in one
weapon. The nearby mode-1 initializer also conflicts with the existing
`PORT-164 giant bird: a victim without contact points answers its own origin`
expectation; that foreign test is unchanged. The original scope stop treated this as new shared state (condition b).
The subsequent copy/xfer and template-event inspection below found that the
weapon point can be derived from existing state. The independent preferred
point still fails condition (c), the protected neighbour expectation.

## Validation and remaining work

- Engine correction: `cbd852fb`, `ObjectGeometry.cpp/.h`; existing parsed
  geometry cache extended, existing solver retained. No collision-manager,
  projectile KindOf, mod data, foreign tests or pins changed.
- Full-target lane and eight test cases: `8315c159`, `Port216.cmake`,
  `test_port_216.cpp`, the two PORT-216 list entries in `CMakeLists.txt`.
  GCC syntax checks passed for both implementation and tests; the mandatory
  pre-commit hook passed. A syntax check is not a doctest result.
- The earlier full build was explicitly stopped at 988/1782, then the full
  Release/Godot-OFF target was reconfigured with the new lane and resumed
  using `cmake --build engine/build -j1 --target openbfme_tests`.
  Current process: `port-216-full-tests-58ef34e2`. The latest wait returned
  running at 139/1082; no PORT-216/PORT-436 doctest result exists yet.
- The queued commands run the required `-tc="PORT-216*"` then `-tc="PORT-436*"`
  on **engine/build/openbfme_tests**, not a standalone test executable.
  The full-target red/green experiment (temporarily undo only the engine
  correction, retain the tests, rebuild, restore and rebuild) remains to do.
- Current validation is of shared RotWK behavior. The engine applies it to
  all editions under its existing edition policy; separate BFME2/RotWK
  installed-data and visual acceptance have not been performed here.
- No fork or upstream PR yet. Fetch/rebase latest main, full runner results,
  affected-neighbour selection, red/green, upstream dependency audit and
  the visible-result check remain before a review claim. No pins will be
  remeasured in this branch.

## Continuation after the owner's original-game check

The owner confirmed in this conversation that the ordinary arrow stops at
the near side in original Ennorath. This resolves the outstanding request
for a visual baseline; the previous sections are investigation checkpoints,
not the final acceptance report.

A further source check narrows the implementation boundary. The complete
INI geometry event stream already exists in `ThingTemplate::geometryEvents`
(including all three contact-row spellings). The immutable template cache
is invalidated on each added event, copied with inherited templates, and
`Object::geometryTemplate()` already identifies the geometry source through
geometry copies and save/load. Thus the weapon-mode stored point can be
reconstructed faithfully from existing data, without introducing runtime
simulation state or changing the collision manager, geometry-upgrade module,
or snapshot schema.

Additional RW instructions read for this decision:

- RW 0x4504B3..0x4504BD: the GeometryInfo constructor initializes all three
  innermost coordinates to zero. This is the state before a height/contact
  parser refresh, not an invented missing-data default.
- RW 0xAD4040..0xAD40A4: a Geometry type row resizes the shape list and
  refreshes bounds; it does not reset the contact records or refresh the
  innermost point.
- RW 0x463E45..0x463E62 (assignment entry RW 0x463DFC): contact records,
  preferred point and innermost point are copied with the shapes.
- RW 0xAD2C60..0xAD2C96 and RW 0xAD3520..0xAD35A5: runtime shape-height /
  active changes refresh bounds, not the stored innermost point. The mode-3
  reader subsequently performs its existing live-height clamps and probe
  fallback against the modified shapes.
- RW 0x403111..0x403155 is indeed the vector length used by the unusual
  world-origin test in Bezier update; that branch will not be rewritten as
  an unproven projectile-to-target distance test.

Implemented: the existing parsed-geometry cache now carries the contact
rows and parse-time innermost point, replays the existing event stream once,
and feeds the same contact-query implementation from it.
Worldspace queries use the object's geometry source, not a mixture of that
source's shapes and the original object's contact rows. The public
value-only shape/point query keeps its existing stateless contract for its
existing callers; production worldspace queries retain INI history.

The independent preferred-point initialization of mode 1 remains outside
this change: the existing PORT-164 no-row expectation would have to change.
S-7387 is narrowed to that remaining issue, not silently closed.
The near-side visual result remains an owner acceptance check; the code and
synthetic tests must establish the exact RW point first, not assert that an
internal aim point is a new nearest-surface algorithm.

## Added regression coverage (execution pending)

| Behavior | RW instructions | PORT-216 test coverage |
|---|---|---|
| Equal final shapes, different height/offset order | 0xAD2BA7, 0xAD1770, 0xAD3172 | INI-order case, with MONSTER, INFANTRY and STRUCTURE subcases; no KindOf-specific repair |
| Compound body point reaches an actual arrow and its FX/homing | 0x6CCAC7, 0x85EC51, 0x85F46A | NO_COLLIDE projectile, independently specified body/elevated shapes, ordinary-infantry neighbour and unchanged four aim-seed calls |
| A single contact row refreshes the cached default, not a two-point segment | 0xAD3DEF, 0xAD31AF | GeometryContactPoint, AttackContactPoint, SwoopContactPoint subcases |
| Inherited geometry preserves history; another height row refreshes it | 0x463E5A, 0xAD2BA7 | Parent, offset-only child, height-refresh grandchild |
| Runtime height/active changes do not refresh the cached point | 0xAD2C96, 0xAD35A5, 0xAD3283 | Growth, disable/re-enable, shrink with live-height clamp |
| Type/radius changes do not refresh the constructor/previous point | 0x4504B3, 0xAD4040, 0xAD330F | No-height sphere with lower clamp; type replacement after a height/offset sequence |
| Point outside current geometry still takes existing fallback | 0xAD329E..0xAD32EB | Late offset outside the probe, plus ignoreCollision neighbour |
| Copy uses one source for shapes, rows and point | 0x463E45..0x463E62 | Copies both with/without rows, mode 0 labels and mode 1 high row, runtime heights, hash and byte-identical save/load |

Production callers inspected: WeaponDelivery (mode 3), BezierProjectileBehavior
(mode 3), GiantBirdAIUpdate (mode 1), AIUpdateCombat/AIAttack (mode 0) and
ControlBarRadialMenu (mode 0). The value-only getBestContactPoint API is not
called by production code outside its implementation; its existing test
callers keep their stateless inputs. Changing copied contact rows can affect
all modes and belongs in neighbour/nightly validation. Mode 1's separate
constructor/high-water-point mismatch is explicitly retained in S-7387.

## Additional collision trace

The saved linear disassemblies of RW 0x85EA5F..0x85EBAC (back-off) and
0x85FDB7..0x85FE92 (handler) have now been read. A non-null collision object
at 0x85FDDF takes the back-off call 0x85FDEA; back-off repeatedly tests the
projectile's geometry against that object's geometry at 0x85EB67, moving
backwards along the prior path delta. The null-object branch at 0x85FE1B
handles ground FX/bounce/expiry instead. Neither is a hidden replacement of
the weapon aim by a nearest-surface point. These branches are unchanged.
The collision-manager NO_COLLIDE opt-out remains intact.

**Visible yes/no:** yes, the intended effect is visible in arrow flight and
impact placement, but the changed build has not been observed in-game.
Original Ennorath's near-side stop is confirmed by the owner. The exact
contribution of the corrected body-height aim to that visual stop remains
**INFERENCE**, not an assertion that the geometry query computes a surface
intersection. No user-facing review/finished status is claimed yet.

## Continuation: main, upstream and neighbour audit

Fetched fork main `1dfe2996`. Since the branch's base `467bc033`, the
ObjectGeometry/ThingTemplate/Object/projectile implementations used here
have no main changes. WeaponDelivery has unrelated contain/horde nugget
changes, not a second fix for this contact-point gap. No duplicate path
was introduced. Rebase remains required before PR submission.

Upstream `Open-BFME/openbfme-godot` main `942fd79e` was fetched and the
required files inspected with `git show upstream/main:<file>`:

- The shape cache, ordered GeometryEvent stream, geometry source and its
  snapshot transfer, CombatWorld, Bezier flightEnd and GameLogic snapshot
  APIs exist.
- `Object::setGeometryMaxHeight` does **not** exist anywhere in upstream
  engine/src. PORT-216's runtime-height and copied-height tests call it;
  that is a real missing dependency, not a test output or a no-reader RW
  conclusion. Upstream's ThingFactory also does not yet record the two
  AttackContactPoint/SwoopContactPoint aliases used by these tests.
- Therefore this complete code/test bundle is not eligible for immediate
  upstream submission under the task's dependency rule. Do not pull other
  ports into PORT-216 or drop their regression coverage just to make a PR.

The copy/snapshot regression additionally checks the mode-0 labelled query
and mode-1 high-row neighbour before and after load. The latter deliberately
uses a row above the initializer's point; it does not mask or change S-7387's
independent no-row mismatch or the protected PORT-164 test.

Actual `tools/test_select.py --base 467bc033` first line (no execution yet):

```text
8 changed files, no test map, the #include rule; FULL suite: engine/CMakeLists.txt: no test can be traced from this file
```

This is not waived or shortened to a filter. The mandatory narrow filters
and full-binary red/green come first; the selected full suite is still a
pre-landing/nightly requirement. The full-build configuration warns that
five MP3 tests lack the lame/mpg123 fixtures in this sandbox; no fixture or
pin expectation has been changed to hide that limitation.

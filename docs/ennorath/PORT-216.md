# PORT-216 — stored projectile contact points on compound geometry

Scope: fork issue 506, dashboard task 192. The stored weapon-contact-point
correction and eight synthetic tests are implemented; the full-target
PORT-216/PORT-436 and neighbour runs, including red/green, have passed. This is **not** a completed port or a claim that the
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
| Neighbour preference 1 | RW 0xAD3340..0xAD3379 | Stored preferred point, clamped to maximum geometry height. The constructor RW 0x4504A4..0x4504AE and initializer RW 0xAD25E6..0xAD2607 also initialize/raise it; this independent preferred-point gap remains S-7426. |
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

## Adjacent verified gap: stored GeometryInfo contact state (S-7426)

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
not conceal this difference. These RW-based expectations now pass in the full-target run; without the
engine correction the old recomputed value fails the regression.

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

Engine correction: `8b6a2a89`; full-target lane/tests: `e7b4c8a9`, strengthened
in `bcb5322c`. Required pre-PR rebase completed on fork main `a3a2e87a`.
The tested green build's provenance is `fe893aae29edbcc32da8571d9a0e2fa7d02fa6be`,
engine identity `8e92f8114fb47ac195dc6969b54c5b2a0fe06f782d5f96131533fb2befe9ab90`.
Later documentation-only commits do not change the tested engine sources.

The required command uses the full target, **not a standalone runner**:

```sh
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENBFME_BUILD_GODOT=OFF
cmake --build engine/build -j1 --target openbfme_tests
engine/build/openbfme_tests -tc="PORT-216*"
```

Actual restored/fixed result, exit 0:

```text
[doctest] test cases:   8 |   8 passed | 0 failed | 6774 skipped
[doctest] assertions: 318 | 318 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Red experiment: temporarily reverse only the entire behavioral correction
in ObjectGeometry.cpp, keep the new tests and CMake lane, rebuild the same
full target and run the same filter. The header delta is comments only;
it was left untouched. Actual result, exit 1:

```text
[doctest] test cases:   8 |   1 passed |  7 failed | 6774 skipped
[doctest] assertions: 285 | 264 passed | 21 failed |
[doctest] Status: FAILURE!
```

Representative real red assertions: INI-order case `34.84 == Approx(26.8)`;
compound projectile's launch, FX and homing `67 == Approx(37.52)`;
copied geometry kept the recipient's wrong contact rows. The engine file
was restored and the full target rebuilt before running green. A clean
`git diff` and the validator's result manifest confirm restoration.

PORT-436, exit 0:

```text
[doctest] test cases:  6 |  6 passed | 0 failed | 6776 skipped
[doctest] assertions: 98 | 98 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Neighbours (`PORT-164*,PORT-34*,LOCAL-41*,path2 geometry*`), exit 0:

```text
[doctest] test cases:  23 |  23 passed | 0 failed | 6759 skipped
[doctest] assertions: 464 | 464 passed | 0 failed |
[doctest] Status: SUCCESS!
```

The skipped counts above are cases outside the requested filters; all
eight synthetic PORT-216 cases ran. Installed game data is not required by
them. No foreign test, hash pin, workflow or mod data was edited.

### Selected FULL suite and implementation-only control

The initial selected FULL run on the same engine sources finished with
6645 cases: 6628 passed, 17 failed, 137 skipped; 1439634 assertions,
1439599 passed, 35 failed. This is **not a full-suite pass**.

To distinguish failures introduced by this correction from other failures,
the full-target implementation-only reversal was repeated at
`7c8683f09f5e63421edc72881f811af727b057e9`: keep all tests/CMake/header,
reverse ObjectGeometry.cpp, rebuild and run the same FULL selection; restore
the source, rebuild and repeat. Actual latest lines:

Without the correction, exit 1:

```text
[doctest] test cases:    6645 |    6621 passed | 24 failed | 137 skipped
[doctest] assertions: 1439612 | 1439556 passed | 56 failed |
[doctest] Status: FAILURE!
```

Restored correction, exit 1:

```text
[doctest] test cases:    6645 |    6628 passed | 17 failed | 137 skipped
[doctest] assertions: 1439633 | 1439598 passed | 35 failed |
[doctest] Status: FAILURE!
```

All 17 remaining failed case names, error lines and printed values match
between these two FULL runs. The seven additional failures without the
correction are precisely PORT-216 cases; restoration also reran the narrow
filters: PORT-216 8/8 (318 assertions), PORT-436 6/6 (98), neighbours
23/23 (464). `git diff --exit-code` confirms restored engine sources.
The one-assertion difference between successful assertion counts in the
initial and restored FULL run is recorded, not normalised into a pin.
Ignored evidence: `workspace/port216/full-control/{red,green,port216,port436,neighbours}.log`
and `result.json` (`sources_restored` and `control_complete` are true).

Five MP3 cases lack the generated lame/mpg123 reference fixtures. Those
five plus cbhunt, heroring1, move SPEED and shroud have the same failures
already recorded in `TEST_BASELINE.md`. The six hero-facing cases,
synthetic HUD case and wall-termination case are **not** waived by that
baseline. Reversing this implementation reproduces them, but is not a
clean-main build: before landing they still need clean-main verification
and the owner's failure triage. No foreign test, pin, baseline, workflow
or fixture expectation was changed. PORT-169's logged allowed-to-fail
case is not part of doctest's 17 failures.

| Failed case in both FULL runs | First actual diagnostic |
|---|---|
| audio decode: MP3 fixtures decode to the mpg123 golden numbers | `engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!;   values: REQUIRE( false )` |
| audio decode: MP3 streaming equals the whole-file decode for any chunk size, rewind restarts | `engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!;   values: REQUIRE( false )` |
| audio decode: MP3 gapless handling, ID3 tags and frame walking | `engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!;   values: REQUIRE( false )` |
| audio decode: MP3 container defects are reported and decided | `engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!;   values: REQUIRE( false )` |
| audio decode: corrupted MP3 fixtures decode or throw AudioDecodeError, nothing else | `engine/tests/test_audio_decode.cpp:830: FATAL ERROR: REQUIRE( problem.empty() ) is NOT correct!;   values: REQUIRE( false )` |
| cbhunt: ActionManager::canDoSpecialPowerAtObject (RW 0x82D925): the generic tests, the yes types, and the unported per-type tests counted as S-7140 | `engine/tests/test_cbhunt.cpp:329: ERROR: CHECK( unported ) is NOT correct!;   values: CHECK( false )` |
| hero facing: an in-range directional ability faces front or behind targets at the locomotor rate before unpacking | `engine/tests/test_hero_facing_execution.cpp:170: ERROR: CHECK( world.spells.casts() == 1 ) is NOT correct!;   values: CHECK( 0 == 1 )` |
| hero facing: a moving object target updates the face command before the directional ability fires | `engine/tests/test_hero_facing_execution.cpp:208: FATAL ERROR: REQUIRE( hero->getAIUpdateInterface()->currentStateId() == (unsigned)AI_FACE_OBJECT ) is NOT correct!;   values: REQUIRE( 0 == 36 )` |
| hero facing: an out-of-range target is approached before the face command and weapon trigger | `engine/tests/test_hero_facing_execution.cpp:228: ERROR: CHECK( hero->getAIUpdateInterface()->currentStateId() == (unsigned)AI_ABILITY_APPROACH ) is NOT correct!;   values: CHECK( 0 == 63 )` |
| hero facing: a player move cancels a pending turn without firing or consuming the power | `engine/tests/test_hero_facing_execution.cpp:246: FATAL ERROR: REQUIRE( hero->getAIUpdateInterface()->currentStateId() == (unsigned)AI_FACE_OBJECT ) is NOT correct!;   values: REQUIRE( 0 == 36 )` |
| hero facing: a target removed during the turn obeys MustFinishAbility without stalling | `engine/tests/test_hero_facing_execution.cpp:273: FATAL ERROR: REQUIRE( hero->getAIUpdateInterface()->currentStateId() == (unsigned)AI_FACE_OBJECT ) is NOT correct!;   values: REQUIRE( 0 == 36 )` |
| hero facing: snapshots of face-object and face-position states restore identical frame-by-frame CRCs | `engine/tests/test_hero_facing_execution.cpp:309: FATAL ERROR: REQUIRE( hero->getAIUpdateInterface()->currentStateId() == (unsigned)(atPosition ? AI_FACE_POSITION : AI_FACE_OBJECT) ) is NOT correct!;   values: REQUIRE( 0 == 36 )` |
| heroring1: the grid carries an alpha decal after the merge decals: its own list in each cell, its turn and its colour | `engine/tests/test_heroring1_decals.cpp:181: ERROR: CHECK( SelectionDecalShaderMode(g) == 0 ) is NOT correct!;   values: CHECK( 4 == 0 )` |
| synthetic HUD: hiding one porter preserves other selections and an unselectable castle | `engine/tests/SyntheticHudTestUtil.h:122: ERROR: CHECK( actual == ids ) is NOT correct!;   values: CHECK( {?} == {?} )` |
| move SPEED: a formation move caps every member's locomotor at the slowest member's max speed (RW 0x77BB58 -> RW 0x77005F(0)) | `engine/tests/test_move_speed.cpp:425: ERROR: CHECK( w.aiOf(fast)->curLocomotor()->temporarySpeedCap() == 6.0f ) is NOT correct!;   values: CHECK( -1 == 6 )` |
| shroud objects: a geometry row that does not parse is an error naming the template, never a default coverage (PLAN rule 10) | `engine/tests/test_shroud.cpp:781: ERROR: test case THREW exception: Bogus: Geometry 'BOGUS' is not SPHERE, CYLINDER or BOX; ` |
| wall span terminates at terrain with a hub and cliff cap | `engine/tests/test_wall_termination.cpp:35: FATAL ERROR: REQUIRE( plan.tiles.size() == 8 ) is NOT correct!;   values: REQUIRE( 23 == 8 )` |

The FULL selection uses the repository tool's declared out-of-fork-scope
exclusions; installed-data cases also print SKIP without ROTWK_INSTALL /
BFME2_INSTALL. The 137 doctest skipped count is not a claim that the
installed-data cases ran. Separate MSVC/BFME2/RotWK installed-data and
changed-game visual acceptance are not claimed.

### Second pre-PR integration baseline

Rebased cleanly onto fetched fork main `a91f3da5`. Replayed implementation
is `6ce3a764`, full-target lane/tests `b27b34dc`, copy-neighbour guards
`526c2667`. ObjectGeometry/Object's relevant paths have no main changes;
ThingTemplate/ThingFactory changed their unrelated ThreatBreakdown handling,
not the contact-point event/cache path. No duplicate fix was introduced.
The completed results above remain explicitly the tested `a3a2e87a`-based
engine, not results for this second rebase. A new full-target build and narrow/FULL checks have been started for
`95bfb8dd8f8446a917e4400c7f4d283c736026e3` (engine identity
`d0f2bade5d6ddc4795d0133f618804c26b1eade338e092a410d8d8b0aca82a7b`).
Process `port-216-latest-main-full-tests-467c6ce9` is building with `-j1`
then runs PORT-216, PORT-436, neighbours and the declared FULL selection.
Evidence is written under ignored `workspace/port216/rebase-a91/`.
No post-rebase result is claimed while it runs; visual acceptance also
remains pending.

Upstream `16474d71` was checked with `git show upstream/main:<file>` for
Object.h, Object.cpp, Common/Thing/ThingFactory.cpp and ThingTemplate.h.
The event stream/cache and geometry source exist; setGeometryMaxHeight
and the AttackContactPoint/SwoopContactPoint aliases are still absent.
This complete regression bundle therefore remains ineligible for an
upstream PR; no dependency ports are pulled into this task.

The earlier stop number S-7387 is occupied on the new main by Port 228.
`tools/fragments.py next` allocated S-7426 for this branch's preferred-point
remainder. Only our fragment/references were renamed; the aggregate stop
and Port 228 were not edited.

### Serial clean-main-engine control queued

Fork PR: https://github.com/Gleb302596555/openbfme-godot/pull/618 (draft).
The owner requested continuation; the existing 95bfb8dd build is still
running, so it was not restarted or rebased again.

A workspace-only validation process is queued behind that exact build:
`port-216-main-baseline-and-red-g-f2c27f3c`, script
`workspace/port216/validate-main-a91.py`. It waits for the original PID and
its completed result manifest, requires successful nonempty narrow runs,
refuses another Ninja, dirty sources/index or a changed engine, and takes
byte-for-byte backups of the five owned engine files. The current branch
is never switched and no other branch is created.

It then temporarily removes **all** five PORT-216 engine deltas (cpp,
comment-only header, test, lane and CMake wiring), not just the cpp fix.
Before building, `git diff --exit-code a91f3da5 -- engine` must succeed.
Thus the control uses exactly main's tracked engine sources, while the
surrounding checkout remains on this Arena branch; that distinction and
both engine tree IDs are recorded in the manifest. It builds the full
`openbfme_tests` target with Release/Godot OFF/`-j1` and runs the same FULL
selection and scope exclusions. No foreign test or pin is edited.

Next it restores this branch's header/tests/lane, keeps only cpp unfixed,
rebuilds and runs PORT-216 red. Finally it restores the implementation,
rebuilds and runs PORT-216, PORT-436, neighbours and FULL. Backups are also
restored on errors and catchable signals. Full-main versus restored-branch
failed case names and error/value lines are compared; a mismatch is not
waived. Logs and the completed manifest will be in ignored
`workspace/port216/main-a91-control/`. This is a queued experiment, **not**
a reported clean-main pass, red/green pass or completed restoration.

The eight previously unbaselined failures are outside this task's
ObjectGeometry class/function (hero-facing ActionManager/AI, HUD selection
and wall planning). Their correction would fail adjacent-work conditions
(a)/(b); no unrelated repair or new test expectation is smuggled into
PORT-216. Their exact clean-main outcome still has to be recorded before
landing, independently of the visual P-127 acceptance.

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
S-7426 is narrowed to that remaining issue, not silently closed.
The near-side visual result remains an owner acceptance check; the code and
synthetic tests must establish the exact RW point first, not assert that an
internal aim point is a new nearest-surface algorithm.

## Added regression coverage (full-target run passed)

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
constructor/high-water-point mismatch is explicitly retained in S-7426.

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
intersection. No user-facing review/finished status is claimed yet; the fork PR must remain a draft until the remaining acceptance checks are resolved.

## Continuation: main, upstream and neighbour audit

Fetched fork main `1dfe2996`. Since the branch's base `467bc033`, the
ObjectGeometry/ThingTemplate/Object/projectile implementations used here
have no main changes. WeaponDelivery has unrelated contain/horde nugget
changes, not a second fix for this contact-point gap. No duplicate path
was introduced. This was an earlier checkpoint; the two completed pre-PR
rebases and their exact bases are recorded in Validation above.

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
uses a row above the initializer's point; it does not mask or change S-7426's
independent no-row mismatch or the protected PORT-164 test.

Actual `tools/test_select.py --base a3a2e87a` first line (FULL run completed):

```text
8 changed files, no test map, the #include rule; FULL suite: engine/CMakeLists.txt: no test can be traced from this file
```

This is not waived or shortened to a filter. The mandatory narrow filters
and full-binary red/green have passed; the selected full suite was then
completed as a separate pre-landing check (17 failures; control above). The full-build configuration warns that
five MP3 tests lack the lame/mpg123 fixtures in this sandbox; no fixture or
pin expectation has been changed to hide that limitation.

## Red/green safeguards

The workspace-only validator waited for the successful full baseline,
refused concurrent Ninja builds and uncommitted source changes, backed up
the engine files, and reversed only the implementation delta. On error or
signal it restores the modified source. It restored before interpreting
the red result, rebuilt the full target, and checked the nonempty green and
neighbour summaries. Its completed manifest records the actual exits
(red 1; green/PORT-436/neighbours 0) and `sources_restored: true`.
The comment-only header was compared but not rewritten, avoiding an
unnecessary rebuild of unrelated translation units without weakening proof.

## Additional inspected RW caller

DONOR BFME2 RVA 0x0023D3AF (`ObjectRva0023D3AF.cpp`) describes transform
history. Its actual RW caller at 0x85EF50 and callee 0x6260E1..0x626196
were then read: it copies the transform/previous position and records the
frame and flags, not a projectile collision registration or surface query.
The fire tail 0x85EF34..0x85EF67 is update, partition refresh, transform
record, update, partition refresh, return. The remaining update epilogue
0x85F84C..0x85F85A restores registers/exception state and returns.
This closes those previously undisplayed tails; it is not a global
no-reader conclusion about the collision manager. No collision behavior
was changed on the strength of this check.

## Cheat Mode reproduction for the changed build (not yet executed)

Visible change: **yes**, the intended effect is the ordinary arrow's path
and impact placement on a compound body. The corrected numerical aim is
RW-backed; the exact near-side visual outcome is still **INFERENCE** until
the changed game is observed. The original-game near-side observation has
already been supplied by the owner and is not being requested again.

1. Load Ennorath **1.9.8.3**, start a skirmish with one computer opponent,
   and enable **Rules → Cheat Mode (OpenBFME rule)** before starting.
2. **Esc → CHEAT MODE → Game**: pause the computer player's AI. Use flat,
   open terrain away from buildings. The command names/panel operations
   below are from `docs/CHEAT_MODE.md`, not invented retail cheats.
3. **Units**, faction **Evilmen**: spawn one **HaradMumakil** for yourself.
   Select it, then **Selected → Freeze**, **Hold fire**, and **Give to** the
   opposing player. Freeze holds the carrier still; it is not a command to
   empty its contain or suppress every passenger.
4. **Units**, faction **Men**: spawn **GondorArcherHorde** for yourself.
   Leave the archers unupgraded (no fire arrows), place them within bow range
   alongside the mumak rather than in front of it, then issue an ordinary
   attack-object order on **HaradMumakil**. Do not use bombard/force-fire.
   Optionally freeze the positioned archer horde; the documented freeze
   command permits firing while holding position.
5. View from the flank at body height, record a few volleys, then repeat
   from the opposite flank. Inspect body-directed shots: the changed common
   aim should use the parse-time body-level point, not the point reconstructed
   from the elevated shape's final offset. If arrows still visibly traverse
   the body to its centre, the visible P-127 acceptance has **not** passed,
   even if the synthetic tests are green.
6. Neighbours: repeat an ordinary shot at an opposing **GondorArcherHorde**,
   and at an opposing **GondorBarracks** spawned from **Buildings**. Normal
   infantry and the already-ported structure-contact path must not regress;
   the full-runner PORT-216/PORT-436 checks remain the numerical guards.

**Fresh-spawn caveat:** the actual HaradMumakil inherits an initial passenger
payload. The inspected `HaradMumakilCommandSetMod` has no evacuation command;
therefore this procedure does not invent an "Unload" button or silently
call a fresh spawn empty. Both actual GondorArcher bow variants permit
passenger retargeting (`weapon.ini` and `includes/weapon.inc`), so passenger-
directed arrows must not be conflated with body-directed ones. Those rules
and mod data are unchanged. This default-payload smoke test is distinct
from the owner's already-confirmed empty-mumak baseline; the synthetic
projectile test isolates the body without installing or modifying mod data.

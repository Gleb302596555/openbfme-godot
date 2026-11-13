# Port 148 — Drogoth does not attack a fortress while flying (P-59)

Status: engine fix implemented; narrow tests green without game files. Awaiting the owner's in-game check and the night run.
The earlier sections are the chronological research log; **Resolution (Port 148)** is the final state.

## Sources

- RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, read with `tools/mod/rw.py dis` (caveat S-001).
- DONOR BFME2 1.06: `Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/AIGiantBirdSwoopStateOnEnter.cpp`, `AIUpdateInterfaceAttackObjectCommands.cpp`.
- Ennorath INI 1.9.8.2: `Drogoth`, `WildDrogothnew`, fell beasts and eagles. The active `Drogoth` WeaponSet uses `NazgulPlowAttack` / `NazgulClawAttack`; `WildDrogothnew` uses `DrogorhPlowAttack` / `DrogothClawAttack`. No mod values are copied here.

## RW chain read so far

- RW 0x76D995 `GiantBirdAIUpdate::aiDoCommand`: commands 0xB/0xC/0x39 go to RW 0x76A139 (attack-object swoop); 0xE attack-position is ignored; 0xF attack-move goes to RW 0x76A09D. While `+0x5FC` attacking, the command is deferred through RW 0x75307D.
- RW 0x76DB18 `AIGiantBirdSwoopState::onEnter`: clears flight bits 3..5 and victim; a PRODUCED_AT_HELIPAD goal is rejected; horde goals are adjusted through RW 0x68D30E; RW 0x76DCB5 calls Object::chooseBestWeaponForTarget(victim, 2, CMD_FROM_AI) through RW 0x68B619 -> RW 0x6C8A4E. No current weapon -> failure. Grab/machine/clip bits are set at RW 0x76DCD1..0x76DD44; the contact point is asked at RW 0x76DDD9 / 0x76DE1B / 0x76DE4C; the path masks are passed at RW 0x76DECD..0x76DEF5.
- RW 0x76B668 `AIGiantBirdAttackState::onEnter`: sets attacking; clip attacks wait for update; the victim contact point is asked at RW 0x76B70C / 0x76B73F; the gate at RW 0x76B744..0x76B794 is `max(speed*3, 8.5)` squared against distance to that contact point, not the weapon's AttackRange. The ordinary shot is RW 0x76B8BD -> 0x69036C (firing condition) -> RW 0x76B8CA -> 0x69200A (fire current weapon at the victim).
- RW 0x69200A: refuses a missing current weapon or a non-ready weapon (RW 0x6CDCE7), then fires through RW 0x6CF3AE / 0x6CF328; the bird wrapper itself has no AttackRange gate.
- RW 0x6C85CA `getVictimAntiMask`: a ground STRUCTURE is `GROUND | STRUCTURE` (0x102), other ground targets `GROUND` (0x2); airborne targets use the airborne bits. RW 0x6C8A4E scores OnlyAgainst / PreferredAgainst / OnlyInCondition and the ready/backup best.

## Not yet established (superseded by Resolution below)

At this stage the first differing port step was not yet proven. The port's bird attack gate already matches the RW speed/contact-point shape above; the remaining comparison is weapon choice / applicability against a structure, contact-point and path arrival, and the synthetic reproduction. No INFERENCE was being ported yet.


## Update (synthetic reproduction attempt)

Three `PORT-148*` tests were added (`engine/tests/test_port_148.cpp`, wired via
`engine/cmake/Port148.cmake`): a no-PRIMARY / `ReadyStatusSharedWithinSet=Yes`
GIANT_BIRD flier with a SECONDARY plow (`PreferredAgainst STRUCTURE ...`) and a
TERTIARY monster-only claw, ordered to attack (a) an enemy STRUCTURE and (b) an
enemy INFANTRY unit, plus a direct `chooseBestWeaponForTarget` check.

Result on unmodified `main` (`8b8af54`): **all three pass** once the fixture is
given ~20 logic frames for the weapons to leave their initial reload state.
`chooseBestWeaponForTarget` selects SECONDARY for both victims; the ordered
attack fires the plow (slot 1) and reduces health in both cases. A sweep over
fortress-like victims (CASTLE_KEEP / COMMANDCENTER / BASE_FOUNDATION, Swoop
contact points at Z:75 and Z:180, StructureBody, distances 100/300, preferred
heights 120/200) also attacks in every case.

Consequence: the "empty PRIMARY slot + shared-ready" hypothesis is **disproven**
for the port's ordered-attack path. The first differing step vs RW is still not
proven. No engine behavior was changed. Next: model the real Ennorath fortress
(`CastleBehavior` unpacked castle, multiple plot geometries) and the
auto-acquire (`ATTACK_BUILDINGS`) path, and re-check the in-game command path,
before any fix.


## Update (exact locomotor / fortress geometry scratch)

A scratch rig (not committed as a test) replaced the synthetic locomotor with the
active Ennorath `FellBeastLocomotor` rows (`TurnTime=3500`,
`SlowTurnRadius=32`, `FastTurnRadius=48`, `Acceleration=400`,
`MinTurnSpeed=30%`, `PreferredHeight=121`, `PreferredAttackHeight=5`,
`CloseEnoughDist=2`, `AirborneTargetingHeight=25`), added the Drogoth
`FollowThrough*` rows, made the claw `MeleeWeapon=Yes` / `AutoReloadsClip=Yes`,
and gave the plow the real `DelayTime=1` and `DamageScalar=70% NONE +STRUCTURE`.
The victim was a keep-like STRUCTURE with four corner tower geometries and a
`Swoop` contact point at Z:180.

Result on unmodified `c9304cd`: the ordered attack still chooses SECONDARY,
reaches the attack state and fires; after the delayed damage frame the keep goes
1000 -> 993. Adding a manual ring of 12 WALL_SEGMENT boxes (radius 70, height 40)
and 8 BASE_FOUNDATION plots (radius 45, height 1) around the keep does not stop
the strike either (1000 -> 993 at distances 100/180/300/500). The lockstep
`MSG_DO_ATTACK_OBJECT` path with a selected bird behaves the same.

RW reads added: `canAttackFromHere` RW 0x6FF7FA matches the port's range / melee
layer shape (no GIANT_BIRD exemption); aerial separation RW 0x777F9F builds a
geometry filter and calls the partition range query, while the port's
`AerialPathfinder::separation` still uses 2D footprints (S-4041 / S-1020). The
3D overlap helper RW 0xAD2CE0 is already ported as
`NotifyCrushModules::geometriesOverlap`. This is a candidate first divergence for
a real unpacked castle, but the scratch `CastleBehavior::unpack` rig crashed
before an attack and is not yet evidence. No engine behavior was changed.

## Resolution (Port 148)

The first differing port step is `AerialPathfinder::separation`, not weapon choice,
anti-mask, `canAttackFromHere`, the bird state machine, or the fortress KindOf. A
`CastleBehavior::unpack` keep shares its position with the `CASTLE_CENTER` shell.
The 2D footprint filter ignored the sampled Z, so at the labelled `Swoop` contact
point Z:180 it still reported a collision with the overlapping 50/100-high shapes
and pushed the flyer away. `AIGiantBirdSwoopState` stayed in 0x3F2 and never
reached `AIGiantBirdAttackState` 0x3F4; the keep took no damage.

RW 0x777F9F builds `PartitionFilterWouldCollide` (RW 0x67C5F3, allow RW 0x66139C)
from the flyer's own GeometryInfo and the sampled position, then calls RW 0xAD2CE0
on the two 3D shape lists. The loop skips KindOf INFANTRY / CAVALRY / HORDE and
uses the IMMOBILE bit for the mobile distance check (RW 0x778053..0x778075).
RW 0xAD2CE0 is already ported as `NotifyCrushModules::geometriesOverlap`; Port 148
uses it through `ConstructionSite::GeometryFilter` in the one shared separation
function. No castle-specific branch, no second attack path, no new simulation
state, no xfer change.

RW facts: RW 0x777F9F / 0x67C5F3 / 0x66139C / 0xAD2CE0 / 0x778053..0x778075.
Donor facts: DONOR BFME2 0x00375C28 (`AerialPathfinder.cpp`, `rva00375C28`) for
the separation loop and its KindOf gates; the synthetic INI is not a retail data
claim.

Tests (`engine/tests/test_port_148.cpp`, `PORT-148*`): weapon choice, ordered
STRUCTURE attack, ordered UNIT attack, the unpacked CASTLE_KEEP attack, and a
direct `AerialPathfinder::separation` probe at Z:80 above the 50-high shell. With
only the two engine files reverted to `903dde7` (the tests unchanged), the two
new cases are red: 3/5 cases pass, 5 assertions fail (`strike=0 state=1010
hp=1000`; `clear=false worst=125.826 push.z=251.651`). With the filter restored,
5/5 cases and 104/104 assertions pass (`strike=1 state=1013 hp=990`;
`clear=true worst=0 push=0,0,0`). The STRUCTURE and UNIT neighbours hold in both
runs. No game or mod files are needed. Full `openbfme_tests` has the same
pre-existing baseline failures (audio decode without lame/mpg123, bfme1 fields,
cbhunt, heroring1, move SPEED, shroud objects); `fly1 retail*` stop-text pins
still pass.

Stops: S-4041 is narrowed to the x87 / CRT parity and now points at the new
project row S-7326 for the unported `AerialPathfindNoFlyZone` map block. The next
free stop id in `origin/main` at the time of the PR is S-7326 (the highest present
id there is S-7325); the 3D shape overlap is no longer an unported part. S-1020
already records RW 0xAD2CE0 as ported. No INFERENCE was added.

Affected Ennorath objects: every template with `GiantBirdAIUpdate` /
`FellBeastLocomotor` / `GIANT_BIRD` uses the shared filter: `Drogoth`,
`WildDrogothnew`, `SpellBookDragonStrikeDragon`, `EvilbeastsHeuschrecken`, the
Mordor / Angmar fell beasts and Witch-king / Khamul mounted templates, Wild dragons
and bats, Gwaihir / eagle summons, Radagast's eagle, Crebain, and the cinematic
`CINE_GrnDrgn_Flying`. Ground-only `DrogothGround` uses `AIUpdateInterface`, not
`GiantBirdAIUpdate`, and is unchanged.

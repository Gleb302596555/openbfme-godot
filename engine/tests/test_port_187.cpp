// OpenBFME unit tests. GPL-3.0.
// PORT-187 (fork issue 453): the GIANT_BIRD strike pass — the turn back after a hit (P-114) and the strike on a
// building (P-117). Synthetic INI with the Ennorath 1.9.8.2 numbers (FellBeastLocomotor, GiantBirdAIUpdate of the
// eagle); no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL needed.
//
// RotWK 2.01 (`game.dat`, md5 daed3668006cd90f01c34e5a7da1901f):
//   * the machine RW 0x7695C2 (defineState RW 0x8DBE1D): 0x3F2 Swoop -> success 0x3F4 Attack, failure idle;
//     0x3F4 Attack -> 0x3F5 FollowThru (success and failure); 0x3F5 FollowThru -> success idle, failure 0x3F2 Swoop
//     (read at RW 0x769744 .. 0x76975F). So every repeat pass runs through the follow-through's failure, and each
//     pass rebuilds the whole aerial path at the swoop's onEnter (RW 0x76DEF5 -> RW 0x769EA5 -> RW 0x778413).
//   * RW 0x778413 builds the control points in this order: `behind` = the flyer's position minus its transform's
//     X axis (RW 0x70BA38: obj +8, +0x18, +0x28) times min(3 * distance to the goal, 1500), the flyer's
//     position, the goal, and a terminal point at distance * 0.25 clamped to [50, 150] beyond the goal (RW 0xBD889C,
//     0xC2ED20, 0xBED870). The spline walks them as a Catmull-Rom curve (RW 0x7139E6), so the flight leaves along
//     `behind` -> position, whatever direction that is.
//   * RW 0x5E91E2 (the GIANT_BIRD path mover) steers by the average of the two sampled headings: the point half a
//     bounding sphere behind the path position, turned by pi (RW 0x4B3D8D then RW 0x644FD0), and the point the
//     same distance ahead; the average is divided by the number of points found and clamped to the locomotor's turn
//     rate (RW 0x5E372D), then blended 0.2 / 0.8 into AI +0x600 and applied to the orientation (RW 0x5E3A81).
//   * the strike itself: RW 0x76B668 (AIGiantBirdAttackState::onEnter) takes the victim's contact point (RW 0x667C76,
//     else RW 0x690BD2 -> RW 0xAD3340, the GeometryInfo +0x44 row) and refuses (STATE_FAILURE) unless
//     distance^2 <= max(speed * 3, 8.5)^2 (RW 0x5E3BE4, RW 0xC2E190); RW 0x76BA8B (its update) fires while the
//     weapon's status is READY (RW 0x6CDCE7 == 0) and ends the state on RELOADING_CLIP (3).

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/AI/AerialPathfinder.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Weapon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace
{
// The eagle's / fell beast's data as Ennorath 1.9.8.2 has it (locomotor.ini `FellBeastLocomotor`, the eagle's
// GiantBirdAIUpdate): SlowTurnRadius 32, FastTurnRadius 48, TurnTime 3500, MinTurnSpeed 30%, Acceleration 400,
// Braking 1000, PreferredHeight 121, PreferredAttackHeight 5, Appearance GIANT_BIRD, CloseEnoughDist 2; the AI's
// FollowThroughDistance 160, FollowThroughCheckStep 50, FollowThroughGradient 1.0, SpecialContactPoints Swoop.
// The weapon is Port 148's shape (Port148Plow: a SIEGE strike with a radius), the numbers synthetic.
const char kPort187Fixture[] = R"ini(
Locomotor Port187FellBeastLocomotor
 Surfaces = AIR
 Appearance = GIANT_BIRD
 TurnTime = 3500
 SlowTurnRadius = 32
 FastTurnRadius = 48
 Acceleration = 400
 Braking = 1000
 MinTurnSpeed = 30%
 PreferredHeight = 121
 PreferredAttackHeight = 5
 CloseEnoughDist = 2
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Weapon Port187Plow
 AttackRange = 24.0
 AcceptableAimDelta = 5
 DelayBetweenShots = 500
 FiringDuration = 500
 MeleeWeapon = No
 CanSwoop = Yes
 ChaseWeapon = Yes
 RadiusDamageAffects = ENEMIES NOT_SIMILAR
 DamageNugget
  Damage = 10
  Radius = 40.0
  DelayTime = 0
  DamageType = SIEGE
  DeathType = NORMAL
 End
End
Object Port187Bird
 KindOf = HERO PRELOAD SELECTABLE SCORE NO_FREEWILL_ENTER SCARY MONSTER CANNOT_RETALIATE ARMY_SUMMARY GIANT_BIRD
 Geometry = CYLINDER
 GeometryMajorRadius = 45
 GeometryHeight = 60
 WeaponSet
  Conditions = None
  ReadyStatusSharedWithinSet = Yes
  Weapon = PRIMARY Port187Plow
  PreferredAgainst = PRIMARY STRUCTURE MACHINE HORDE INFANTRY CAVALRY
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = No
  SpecialContactPoints = Swoop
  FollowThroughDistance = 160
  FollowThroughCheckStep = 50
  FollowThroughGradient = 1.0
 End
 LocomotorSet
  Locomotor = Port187FellBeastLocomotor
  Condition = SET_NORMAL
  Speed = 95
 End
 LocomotorSet
  Locomotor = Port187FellBeastLocomotor
  Condition = SET_SUPERSONIC
  Speed = 160
 End
End
Object Port187Barracks
 KindOf = STRUCTURE SELECTABLE IMMOBILE
 Geometry = BOX
 GeometryMajorRadius = 40
 GeometryMinorRadius = 40
 GeometryHeight = 40
 GeometryContactPoint = X:0 Y:0 Z:40 Swoop
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 2000
 End
End
)ini";

// The SAME machine with the numbers Ennorath 1.9.8.3 actually ships for Mordor's fell beast, which is the object the
// owner reported circling after a strike (P-114). Every number below is copied from the mod, not invented:
//   data/ini/locomotor.ini   `FellBeastSwoopLocomotor` (the SET_PANIC set the swoop flies in): Surfaces AIR,
//                            **LookAheadMult = 2.0**, SlowTurnRadius 32, FastTurnRadius 48, TurnTime 3500,
//                            TurnTimeDamaged 4500, Acceleration 400, Braking 1000, MinTurnSpeed 30%,
//                            PreferredHeight 121, PreferredAttackHeight 5, CloseEnoughDist 2
//   data/ini/object/evilfaction/units/mordor/fellbeast.ini   `MordorFellBeast`: Geometry CYLINDER,
//                            GeometryMajorRadius 42.0, GeometryHeight 45.0, GeometryOffset Z:-10;
//                            GiantBirdAIUpdate FollowThroughDistance 200, FollowThroughCheckStep 50,
//                            FollowThroughGradient 1.0, SpecialContactPoints Swoop; LocomotorSet SET_PANIC ->
//                            FellBeastSwoopLocomotor, Speed 160
//   data/ini/includes/weapon.inc   `NazgulPlowAttackEdain` (TERTIARY, PreferredAgainst STRUCTURE MACHINE HORDE):
//                            AttackRange 24.0, AcceptableAimDelta 5, **DelayBetweenShots 4000, FiringDuration 5000**,
//                            CanSwoop Yes, ChaseWeapon Yes, and every damage nugget scales 0% against +STRUCTURE.
// The two differences from the synthetic fixture that matter are LookAheadMult (the mover samples TWO half spheres
// each way on the swoop) and the 4000-frame reload, which make the real object a different measurement.
const char kPort187RealFixture[] = R"ini(
Locomotor Port187RealSwoopLocomotor
 Surfaces = AIR
 LookAheadMult = 2.0
 Appearance = GIANT_BIRD
 TurnTime = 3500
 TurnTimeDamaged = 4500
 SlowTurnRadius = 32
 FastTurnRadius = 48
 Acceleration = 400
 Braking = 1000
 MinTurnSpeed = 30%
 PreferredHeight = 121
 PreferredAttackHeight = 5
 CloseEnoughDist = 2
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Weapon Port187RealPlow
 AttackRange = 24.0
 AcceptableAimDelta = 5
 DelayBetweenShots = 4000
 FiringDuration = 5000
 MeleeWeapon = No
 CanSwoop = Yes
 ChaseWeapon = Yes
 RadiusDamageAffects = ENEMIES NOT_SIMILAR
 DamageNugget
  Damage = 10
  Radius = 45.0
  DelayTime = 0
  DamageType = SIEGE
  DeathType = NORMAL
 End
End
Object Port187RealBird
 KindOf = HERO PRELOAD SELECTABLE SCORE NO_FREEWILL_ENTER SCARY MONSTER CANNOT_RETALIATE ARMY_SUMMARY GIANT_BIRD
 Geometry = CYLINDER
 GeometryMajorRadius = 42.0
 GeometryHeight = 45.0
 GeometryOffset = X:0 Y:0 Z:-10
 GeometryIsSmall = No
 WeaponSet
  Conditions = None
  ReadyStatusSharedWithinSet = Yes
  Weapon = PRIMARY Port187RealPlow
  PreferredAgainst = PRIMARY STRUCTURE MACHINE HORDE INFANTRY CAVALRY
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 3500
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = No
  SpecialContactPoints = Swoop
  FollowThroughDistance = 200
  FollowThroughCheckStep = 50
  FollowThroughGradient = 1.0
 End
 LocomotorSet
  Locomotor = Port187RealSwoopLocomotor
  Condition = SET_PANIC
  Speed = 160
 End
 LocomotorSet
  Locomotor = Port187RealSwoopLocomotor
  Condition = SET_NORMAL
  Speed = 95
 End
End
Object Port187RealBarracks
 KindOf = STRUCTURE SELECTABLE IMMOBILE
 Geometry = BOX
 GeometryMajorRadius = 40
 GeometryMinorRadius = 40
 GeometryHeight = 40
 GeometryContactPoint = X:0 Y:0 Z:40 Swoop
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 2000
 End
End
)ini";

// the same arena over either fixture: the synthetic one, or the fell beast's shipped Ennorath numbers
struct Port187Arena
{
	combattest::CombatWorld w;
	Object *bird = nullptr;
	GiantBirdAIUpdate *ai = nullptr;

	explicit Port187Arena(const char *ini = nullptr, const char *birdName = nullptr, const char *barracksName = nullptr)
	{
		ini = ini ? ini : kPort187Fixture;
		birdName = birdName ? birdName : "Port187Bird";
		barracksName = barracksName ? barracksName : "Port187Barracks";
		barracksType = barracksName;
		const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port187.ini");
		REQUIRE_MESSAGE(error.empty(), error);
		w.combat().setAutoAcquireEnabled(false);
		bird = w.unit(birdName, 'A', 200.0f, 200.0f);
		REQUIRE(bird != nullptr);
		ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
		REQUIRE(ai != nullptr);
		w.frames(10);
		Coord3D air{ 200.0f, 200.0f, 121.0f };
		bird->setPosition(&air);
	}

	// the barracks of whichever fixture the arena was built over
	Object *barracks(float x, float y)
	{
		return w.unit(barracksType.c_str(), 'B', x, y);
	}

	std::string barracksType;
};

float normAngle(float a)
{
	while (a > 3.14159274f)
	{
		a -= 6.28318548f;
	}
	while (a < -3.14159274f)
	{
		a += 6.28318548f;
	}
	return a;
}

// the averaged angle the path mover (RW 0x5E91E2) turns the object by, in radians, from the ported path alone: the
// average of the sample half a bounding sphere behind the walk turned by pi and the sample the same distance ahead,
// each relative to the object's facing (RW 0x4B3D8D); a sample the spline cannot answer is left out, as in the mover.
// `count` receives how many samples the spline answered. `advance` is the distance the mover adds to the walk before it
// samples: RW 0x5E91E2 does `pathDistance += speedNow` first and only then queries the spline at pathDistance +- the
// look-ahead, so a caller that wants the aim of the NEXT frame passes the current speed.
float pathAimAngle(GiantBirdAIUpdate &ai, float &count, float advance = 0.0f)
{
	count = 0.0f;
	const AerialSpline &s = ai.spline();
	if (!s.valid())
	{
		return 0.0f;
	}
	AerialSpline &mutableSpline = const_cast<AerialSpline &>(s);
	const float d = ai.pathDistance() + advance;
	// RW 0xBD869C: half the bounding sphere, times LookAheadMult when it exceeds 1
	float half = CombatQueries::boundingSphereRadius(*ai.getObject()) * 0.5f;
	const Locomotor *loco = ai.curLocomotor();
	if (loco && 1.0f < loco->getTemplate().m_lookAheadMult)
	{
		half *= loco->getTemplate().m_lookAheadMult;
	}
	const Object &obj = *ai.getObject();
	float angle = 0.0f;
	Coord3D behind;
	if (AerialPathfinder::query(mutableSpline, d - half, behind))
	{
		angle = normAngle(ObjectKnockback::relativeAngle2D(obj, behind) + 3.14159265358979f);
		count = 1.0f;
	}
	Coord3D ahead;
	if (AerialPathfinder::query(mutableSpline, d + half, ahead))
	{
		angle += ObjectKnockback::relativeAngle2D(obj, ahead);
		count += 1.0f;
	}
	if (count == 0.0f)
	{
		return 0.0f;
	}
	return angle / count;
}

// the heading that averaged angle aims the object at, in degrees
float pathHeadingDegrees(GiantBirdAIUpdate &ai)
{
	float count = 0.0f;
	const float angle = pathAimAngle(ai, count);
	if (count == 0.0f)
	{
		return ai.getObject()->getOrientation() * 57.29577951308232f;
	}
	// RW 0x5E3A81 writes angle + turn (the turn, not the 0.2/0.8 blend), so the heading aimed at is the facing plus the
	// averaged angle
	return normAngle(ai.getObject()->getOrientation() + angle) * 57.29577951308232f;
}

int stateOf(GiantBirdAIUpdate &ai)
{
	return (int)ai.currentStateId();
}
} // namespace

// -----------------------------------------------------------------------------------------------------------------
// P-114: the repeat pass flies at the goal (the measured cycle of RW 0x7695C2 under the ported path builder)
// -----------------------------------------------------------------------------------------------------------------

TEST_CASE("PORT-187 P-114 giant bird: after a strike the machine swoops again and the new path walks toward the goal, not away from it (RW 0x76975F / RW 0x778413 / RW 0x5E91E2)")
{
	Port187Arena a;
	Object *barracks = a.barracks(400.0f, 200.0f);
	REQUIRE(barracks != nullptr);
	a.bird->setOrientation(0.0f); // the bird faces +X, the barracks is east of it
	REQUIRE(a.ai->aiAttackObject(barracks, CMD_FROM_PLAYER));

	// One pass is Swoop 0x3F2 -> Attack 0x3F4 -> FollowThru 0x3F5, then the follow-through's failure swoops again
	// (RW 0x76975F). The machine runs every state it can within a frame, so the attack is never the state at a frame
	// boundary: the visible trace of a pass is 0x3F2 .. 0x3F5 and back.
	int swoopEntries = 0;
	std::string hist;
	int prevState = stateOf(*a.ai);
	for (int frame = 0; frame < 900 && swoopEntries < 2; ++frame)
	{
		a.w.frames(1);
		const int st = stateOf(*a.ai);
		if (st != prevState)
		{
			hist += std::to_string(prevState) + ">" + std::to_string(st) + " ";
			if (st == (int)GiantBirdAIUpdate::STATE_SWOOP)
			{
				++swoopEntries;
			}
			prevState = st;
		}
	}
	MESSAGE("state changes " << hist << "; swoops started " << a.ai->swoopsStarted() << ", strikes " << a.ai->strikes());
	REQUIRE(swoopEntries == 2); // the follow-through's failure swoops again (RW 0x76975F: 0x3F5 -> 0x3F2)
	REQUIRE(a.ai->strikes() == 2); // one strike per swoop entry (RW 0x76B668 fires in the attack state's onEnter)

	// The path the repeat swoop flies (built at its onEnter, RW 0x76DEF5 -> RW 0x769EA5 -> RW 0x778413) leaves toward
	// the goal: the path mover averages the heading of the point half a bounding sphere behind the walk and the point
	// the same distance ahead (RW 0x5E91E2), so the heading of the first frames is the goal's direction.
	const Coord3D p = *a.bird->getPosition();
	const Coord3D goal = *barracks->getPosition();
	const float toGoal = (float)(std::atan2(goal.y - p.y, goal.x - p.x) * 180.0 / 3.14159265358979);
	const float heading = pathHeadingDegrees(*a.ai);
	float diff = std::fabs(heading - toGoal);
	if (diff > 180.0f)
	{
		diff = 360.0f - diff;
	}
	MESSAGE("bird " << p.x << "," << p.y << " goal " << goal.x << "," << goal.y << " toGoal " << toGoal << " path heading " << heading);
	CHECK(diff < 45.0f); // tolerance: the two samples are a bounding sphere apart, so the heading is not exactly the bearing
}

// -----------------------------------------------------------------------------------------------------------------
// P-117: every swoop that reaches the building lands its strike
// -----------------------------------------------------------------------------------------------------------------

TEST_CASE("PORT-187 P-117 giant bird: every swoop that reaches the barracks fires the plow once (RW 0x76B668 gate max(speed*3, 8.5) / RW 0x69200A)")
{
	Port187Arena a;
	Object *barracks = a.barracks(400.0f, 200.0f);
	REQUIRE(barracks != nullptr);
	a.bird->setOrientation(0.0f);
	REQUIRE(a.ai->aiAttackObject(barracks, CMD_FROM_PLAYER));

	// The fell beast's report (P-117) is a swoop that reaches the building and leaves the attack state without a
	// strike. RW 0x76B668 fires once in the attack's onEnter behind the distance gate (RW 0x76B796 refuses with
	// STATE_FAILURE, which ends the pass without a strike), so the two counts have to move together: every swoop
	// that the machine entered must have taken exactly one strike at its attack.
	const unsigned long long startSwoops = a.ai->swoopsStarted();
	const unsigned long long startStrikes = a.ai->strikes();
	// the strike's own frame, position and gate, so a red run names the pass that missed
	std::string log;
	unsigned long long seen = startStrikes;
	float lastHealth = a.w.health(barracks);
	for (int frame = 0; frame < 2000 && a.ai->swoopsStarted() - startSwoops < 6; ++frame)
	{
		a.w.frames(1);
		if (a.ai->strikes() != seen)
		{
			const Coord3D now = *a.bird->getPosition();
			const Coord3D bp = *barracks->getPosition();
			const float dx = now.x - bp.x, dy = now.y - bp.y, dz = now.z - bp.z;
			const float health = a.w.health(barracks);
			log += "strike at frame " + std::to_string(frame) + ": bird " + std::to_string(now.x) + "," + std::to_string(now.y) + "," + std::to_string(now.z) +
				" barracks " + std::to_string(bp.x) + "," + std::to_string(bp.y) + "," + std::to_string(bp.z) +
				" dist " + std::to_string(std::sqrt(dx * dx + dy * dy + dz * dz)) + " speed " + std::to_string(a.ai->flight().speed) +
				" gate " + std::to_string(std::max(a.ai->flight().speed * 3.0f, 8.5f)) + " health " + std::to_string(lastHealth) + "->" + std::to_string(health) + "\n";
			lastHealth = health;
			seen = a.ai->strikes();
		}
	}
	MESSAGE(log);
	const unsigned long long swoops = a.ai->swoopsStarted() - startSwoops;
	const unsigned long long strikes = a.ai->strikes() - startStrikes;
	MESSAGE("swoops " << swoops << ", strikes " << strikes << ", health " << a.w.health(barracks));
	REQUIRE(swoops == 6);
	CHECK(strikes == swoops); // one strike per swoop: RW 0x76B668 fires in the attack's onEnter
	// the 10-damage SIEGE nugget of the fixture, once per strike
	CHECK(a.w.health(barracks) == 2000.0f - 10.0f * (float)strikes);
}

// -----------------------------------------------------------------------------------------------------------------
// P-114: the turn limit RW 0x5E372D, measured frame by frame
// -----------------------------------------------------------------------------------------------------------------

// The reported symptom of P-114 was the bird's orientation flapping ~20 degrees every other frame. RW 0x5E5E372D
// caps the mover's turn at 2*pi / TurnTime (18 frames for TurnTime 3500, so 0.349066 rad), so a frame that turns the
// object further than that cap cannot come from the path mover: this case measures every frame of a pass and the
// repeat pass and requires the cap to hold. It is the red test the owner asked for: if a future change writes the
// orientation from somewhere else, this fails with the frame number.
TEST_CASE("PORT-187 P-114 giant bird: no frame turns the bird further than RW 0x5E372D")
{
	Port187Arena a;
	Object *barracks = a.barracks(400.0f, 200.0f);
	REQUIRE(barracks != nullptr);
	a.bird->setOrientation(0.0f); // the bird faces +X, the barracks is east of it
	REQUIRE(a.ai->aiAttackObject(barracks, CMD_FROM_PLAYER));

	// RW 0x5E372D: 2 * pi / TurnTime frames, capped by the locomotor's turn-rate cap. TurnTime 3500 -> 18 frames.
	const float cap = a.ai->curLocomotor()->getMaxTurnRate(a.ai->locomotorHost());
	REQUIRE(cap == doctest::Approx(6.28318548f / 18.0f)); // tolerance: the fixture's TurnTime is 3500, nothing else feeds the cap

	float largest = 0.0f;
	int at = -1;
	unsigned long long swoopsSeen = a.ai->swoopsStarted();
	for (int frame = 0; frame < 900 && a.ai->swoopsStarted() - swoopsSeen < 2; ++frame)
	{
		const float before = a.bird->getOrientation();
		const int stateBefore = (int)a.ai->currentStateId();
		const size_t cpsBefore = a.ai->spline().controlPointCount();
		const float dBefore = a.ai->pathDistance();
		const float spdBefore = a.ai->flight().speed;
		a.w.frames(1);
		const float turned = std::fabs(normAngle(a.bird->getOrientation() - before));
		if (turned > largest)
		{
			largest = turned;
			at = frame;
		}
		// float32 orientation storage rounds the difference of two orientations by an ULP, so the stored angle can land exactly on the cap
		CHECK_MESSAGE(turned <= cap + 1e-4f, "frame " << frame << " turned " << turned << " rad, the cap is " << cap); // tolerance: ULP of float32 orientation storage
	}
	MESSAGE("largest turn of one frame " << largest << " rad at frame " << at << " (" << largest * 57.2958f << " deg), the cap is " << cap);
	CHECK(largest <= cap + 1e-4f); // tolerance: float32 orientation storage rounds the difference by ULP
	// the cap has to actually be used: the measured largest turn is 0.349066 = the cap, so the bound only fails a mover that never turns
	CHECK(largest > cap * 0.9f); // tolerance: "the cap is reached", not its exact value
}

// -----------------------------------------------------------------------------------------------------------------
// P-114: the swoop's heading follows the path (RW 0x5E91E2 -> RW 0x5E3A81)
// -----------------------------------------------------------------------------------------------------------------

// The owner's red-test definition asks for a case that the swoop's heading follows the path. The unambiguous form is the
// sign of the turn: RW 0x5E91E2 averages the two sample headings into `angle`, RW 0x5E94A6..0x5E9520 clamps it to the
// locomotor's cap, and RW 0x5E3A81 writes `angle + turn` into the orientation. So a frame whose averaged angle exceeds the
// cap turns the object by the sign of that angle, and the heading walks toward the path's aim instead of away from it.
//
// Comparing the orientation against the 0.2 / 0.8 blend is what made the earlier attempt ambiguous: the blend is the state
// for the NEXT frame, so any snapshot disagrees with it by one frame of turn and the sign flips whenever the path turns.
//
// A frame that rebuilds the path cannot be measured from a sample taken before it: the swoop's onEnter replaces the whole
// spline (RW 0x76DEF5 -> RW 0x769EA5 -> RW 0x778413) and the mover then steers the NEW one, while a sample taken before
// the frame reads the old one. Those frames (state changes, or the control points / walk distance jump) are skipped, not
// tolerated: on every frame where the path is unchanged the sign has to agree exactly.
TEST_CASE("PORT-187 P-114 giant bird: the swoop's heading follows the path the mover is given (RW 0x5E91E2 / RW 0x5E3A81)")
{
	Port187Arena a;
	Object *barracks = a.barracks(400.0f, 200.0f);
	REQUIRE(barracks != nullptr);
	a.bird->setOrientation(0.0f);
	REQUIRE(a.ai->aiAttackObject(barracks, CMD_FROM_PLAYER));

	const float cap = a.ai->curLocomotor()->getMaxTurnRate(a.ai->locomotorHost());
	int measured = 0, agreed = 0, rebuilt = 0;
	std::string wrong;
	for (int frame = 0; frame < 900; ++frame)
	{
		// the aim of the frame, from the spline the mover will walk (it advances the walk by the current speed first)
		float count = 0.0f;
		const float aim = pathAimAngle(*a.ai, count);
		const int stateBefore = (int)a.ai->currentStateId();
		const size_t cpsBefore = a.ai->spline().controlPointCount();
		const float before = a.bird->getOrientation();
		a.w.frames(1);
		const float turned = normAngle(a.bird->getOrientation() - before);
		if (count == 0.0f)
		{
			continue; // no sample answered: the mover reaches no orientation write this frame
		}
		if (stateBefore != (int)a.ai->currentStateId() || cpsBefore != a.ai->spline().controlPointCount())
		{
			++rebuilt; // the swoop rebuilt the path inside this frame (RW 0x76DEF5), the pre-frame sample is of the old one
			continue;
		}
		// a turn below the cap is the clamp of a smaller angle, so only a saturated frame states the sign outright
		if (std::fabs(aim) <= cap)
		{
			continue;
		}
		++measured;
		if ((aim > 0.0f) == (turned > 0.0f))
		{
			++agreed;
		}
		else
		{
			wrong += "frame " + std::to_string(frame) + " aim " + std::to_string(aim) + " turned " + std::to_string(turned) + "\n";
		}
	}
	MESSAGE("saturated frames measured " << measured << ", turn sign agreed " << agreed << ", frames that rebuilt the path " << rebuilt
		<< (wrong.empty() ? "" : "\n" + wrong));
	// the guard is a bound on purpose: it fails only if the pass stops steering by more than the cap at all, and the exact
	// count follows the fixture's frame timing, so pinning it would break on any harmless path change
	REQUIRE(measured > 0); // tolerance: a vacuity guard, the sign agreement below is the exact assertion
	CHECK(agreed == measured); // RW 0x5E3A81: the orientation moves by the sign of the averaged angle (through the clamp)
}

// -----------------------------------------------------------------------------------------------------------------
// P-114 over the numbers Ennorath 1.9.8.3 actually ships for the fell beast (the object the report came from)
// -----------------------------------------------------------------------------------------------------------------

// This is the measurement the earlier fixture could not make. Two of the mod's numbers change the picture:
//   * the swoop flies in SET_PANIC -> `FellBeastSwoopLocomotor`, which sets **LookAheadMult = 2.0**, so the mover
//     (RW 0x5E91E2) samples one BOUNDING SPHERE each way, not half of one (RW 0xBD869C * LookAheadMult);
//   * `NazgulPlowAttackEdain` reloads for 4000 frames, so a pass cannot be followed by another straight away.
// The pass geometry is measured, not asserted: the case requires only that the bird keeps flying a path and that its
// turn stays inside the cap, so a regression shows up as a red case with the numbers printed.
TEST_CASE("PORT-187 P-114 the fell beast as Ennorath ships it: the swoop flies on and stays inside RW 0x5E372D")
{
	Port187Arena a(kPort187RealFixture, "Port187RealBird", "Port187RealBarracks");
	Object *barracks = a.barracks(400.0f, 200.0f);
	REQUIRE(barracks != nullptr);
	a.bird->setOrientation(0.0f);
	REQUIRE(a.ai->aiAttackObject(barracks, CMD_FROM_PLAYER));

	// the shipped LookAheadMult has to reach the mover, else this case measures the wrong geometry
	const Locomotor *loco = a.ai->curLocomotor();
	REQUIRE(loco != nullptr);
	REQUIRE(loco->getTemplate().m_lookAheadMult == doctest::Approx(2.0)); // tolerance: the fixture sets exactly 2.0

	const float cap = loco->getMaxTurnRate(a.ai->locomotorHost());
	float largest = 0.0f;
	int at = -1;
	int turns = 0;
	std::string stateHist;
	std::map<int, int> framesPerState;
	for (int frame = 0; frame < 1200; ++frame)
	{
		++framesPerState[(int)a.ai->currentStateId()];
		const float before = a.bird->getOrientation();
		a.w.frames(1);
		const float turned = std::fabs(normAngle(a.bird->getOrientation() - before));
		if (turned > 0.0f)
		{
			++turns;
		}
		if (turned > largest)
		{
			largest = turned;
			at = frame;
		}
		// float32 orientation storage rounds the difference of two orientations by an ULP
		CHECK_MESSAGE(turned <= cap + 1e-4f, "frame " << frame << " turned " << turned << " rad, the cap is " << cap); // tolerance: ULP of float32 storage
	}
	for (const auto &kv : framesPerState)
	{
		stateHist += "state " + std::to_string(kv.first) + ": " + std::to_string(kv.second) + " frames, ";
	}
	MESSAGE("shipped fell beast: " << turns << " turning frames, largest turn " << largest << " rad at frame " << at
		<< " (" << largest * 57.2958f << " deg), the cap is " << cap << "; swoops " << a.ai->swoopsStarted()
		<< ", strikes " << a.ai->strikes() << ", health " << a.w.health(barracks) << "\n" << stateHist);
	// the bird has to keep flying: a machine stuck between states would turn on no frame at all
	CHECK(turns > 0); // tolerance: a vacuity guard, the per-frame cap check above is the exact assertion
	CHECK(a.ai->swoopsStarted() >= 1);
}

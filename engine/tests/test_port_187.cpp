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

struct Port187Arena
{
	combattest::CombatWorld w;
	Object *bird = nullptr;
	GiantBirdAIUpdate *ai = nullptr;

	Port187Arena()
	{
		const std::string error = w.w.load(kPort187Fixture, INI_LOAD_OVERWRITE, "port187.ini");
		REQUIRE_MESSAGE(error.empty(), error);
		w.combat().setAutoAcquireEnabled(false);
		bird = w.unit("Port187Bird", 'A', 200.0f, 200.0f);
		REQUIRE(bird != nullptr);
		ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
		REQUIRE(ai != nullptr);
		w.frames(10);
		Coord3D air{ 200.0f, 200.0f, 121.0f };
		bird->setPosition(&air);
	}
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

// the heading the path mover (RW 0x5E91E2) aims the object at, in degrees, from the ported path alone: the average of
// the sample half a bounding sphere behind the walk turned by pi and the sample the same distance ahead, each relative
// to the object's facing (RW 0x4B3D8D); a sample the spline cannot answer is left out, as in the mover.
float pathHeadingDegrees(GiantBirdAIUpdate &ai)
{
	const AerialSpline &s = ai.spline();
	if (!s.valid())
	{
		return ai.getObject()->getOrientation() * 57.29577951308232f;
	}
	AerialSpline &mutableSpline = const_cast<AerialSpline &>(s);
	const float d = ai.pathDistance();
	const float half = CombatQueries::boundingSphereRadius(*ai.getObject()) * 0.5f;
	const Object &obj = *ai.getObject();
	float angle = 0.0f, count = 0.0f;
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
		return ai.getObject()->getOrientation() * 57.29577951308232f;
	}
	// the mover turns by this relative angle (RW 0x5E3A81 writes angle + turn), so the heading it aims at is the facing
	// plus the averaged angle
	return normAngle(ai.getObject()->getOrientation() + angle / count) * 57.29577951308232f;
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
	Object *barracks = a.w.unit("Port187Barracks", 'B', 400.0f, 200.0f);
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
	Object *barracks = a.w.unit("Port187Barracks", 'B', 400.0f, 200.0f);
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
	Object *barracks = a.w.unit("Port187Barracks", 'B', 400.0f, 200.0f);
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

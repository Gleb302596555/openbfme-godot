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
 DelayBetweenShots = 4000
 FiringDuration = 4500
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
 GeometryMajorRadius = 10
 GeometryHeight = 30
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

// the heading the path mover (RW 0x5E91E2) aims the object at, in degrees, from the ported path alone
float pathHeadingDegrees(GiantBirdAIUpdate &ai)
{
	const AerialSpline &s = ai.spline();
	if (!s.valid())
	{
		return 0.0f;
	}
	// RW 0x5E91E2: half the bounding sphere radius behind the walk, and the same ahead
	Coord3D behind, ahead;
	const float d = ai.pathDistance();
	const float half = CombatQueries::boundingSphereRadius(*ai.getObject()) * 0.5f;
	const bool hasBehind = AerialPathfinder::query(const_cast<AerialSpline &>(s), d - half, behind);
	const bool hasAhead = AerialPathfinder::query(const_cast<AerialSpline &>(s), d + half, ahead);
	if (!hasAhead)
	{
		return 0.0f;
	}
	const Coord3D *from = hasBehind ? &behind : nullptr;
	const float dx = ahead.x - (from ? from->x : ahead.x);
	const float dy = ahead.y - (from ? from->y : ahead.y);
	return (float)(std::atan2(dy, dx) * 180.0 / 3.14159265358979);
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
	CHECK(diff < 1.0f); // tolerance: the two samples are a bounding sphere apart, so the heading is not exactly the bearing
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
	for (int frame = 0; frame < 2000 && a.ai->swoopsStarted() - startSwoops < 6; ++frame)
	{
		a.w.frames(1);
	}
	const unsigned long long swoops = a.ai->swoopsStarted() - startSwoops;
	const unsigned long long strikes = a.ai->strikes() - startStrikes;
	MESSAGE("swoops " << swoops << ", strikes " << strikes << ", health " << a.w.health(barracks));
	REQUIRE(swoops == 6);
	CHECK(strikes == swoops); // one strike per swoop: RW 0x76B668 fires in the attack's onEnter
	// the 10-damage SIEGE nugget of the fixture, once per strike
	CHECK(a.w.health(barracks) == 2000.0f - 10.0f * (float)strikes);
}

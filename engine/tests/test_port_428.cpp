// OpenBFME unit tests. GPL-3.0.
// PORT-428 (issue #428, P-111): the aerial path's would-collide filter is RotWK's 3D shape overlap
// (RW 0x66139C -> RW 0xAD2CE0), not a 2D footprint test. Synthetic INI, no game installation.
//
// Retail chain (read with tools/mod/rw.py on game.dat md5 daed3668...):
//   * RW 0x778413 computeAerialPath: the swoop's attack run (behind, flyer, goal, terminal point);
//   * RW 0x77824E probe / RW 0x778176 pathClear: the blocked-run test that RW 0x76A80A checkAhead asks;
//   * RW 0x777F9F separation: the partition query filtered by RW 0x67C5F3's PartitionFilterWouldCollide;
//     its allow RW 0x66139C compares BOTH objects' GeometryInfo at their positions and angles through
//     RW 0xAD2CE0 (3D, height-aware), then keeps the pair only when they overlap (RW 0x6613C7);
//   * RW 0x76DF1D the swoop's update: while checkAhead reports a blocked run, steer (RW 0x76A933)
//     rebuilds the path every frame and the bird circles instead of reaching RW 0x76B79E's strike gate.
// DONOR BFME2 0x00375C28 (Code/GameEngine/Source/GameLogic/AI/AerialPathfinder.cpp:184) and its
// Rva00261603Filter::allow (Code/GameEngine/Source/Common/GapBodiesGE07.cpp:146, GeometryInfo::CollidesWith)
// show the same filter; RotWK's instructions above are the source of truth.
//
// The shape below is the synthetic stand-in of Ennorath's SupplyRocksPile* (mod
// data/ini/object/neutral/supplyrocks.ini): KindOf STRUCTURE IMMOBILE NO_COLLIDE, Geometry = BOX,
// a low wide box (height 15). The infantry victim stands inside that footprint, the bird approaches
// above the box's top. Nothing here is read from the game or the mod at run time.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/AI/AerialPathfinder.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SimMath.h"

namespace
{
const char kPort428Objects[] = R"ini(
Object Port428Bird
  KindOf = MONSTER SELECTABLE CAN_ATTACK GIANT_BIRD
  Geometry = CYLINDER
  GeometryMajorRadius = 12
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
  End
  Behavior = GiantBirdAIUpdate ModuleTag_AI
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 110
  End
End
Object Port428Pile
  KindOf = STRUCTURE IMMOBILE NO_COLLIDE
  Geometry = BOX
  GeometryMajorRadius = 35
  GeometryMinorRadius = 25
  GeometryHeight = 15
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port428Victim
  KindOf = INFANTRY
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
End
)ini";

struct Arena
{
	combattest::CombatWorld world{ kPort428Objects };
	Object *bird = nullptr;
	Object *pile = nullptr;
	Object *victim = nullptr;

	Arena()
	{
		bird = world.unit("Port428Bird", 'A', 200.0f, 200.0f);
		pile = world.unit("Port428Pile", 'B', 200.0f, 200.0f);
		victim = world.unit("Port428Victim", 'B', 200.0f, 200.0f);
		REQUIRE(bird != nullptr);
		REQUIRE(pile != nullptr);
		REQUIRE(victim != nullptr);
		world.frames(1); // the objects are in the partition and the geometry caches are built
		const Coord3D ground{ 200.0f, 200.0f, 0.0f };
		pile->setPosition(&ground);
		victim->setPosition(&ground);
	}

	void birdAt(float z)
	{
		const Coord3D p{ 200.0f, 200.0f, z };
		bird->setPosition(&p);
	}

	// RW 0x777FB1: the flyer's own GeometryInfo (+ 0xA8), its AI's partner (+ 0x584) and the
	// current locomotor's PreferredHeight (+ 0x48) are what separation and probe read.
	AerialPathfinder::Flyer flyer() const
	{
		AerialPathfinder::Flyer f;
		f.object = bird;
		f.hasAI = true;
		f.preferredHeight = 30.0f;
		f.partner = victim->getID();
		return f;
	}
};
} // namespace

// Expectation from RW 0x66139C -> RW 0xAD2CE0: the box top is at z = 15 and the flyer's shape
// starts at z = 20, so the height test of the shape overlap rejects the pair. The old 2D footprint
// filter reported the pile at every altitude and this CHECK failed.
TEST_CASE("PORT-428: a bird above a low supply pile has a clear aerial probe (RW 0x66139C -> 0xAD2CE0, RW 0x77824E)")
{
	Arena a;
	a.birdAt(20.0f);
	const Coord3D pos{ 200.0f, 200.0f, 20.0f };
	const AerialPathfinder::Flyer f = a.flyer();
	float worst = 0.0f;
	Coord3D push{ 0.0f, 0.0f, 0.0f };
	CHECK(AerialPathfinder::separation(f, pos, worst, push)); // RW 0x777F9F: nothing is in the way
	CHECK(worst == 0.0f);
	CHECK(push.x == 0.0f);
	CHECK(push.y == 0.0f);
	CHECK(push.z == 0.0f);
	Coord3D probePush{ 0.0f, 0.0f, 0.0f };
	CHECK(AerialPathfinder::probe(f, pos, probePush) == 0); // RW 0x77824E: 0 = clear
	CHECK(probePush.z == 0.0f);                              // no terrain intrusion either (RW 0x777ED7)
}

// Neighbour behaviour that must survive the fix: at the pile's own height the shape overlap does
// hit, so the pile is still an obstacle; and the AI's partner is still skipped (RW 0x77804A), as
// are INFANTRY / CAVALRY / HORDE (RW 0x77805C / 0x778065). With the partner set to the pile, the
// only remaining object in the query is the INFANTRY victim, which retail ignores.
TEST_CASE("PORT-428: the pile still blocks at its own height; the partner and infantry are still skipped (RW 0x777F9F)")
{
	Arena a;
	const Coord3D inside{ 200.0f, 200.0f, 8.0f }; // within the box's 0..15 height
	a.birdAt(8.0f);
	AerialPathfinder::Flyer f = a.flyer();
	float worst = 0.0f;
	Coord3D push{ 0.0f, 0.0f, 0.0f };
	CHECK_FALSE(AerialPathfinder::separation(f, inside, worst, push));
	CHECK_FALSE(worst == 0.0f); // RW 0x7780A0..0x778144: 2 * r_other + r_self - distance is nonzero
	CHECK_FALSE(push.z == 0.0f); // the push along +Z points out of the pile
	Coord3D probePush{ 0.0f, 0.0f, 0.0f };
	CHECK(AerialPathfinder::probe(f, inside, probePush) == 1); // RW 0x77824E: 1 = something intrudes

	f.partner = a.pile->getID(); // RW 0x777FD2..0x77804A: the partner is ignored by the separation
	worst = 0.0f;
	push = Coord3D{ 0.0f, 0.0f, 0.0f };
	CHECK(AerialPathfinder::separation(f, inside, worst, push));
	CHECK(worst == 0.0f);
}

// The visible case: RW 0x778413 builds the swoop's attack run from the bird down to the contact
// point of the victim standing inside the pile's footprint. RW 0x778176's pathClear (the test RW
// 0x76A80A's checkAhead applies) must answer "clear" at the flyer's altitude; when it answers
// "blocked" for the whole path, RW 0x76A933's steer rebuilds the path each frame and the bird
// circles (P-111) instead of reaching RW 0x76B79E's strike distance.
TEST_CASE("PORT-428: the swoop's attack run over the pile is clear, so steer does not circle (RW 0x778413 / 0x778176)")
{
	Arena a;
	a.birdAt(20.0f);
	const AerialPathfinder::Flyer f = a.flyer();
	const Coord3D goal{ 200.0f, 200.0f, 8.0f }; // the victim's contact point, inside the footprint
	AerialSpline spline;
	REQUIRE(AerialPathfinder::computeAerialPath(f, goal, spline, AerialPathfinder::MASK_A, nullptr, false));
	REQUIRE(spline.valid());
	CHECK_FALSE(spline.controlPointCount() == 0u); // behind, flyer, goal, terminal (RW 0x778600..0x778978)
	CHECK(AerialPathfinder::pathClear(f, spline, 0.0f, 5.0f)); // RW 0x778176: the start of the run
}

// A killed / carried-away victim leaves the attack state through its own dead-object gate
// (RW 0x76B6F2, Port 164: out of scope here). The separation filter holds only the partner's ID
// (RW 0x777FD2 -> AI + 0x584) and never dereferences it, so a victim destroyed mid-swoop must not
// change the geometry answer: the low pile is still 3D-clear above its top and still solid inside it.
TEST_CASE("PORT-428: a victim killed mid-swoop does not change the pile's separation (RW 0x777FD2 / 0x76B6F2)")
{
	Arena a;
	const ObjectID victimId = a.victim->getID();
	a.world.logic->destroyObject(a.victim);
	a.world.logic->processDestroyList();
	AerialPathfinder::Flyer f;
	f.object = a.bird;
	f.hasAI = true;
	f.preferredHeight = 30.0f;
	f.partner = victimId; // the stale ID is only compared (RW 0x77804A), never resolved
	a.birdAt(20.0f);
	const Coord3D above{ 200.0f, 200.0f, 20.0f };
	float worst = 0.0f;
	Coord3D push{ 0.0f, 0.0f, 0.0f };
	CHECK(AerialPathfinder::separation(f, above, worst, push));
	CHECK(worst == 0.0f);
	a.birdAt(8.0f);
	const Coord3D inside{ 200.0f, 200.0f, 8.0f };
	worst = 0.0f;
	push = Coord3D{ 0.0f, 0.0f, 0.0f };
	CHECK_FALSE(AerialPathfinder::separation(f, inside, worst, push));
	CHECK_FALSE(worst == 0.0f);
}

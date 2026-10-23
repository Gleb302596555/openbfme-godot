// OpenBFME unit tests, lane SIEGE-1: siege weapons against the fortress gates after lane FLANK-1 made projectiles meet objects in flight. GPL-3.0.
//
// Test from community PR #64 (NotNBR, test_siege1_attack.cpp and its fixture Siege1TestUtil.h, kept unchanged): a Mordor catapult made 500 from Helm's Deep's
// main gate toward Player_2_Start never damaged the gate since lane FLANK-1. The cause was the pair rule of the in-flight contact test: RotWK runs a pair's
// narrow test from the side of its lower ObjectID (RW 0xB6D730 orders the pair by the id RW 0x670216 returns, RW 0xB6D060 calls first->test(second's object),
// RW 0x68C7B2 -> 0x68BA2A -> GeometryInfo::CollidesWith RW 0xAD2CE0 with the first object's own GeometryInfo), and RW 0xAD2CE0's height rules are not
// symmetric. FLANK-1 always tested from the stone's side, so the lintel of the gate arch (HelmsDeepGatehouseCenter, a BOX at offset z 110, height 30) reached
// down to its base and caught every stone falling on the gate. The frames below are this port's deterministic results (seed 9): regression pins, not retail
// measurements. SKIPs loudly without the installs.

#include "Siege1TestUtil.h"

#include "GameLogic/Combat/CombatState.h"

#include <cstdio>

using namespace siege1test;

namespace
{
const char *const kHelmsDeep = "maps/map wor helms deep/map wor helms deep.map";
const char *const kMinasTirith = "maps/map wor minas tirith/map wor minas tirith.map";

struct GateRun
{
	bool ordered = false;
	int shots = 0, contacts = 0;         // projectiles launched; in-flight contacts canCollideWith accepted (CombatState counters)
	int firstHit = -1, deathFrame = -1;  // the gate's first damage and death frames, -1: none
	float healthAfter = 0.0f;
	std::string otherDamaged;            // the other objects that lost health, in object list order
};

// `unit` made `d` in front of the gate named `gateName` (along the gate's own facing, on the side of Player_2_Start), ordered to attack it for `frames` frames
GateRun attackFromFront(starttest::Shared &s, const char *map, const char *gateName, const char *unit, float d, int frames)
{
	GateRun r;
	FortressGame g(s, map);
	g.frames(2);
	Object *gate = g.named(gateName);
	REQUIRE(gate);
	Player *me = g.human();
	REQUIRE(me);
	g.game->shroud().revealMapForPlayerPermanently(me->getPlayerIndex());
	const Waypoint *w = g.game->logic().terrain()->findWaypointByName("Player_2_Start");
	REQUIRE(w);
	const Coord3D tp = *gate->getPosition();
	float ax = std::cos(gate->getOrientation()), ay = std::sin(gate->getOrientation());
	if (ax * (w->location.x - tp.x) + ay * (w->location.y - tp.y) < 0.0f)
	{
		ax = -ax;
		ay = -ay;
	}
	std::string error;
	Object *o = g.game->createObject(unit, me->getPlayerIndex(), Coord3D{ tp.x + ax * d, tp.y + ay * d, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(o, error);
	g.frames(5);
	std::vector<std::pair<ObjectID, float>> health;
	for (Object *x = g.game->logic().getFirstObject(); x; x = x->getNextObject())
	{
		health.emplace_back(x->getID(), healthOf(x));
	}
	const CombatState::Counters &c = g.game->logic().combat().counters();
	const unsigned long long launched = c.projectilesLaunched, hits = c.projectileContactHits;
	AIUpdateInterface *ai = o->getAIUpdateInterface();
	r.ordered = ai && ai->aiAttackObject(gate, CMD_FROM_PLAYER);
	const ObjectID gid = gate->getID();
	const float before = healthOf(gate);
	for (int f = 1; f <= frames; ++f)
	{
		g.frames(1);
		Object *t = g.game->logic().findObjectByID(gid);
		if (r.firstHit < 0 && (!t || healthOf(t) < before))
		{
			r.firstHit = f;
		}
		if (r.deathFrame < 0 && (!t || t->isEffectivelyDead()))
		{
			r.deathFrame = f;
		}
	}
	Object *t = g.game->logic().findObjectByID(gid);
	r.healthAfter = t ? healthOf(t) : 0.0f;
	r.shots = (int)(c.projectilesLaunched - launched);
	r.contacts = (int)(c.projectileContactHits - hits);
	for (const std::pair<ObjectID, float> &h : health)
	{
		Object *x = g.game->logic().findObjectByID(h.first);
		if (x && x != o && h.first != gid && healthOf(x) < h.second)
		{
			r.otherDamaged += x->getTemplate()->getName() + " ";
		}
	}
	MESSAGE(std::string(unit) << " in front of " << std::string(gateName) << " from " << d << ": ordered " << r.ordered << ", shots " << r.shots << ", contacts " << r.contacts << ", first damage "
				  << r.firstHit << ", death " << r.deathFrame << ", health at the end " << r.healthAfter << ", others damaged: " << r.otherDamaged);
	return r;
}
} // namespace

TEST_CASE("siege1 gate: #64's catapult 500 from Helm's Deep's gate toward Player_2_Start throws into the gatehouse tower in its line of fire")
{
	// The direction toward Player_2_Start is 26 degrees off the gate's facing: HelmsDeepGatehouseRight (a BOX 74 x 95, height 140, on the ground 130 from
	// the gate) stands in the stone's path, whose arc is FirstHeight 53 above the highest terrain along the line (RW 0x85E658 -> TheTerrainLogic vslot 0x40,
	// RW 0x466621: the height map only). The tower was made before the stone, so the pair is tested from the tower's side, and a stone at z ~209 is inside
	// its 100 .. 240: the warhead's ProjectileCollidesWith (MONSTERS NEUTRAL STRUCTURES) meets it (RW 0x6CC79C; the WALK_ON_TOP_OF_WALL pass-over needs the
	// gate on a raised layer, RW 0x68BBE0 > 1). Before FLANK-1 the stone flew through and hit the gate at frame 24 (the #64 pin).
	OPENBFME_REQUIRE_START(s);
	const AttackResult r = siegeAttack(*s, kHelmsDeep, "Main Gate", "MordorCatapult", false, 500.0f, 600);
	MESSAGE("MordorCatapult toward Player_2_Start: first damage " << r.firstHit << ", health " << r.healthBefore << " -> " << r.healthAfter);
	CHECK(r.ordered);
	CHECK(r.target == "RBHelmsDeepGateDoorBig");
	CHECK(r.firstHit == -1);
	CHECK(r.healthAfter == 4000.0f);
}

TEST_CASE("siege1 gate: #64's catapult made inside its MinimumAttackRange of Helm's Deep's gate backs off and breaks it (the stones clear the arch)")
{
	OPENBFME_REQUIRE_START(s);
	const AttackResult r = siegeAttack(*s, kHelmsDeep, "Main Gate", "MordorCatapult", false, 100.0f, 600);
	MESSAGE("MordorCatapult from 100: first damage " << r.firstHit << " from " << r.distAtHit << ", death " << r.deathFrame);
	CHECK(r.ordered);
	CHECK(r.slotAtHit == 0);
	CHECK(r.firstHit == 87);
	CHECK(r.distAtHit > 150.0f); // tolerance: it fired from outside its minimum range (150)
	CHECK(r.deathFrame == 359);  // FLANK-1's pair rule: the gate still stood at frame 600 (613.5 health)
}

TEST_CASE("siege1 gate: the stone and bolt throwers in front of Helm's Deep's gate break it; the arch takes no stone meant for the gate")
{
	OPENBFME_REQUIRE_START(s);
	// FLANK-1's pair rule (the same placements): the catapult broke the gate at frame 560 with 17 contacts in 17 shots, the Gondor trebuchet left it at 197,
	// the Dwarven catapult at 113, the Isengard ballista at 3250
	const GateRun c = attackFromFront(*s, kHelmsDeep, "Main Gate", "MordorCatapult", 400.0f, 600);
	CHECK(c.ordered);
	CHECK(c.shots == 8);
	CHECK(c.contacts == 0);
	CHECK(c.firstHit == 17);
	CHECK(c.deathFrame == 255);
	const GateRun t = attackFromFront(*s, kHelmsDeep, "Main Gate", "GondorTrebuchet", 400.0f, 600);
	CHECK(t.contacts == 1);
	CHECK(t.firstHit == 16);
	CHECK(t.deathFrame == 384);
	const GateRun dc = attackFromFront(*s, kHelmsDeep, "Main Gate", "DwarvenCatapult", 400.0f, 600);
	CHECK(dc.deathFrame == 365);
	const GateRun b = attackFromFront(*s, kHelmsDeep, "Main Gate", "IsengardBallista", 400.0f, 600);
	CHECK(b.deathFrame == 351);
}

TEST_CASE("siege1 gate: Mordor's catapult and Gondor's trebuchet in front of Minas Tirith's gate break it")
{
	OPENBFME_REQUIRE_START(s);
	// FLANK-1's pair rule: the catapult broke it at frame 293 (7 contacts in 9 shots), the trebuchet at 250
	const GateRun c = attackFromFront(*s, kMinasTirith, "Gate", "MordorCatapult", 400.0f, 600);
	CHECK(c.ordered);
	CHECK(c.contacts == 1);
	CHECK(c.firstHit == 22);
	CHECK(c.deathFrame == 192);
	const GateRun t = attackFromFront(*s, kMinasTirith, "Gate", "GondorTrebuchet", 400.0f, 600);
	CHECK(t.contacts == 0);
	CHECK(t.firstHit == 20);
	CHECK(t.deathFrame == 205);
}

TEST_CASE("siege1 gate: the rams, Grond and the mumak in front of the gates (no projectile: unchanged by the pair rule)")
{
	OPENBFME_REQUIRE_START(s);
	CHECK(attackFromFront(*s, kHelmsDeep, "Main Gate", "IsengardBatteringRam", 250.0f, 600).deathFrame == 99);
	CHECK(attackFromFront(*s, kHelmsDeep, "Main Gate", "DwarvenDemolisher", 250.0f, 600).deathFrame == 101);
	CHECK(attackFromFront(*s, kHelmsDeep, "Main Gate", "MordorGrond", 250.0f, 600).deathFrame == 128);
	CHECK(attackFromFront(*s, kHelmsDeep, "Main Gate", "MordorMumakil", 250.0f, 600).deathFrame == 243);
	CHECK(attackFromFront(*s, kMinasTirith, "Gate", "IsengardBatteringRam", 250.0f, 600).deathFrame == 57);
	CHECK(attackFromFront(*s, kMinasTirith, "Gate", "MordorGrond", 250.0f, 600).deathFrame == 83);
}

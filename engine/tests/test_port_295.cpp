#include <cmath>
// PORT-295 (issue #687): GettingBuiltBehavior's neighborhood rebuild, its affordability and its destructor's pathfinder removal. GPL-3.0.
// Expected values come from the RotWK disassembly (RW 0x857182, 0x856FBE, 0x856280, 0x85733A) and the INI below; see each case.
#include "doctest.h"

#include "FlatTerrainTestUtil.h"
#include "LogicTestUtil.h"

#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/ConstructionModules.h"

using logictest::LogicWorld;

namespace
{
Object *spawnObj(LogicWorld &world, const char *name, const char *player, float x, float y)
{
	const ThingTemplate *t = world.w.things.findTemplate(name);
	REQUIRE(t != nullptr);
	Team *team = world.players.findPlayerWithName(player)->getDefaultTeam();
	Object *o = world.logic->newObject(t, team, ObjectStatusMaskType{});
	REQUIRE(o != nullptr);
	const Coord3D p{ x, y, 0.0f };
	o->setPosition(&p);
	return o;
}

GettingBuiltBehavior *findGB(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (auto *gb = dynamic_cast<GettingBuiltBehavior *>(m.get()))
		{
			return gb;
		}
	}
	return nullptr;
}

const char kWallIni[] = R"(
Object Port295Wall
	KindOf = STRUCTURE IMMOBILE WALL_SEGMENT
	BuildCost = 100
	BuildTime = 5.0
	Geometry = BOX
	GeometryMajorRadius = 20
	GeometryMinorRadius = 20
	GeometryHeight = 10
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
		RebuildTimeSeconds = 2.0
		RebuildWhenDead = Yes
	End
End
)";
} // namespace

// RW 0x857182: the source must be effectively dead (Object + 0x458 bit 0) and canRebuild; then one contiguous (id +- 1) dead equivalent that can rebuild is enough.
// RW 0x856FBE: the source must be effectively dead; startConstruction (vslot 0x10) runs on it and on each contiguous dead equivalent.
TEST_CASE("PORT-295 GettingBuiltBehavior::canNeighborhoodRebuild and startNeighborhoodRebuild (RW 0x00857182, 0x00856FBE)")
{
	LogicWorld world;
	ConstructionModules::registerAll(world.w.modules);
	const std::string err = world.w.load(kWallIni, INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *w1 = spawnObj(world, "Port295Wall", "Alice", 100, 100);
	Object *w2 = spawnObj(world, "Port295Wall", "Alice", 140, 100);
	auto *gb1 = findGB(w1);
	auto *gb2 = findGB(w2);
	REQUIRE(gb1 != nullptr);
	REQUIRE(gb2 != nullptr);
	REQUIRE(w2->getID() == w1->getID() + 1);

	// alive: RW 0x857182 and 0x856FBE both answer / do nothing
	CHECK(!gb1->canNeighborhoodRebuild());
	gb1->startNeighborhoodRebuild(true);
	CHECK(!gb1->isConstructing());
	CHECK(!gb2->isConstructing());

	// both effectively dead: the neighborhood can rebuild and starts on both pieces
	w1->friend_setEffectivelyDead(true);
	w2->friend_setEffectivelyDead(true);
	CHECK(gb1->canNeighborhoodRebuild());
	gb1->startNeighborhoodRebuild(true);
	CHECK(gb1->isConstructing());
	CHECK(gb2->isConstructing());
	CHECK(!w1->isEffectivelyDead()); // startConstruction (RW 0x68D950(0)) brings the structure back
	CHECK(!w2->isEffectivelyDead());
}

// RW 0x856280: the money (player + 0x94) >= (int)ceil(neighborhoodRebuildCost) through vslot 0x50 (RW 0x857074: source plus every contiguous dead equivalent that can rebuild);
// a null player answers false (RW 0x856287 .. 0x85628B)
TEST_CASE("PORT-295 GettingBuiltBehavior::canAffordNeighborhoodRebuild (RW 0x00856280, vslot 0x50 = RW 0x857074)")
{
	LogicWorld world;
	ConstructionModules::registerAll(world.w.modules);
	const std::string err = world.w.load(kWallIni, INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	Player *p = world.players.findPlayerWithName("Alice");
	REQUIRE(p != nullptr);

	Object *w1 = spawnObj(world, "Port295Wall", "Alice", 100, 100);
	Object *w2 = spawnObj(world, "Port295Wall", "Alice", 140, 100);
	auto *gb1 = findGB(w1);
	REQUIRE(gb1 != nullptr);
	w1->friend_setEffectivelyDead(true);
	w2->friend_setEffectivelyDead(true);

	const float one = gb1->rebuildCost(*p);
	const float both = gb1->neighborhoodRebuildCost(*p);
	// RW 0x85657E: calcCostToBuild (BuildCost = 100) * PercentOfBuildCostToRebuildPristine (default 0.25, RW data ctor RW 0x8562DB: the body is at full health); the
	// neighborhood (RW 0x857074) adds the second dead equivalent
	CHECK(one == 25.0f);
	CHECK(both == 50.0f);
	const unsigned money = p->getMoney()->countMoney();
	// the player pays exactly what the neighborhood costs: left with ceil(both) the neighborhood is affordable, one short it is not
	const unsigned price = (unsigned)std::ceil(both);
	REQUIRE(price == 50u);
	REQUIRE(money > price); // tolerance: the player must start with more than the price to be withdrawn down to it
	p->getMoney()->withdraw(money - price, false);
	CHECK(gb1->canAffordNeighborhoodRebuild(p));
	p->getMoney()->withdraw(1, false);
	CHECK(!gb1->canAffordNeighborhoodRebuild(p));
	CHECK(!gb1->canAffordNeighborhoodRebuild(nullptr));
}

// RW 0x85733A: the destructor body walks the linked list (+0x40): for each piece RW 0x449681 finds that is effectively dead (+0x458 bit 0) it calls RW 0x6E85FB on
// [0xDE4B40] + 0x10 (the pathfinder's map removal). It destroys nothing; a living piece keeps its footprint.
TEST_CASE("PORT-295 GettingBuiltBehavior destructor: dead linked pieces leave the pathfinder map, nothing is destroyed (RW 0x0085733A)")
{
	LogicWorld world;
	ConstructionModules::registerAll(world.w.modules);
	flatterrain::Flat flat(world, 100);
	const std::string err = world.w.load(kWallIni, INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	struct Cleanup { LogicWorld &w; ~Cleanup() { w.logic->reset(); } } cleanup{ world }; // the objects leave the pathfinder before the AIWorld goes

	Object *hub = spawnObj(world, "Port295Wall", "Alice", 100, 100);
	Object *dead = spawnObj(world, "Port295Wall", "Alice", 300, 300);
	Object *alive = spawnObj(world, "Port295Wall", "Alice", 500, 500);
	auto *gb = findGB(hub);
	REQUIRE(gb != nullptr);
	gb->addLinkedPiece(*dead);
	gb->addLinkedPiece(*alive);
	flat.ai->addObjectToPathfindMap(*dead);
	flat.ai->addObjectToPathfindMap(*alive);
	Pathfinder &pf = flat.ai->pathfinder();
	REQUIRE(pf.cellAt(30, 30)->getType() == PathfindCell::CELL_OBSTACLE);
	REQUIRE(pf.cellAt(50, 50)->getType() == PathfindCell::CELL_OBSTACLE);

	dead->friend_setEffectivelyDead(true); // RW 0x857366: Object + 0x458 bit 0
	const ObjectID deadId = dead->getID();
	const ObjectID aliveId = alive->getID();
	world.logic->destroyObject(hub);
	world.logic->update(0);

	CHECK(world.logic->findObjectByID(deadId) != nullptr); // not destroyed
	CHECK(world.logic->findObjectByID(aliveId) != nullptr);
	CHECK(pf.cellAt(30, 30)->getType() == PathfindCell::CELL_CLEAR);    // the dead piece left the map
	CHECK(pf.cellAt(50, 50)->getType() == PathfindCell::CELL_OBSTACLE); // the living one did not
}

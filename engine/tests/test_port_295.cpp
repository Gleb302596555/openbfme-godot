#include "doctest.h"

#include "LogicTestUtil.h"

#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/ConstructionModules.h"

using logictest::LogicWorld;

namespace
{
Object *spawnObj(LogicWorld &world, const char *name, const char *player)
{
	const ThingTemplate *t = world.w.things.findTemplate(name);
	REQUIRE(t != nullptr);
	Team *team = world.players.findPlayerWithName(player)->getDefaultTeam();
	Object *o = world.logic->newObject(t, team, ObjectStatusMaskType{});
	REQUIRE(o != nullptr);
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
} // namespace

TEST_CASE("PORT-295 GettingBuiltBehavior::canNeighborhoodRebuild and startNeighborhoodRebuild (RW 0x00857182, 0x00856FBE)")
{
	LogicWorld world;
	const std::string err = world.w.load(R"(
Object Port295Wall
	KindOf = STRUCTURE WALL_SEGMENT
	BuildCost = 100
	BuildTime = 5.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
		RebuildTimeSeconds = 2.0
	End
End
)", INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *w1 = spawnObj(world, "Port295Wall", "Alice");
	Object *w2 = spawnObj(world, "Port295Wall", "Alice");

	w1->setStatus(0, true);
	w2->setStatus(0, true);

	auto *gb1 = findGB(w1);
	auto *gb2 = findGB(w2);
	REQUIRE(gb1 != nullptr);
	REQUIRE(gb2 != nullptr);

	CHECK(gb1->canNeighborhoodRebuild());
	gb1->startNeighborhoodRebuild(true);

	CHECK(!w1->isEffectivelyDead());
	CHECK(!w2->isEffectivelyDead());
}

TEST_CASE("PORT-295 GettingBuiltBehavior::destroyDeadLinkedPiecesOnDelete (RW 0x0085733A)")
{
	LogicWorld world;
	const std::string err = world.w.load(R"(
Object Port295Hub
	KindOf = STRUCTURE WALL_HUB
	BuildCost = 100
	BuildTime = 5.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
	End
End
)", INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *hub = spawnObj(world, "Port295Hub", "Alice");
	Object *piece = spawnObj(world, "Port295Hub", "Alice");

	auto *gb = findGB(hub);
	REQUIRE(gb != nullptr);
	gb->addLinkedPiece(*piece);

	const ObjectID pieceId = piece->getID();
	piece->setStatus(0, true);

	world.logic->destroyObject(hub);
	world.logic->update(0);

	CHECK(world.logic->findObjectByID(pieceId) == nullptr);
}

TEST_CASE("PORT-295 GettingBuiltBehavior::canAffordRebuild and stopConstruction (RW 0x00856280, 0x0085757F)")
{
	LogicWorld world;
	const std::string err = world.w.load(R"(
Object Port295Struct
	KindOf = STRUCTURE
	BuildCost = 100
	BuildTime = 5.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
	End
End
)", INI_LOAD_OVERWRITE, "port295.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Player *p = world.players.findPlayerWithName("Alice");
	REQUIRE(p != nullptr);
	p->getMoney()->withdraw(2000, false);

	Object *s = spawnObj(world, "Port295Struct", "Alice");
	auto *gb = findGB(s);
	REQUIRE(gb != nullptr);

	CHECK(gb->canAffordRebuild(*p));
	gb->stopConstruction();
	CHECK(!gb->isConstructing());
}

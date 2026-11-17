// PORT-233: EmotionTrackerUpdate's visibility predicate is viewer-relative for disguised objects.
// RotWK RW 0x8B5186..0x8B5195 passes the tracker owner's player to Object::isVisibleTo at RW 0x694CCC (mode 0).

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kPort233Objects[] =
	"Object PORT233Disguiser\n"
	"  KindOf = INFANTRY SELECTABLE DISGUISER\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = StealthUpdate ModuleTag_Stealth\n"
	"  End\n"
	"End\n"
	"Object PORT233RespawnHero\n"
	"  KindOf = SELECTABLE INFANTRY HERO\n"
	"  Body = RespawnBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = RespawnUpdate ModuleTag_Respawn\n"
	"    RespawnRules = AutoSpawn:No Cost:1000 Time:60000 Health:150%\n"
	"  End\n"
	"End\n";

struct Port233World : movetest::MoveWorld
{
	Port233World()
		: movetest::MoveWorld(200, 200, kPort233Objects)
	{
	}
};

struct Port233Wall : pathtest::TestObject
{
	PathfindWallGeometry wallGeometry;
	const PathfindWallGeometry *getWallGeometry() const override { return &wallGeometry; }
};

PathfindWallSurface port233WallTop()
{
	PathfindWallSurface surface;
	PathfindWallTriangle a, b;
	a.v[0] = { 150.0f, 90.0f, 60.0f };
	a.v[1] = { 250.0f, 90.0f, 60.0f };
	a.v[2] = { 250.0f, 110.0f, 60.0f };
	b.v[0] = { 150.0f, 90.0f, 60.0f };
	b.v[1] = { 250.0f, 110.0f, 60.0f };
	b.v[2] = { 150.0f, 110.0f, 60.0f };
	surface.triangles = { a, b };
	surface.loX = 150.0f;
	surface.loY = 90.0f;
	surface.hiX = 250.0f;
	surface.hiY = 110.0f;
	surface.height = 60.0f;
	return surface;
}
} // namespace

TEST_CASE("PORT-233 tracker visibility uses the viewer's relationship to a disguised object's apparent owner (RW 0x694CCC, 0x8B5186)")
{
	Port233World w;
	Object *viewer = w.make("Walker", w.teamOf("Alice"));
	Object *disguiser = w.make("PORT233Disguiser", w.teamOf("Bob"));
	Object *asAlly = w.make("Walker", w.teamOf("Alice"));
	Object *asEnemy = w.make("Walker", w.teamOf("Bob"));
	REQUIRE(viewer != nullptr);
	REQUIRE(disguiser != nullptr);
	REQUIRE(asAlly != nullptr);
	REQUIRE(asEnemy != nullptr);
	StealthUpdate *stealth = StealthUpdate::of(*disguiser);
	REQUIRE(stealth != nullptr);

	// A disguise as an ally is hidden from that viewer by RW 0x694E87..0x694ED1; the old viewer-less
	// DISGUISER shortcut returned true regardless of the apparent owner's relationship.
	stealth->disguiseAsObject(asAlly);
	REQUIRE(stealth->disguisePlayerIndex() == asAlly->getControllingPlayer()->getPlayerIndex());
	disguiser->setStatus((unsigned)CombatNames::status("STEALTHED"), true);
	disguiser->setStatus((unsigned)CombatNames::status("DETECTED"), false);
	CHECK_FALSE(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Neighbor: a disguise as an enemy remains visible to this viewer (same RW branch).
	stealth->disguiseAsObject(asEnemy);
	CHECK(EmotionModules::visibleToScans(*disguiser, viewer->getControllingPlayer()));

	// Existing callers without a viewer retain their legacy query contract.
	CHECK(EmotionModules::visibleToScans(*disguiser));
}

TEST_CASE("PORT-233 RespawnUpdate above 100 percent raises max health without changing revive reference (RW 0x8C47DE / 0x8C47F6)")
{
	Port233World w;
	Object *hero = w.make("PORT233RespawnHero", w.teamOf("Alice"));
	REQUIRE(hero != nullptr);
	ActiveBody *body = dynamic_cast<ActiveBody *>(hero->getBodyModule());
	RespawnUpdate *respawn = dynamic_cast<RespawnUpdate *>(hero->findModule("RespawnUpdate"));
	REQUIRE(body != nullptr);
	REQUIRE(respawn != nullptr);
	CHECK(body->getMaxHealth() == 500.0f);
	respawn->onRevived(nullptr);
	CHECK(body->getMaxHealth() == 750.0f);
	CHECK(body->getReviveReference() == 500.0f); // RW 0x8C47F6 changes max health (+0x10), not the revive reference (+0x2C)
	CHECK(body->getHealth() == 750.0f);
}

TEST_CASE("PORT-233 taunt scan wall exclusion reads the ground cell layer, not its connect layer (RW 0x6EA857)")
{
	pathtest::SyntheticTerrain terrain{ 40, 20 };
	pathtest::TestWorld world;
	Port233Wall wall;
	wall.id = 50;
	wall.pos = { 200.0f, 100.0f, 0.0f };
	wall.structure = true;
	wall.kinds.insert(PK_WALK_ON_TOP_OF_WALL);
	wall.geometry.type = PATHFIND_GEOMETRY_BOX;
	wall.geometry.majorRadius = 50.0f;
	wall.geometry.minorRadius = 30.0f;
	wall.wallGeometry.walls.push_back(port233WallTop());
	world.objects[wall.id] = &wall;

	Pathfinder pathfinder(pathtest::testConfig(), &world);
	pathfinder.newMap(terrain);
	pathfinder.addObjectToPathfindMap(wall);
	const Coord3D onWall{ 200.0f, 100.0f, 60.0f };
	const PathfindCell *wallCell = pathfinder.getCell(LAYER_GROUND, &onWall);
	REQUIRE(wallCell != nullptr);
	CHECK(isWallLayer(wallCell->getLayer()));
	CHECK((int)wallCell->getConnectLayer() < (int)LAYER_WALL_FIRST);
	CHECK(EmotionModules::isOnWallLayer(pathfinder, onWall));
	CHECK_FALSE(EmotionModules::isOnWallLayer(pathfinder, Coord3D{ 300.0f, 100.0f, 0.0f }));
}

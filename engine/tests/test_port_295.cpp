#include "doctest.h"

#include "Common/GameData.h"
#include "Common/INI/INI.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Object/Object.h"

namespace
{
void initPort295World(GameLogic &logic, ThingFactory &tf, const char *iniText)
{
	INI ini;
	ini.loadText(iniText, "port295.ini");
	logic.setGameData(std::make_unique<GameData>());
	logic.addPlayer(std::make_unique<Player>("Player_1", "Men", 0));
}
} // namespace

TEST_CASE("PORT-295 GettingBuiltBehavior::canNeighborhoodRebuild and startNeighborhoodRebuild (RW 0x00857182, 0x00856FBE)")
{
	ThingFactory tf;
	GameLogic logic;
	initPort295World(logic, tf, R"(
Object Port295Wall
	KindOf = STRUCTURE WALL_SEGMENT
	BuildCost = 100
	BuildTime = 5.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
		SelfBuildingLoop = None
		SelfRepairFromDamageLoop = None
		SelfRepairFromRubbleLoop = None
		RebuildTimeSeconds = 2.0
		PercentOfBuildCostToRebuildNormally = 0.5
		PercentOfBuildCostToRebuildDamaged = 0.5
		PercentOfBuildCostToRebuildReallyDamaged = 0.5
		PercentOfBuildCostToRebuildRubble = 0.5
	End
End
)");

	const ThingTemplate *tmpl = tf.findTemplate("Port295Wall");
	REQUIRE(tmpl != nullptr);
	Player *p = logic.findPlayer("Player_1");
	REQUIRE(p != nullptr);
	p->getMoney().deposit(500);

	Object *w1 = logic.createObject(tmpl, p);
	Object *w2 = logic.createObject(tmpl, p);
	REQUIRE(w1 != nullptr);
	REQUIRE(w2 != nullptr);

	w1->setEffectivelyDead(true);
	w2->setEffectivelyDead(true);

	auto *gb1 = dynamic_cast<GettingBuiltBehavior *>(w1->findModule("GettingBuiltBehavior"));
	auto *gb2 = dynamic_cast<GettingBuiltBehavior *>(w2->findModule("GettingBuiltBehavior"));
	REQUIRE(gb1 != nullptr);
	REQUIRE(gb2 != nullptr);

	CHECK(gb1->canNeighborhoodRebuild());
	gb1->startNeighborhoodRebuild(true);

	CHECK(!w1->isEffectivelyDead());
	CHECK(!w2->isEffectivelyDead());
}

TEST_CASE("PORT-295 GettingBuiltBehavior::destroyDeadLinkedPiecesOnDelete (RW 0x0085733A)")
{
	ThingFactory tf;
	GameLogic logic;
	initPort295World(logic, tf, R"(
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
)");

	const ThingTemplate *tmpl = tf.findTemplate("Port295Hub");
	REQUIRE(tmpl != nullptr);
	Player *p = logic.findPlayer("Player_1");
	REQUIRE(p != nullptr);

	Object *hub = logic.createObject(tmpl, p);
	Object *piece = logic.createObject(tmpl, p);
	REQUIRE(hub != nullptr);
	REQUIRE(piece != nullptr);

	auto *gb = dynamic_cast<GettingBuiltBehavior *>(hub->findModule("GettingBuiltBehavior"));
	REQUIRE(gb != nullptr);
	gb->addLinkedPiece(*piece);

	const ObjectID pieceId = piece->getID();
	piece->setEffectivelyDead(true);

	logic.destroyObject(hub);
	logic.update();

	CHECK(logic.findObjectByID(pieceId) == nullptr);
}

TEST_CASE("PORT-295 GettingBuiltBehavior::canAffordRebuild and stopConstruction (RW 0x00856280, 0x0085757F)")
{
	ThingFactory tf;
	GameLogic logic;
	initPort295World(logic, tf, R"(
Object Port295Struct
	KindOf = STRUCTURE
	BuildCost = 100
	BuildTime = 5.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200.0
		InitialHealth = 200.0
	End
	Behavior = GettingBuiltBehavior ModuleTag_GB
		PercentOfBuildCostToRebuildNormally = 0.5
		PercentOfBuildCostToRebuildDamaged = 0.5
		PercentOfBuildCostToRebuildReallyDamaged = 0.5
		PercentOfBuildCostToRebuildRubble = 0.5
	End
End
)");

	const ThingTemplate *tmpl = tf.findTemplate("Port295Struct");
	REQUIRE(tmpl != nullptr);
	Player *p = logic.findPlayer("Player_1");
	REQUIRE(p != nullptr);

	Object *s = logic.createObject(tmpl, p);
	REQUIRE(s != nullptr);
	auto *gb = dynamic_cast<GettingBuiltBehavior *>(s->findModule("GettingBuiltBehavior"));
	REQUIRE(gb != nullptr);

	CHECK(!gb->canAffordRebuild(*p));
	p->getMoney().deposit(100);
	CHECK(gb->canAffordRebuild(*p));

	gb->stopConstruction();
	CHECK(!gb->isConstructing());
}

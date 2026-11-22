// OpenBFME. PORT-320 (fork issue 756): RotWK permanently reveals the defeated player's map during the defeat update (RW 0x808F53 / 0x8090AA -> 0xB4D940 -> 0xB4F5D0). The surviving player's map remains shrouded; expectations are from those RW calls. Synthetic INI only.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "LogicTestUtil.h"
#include "StartTestUtil.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/VictoryConditions.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort320Objects[] =
	"Object Port320Keep\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SCORE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"End\n";

const char kPort320GameData[] =
	"GameData\n"
	"  ForceModelsToFollowTimeOfDay = Yes\n"
	"  ForceModelsToFollowWeather = Yes\n"
	"  UnitDamagedThreshold = 0.5\n"
	"  UnitReallyDamagedThreshold = 0.25\n"
	"  DefaultStartingCash = 1000\n"
	"  VictoryConditionStructureObjectFilter = NONE +STRUCTURE -IGNORE_FOR_VICTORY -UNATTACKABLE\n"
	"  VictoryConditionUnitObjectFilter = ANY -DOZER\n"
	"End\n";

struct Port320AIWorld : CombatWorld
{
	SkirmishAIStore store{ w.keys };
	INIEnvironment env;

	Port320AIWorld()
		: CombatWorld(kPort320Objects)
	{
		static const char aiData[] = "SkirmishAIData TheSkirmishAIData\n DisableWallBuilding = No\nEnd\n"
									  "ArmyDefinition AlphaArmy\n Side = Alpha\n ArmyMemberDefinition M\n  Unit = Soldier\n  PercentageOfArmyPhase1 = 100\n End\nEnd\n"
									  "AIDozerAssignment AlphaDozer\n Side = Alpha\n Unit = AlphaPorter\nEnd\n";
		store.registerBlocks(env.blocks);
		INI ini(env);
		const std::string text = aiData;
		ini.loadMemory("port320-ai.ini", std::vector<std::uint8_t>(text.begin(), text.end()), INI_LOAD_OVERWRITE);
		SkirmishSetup setup;
		setup.players = { { "H", "FactionA", true, 0, 0, 0 }, { "C1", "FactionA", false, 1, 0, 1 }, { "C2", "FactionB", false, 1, 0, 2 },
			{ "C3", "FactionA", false, 2, 0, 3 } };
		setup.defaultStartingCash = 2000;
		REQUIRE(players.setupSkirmish(setup).empty());
		logic->skirmishAI().setStore(&store);
		REQUIRE(logic->skirmishAI().newGame(true) == 3);
	}

	Player *player(const char *name) { return players.findPlayerWithName(name); }
};

struct Port320World : CombatWorld
{
	ShroudManager shroud{ *logic };

	Port320World()
		: CombatWorld(kPort320Objects)
	{
		GameLogicSettings scratch;
		std::string error;
		REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(kPort320GameData, scratch, &error), error);
		logic->settings().victoryRulesLoaded = scratch.victoryRulesLoaded;
		logic->settings().victoryStructureFilter = scratch.victoryStructureFilter;
		logic->settings().victoryUnitFilter = scratch.victoryUnitFilter;
		logic->settings().secondsBeforeBaseCheckActive = scratch.secondsBeforeBaseCheckActive;
		logic->victory().init();
		shroud.init(40.0f, 1);
		shroud.setExtent(0.0f, 0.0f, 4000.0f, 4000.0f);
		shroud.attach();
		logic->setShroud(&shroud);
	}

	~Port320World()
	{
		logic->setShroud(nullptr);
	}

	void defeatKeeper(Object *keeper, Object *attacker)
	{
		DamageInfo damage;
		damage.m_input.m_sourceID = attacker->getID();
		damage.m_input.m_amount = 100000.0f;
		damage.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		keeper->attemptDamage(damage);
	}
};
} // namespace

TEST_CASE("PORT-320: killPlayer preserves the shared AI group's objects and money, but the ordinary AI still loses its money (RW 0x6ABE7C -> RW 0x6C7518)")
{
	Port320AIWorld world;
	Player *first = world.player("C1");
	Player *shared = world.player("C2");
	Player *alone = world.player("C3");
	REQUIRE(first != nullptr);
	REQUIRE(shared != nullptr);
	REQUIRE(alone != nullptr);
	REQUIRE(world.logic->skirmishAI().findGroup(first->getPlayerIndex()) == world.logic->skirmishAI().findGroup(shared->getPlayerIndex()));
	REQUIRE(world.logic->skirmishAI().findGroup(first->getPlayerIndex()) != world.logic->skirmishAI().findGroup(alone->getPlayerIndex()));
	Object *sharedAsset = world.spawn("Port320Keep", 300.0f, 300.0f, 0.0f, world.teamOf("C2"));
	Object *aloneAsset = world.spawn("Port320Keep", 600.0f, 300.0f, 0.0f, world.teamOf("C3"));
	REQUIRE(sharedAsset != nullptr);
	REQUIRE(aloneAsset != nullptr);
	const auto sharedMoney = shared->getMoney()->countMoney();
	world.logic->victory().killPlayer(shared);
	CHECK(shared->isDefeated());
	CHECK(shared->getMoney()->countMoney() == sharedMoney); // RW 0x6ABE9E -> 0x6ABF5E skips ordinary asset elimination
	CHECK_FALSE(sharedAsset->isEffectivelyDead());
	CHECK_FALSE(first->isDefeated());
	const auto aloneMoney = alone->getMoney()->countMoney();
	REQUIRE(aloneMoney > 0);
	world.logic->victory().killPlayer(alone);
	CHECK(alone->isDefeated());
	CHECK(alone->getMoney()->countMoney() == 0); // the non-shared branch reaches RW 0x6ABF3C .. 0x6ABF57
	world.frames(1);
	CHECK(aloneAsset->isEffectivelyDead());
}

TEST_CASE("PORT-320: defeated player's whole map is permanently revealed and the surviving player's map is unchanged (RW 0x808F53 / RW 0x8090AA -> RW 0xB4D940 -> RW 0xB4F5D0)")
{
	Port320World world;
	Player *alice = world.playerOf('A');
	Player *bob = world.playerOf('B');
	REQUIRE(alice != nullptr);
	REQUIRE(bob != nullptr);
	Object *aliceKeep = world.unit("Port320Keep", 'A', 300, 300);
	Object *bobKeep = world.unit("Port320Keep", 'B', 600, 300);
	REQUIRE(aliceKeep != nullptr);
	REQUIRE(bobKeep != nullptr);
	world.frames(30); // RW 0x808F81: the retail 25-frame base-check delay has elapsed.
	CHECK(world.shroud.getCellStatus(bob->getPlayerIndex(), 80, 80) == CELLSHROUD_SHROUDED);
	CHECK(world.shroud.getCellStatus(alice->getPlayerIndex(), 80, 80) == CELLSHROUD_SHROUDED);
	world.defeatKeeper(bobKeep, aliceKeep);
	world.frames(1);
	REQUIRE(world.logic->victory().isDefeated(1));
	CHECK(world.shroud.getCellStatus(bob->getPlayerIndex(), 80, 80) == CELLSHROUD_CLEAR);
	CHECK(world.shroud.getCellStatus(alice->getPlayerIndex(), 80, 80) == CELLSHROUD_SHROUDED);
}

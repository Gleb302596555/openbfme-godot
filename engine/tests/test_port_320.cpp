// OpenBFME. PORT-320 (fork issue 756): RotWK permanently reveals the defeated player's map during the defeat update (RW 0x808F53 / 0x8090AA -> 0xB4D940 -> 0xB4F5D0). The surviving player's map remains shrouded; expectations are from those RW calls. Synthetic INI only.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/VictoryConditions.h"

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

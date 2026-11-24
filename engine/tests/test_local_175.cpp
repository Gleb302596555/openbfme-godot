// OpenBFME unit tests. GPL-3.0.
// LOCAL-175: War of the Ring logic ports verified against RotWK 2.01 disassembly:
//   - DisbandAllArmyMembers (RW 0x6B48F2, MSG_LW_DISBAND_ARMY = 1714)
//   - HandleSwapArmiesInBattleMessage (RW 0x6B4B39, RW 0x6B379E, RW 0x6B3822, RW 0x7F69EE, MSG_LW_SWAP_ARMIES_IN_BATTLE = 1716)

#include "doctest.h"

#include "Common/INI.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/LivingWorld/LivingWorldData.h"
#include "GameLogic/LivingWorld/LivingWorldLogic.h"

#include <string>
#include <vector>

using namespace LivingWorld;

namespace
{
void parseLocal175Ini(LivingWorldStores &stores, const std::string &text)
{
	INIEnvironment env;
	stores.registerBlocks(env.blocks);
	INI ini(env);
	std::vector<std::uint8_t> bytes(text.begin(), text.end());
	ini.loadMemory("local175-test.ini", bytes, INI_LOAD_OVERWRITE);
}

UnitInfo local175Units(const std::string &name)
{
	UnitInfo u;
	u.known = true;
	if (name == "GoodHero" || name == "GoodHeroB" || name == "EvilHero")
	{
		u.hero = true;
		u.buildCost = 1000;
		u.strength = 3000;
		u.autoResolveUnitType = AR_HERO;
		u.autoResolveBody = "AutoResolve_HeroBody";
		u.levels = { { 1, 1, 50 } };
	}
	else if (name == "GoodSoldiers" || name == "GoodArchers" || name == "EvilSoldiers")
	{
		u.buildCost = 300;
		u.commandPoints = 20;
		u.strength = 300;
		u.autoResolveUnitType = AR_SOLDIER;
	}
	else
	{
		u.known = false;
	}
	return u;
}

const char *kLocal175Ini = R"(
LivingWorldPlayerTemplate PlayerGood
	Faction = FactionGood
	StartingWorldCP = 200
	MaxWorldCP = 400
	ScenarioStartResources = 1000
	ScenarioMaxResources = 5000
End
LivingWorldPlayerTemplate PlayerEvil
	Faction = FactionEvil
	StartingWorldCP = 200
	MaxWorldCP = 400
	ScenarioStartResources = 1000
	ScenarioMaxResources = 5000
End
AutoResolveWeapon AutoResolve_DefaultWeapon
	DamagePerRound = Damage:50 Against:AutoResolveUnit_Soldier
	DamagePerRound = Damage:50 Against:AutoResolveUnit_Hero
	MissPercentChance = 50
End
AutoResolveArmor AutoResolve_DefaultArmor
	Armor = AutoResolveUnit_Soldier 100%
End
AutoResolveBody AutoResolve_DefaultBody
	HitpointsAtLevel = Hitpoints:300 Level:0
End
AutoResolveBody AutoResolve_HeroBody
	HitpointsAtLevel = Hitpoints:2000 Level:1
	LeaveInArmySummary = Yes
End
AutoResolveCombatChain AutoResolve_DefaultCombatChain
End
AutoResolveReinforcementSchedule Defender
	1 = 0
	EachRemaining = +1
End
AutoResolveReinforcementSchedule Attacker
	1 = 0
	EachRemaining = +1
End
LivingWorldAITemplate DefaultAITemplate
	BuildingScoreArmory = 50
	BuildingScoreBarracks = 40
	BuildingScoreCastle = 40
	BuildingScoreFarm = 50
End
LivingWorldPlayerArmy
	Name = GoodHeroArmyA
	ArmyEntry
		ThingTemplate = GoodHero
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = GoodSoldiers
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = GoodArchers
		Quantity = 1
	End
End
LivingWorldPlayerArmy
	Name = GoodHeroArmyB
	ArmyEntry
		ThingTemplate = GoodHeroB
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = GoodSoldiers
		Quantity = 1
	End
End
LivingWorldPlayerArmy
	Name = EvilHeroArmy
	ArmyEntry
		ThingTemplate = EvilHero
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = EvilSoldiers
		Quantity = 1
	End
End
LivingWorldRegionCampaign Local175RegionCampaign
	Region RegA
		MapName = "MAP A"
		OwnedByPlayer = Good
		ConnectsTo = RegB
	End
	Region RegB
		MapName = "MAP B"
		OwnedByPlayer = Evil
		ConnectsTo = RegA
	End
End
LivingWorldCampaign Local175Scenario
	Scenario
		RegionCampaign = Local175RegionCampaign
		MaxPlayers = 2
		DefaultStartSpots = RegA RegB
		PlayerDefeatCondition
			Teams = 1 2
			NumControlledRegionsLessOrEqualTo = 0
		End
	End
	Act One
		SpawnArmy
			ScriptingName = GoodArmyA
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHero
			PlayerArmy = GoodHeroArmyA
			InitialRegion = RegA
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = GoodArmyB
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHeroB
			PlayerArmy = GoodHeroArmyB
			InitialRegion = RegA
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = EvilArmyA
			SpawnForTemplates = PlayerEvil
			HeroTemplateName = EvilHero
			PlayerArmy = EvilHeroArmy
			InitialRegion = RegB
			SpawnAtActStart = Yes
		End
	End
End
)";

bool startLocal175Game(LivingWorldStores &stores, LivingWorldLogic &logic)
{
	parseLocal175Ini(stores, kLocal175Ini);
	GameSetup setup;
	setup.campaign = "Local175Scenario";
	setup.seed = 7;
	SetupSlot a;
	a.name = "Good";
	a.faction = "FactionGood";
	a.human = true;
	a.team = 1;
	SetupSlot b;
	b.name = "Evil";
	b.faction = "FactionEvil";
	b.human = true;
	b.team = 2;
	setup.slots = { a, b };
	setup.ranks = { { true, 5u, 0 } };
	std::string err;
	return logic.start(setup, &err);
}
} // namespace

// Expectations from RotWK 2.01 DisbandAllArmyMembers (RW 0x6B48F2):
// iterates over all entries of the army summary, calls CanDisbandArmyMember (RW 0x6B4876),
// cancels pending upgrades (RW 0x80EDB8), disbands every eligible entry and refunds its cost,
// leaving non-disbandable hero entries untouched.
TEST_CASE("LOCAL-175 disbandArmy (RW 0x6B48F2): disbandArmy disbands non-hero entries, refunds cost, keeps hero")
{
	LivingWorldStores stores;
	LivingWorldLogic logic(stores, local175Units);
	REQUIRE(startLocal175Game(stores, logic));

	int armyId = -1;
	for (const Army &a : logic.armies())
	{
		if (a.owner == 0 && a.members.size() == 3)
		{
			armyId = a.id;
			break;
		}
	}
	REQUIRE(armyId == 1);
	const int treasuryBefore = logic.treasury(0);

	std::string err;
	REQUIRE(logic.disbandArmy(0, armyId, &err));

	const Army *after = logic.findArmy(armyId);
	REQUIRE(after != nullptr);
	REQUIRE(after->members.size() == 1);
	CHECK(after->members[0].thingTemplate == "GoodHero");
	CHECK(logic.treasury(0) == treasuryBefore + 600);
}

// Expectations from RotWK 2.01 DisbandAllArmyMembers (RW 0x6B48F2) + OpenBFME lockstep owner/phase guard:
// foreign sender is refused without modifying the army.
TEST_CASE("LOCAL-175 disbandArmy (RW 0x6B48F2): rejects foreign player")
{
	LivingWorldStores stores;
	LivingWorldLogic logic(stores, local175Units);
	REQUIRE(startLocal175Game(stores, logic));

	int armyId = -1;
	for (const Army &a : logic.armies())
	{
		if (a.owner == 0 && a.members.size() == 3)
		{
			armyId = a.id;
			break;
		}
	}
	REQUIRE(armyId == 1);

	std::string err;
	CHECK_FALSE(logic.disbandArmy(1, armyId, &err));
	CHECK(logic.findArmy(armyId)->members.size() == 3);
}

// Expectations from RotWK 2.01 HandleSwapArmiesInBattleMessage (RW 0x6B4B39, helpers RW 0x6B379E / RW 0x6B3822,
// LivingWorldBattle::swapArmies RW 0x7F69EE): swaps two army slots belonging to the sending player on a battle side,
// and rejects out-of-range battle/side/player/slot or foreign sender.
TEST_CASE("LOCAL-175 swapArmiesInBattle (RW 0x6B4B39): MSG_LW_SWAP_ARMIES_IN_BATTLE swaps player's army slots and validates bounds")
{
	LivingWorldStores stores;
	LivingWorldLogic logic(stores, local175Units);
	REQUIRE(startLocal175Game(stores, logic));

	std::vector<int> goodArmies;
	for (const Army &a : logic.armies())
	{
		if (a.owner == 0)
		{
			goodArmies.push_back(a.id);
		}
	}
	REQUIRE(goodArmies.size() == 2);

	std::string err;
	REQUIRE(logic.orderMove(0, goodArmies[0], 1, &err));
	REQUIRE(logic.orderMove(0, goodArmies[1], 1, &err));
	REQUIRE(logic.endTurn(0, &err));
	REQUIRE(logic.endTurn(1, &err));
	for (int i = 0; i < 10 && logic.phase() != PHASE_BATTLES; ++i)
	{
		logic.update();
	}
	REQUIRE(logic.phase() == PHASE_BATTLES);
	REQUIRE(logic.battles().size() == 1);

	const Battle &bBefore = logic.battles()[0];
	int sideIdx = -1, playerIdx = -1;
	for (int s = 0; s < (int)bBefore.sides.size(); ++s)
	{
		for (int p = 0; p < (int)bBefore.sides[(size_t)s].players.size(); ++p)
		{
			if (bBefore.sides[(size_t)s].players[(size_t)p].player == 0)
			{
				sideIdx = s;
				playerIdx = p;
			}
		}
	}
	REQUIRE(sideIdx == 0);
	REQUIRE(playerIdx == 0);

	std::vector<int> p0Before;
	for (int id : bBefore.armies)
	{
		const Army *a = logic.findArmy(id);
		if (a && a->owner == 0)
		{
			p0Before.push_back(id);
		}
	}
	REQUIRE(p0Before.size() == 2);

	::GameMessage swapMsg(MSG_LW_SWAP_ARMIES_IN_BATTLE, 0);
	swapMsg.appendIntegerArgument(0);
	swapMsg.appendIntegerArgument(sideIdx);
	swapMsg.appendIntegerArgument(playerIdx);
	swapMsg.appendIntegerArgument(0);
	swapMsg.appendIntegerArgument(1);
	REQUIRE(logic.dispatch(swapMsg, 0, &err));

	std::vector<int> p0After;
	for (int id : logic.battles()[0].armies)
	{
		const Army *a = logic.findArmy(id);
		if (a && a->owner == 0)
		{
			p0After.push_back(id);
		}
	}
	REQUIRE(p0After.size() == 2);
	CHECK(p0After[0] == p0Before[1]);
	CHECK(p0After[1] == p0Before[0]);

	CHECK_FALSE(logic.dispatch(swapMsg, 1, &err));

	::GameMessage badSlot(MSG_LW_SWAP_ARMIES_IN_BATTLE, 0);
	badSlot.appendIntegerArgument(0);
	badSlot.appendIntegerArgument(sideIdx);
	badSlot.appendIntegerArgument(playerIdx);
	badSlot.appendIntegerArgument(0);
	badSlot.appendIntegerArgument(5);
	CHECK_FALSE(logic.dispatch(badSlot, 0, &err));

	::GameMessage badBattle(MSG_LW_SWAP_ARMIES_IN_BATTLE, 0);
	badBattle.appendIntegerArgument(9);
	badBattle.appendIntegerArgument(sideIdx);
	badBattle.appendIntegerArgument(playerIdx);
	badBattle.appendIntegerArgument(0);
	badBattle.appendIntegerArgument(1);
	CHECK_FALSE(logic.dispatch(badBattle, 0, &err));
}

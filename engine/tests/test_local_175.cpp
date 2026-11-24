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
	HitpointsAtLevel = Hitpoints:3000 Level:0
End
LivingWorldPlayerArmy GoodHeroArmyA
	Hero = GoodHero
	Entry = GoodSoldiers 1
	Entry = GoodArchers 1
End
LivingWorldPlayerArmy GoodHeroArmyB
	Hero = GoodHeroB
	Entry = GoodSoldiers 1
End
LivingWorldPlayerArmy EvilHeroArmy
	Hero = EvilHero
	Entry = EvilSoldiers 1
End
RegionCampaign TestCampaign
	Region RegA
		DisplayName = RegA
		ConnectedRegions = RegB
		CreateAutoFort = No
		BuildPlot = X:0 Y:0
	End
	Region RegB
		DisplayName = RegB
		ConnectedRegions = RegA
		CreateAutoFort = No
		BuildPlot = X:0 Y:0
	End
End
LivingWorldCampaign TestScenario
	RegionCampaign = TestCampaign
	Act ActOne
		OwnershipSet Set1
			Regions = RegA
			SpawnArmy
				PlayerArmy = GoodHeroArmyA
				Region = RegA
			End
			SpawnArmy
				PlayerArmy = GoodHeroArmyB
				Region = RegA
			End
		End
		OwnershipSet Set2
			Regions = RegB
			SpawnArmy
				PlayerArmy = EvilHeroArmy
				Region = RegB
			End
		End
	End
End
)";

bool startLocal175Game(LivingWorldStores &stores, LivingWorldLogic &logic)
{
	parseLocal175Ini(stores, kLocal175Ini);
	GameSetup setup;
	setup.scenario = "TestScenario";
	setup.players.push_back({ "PlayerGood", "FactionGood", 0, false });
	setup.players.push_back({ "PlayerEvil", "FactionEvil", 1, false });
	std::string err;
	return logic.start(setup, &err);
}
} // namespace

// Expectations from RotWK 2.01 DisbandAllArmyMembers (RW 0x6B48F2):
// iterates over all entries of the army summary, calls CanDisbandArmyMember (RW 0x6B4876),
// cancels pending upgrades (RW 0x80EDB8), disbands every eligible entry and refunds its cost,
// leaving non-disbandable hero entries untouched.
TEST_CASE("LOCAL-175 disbandArmy (RW 0x6B48F2): MSG_LW_DISBAND_ARMY disbands non-hero entries, refunds cost, keeps hero")
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

	::GameMessage msg(MSG_LW_DISBAND_ARMY);
	msg.appendIntegerArgument(armyId);
	std::string err;
	REQUIRE(logic.dispatch(msg, 0, &err));

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

	::GameMessage msg(MSG_LW_DISBAND_ARMY);
	msg.appendIntegerArgument(armyId);
	std::string err;
	CHECK_FALSE(logic.dispatch(msg, 1, &err));
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
	for (int i = 0; i < 10 && logic.turnPhase() != PHASE_BATTLES; ++i)
	{
		logic.update();
	}
	REQUIRE(logic.turnPhase() == PHASE_BATTLES);
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

	::GameMessage swapMsg(MSG_LW_SWAP_ARMIES_IN_BATTLE);
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

	::GameMessage badSlot(MSG_LW_SWAP_ARMIES_IN_BATTLE);
	badSlot.appendIntegerArgument(0);
	badSlot.appendIntegerArgument(sideIdx);
	badSlot.appendIntegerArgument(playerIdx);
	badSlot.appendIntegerArgument(0);
	badSlot.appendIntegerArgument(5);
	CHECK_FALSE(logic.dispatch(badSlot, 0, &err));

	::GameMessage badBattle(MSG_LW_SWAP_ARMIES_IN_BATTLE);
	badBattle.appendIntegerArgument(9);
	badBattle.appendIntegerArgument(sideIdx);
	badBattle.appendIntegerArgument(playerIdx);
	badBattle.appendIntegerArgument(0);
	badBattle.appendIntegerArgument(1);
	CHECK_FALSE(logic.dispatch(badBattle, 0, &err));
}

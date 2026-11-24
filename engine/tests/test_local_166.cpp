// OpenBFME unit tests. GPL-3.0.
// LOCAL-166: War of the Ring logic ports verified against RotWK 2.01 disassembly:
//   - LivingWorldRegion::SetupRegionOwnership (RW 0x7F4696, findPlayerByName RW 0x6B774C)
//   - LivingWorldLogic::EnforceArmyRegionOwnership (RW 0x6B56F8, from StartGame RW 0x6B90A7)
//   - LivingWorldLogic::OnRegionChangedOwnership hostile garrison cleanup (RW 0x6BB2EF)

#include "doctest.h"

#include "Common/INI.h"
#include "GameLogic/LivingWorld/LivingWorldData.h"
#include "GameLogic/LivingWorld/LivingWorldLogic.h"

#include <string>
#include <vector>

using namespace LivingWorld;

namespace
{
void parseLocal166Ini(LivingWorldStores &stores, const std::string &text)
{
	INIEnvironment env;
	stores.registerBlocks(env.blocks);
	INI ini(env);
	std::vector<std::uint8_t> bytes(text.begin(), text.end());
	ini.loadMemory("local166-test.ini", bytes, INI_LOAD_OVERWRITE);
}

UnitInfo local166Units(const std::string &name)
{
	UnitInfo u;
	u.known = true;
	if (name == "GoodHero" || name == "EvilHero")
	{
		u.hero = true;
		u.buildCost = 1000;
		u.strength = 3000;
		u.autoResolveUnitType = AR_HERO;
		u.autoResolveBody = "AutoResolve_HeroBody";
		u.levels = { { 1, 1, 50 } };
	}
	else if (name == "GoodSoldiers" || name == "EvilSoldiers")
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

const char *kCommonLocal166Ini = R"(
LivingWorldPlayerTemplate PlayerGood
	Faction = FactionGood
	StartingWorldCP = 100
	MaxWorldCP = 300
	ScenarioStartResources = 1000
	ScenarioMaxResources = 5000
End
LivingWorldPlayerTemplate PlayerEvil
	Faction = FactionEvil
	StartingWorldCP = 100
	MaxWorldCP = 300
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
	Name = GoodHeroArmy
	ArmyEntry
		ThingTemplate = GoodHero
		Quantity = 1
	End
End
LivingWorldPlayerArmy
	Name = GoodGarrisonArmy
	ArmyEntry
		ThingTemplate = GoodSoldiers
		Quantity = 1
	End
End
LivingWorldPlayerArmy
	Name = EmptyEvilGarrison
End
LivingWorldPlayerArmy
	Name = EvilHeroArmy
	ArmyEntry
		ThingTemplate = EvilHero
		Quantity = 1
	End
End
)";

GameSetup makeTwoPlayerSetup(const std::string &campaignName, bool firstHuman = true)
{
	GameSetup s;
	s.campaign = campaignName;
	s.seed = 7;
	SetupSlot a;
	a.name = "Good";
	a.faction = "FactionGood";
	a.human = firstHuman;
	a.team = 1;
	SetupSlot b;
	b.name = "Evil";
	b.faction = "FactionEvil";
	b.human = true;
	b.team = 2;
	s.slots = { a, b };
	s.ranks = { { true, 5u, 0 } };
	return s;
}
} // namespace

// Expected behaviour from RotWK 2.01 LivingWorldRegion::SetupRegionOwnership (RW 0x7F4696)
// and LivingWorldLogic::findPlayerByName (RW 0x6B774C): non-empty OwnedByPlayer assigns the
// region to the matching player by exact name; unknown player names leave the region unowned.
TEST_CASE("LOCAL-166 LivingWorldRegion: SetupRegionOwnership (RW 0x7F4696) assigns OwnedByPlayer regions at start")
{
	const std::string ini = std::string(kCommonLocal166Ini) + R"(
LivingWorldRegionCampaign SetupOwnerCampaign
	Region A
		MapName = "MAP A"
		ConnectsTo = B
	End
	Region B
		MapName = "MAP B"
		OwnedByPlayer = Good
		ConnectsTo = A C
	End
	Region C
		MapName = "MAP C"
		OwnedByPlayer = UnknownPlayer
		ConnectsTo = B D
	End
	Region D
		MapName = "MAP D"
		ConnectsTo = C
	End
End
LivingWorldCampaign SetupOwnerScenario
	Scenario
		RegionCampaign = SetupOwnerCampaign
		MaxPlayers = 2
		DefaultStartSpots = A D
		PlayerDefeatCondition
			Teams = 1 2
			NumControlledRegionsLessOrEqualTo = 0
		End
	End
	Act One
	End
End
)";
	LivingWorldStores stores;
	parseLocal166Ini(stores, ini);
	LivingWorldLogic logic(stores, local166Units);
	std::string err;
	REQUIRE(logic.start(makeTwoPlayerSetup("SetupOwnerScenario"), &err));
	CHECK(err.empty());
	REQUIRE(logic.regions().size() == 4);
	CHECK(logic.regions()[0].owner == 0);  // A: Good's capital
	CHECK(logic.regions()[1].owner == 0);  // B: OwnedByPlayer = Good (RW 0x7F4696)
	CHECK(logic.regions()[2].owner == -1); // C: OwnedByPlayer = UnknownPlayer -> stays neutral
	CHECK(logic.regions()[3].owner == 1);  // D: Evil's capital
}

// Expected behaviour from RotWK 2.01 LivingWorldLogic::EnforceArmyRegionOwnership (RW 0x6B56F8):
// a non-garrison army standing in an unowned region (owner == -1) claims it for its owner;
// a garrison army (RW 0x6B5744) does not claim a neutral region, and an already owned region
// (RW 0x6B575B) is not reassigned.
TEST_CASE("LOCAL-166 LivingWorldLogic: EnforceArmyRegionOwnership (RW 0x6B56F8) awards neutral regions holding starting non-garrison armies")
{
	const std::string ini = std::string(kCommonLocal166Ini) + R"(
LivingWorldRegionCampaign EnforceCampaign
	Region A
		MapName = "MAP A"
		ConnectsTo = B
	End
	Region B
		MapName = "MAP B"
		ConnectsTo = A C
	End
	Region C
		MapName = "MAP C"
		ConnectsTo = B D
	End
	Region D
		MapName = "MAP D"
		ConnectsTo = C
	End
End
LivingWorldCampaign EnforceScenario
	Scenario
		RegionCampaign = EnforceCampaign
		MaxPlayers = 2
		DefaultStartSpots = A D
		PlayerDefeatCondition
			Teams = 1 2
			NumControlledRegionsLessOrEqualTo = 0
		End
	End
	Act One
		SpawnArmy
			ScriptingName = GoodHeroInB
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHero
			PlayerArmy = GoodHeroArmy
			InitialRegion = B
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = GoodGarrisonInC
			SpawnForTemplates = PlayerGood
			PlayerArmy = GoodGarrisonArmy
			InitialRegion = C
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = GoodHeroInD
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHero
			PlayerArmy = GoodHeroArmy
			InitialRegion = D
			SpawnAtActStart = Yes
		End
	End
End
)";
	LivingWorldStores stores;
	parseLocal166Ini(stores, ini);
	LivingWorldLogic logic(stores, local166Units);
	std::string err;
	REQUIRE(logic.start(makeTwoPlayerSetup("EnforceScenario"), &err));
	CHECK(err.empty());
	REQUIRE(logic.regions().size() == 4);
	CHECK(logic.regions()[0].owner == 0);  // A: Good's capital
	CHECK(logic.regions()[1].owner == 0);  // B: claimed by Good's non-garrison hero army (RW 0x6B56F8)
	CHECK(logic.regions()[2].owner == -1); // C: garrison army does not claim neutral region (RW 0x6B5744)
	CHECK(logic.regions()[3].owner == 1);  // D: already owned by Evil, not stolen (RW 0x6B575B)
}

// Expected behaviour from RotWK 2.01 LivingWorldLogic::OnRegionChangedOwnership (RW 0x6BB2EF):
// when a region changes ownership to a hostile player, any garrison in that region belonging
// to a player hostile to the new owner is removed (RW 0x6B540B -> RW 0x6B9679 / RW 0x6B87A9),
// while an allied garrison in the same region is preserved.
TEST_CASE("LOCAL-166 LivingWorldLogic: OnRegionChangedOwnership (RW 0x6BB2EF) removes hostile garrisons when a region changes owner")
{
	const std::string ini = std::string(kCommonLocal166Ini) + R"(
LivingWorldPlayerTemplate PlayerEvilAlly
	Faction = FactionEvilAlly
	StartingWorldCP = 100
	MaxWorldCP = 300
	ScenarioStartResources = 1000
	ScenarioMaxResources = 5000
End
LivingWorldPlayerArmy
	Name = GoodBigHeroArmy
	ArmyEntry
		ThingTemplate = GoodHero
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = GoodSoldiers
		Quantity = 3
	End
End
LivingWorldRegionCampaign OwnerChangeCampaign
	Region A
		MapName = "MAP A"
		ConnectsTo = B
	End
	Region B
		MapName = "MAP B"
		OwnedByPlayer = Evil
		ConnectsTo = A C D
	End
	Region C
		MapName = "MAP C"
		ConnectsTo = B
	End
	Region D
		MapName = "MAP D"
		ConnectsTo = B
	End
End
LivingWorldCampaign OwnerChangeScenario
	Scenario
		RegionCampaign = OwnerChangeCampaign
		MaxPlayers = 3
		DefaultStartSpots = A C D
		PlayerDefeatCondition
			Teams = 1 2
			NumControlledRegionsLessOrEqualTo = 0
		End
	End
	Act One
		SpawnArmy
			ScriptingName = GoodBigHeroInB
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHero
			PlayerArmy = GoodBigHeroArmy
			InitialRegion = B
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = GoodGarrisonInB
			SpawnForTemplates = PlayerGood
			PlayerArmy = GoodGarrisonArmy
			InitialRegion = B
			SpawnAtActStart = Yes
		End
		SpawnArmy
			ScriptingName = EvilAllyGarrisonInB
			SpawnForTemplates = PlayerEvilAlly
			PlayerArmy = EmptyEvilGarrison
			InitialRegion = B
			SpawnAtActStart = Yes
		End
	End
End
)";
	LivingWorldStores stores;
	parseLocal166Ini(stores, ini);
	LivingWorldLogic logic(stores, local166Units);
	GameSetup setup;
	setup.campaign = "OwnerChangeScenario";
	setup.seed = 7;
	SetupSlot s0;
	s0.name = "Good";
	s0.faction = "FactionGood";
	s0.human = true;
	s0.team = 1;
	SetupSlot s1;
	s1.name = "Evil";
	s1.faction = "FactionEvil";
	s1.human = true;
	s1.team = 2;
	SetupSlot s2;
	s2.name = "EvilAlly";
	s2.faction = "FactionEvilAlly";
	s2.human = true;
	s2.team = 2;
	setup.slots = { s0, s1, s2 };
	setup.ranks = { { true, 5u, 0 } };

	std::string err;
	REQUIRE(logic.start(setup, &err));
	CHECK(err.empty());
	REQUIRE(logic.regions()[1].owner == 1); // B starts owned by Evil (RW 0x7F4696)

	int evilAllyGarrisonBefore = 0;
	int goodGarrisonBefore = 0;
	for (const Army &a : logic.armies())
	{
		if (a.region == 1 && a.garrison && a.owner == 2)
		{
			++evilAllyGarrisonBefore;
		}
		if (a.region == 1 && a.garrison && a.owner == 0)
		{
			++goodGarrisonBefore;
		}
	}
	REQUIRE(evilAllyGarrisonBefore == 1);
	REQUIRE(goodGarrisonBefore == 1);

	// Evil surrenders: B transfers from Evil (team 2) to Good (team 1, largest army in B) via setRegionOwner.
	// OnRegionChangedOwnership (RW 0x6BB2EF) must remove EvilAlly's hostile garrison in B while keeping Good's garrison.
	logic.surrender(1);

	CHECK(logic.regions()[1].owner == 0);
	int evilAllyGarrisonAfter = 0;
	int goodGarrisonAfter = 0;
	for (const Army &a : logic.armies())
	{
		if (a.region == 1 && a.garrison && a.owner == 2)
		{
			++evilAllyGarrisonAfter;
		}
		if (a.region == 1 && a.garrison && a.owner == 0)
		{
			++goodGarrisonAfter;
		}
	}
	CHECK(evilAllyGarrisonAfter == 0);
	CHECK(goodGarrisonAfter == 1);
}

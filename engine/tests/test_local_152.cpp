// LOCAL-152 (fork issue 885): Ennorath 1.9.8.4 War of the Ring online —
//   1. >2 players in online / LAN War of the Ring (no 2-player cap in port; 6 -> 8 under ennorath.wotr-8-players, RW 0x840A91 / RW 0x84692B)
//   2. Land transfer without the host: RegionAwardDispute + MSG_LW_RESOLVE_REGION_AWARD_DISPUTE (0x6B7)
//      (RW 0x900E58 / RW 0x900EBA / RW 0x6BEE5B / RW 0x6BB99F; PYSAGE 1.9.8.5 .wtrdisp 0x00F2F078)
//   3. Non-participating human observer seat in LAN / online War of the Ring real-time battle lockstep (S-7849, .wotrobs 0x00F291BB / .wtrview 0x00F2A000)

#include "doctest.h"

#include "Common/INI.h"
#include "Common/ModEngineFeatures.h"
#include "GameClient/GUI/Skirmish/SkirmishGameSetup.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/LivingWorld/LivingWorldData.h"
#include "GameLogic/LivingWorld/LivingWorldLogic.h"
#include "GameNetwork/LANLobby.h"
#include "GameNetwork/Network.h"
#include "GameNetwork/Transport.h"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct EnnorathWotrGuard
{
	explicit EnnorathWotrGuard(bool on)
	{
		ModEngineFeatures::reset();
		if (on)
		{
			std::string error;
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &error), error);
		}
	}
	~EnnorathWotrGuard() { ModEngineFeatures::reset(); }
};

struct Local152SetupSource : SkirmishSetupSource
{
	std::vector<MapCacheEntry> mapList;
	std::vector<SkirmishFaction> factionList;
	std::vector<SkirmishColor> colorList;
	std::vector<int> cash{ 1000 };
	Local152SetupSource()
	{
		MapCacheEntry m;
		m.name = "maps/wotr/wotr.map";
		m.displayName = u"$Map:WotR";
		m.numPlayers = 8;
		m.isMultiplayer = true;
		m.isOfficial = true;
		mapList.push_back(m);
		for (int i = 0; i < 8; ++i)
		{
			SkirmishColor c;
			c.name = "Color" + std::to_string(i);
			c.rgb = 0x101010u * (unsigned)(i + 1);
			c.availableInWotR = true;
			colorList.push_back(c);
		}
	}
	const std::vector<SkirmishFaction> &factions() const override { return factionList; }
	const std::vector<SkirmishColor> &colors() const override { return colorList; }
	const std::vector<MapCacheEntry> &maps() const override { return mapList; }
	int defaultStartingCash() const override { return 1000; }
	const std::vector<int> &startingCashChoices() const override { return cash; }
	int mapContentsMask(const MapCacheEntry &) const override { return 1; }
	bool fileExists(const std::string &) const override { return true; }
	bool hasFileSystem() const override { return false; }
};

struct Local152Text : GameTextSource
{
	std::map<std::string, std::u16string> labels;
	void set(const std::string &k, const std::u16string &v) { labels[k] = v; }
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		auto it = labels.find(label);
		if (it == labels.end())
		{
			return false;
		}
		out = it->second;
		return true;
	}
};

const char *const kLocal152WotrIni = R"INI(
LivingWorldPlayerTemplate PlayerGood
	Faction = FactionGood
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
	BuildingScoreFarm = 50
End
LivingWorldPlayerArmy
	Name = GoodHeroArmy
	ArmyEntry
		ThingTemplate = GoodHero
		Quantity = 1
	End
	ArmyEntry
		ThingTemplate = GoodSoldiers
		Quantity = 2
	End
End
LivingWorldRegionCampaign Local152Campaign
	SmallArmyCommandPoints = 120
	Region R0
		MapName = "MAP R0"
		ConnectsTo = R3
	End
	Region R1
		MapName = "MAP R1"
		ConnectsTo = R3
	End
	Region R2
		MapName = "MAP R2"
		ConnectsTo = R3
	End
	Region R3
		MapName = "MAP R3"
		ConnectsTo = R0 R1 R2
	End
	Region R4
		MapName = "MAP R4"
	End
End
LivingWorldCampaign Local152Scenario
	Scenario
		RegionCampaign = Local152Campaign
		MaxPlayers = 8
		MinPlayers = 2
		DefaultStartSpots = R0 R1 R2 R4
		PlayerDefeatCondition
			Teams = 1 2 3
			NumControlledRegionsLessOrEqualTo = 0
		End
		TeamDefeatCondition
			Teams = 1 2 3
			NumControlledRegionsLessOrEqualTo = 0
		End
	End
	Act One
		SpawnArmy
			ScriptingName = StartArmy
			SpawnForTemplates = PlayerGood
			HeroTemplateName = GoodHero
			PlayerArmy = GoodHeroArmy
			Icon = GoodHeroIcon
		End
	End
End
)INI";

LivingWorld::LivingWorldStores &local152WotrStores()
{
	static LivingWorld::LivingWorldStores stores;
	static bool loaded = false;
	if (!loaded)
	{
		INIEnvironment env;
		stores.registerBlocks(env.blocks);
		INI ini(env);
		const std::string text = kLocal152WotrIni;
		std::vector<std::uint8_t> bytes(text.begin(), text.end());
		ini.loadMemory("local152-wotr.ini", bytes, INI_LOAD_OVERWRITE);
		loaded = true;
	}
	return stores;
}

LivingWorld::UnitInfo lookupLocal152Unit(const std::string &name)
{
	LivingWorld::UnitInfo u;
	u.known = true;
	u.autoResolveCombatChain = "AutoResolve_DefaultCombatChain";
	if (name == "GoodHero")
	{
		u.hero = true;
		u.buildCost = 1000;
		u.strength = 1000;
		u.autoResolveUnitType = LivingWorld::AR_HERO;
		u.autoResolveBody = "AutoResolve_HeroBody";
		u.levels = { { 1, 1, 50 } };
	}
	else
	{
		u.buildCost = 300;
		u.commandPoints = 20;
		u.strength = 300;
		u.autoResolveUnitType = LivingWorld::AR_SOLDIER;
		u.autoResolveBody = "AutoResolve_DefaultBody";
	}
	return u;
}

} // namespace

// Source: RW 0x840A91 / RW 0x84692B (6 -> 8 in Ennorath 1.9.8.4 / 1.9.8.5; port has no 2-player online War of the Ring limit)
TEST_CASE("LOCAL-152: War of the Ring online / LAN has no 2-player limit and lifts 6 -> 8 under ennorath.wotr-8-players")
{
	Local152SetupSource src;
	for (const char *name : { "FactionMen", "FactionMordor" })
	{
		SkirmishFaction f;
		f.templateName = name;
		f.side = name;
		f.playableSide = true;
		src.factionList.push_back(f);
	}
	Local152Text text;
	text.set("LAN:TooManyPlayers", u"Too many players (max %d)");

	auto checkHumans = [&](int count) {
		SkirmishGameSetup setup(src, &text);
		REQUIRE(setup.init(u"Host", 7, "maps/wotr/wotr.map", nullptr));
		setup.setStrategicMode(true);
		SkirmishGameInfo g = setup.info();
		for (int i = 0; i < count; ++i)
		{
			g.slots[i].state = SLOT_PLAYER;
			g.slots[i].name = u"P" + std::u16string(1, char16_t('0' + i));
			g.slots[i].accepted = true;
			g.slots[i].hasMap = true;
			g.slots[i].teamNumber = i % 2;
			g.slots[i].playerTemplate = i % 2;
			g.slots[i].startPos = i;
		}
		setup.replaceInfo(g);
		setup.setStrategicScenario({ 8, 2, {} });
		return setup.checkStart(0x30); // online/LAN flags: 0x10 (2+ humans) | 0x20 (all accepted)
	};

	{
		EnnorathWotrGuard off(false);
		CHECK(checkHumans(3).ok); // no 2-player cap in port
		CHECK(checkHumans(6).ok); // retail 2.01 cap = 6
		const auto over = checkHumans(8);
		CHECK_FALSE(over.ok);
		CHECK(over.refusalLabel == "LAN:TooManyPlayers");
	}
	{
		EnnorathWotrGuard on(true);
		CHECK(checkHumans(3).ok);
		CHECK(checkHumans(8).ok); // Ennorath 1.9.8.4 / 1.9.8.5: up to 8 online humans
	}
}

// Source: RW 0x900E58 (RegionAwardDispute::getDecidingPlayerId), RW 0x900EBA, RW 0x6BEE5B, RW 0x6BB99F;
// PYSAGE 1.9.8.5 .wtrdisp 0x00F2F078 (hook RW 0x900EBA): host-free land transfer in War of the Ring.
TEST_CASE("LOCAL-152: War of the Ring allied region award dispute requires host in retail 2.01 and resolves without host under ennorath.wotr-8-players")
{
	auto &stores = local152WotrStores();
	auto makeSetup = [&](bool p1Human, bool p2Human) {
		LivingWorld::GameSetup s;
		s.campaign = "Local152Scenario";
		s.seed = 77;
		s.ranks = { { true, 5u, 0 }, { true, 1u, 100 } };
		// Player 0 = Host (Team 0), Player 1 = Ally A (Team 1), Player 2 = Ally B (Team 1), Player 3 = Enemy (Team 2)
		const char *starts[4] = { "R0", "R1", "R2", "R4" };
		const int teams[4] = { 0, 1, 1, 2 };
		const bool humans[4] = { true, p1Human, p2Human, false };
		for (int i = 0; i < 4; ++i)
		{
			LivingWorld::SetupSlot ps;
			ps.name = "P" + std::to_string(i);
			ps.faction = "FactionGood";
			ps.human = humans[i];
			ps.team = teams[i];
			ps.startRegion = starts[i];
			s.slots.push_back(ps);
		}
		return s;
	};

	auto buildNonHostDispute = [&](LivingWorld::LivingWorldLogic &lw, bool p1Human, bool p2Human) {
		std::string error;
		REQUIRE_MESSAGE(lw.start(makeSetup(p1Human, p2Human), &error), error);
		lw.setHostPlayer(0);
		int army1 = -1, army2 = -1;
		for (const auto &a : lw.armies())
		{
			if (!a.garrison && a.owner == 1) army1 = a.id;
			if (!a.garrison && a.owner == 2) army2 = a.id;
		}
		REQUIRE(army1 >= 0); // tolerance: hero army id assigned by LivingWorldLogic
		REQUIRE(army2 >= 0); // tolerance: hero army id assigned by LivingWorldLogic
		REQUIRE_MESSAGE(lw.orderMove(1, army1, 3, &error), error);
		REQUIRE_MESSAGE(lw.orderMove(2, army2, 3, &error), error);
		for (int i = 0; i < 4; ++i)
		{
			if (lw.players()[(size_t)i].human)
			{
				REQUIRE_MESSAGE(lw.endTurn(i, &error), error);
			}
		}
		for (int step = 0; step < 6 && lw.awardDisputes().empty(); ++step)
		{
			lw.update();
		}
	};

	// 1. Two non-host allied humans (P1 & P2, host is P0):
	//    Retail 2.01 (RW 0x900EBA): deciding player is -1, MSG_LW_RESOLVE_REGION_AWARD_DISPUTE is refused.
	//    Ennorath (.wtrdisp 0x00F2F078): deciding player is P1 (first human contender), P1 can transfer R3 to P2.
	{
		LivingWorld::LivingWorldLogic lw(stores, lookupLocal152Unit);
		buildNonHostDispute(lw, true, true);
		REQUIRE(lw.awardDisputes().size() == 1);
		const auto &d = lw.awardDisputes()[0];
		CHECK(d.region == 3);
		CHECK_FALSE(d.resolved);
		CHECK(lw.regions()[3].owner == -1);

		GameMessage award(MSG_LW_RESOLVE_REGION_AWARD_DISPUTE, 1);
		award.appendIntegerArgument(3);
		award.appendIntegerArgument(2);

		{
			EnnorathWotrGuard off(false);
			CHECK(lw.disputeDecidingPlayer(d) == -1);
			CHECK_FALSE(lw.hasUnresolvedDisputeForPlayer(1));
			std::string err;
			CHECK_FALSE(lw.dispatch(award, 1, &err));
			CHECK(lw.regions()[3].owner == -1);
		}
		{
			EnnorathWotrGuard on(true);
			CHECK(lw.disputeDecidingPlayer(d) == 1);
			CHECK(lw.hasUnresolvedDisputeForPlayer(1));
			CHECK_FALSE(lw.hasUnresolvedDisputeForPlayer(2));

			// Save/load round-trip (xfer v8) preserves award disputes and state hash
			std::vector<std::uint8_t> saved;
			std::string err;
			REQUIRE_MESSAGE(lw.saveState(saved, &err), err);
			LivingWorld::LivingWorldLogic restored(stores, lookupLocal152Unit);
			REQUIRE_MESSAGE(restored.loadState(makeSetup(true, true), saved, &err), err);
			CHECK(restored.stateHash() == lw.stateHash());
			REQUIRE(restored.awardDisputes().size() == 1);
			CHECK(restored.disputeDecidingPlayer(restored.awardDisputes()[0]) == 1);

			// Non-decider (P2) is refused; decider (P1) transfers R3 to P2
			CHECK_FALSE(lw.dispatch(award, 2, &err));
			REQUIRE_MESSAGE(lw.dispatch(award, 1, &err), err);
			CHECK(lw.regions()[3].owner == 2);
			CHECK(lw.awardDisputes()[0].resolved);
			CHECK_FALSE(lw.hasUnresolvedDisputeForPlayer(1));
		}
	}

	// 2. Neighboring behavior: host (P0) IS among the 2 human contenders -> both retail 2.01 and Ennorath pick host P0 (RW 0x900EC7).
	{
		LivingWorld::LivingWorldLogic lw(stores, lookupLocal152Unit);
		std::string err;
		REQUIRE_MESSAGE(lw.start(makeSetup(true, true), &err), err);
		lw.setHostPlayer(0);
		LivingWorld::RegionAwardDispute d;
		d.region = 3;
		d.contenders = { 1, 0 }; // host is second in list, still picked by RW 0x900EC7
		{
			EnnorathWotrGuard off(false);
			CHECK(lw.disputeDecidingPlayer(d) == 0);
		}
		{
			EnnorathWotrGuard on(true);
			CHECK(lw.disputeDecidingPlayer(d) == 0);
		}
	}

	// 3. Neighboring behavior: 1 human (P1) + 1 AI (P2) -> both pick human P1 (RW 0x900E8A); 0 humans -> AI-only auto-resolves to contenders[0] (RW 0x900E7B).
	{
		LivingWorld::LivingWorldLogic lw(stores, lookupLocal152Unit);
		buildNonHostDispute(lw, true, false);
		REQUIRE(lw.awardDisputes().size() == 1);
		EnnorathWotrGuard off(false);
		CHECK(lw.disputeDecidingPlayer(lw.awardDisputes()[0]) == 1);
	}
	{
		LivingWorld::LivingWorldLogic lw(stores, lookupLocal152Unit);
		buildNonHostDispute(lw, false, false);
		CHECK(lw.regions()[3].owner == 1);
	}
}

// Source: PYSAGE 1.9.8.5 .wotrobs 0x00F291BB / .wtrview 0x00F2A000 (hooks RW 0x627C41 / RW 0x62FE9C); S-7849.
TEST_CASE("LOCAL-152: War of the Ring 3-peer battle lockstep carries non-participating human observer seat (S-7849)")
{
	// Verify LobbyStart war-slot observer helpers
	LobbyStart observerStart;
	observerStart.battleUsesWarSlots = true;
	observerStart.battleSlotOfWarSlot.fill(-1);
	observerStart.battleSlotOfWarSlot[0] = 0;
	observerStart.battleSlotOfWarSlot[1] = 1;
	observerStart.localSlot = 2;
	CHECK(observerStart.gameSlot(0) == 0);
	CHECK(observerStart.gameSlot(1) == 1);
	CHECK(observerStart.gameSlot(2) == -1);
	CHECK(observerStart.isWarBattleObserver());

	LobbyStart fighterStart = observerStart;
	fighterStart.localSlot = 0;
	CHECK_FALSE(fighterStart.isWarBattleObserver());

	// 3 real loopback UDP peers: slot 0 (player 0), slot 1 (player 1), slot 2 (non-participating War of the Ring human observer: slotPlayerIndex = -1, observerSlot = true).
	UDP socks[3];
	std::string err;
	for (int i = 0; i < 3; ++i)
	{
		REQUIRE_MESSAGE(socks[i].bind(0x7F000001u, 0, &err), err);
	}

	std::unique_ptr<Transport> transports[3];
	std::unique_ptr<Network> nets[3];
	for (int i = 0; i < 3; ++i)
	{
		Transport::Options to;
		transports[i] = std::make_unique<Transport>(socks[i], i, to);
		for (int j = 0; j < 3; ++j)
		{
			if (j != i)
			{
				transports[i]->setPeer(j, socks[j].localAddress());
			}
		}
		NetworkConfig cfg;
		cfg.localSlot = i;
		cfg.runAhead = 1;
		cfg.crcInterval = 5;
		cfg.slotPlayerIndex[0] = 0;
		cfg.slotPlayerIndex[1] = 1;
		cfg.slotPlayerIndex[2] = -1;
		cfg.observerSlot[2] = true; // observer seat: active lockstep peer with no tactical player
		nets[i] = std::make_unique<Network>(cfg, *transports[i]);
		nets[i]->sendLoadComplete();
	}

	// Complete LOADCOMPLETE barrier across all 3 peers (2 fighters + 1 observer)
	CommandList emptyPending;
	bool allLoaded = false;
	for (int iter = 0; iter < 400 && !allLoaded; ++iter)
	{
		allLoaded = true;
		for (int i = 0; i < 3; ++i)
		{
			nets[i]->update(0, emptyPending);
			allLoaded = allLoaded && nets[i]->allLoaded();
		}
		if (!allLoaded)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}
	REQUIRE(allLoaded);

	// Step 12 lockstep frames with CRC exchange (including observer peer 2) and a fighter order from slot 0
	UnsignedInt frames[3] = { 0, 0, 0 };
	CommandList pendings[3];
	bool sentOrder = false;
	int observerSawFighterCmd = 0;
	for (int iter = 0; iter < 800 && (frames[0] < 12 || frames[1] < 12 || frames[2] < 12); ++iter)
	{
		for (int i = 0; i < 3; ++i)
		{
			if (i == 0 && frames[i] == 2 && !sentOrder)
			{
				GameMessage m(MSG_DO_STOP, 0);
				pendings[i].append(m);
				sentOrder = true;
			}
			nets[i]->update(frames[i], pendings[i]);
			if (frames[i] < 12 && nets[i]->isFrameReady(frames[i]))
			{
				CommandList ready;
				nets[i]->relayCommands(frames[i], ready);
				if (i == 2)
				{
					for (const GameMessage &m : ready.messages())
					{
						if (m.getType() == MSG_DO_STOP && m.getPlayerIndex() == 0)
						{
							++observerSawFighterCmd;
						}
					}
				}
				++frames[i];
				if (frames[i] % 5 == 0)
				{
					FrameCompletion fc;
					fc.frame = frames[i];
					fc.hashed = true;
					fc.breakdown = true;
					fc.hash = 0x12340000u + frames[i];
					nets[i]->frameCompleted(fc, pendings[i]);
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	CHECK(frames[0] == 12);
	CHECK(frames[1] == 12);
	CHECK(frames[2] == 12);
	CHECK(observerSawFighterCmd == 1);
	for (int i = 0; i < 3; ++i)
	{
		for (const std::string &e : nets[i]->errors())
		{
			FAIL_CHECK("peer " << i << " error: " << e);
		}
		CHECK(nets[i]->crcChecksPassed() == 2);
		CHECK(nets[i]->desyncs().empty());
	}
}

// OpenBFME. GPL-3.0.
// LOCAL-144 (fork issue 861): Ennorath 1.9.8.5 pySAGE edits —
//   1. ennorath.replay-tabs-chat-pause (.rpchat 0xED7000, .rppause 0xF2E000, .rppvar 0xF2D000)
//   2. ennorath.wotr-8-players (.wotrui 0xF28000, .wotrobs 0xF29000, .wtrview 0xF2A000, .wtrdisp 0xF2F000, .lobfac 0xED9000,
//      and in-place edits RW 0x840A91, RW 0x84692B, RW 0x6BEBE5).
// Expectations come from PYSAGE 1.9.8.5 disassembly and Ennorath 1.9.8.5 data (############enrth_data_wotr8.big).

#include "Common/INI.h"
#include "Common/ModEngineFeatures.h"
#include "Common/Recorder.h"
#include "GameClient/GUI/Skirmish/LivingWorldSetupText.h"
#include "GameClient/GUI/Skirmish/SkirmishGameSetup.h"
#include "GameClient/ReplaySeek.h"
#include "GameLogic/LivingWorld/LivingWorldData.h"
#include "GameLogic/LivingWorld/LivingWorldLogic.h"
#include "GameLogic/LivingWorld/LivingWorldUnits.h"

#include "doctest.h"

#include <map>
#include <string>
#include <vector>

namespace
{
using namespace LivingWorld;

struct Ennorath1985Guard
{
	explicit Ennorath1985Guard(bool enable)
	{
		ModEngineFeatures::reset();
		if (enable)
		{
			std::string error;
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &error), error);
		}
	}
	~Ennorath1985Guard() { ModEngineFeatures::reset(); }
};

struct Local144SetupSource : SkirmishSetupSource
{
	std::vector<MapCacheEntry> mapList;
	std::vector<SkirmishFaction> factionList;
	std::vector<SkirmishColor> colorList;
	std::vector<int> cash{ 1000 };
	Local144SetupSource()
	{
		MapCacheEntry m;
		m.name = "maps/four/four.map";
		m.displayName = u"$Map:Four";
		m.numPlayers = 4;
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

struct Local144Text : GameTextSource
{
	std::map<std::string, std::u16string> map;
	void set(const std::string &k, const std::u16string &v) { map[k] = v; }
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		auto it = map.find(label);
		if (it == map.end()) return false;
		out = it->second;
		return true;
	}
};

const char *const kLocal144WotrIni = R"INI(
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
LivingWorldRegionCampaign Local144Campaign
	SmallArmyCommandPoints = 120
	Region A
		MapName = "MAP A"
		ConnectsTo = B C
	End
	Region B
		MapName = "MAP B"
		ConnectsTo = A C
	End
	Region C
		MapName = "MAP C"
		ConnectsTo = A B
	End
End
LivingWorldCampaign Local144Scenario
	Scenario
		RegionCampaign = Local144Campaign
		MaxPlayers = 8
		MinPlayers = 2
		DefaultStartSpots = A B C
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

LivingWorldStores &local144WotrStores()
{
	static LivingWorldStores stores;
	static bool loaded = false;
	if (!loaded)
	{
		INIEnvironment env;
		stores.registerBlocks(env.blocks);
		INI ini(env);
		const std::string text = kLocal144WotrIni;
		std::vector<std::uint8_t> bytes(text.begin(), text.end());
		ini.loadMemory("local144-wotr.ini", bytes, INI_LOAD_OVERWRITE);
		loaded = true;
	}
	return stores;
}

UnitInfo local144Units(const std::string &name)
{
	UnitInfo u;
	u.known = true;
	u.autoResolveCombatChain = "AutoResolve_DefaultCombatChain";
	if (name == "GoodHero")
	{
		u.hero = true;
		u.buildCost = 1000;
		u.strength = 1000;
		u.autoResolveUnitType = AR_HERO;
		u.autoResolveBody = "AutoResolve_HeroBody";
		u.levels = { { 1, 1, 50 } };
	}
	else
	{
		u.buildCost = 300;
		u.commandPoints = 20;
		u.strength = 300;
		u.autoResolveUnitType = AR_SOLDIER;
		u.autoResolveBody = "AutoResolve_DefaultBody";
	}
	return u;
}
} // namespace

// Source: PYSAGE 1.9.8.5 .rpchat 0x00ED7B40 / 0x00ED7C80 (hooks RW 0x78419D, RW 0x77DCFC, RW 0x77DE0F, RW 0x77F7B7)
TEST_CASE("LOCAL-144: replay chat record round-trip, 504 UTF-16 cap, and ReplayPlayback::takeChatsUpTo")
{
	ReplayWriter writer;
	writer.setKeepCopy(true);
	ReplayHeader header;
	header.game.game.mapName = "maps/test/test.map";
	std::string error;
	REQUIRE_MESSAGE(writer.open("", header, &error), error);

	writer.recordFrame(0, CommandList{});
	writer.recordChat(10, 0, 0xFF0000, "Player1", "gl hf");
	// 510 ASCII chars -> capped to 0x1F8 (504) UTF-16 code units (.rpchat 0x00ED7B96)
	const std::string longLine(510, 'A');
	writer.recordChat(25, 1, 0x00FF00, "Player2", longLine);
	// 503 ASCII + 1 4-byte emoji (2 UTF-16 units) -> capped at 503 so surrogate pair is not split (.rpchat 0x00ED7BA3..0x00ED7BB6)
	const std::string surrogateBoundary = std::string(503, 'B') + "\xF0\x9F\x98\x80";
	writer.recordChat(30, 2, 0x0000FF, "Player3", surrogateBoundary);
	writer.recordHash(35, 0x12345678u);

	std::vector<std::uint8_t> bytes;
	REQUIRE_MESSAGE(writer.logUpTo(35, 0x12345678u, bytes, &error), error);

	ReplayFile parsed;
	REQUIRE_MESSAGE(ReplayFile::parse(bytes, parsed, &error), error);
	REQUIRE(parsed.chats.size() == 3);
	CHECK(parsed.chats[0].frame == 10);
	CHECK(parsed.chats[0].slot == 0);
	CHECK(parsed.chats[0].color == 0xFF0000);
	CHECK(parsed.chats[0].sender == "Player1");
	CHECK(parsed.chats[0].text == "gl hf");
	CHECK(parsed.chats[1].text.size() == 504);
	CHECK(parsed.chats[2].text.size() == 503);

	ReplayPlayback playback(parsed);
	CHECK(playback.takeChatsUpTo(9).empty());
	auto batch1 = playback.takeChatsUpTo(10);
	REQUIRE(batch1.size() == 1);
	CHECK(batch1[0].text == "gl hf");
	CHECK(playback.takeChatsUpTo(10).empty()); // delivered once
	auto batch2 = playback.takeChatsUpTo(35);
	REQUIRE(batch2.size() == 2);
	playback.resetChatCursor();
	CHECK(playback.takeChatsUpTo(35).size() == 3);
}

// Source: PYSAGE 1.9.8.5 .rppause 0x00F2E000 / .rppvar 0x00F2D000 (hooks RW 0x63F1B9, RW 0x6325D5, RW 0x77F66B)
TEST_CASE("LOCAL-144: replay Ctrl+] pause toggle gated by ennorath.replay-tabs-chat-pause")
{
	const int ctrlDown = ReplayPauseInput::kStateDown | 0x04;
	const int rctrlDown = ReplayPauseInput::kStateDown | 0x08;
	{
		Ennorath1985Guard off(false);
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown, true, false));
	}
	{
		Ennorath1985Guard on(true);
		CHECK(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown, true, false));
		CHECK(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, rctrlDown, true, false));
		// Rejected when Shift/Alt held, autorepeat, modal open, non-replay, or wrong key
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown | 0x10, true, false));
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown | 0x40, true, false));
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown | ReplayPauseInput::kStateAutoRepeat, true, false));
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown, false, false));
		CHECK_FALSE(ReplayPauseInput::shouldToggle(ReplayPauseInput::kKeyRightBracket, ctrlDown, true, true));
		CHECK_FALSE(ReplayPauseInput::shouldToggle(0x1A, ctrlDown, true, false));
	}
}

// Source: PYSAGE 1.9.8.5 .wtrdisp 0x00F2F0E8..0x00F2F3BA (hooks RW 0x9760B4, RW 0x9765B3, RW 0x976BA8) & Ennorath wotrscenario001/042/043.inc
TEST_CASE("LOCAL-144: War of the Ring 8-player scenario display name and description rewrite (.wtrdisp 0xF2F0E8)")
{
	LivingWorldScenarioEntry sc001;
	sc001.name = "WOTRScenario001";
	sc001.displayLabel = "LWScenario:WOTRScenario001";
	sc001.descriptionLabel = "LWScenario:WOTRScenario001Description";
	sc001.maxPlayers = 8;

	LivingWorldScenarioEntry sc042;
	sc042.name = "WOTRScenario042";
	sc042.displayLabel = "LWScenario:WOTRScenario042";
	sc042.descriptionLabel = "LWScenario:WOTRScenario042Description";
	sc042.maxPlayers = 8;

	LivingWorldScenarioEntry sc043;
	sc043.name = "WOTRScenario043";
	sc043.displayLabel = "LWScenario:WOTRScenario043";
	sc043.descriptionLabel = "LWScenario:WOTRScenario043Description";
	sc043.maxPlayers = 8;

	Local144Text text;
	text.set(sc001.displayLabel, u"Capital Assault (2-6)");
	text.set(sc001.descriptionLabel, u"Players: 2\u20136\nConquer capitals.");
	text.set(sc042.displayLabel, u"War of the North (2)");
	text.set(sc042.descriptionLabel, u"Players: 2\nFight in the north.");
	text.set(sc043.displayLabel, u"War of the South (1vs1)");
	text.set(sc043.descriptionLabel, u"Fight 2-2 in the south.");

	{
		Ennorath1985Guard off(false);
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc001, &text) == u"Capital Assault (2-6)");
		CHECK(LivingWorldSetupText::scenarioDescription(sc001, -1, &text) == u"Players: 2\u20136\nConquer capitals.");
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc042, &text) == u"War of the North (2)");
		CHECK(LivingWorldSetupText::scenarioDescription(sc042, -1, &text) == u"Players: 2\nFight in the north.");
	}
	{
		Ennorath1985Guard on(true);
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc001, &text) == u"Capital Assault (2-8)");
		CHECK(LivingWorldSetupText::scenarioDescription(sc001, -1, &text) == u"Players: 2\u20138\nConquer capitals.");
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc042, &text) == u"War of the North (2\u20138)");
		CHECK(LivingWorldSetupText::scenarioDescription(sc042, -1, &text) == u"Players: 2\u20138\nFight in the north.");
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc043, &text) == u"War of the South (2\u20138)");
		CHECK(LivingWorldSetupText::scenarioDescription(sc043, -1, &text) == u"Fight 2-8 in the south.");
		// Non-8-player scenario is left untouched
		LivingWorldScenarioEntry sc6 = sc001;
		sc6.maxPlayers = 6;
		CHECK(LivingWorldSetupText::scenarioDisplayName(sc6, &text) == u"Capital Assault (2-6)");
	}
}

// Source: RW 0x840A91 / RW 0x84692B (6 -> 8 in Ennorath 1.9.8.5)
TEST_CASE("LOCAL-144: War of the Ring lobby start check caps at 6 in retail 2.01 and allows 8 with ennorath.wotr-8-players")
{
	Local144SetupSource src;
	for (const char *name : { "FactionMen", "FactionMordor" })
	{
		SkirmishFaction f;
		f.templateName = name;
		f.side = name;
		f.playableSide = true;
		src.factionList.push_back(f);
	}
	Local144Text text;
	text.set("LAN:TooManyPlayers", u"Too many players (max %d)");

	auto runCheck = [&]() {
		SkirmishGameSetup setup(src, &text);
		REQUIRE(setup.init(u"Host", 7, "maps/four/four.map", nullptr));
		setup.setStrategicMode(true);
		SkirmishGameInfo g = setup.info();
		for (int i = 0; i < 8; ++i)
		{
			g.slots[i].state = (i == 0) ? SLOT_PLAYER : SLOT_EASY_AI;
			g.slots[i].teamNumber = i % 2;
			g.slots[i].playerTemplate = i % 2;
			g.slots[i].startPos = i;
		}
		setup.replaceInfo(g);
		setup.setStrategicScenario({ 8, 2, {} });
		return setup.checkStart();
	};

	{
		Ennorath1985Guard off(false);
		const auto c = runCheck();
		CHECK_FALSE(c.ok);
		CHECK(c.refusalLabel == "LAN:TooManyPlayers");
	}
	{
		Ennorath1985Guard on(true);
		const auto c = runCheck();
		CHECK(c.ok);
	}
}

// Source: RW 0x6BEBE5 (jge -> jmp in Ennorath 1.9.8.5)
TEST_CASE("LOCAL-144: War of the Ring networkVote forces auto-resolve for partial-human battles in retail 2.01 and allows real-time with ennorath.wotr-8-players")
{
	auto runVote = [&](bool enableEnnorath) -> bool {
		Ennorath1985Guard guard(enableEnnorath);
		LivingWorldLogic lw(local144WotrStores(), local144Units);
		GameSetup s;
		s.campaign = "Local144Scenario";
		s.seed = 42;
		s.ranks = { { true, 5u, 0 }, { true, 1u, 100 }, { true, 1u, 200 } };
		const char *regions[3] = { "A", "B", "C" };
		for (int i = 0; i < 3; ++i)
		{
			SetupSlot ps;
			ps.name = "P" + std::to_string(i);
			ps.faction = "FactionGood";
			ps.human = true;
			ps.team = i;
			ps.startRegion = regions[i];
			s.slots.push_back(ps);
		}
		std::string error;
		REQUIRE_MESSAGE(lw.start(s, &error), error);
		lw.setNetworked(0, 1); // battleChoice = 1 (RTS wins tie)
		REQUIRE(lw.armies().size() == 3);
		int army0 = -1;
		for (const auto &a : lw.armies())
			if (a.owner == 0 && !a.garrison) { army0 = a.id; break; }
		REQUIRE(army0 >= 0); // tolerance: hero army id assigned by LivingWorldLogic
		REQUIRE_MESSAGE(lw.orderMove(0, army0, 1, &error), error);
		for (int i = 0; i < 3; ++i)
			REQUIRE_MESSAGE(lw.endTurn(i, &error), error);
		lw.update();
		REQUIRE(lw.battles().size() == 1);
		// Both battle participants (P0 and P1) vote real-time (method 3 -> bit 8), while P2 is alive and not in the battle
		auto makeVote = [](int player, int method, int battle) {
			GameMessage m(MSG_LW_SET_CONFLICT_RESOLUTION_METHOD, player);
			m.appendIntegerArgument(method);
			m.appendIntegerArgument(battle);
			return m;
		};
		REQUIRE_MESSAGE(lw.dispatch(makeVote(0, 3, 0), 0, &error), error);
		REQUIRE_MESSAGE(lw.dispatch(makeVote(1, 3, 0), 1, &error), error);
		BattleSetup setup;
		return lw.takeBattleSetup(setup);
	};

	CHECK_FALSE(runVote(false)); // retail 2.01: active (2) < liveHumans (3) -> forced auto-resolve
	CHECK(runVote(true));        // Ennorath 1.9.8.5 (RW 0x6BEBE5): real-time battle starts
}

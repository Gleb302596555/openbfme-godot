// OpenBFME. GPL-3.0. No installed audio, objects, or terrain files needed.
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/LargeGroupAudioGrid.h"
#include "GameClient/LiveGame.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "IniTestUtil.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "MapTestUtil.h"
#include "doctest.h"

namespace
{
struct Crowd
{
	initest::Fixture fx;
	AudioIniState ini;
	const LargeGroupAudioMap *map;
	const std::vector<std::string> unit{"Boots"}, cell{"BurningCell"};
	Crowd(const std::string &extra = "")
	{
		ini.registerBlocks(fx.env.blocks);
		REQUIRE(initest::loadError(fx.env, "crowd.ini",
								   "AudioEvent DefaultSoundEffect\n Type = DEFAULT\nEnd\nAudioEvent Rustle\n Sounds = rustle\nEnd\n"
								   "LargeGroupAudioMap Crowd\n Sound Feet\n Sound = Rustle\n Key = Boots BurningCell\n End\n"
								   " Size = 100\n StartThreshold = 4\n StopThreshold = 3\n" +
									   extra + "End\n")
					.empty());
		map = ini.largeGroupAudio.find("Crowd");
		REQUIRE(map);
		REQUIRE(map->sounds.size() == 1);
	}
	LargeGroupAudioEvent member(float x, float y, unsigned short weight, bool object) const
	{
		LargeGroupAudioEvent e;
		e.x = x;
		e.y = y;
		e.weight = weight;
		e.keys = object ? &unit : &cell;
		if (object)
			e.object = 17;
		return e;
	}
	void send(LargeGroupAudioGrid &g, const LargeGroupAudioEvent &e)
	{
		g.notify(*map, map->sounds.front(), e, 0);
	}
	void tick(LargeGroupAudioGrid &g)
	{
		bool allocated = false;
		g.advance(map->startThreshold, map->stopThreshold, 7, allocated);
	}
};
} // namespace
TEST_CASE("PORT-290 weighted object and two anonymous members choose the doubly shifted cell")
{
	Crowd c;
	LargeGroupAudioGrid g(c.map->size, 1000, 1000);
	// RW 0x97037B, RW 0x971326: only grid 3 contains all four units of weight.
	// The two anonymous contributions deliberately have the same (invalid) ObjectID.
	c.send(g, c.member(195, 195, 2, true));
	auto anon = c.member(205, 205, 1, false);
	c.send(g, anon);
	c.tick(g);
	CHECK(g.playing.empty());
	c.send(g, anon);
	c.tick(g);
	REQUIRE(g.playing.size() == 1);
	auto winner = *g.playing.begin();
	CHECK(winner == LargeGroupAudioGrid::Cell(4, 4));
	// RW 0x9D7DF6: opposite neighbour weights balance around the (200,200) cell centre.
	CHECK(g.position(winner).x == 200);
	CHECK(g.position(winner).y == 200);
	anon.kind = LargeGroupAudioEvent::REMOVE;
	anon.storedX = anon.x;
	anon.storedY = anon.y;
	anon.storedFrame = 1;
	c.send(g, anon);
	c.tick(g);
	CHECK(g.playing.count(winner) == 1); // StopThreshold is inclusive
	c.send(g, anon);
	c.tick(g);
	CHECK(g.playing.empty());
}
TEST_CASE("PORT-290 tie order is the port's allocation order")
{
	Crowd c;
	LargeGroupAudioGrid g(c.map->size, 1000, 1000);
	c.send(g, c.member(210, 210, 4, true));
	c.tick(g);
	// INFERENCE, not a retail parity oracle: all four overlapping interior cells have equal weight and priority.
	REQUIRE(g.playing.size() == 1);
	CHECK(*g.playing.begin() == LargeGroupAudioGrid::Cell(5, 5));
}
TEST_CASE("PORT-290 Sound key, model, status, stealth and stored frame filter member transitions")
{
	Crowd c(" RequiredModelConditionFlags = MOVING\n ExcludedObjectStatusBits = INSIDE_GARRISON\n IgnoreStealthedUnits = Yes\n");
	LargeGroupAudioGrid g(c.map->size, 1000, 1000);
	auto e = c.member(210, 210, 4, true);
	// RW 0x7EF032 / RW 0x7EEF0D / RW 0x7EF0A1. Check admission, not an ambiguous winning coordinate.
	c.send(g, e);
	CHECK(g.members == 0);
	e.conditions = c.map->requiredModelConditionFlags;
	e.status = c.map->excludedObjectStatusBits;
	c.send(g, e);
	CHECK(g.members == 0);
	e.status = {};
	std::vector<std::string> wrong{"boots"};
	e.keys = &wrong;
	c.send(g, e);
	CHECK(g.members == 0);
	e.keys = &c.unit;
	e.stealthed = true;
	c.send(g, e);
	CHECK(g.members == 0);
	e.stealthed = false;
	c.send(g, e);
	REQUIRE(g.members == 1);
	e.kind = LargeGroupAudioEvent::UPDATE;
	e.storedConditions = e.conditions;
	e.storedX = e.x;
	e.storedY = e.y;
	e.stealthed = true;
	c.send(g, e);
	CHECK(g.members == 1); // stored frame == gate: never remove
	e.storedFrame = 1;
	c.send(g, e);
	CHECK(g.members == 0);
	c.tick(g);
	CHECK(g.playing.empty());
}

namespace
{
using namespace maptest;
// The game is deliberately a bare 16x16 map with only the default neutral player,
// no objects, retail mounts, scripted events or renderer. Inputs use an in-memory BIG.
// The INI values are only the mandatory settings needed by LiveGame::load; they are
// not purported to be values extracted from a retail installation.
const char kGameData[] = R"(
GameData
 ForceModelsToFollowTimeOfDay = No
 ForceModelsToFollowWeather = No
 UnitDamagedThreshold = 0.5
 UnitReallyDamagedThreshold = 0.25
 DefaultStartingCash = 100
 TerrainResourceCellSize = 10
 GoodCommandPoints = 10 100
 EvilCommandPoints = 10 100
 GoodCommandPointsAI = 10 100
 EvilCommandPointsAI = 10 100
 GoodCommandPointsBonus = 0
 EvilCommandPointsBonus = 0
 GoodCommandPointLimit = 100
 EvilCommandPointLimit = 100
 PowerLimit = 100
 ResourceBonusMultiplier = 1
 ResourceMultiplierLimit = 1
 MultiPlayMoneyMult = MP1:1.0
 GoodCommandPointsMP2 = 10 100
 GoodCommandPointsMP3 = 10 100
 GoodCommandPointsMP4 = 10 100
 GoodCommandPointsMP5 = 10 100
 GoodCommandPointsMP6 = 10 100
 GoodCommandPointsMP7 = 10 100
 GoodCommandPointsMP8 = 10 100
 EvilCommandPointsMP2 = 10 100
 EvilCommandPointsMP3 = 10 100
 EvilCommandPointsMP4 = 10 100
 EvilCommandPointsMP5 = 10 100
 EvilCommandPointsMP6 = 10 100
 EvilCommandPointsMP7 = 10 100
 EvilCommandPointsMP8 = 10 100
 PartitionCellSize = 10
 UnlookPersistDuration = 1000
 ShroudColor = R:0 G:0 B:0
 FogAlpha = 100
 ShroudAlpha = 200
 StealthFriendlyOpacity = 50%
 MaxPathfindCellsPerFrame = 100
 MaxCellsAdjustDestination = 100
 MaxCellsAdjustHordeMeleeDestination = 100
 MaxCellsPatchPath = 100
 MaxCellsFindPathLimit = 100
 MaxCellsFindAttackPath = 100
 MaxCellsAdjustToMeleeDestination = 100
 MaxCellsFindMeleeEngagementLocation = 100
 MaxCellsAdjustToPossibleDestination = 100
 MaxCellsToExamineTowardsGoal = 100
 MovementPenaltyDamageState = DAMAGED
End
)";
const char kAIData[] = R"(
AIData
 EnableRepulsors = No
 RepulsedDistance = 1
 WadeWaterDepth = 1
 DeepWaterDepth = 2
 HordesWaitForHordes = No
 MeleeApproachTolerance = 1
 MeleeApproachDist = 1
 CastleSiegeStandBackDistance = 1
 AttackUsesLineOfSight = No
End
)";
const char kMultiplayer[] = R"(
MultiplayerSettings
 InitialCreditsVeryLow = 100
 InitialCreditsLow = 100
 InitialCreditsMedium = 100
 InitialCreditsHigh = 100
 InitialCreditsVeryHigh = 100
 UseShroud = No
End
)";

std::string tinyMap()
{
	Toc t;
	W h;
	h.i32(16).i32(16).i32(0).i32(1).i32(16).i32(16).i32(256);
	for (int i = 0; i < 256; ++i)
		h.u16(0);
	const Bytes chunks = cat(chunk(t, "HeightMapData", 5, h.b), chunk(t, "WorldInfo", 1, D(t).i("weather", 0).bytes()));
	const Bytes map = file(t, chunks);
	return std::string(map.begin(), map.end());
}

} // namespace
TEST_CASE("PORT-290 live client selects the fire sound across the base grid boundary")
{
	Crowd c;
	c.fx.mount({{"Maps\\synthetic\\synthetic.map", tinyMap()},
				{"Data\\INI\\Default\\SoundEffects.ini", "AudioEvent DefaultSoundEffect\n Type = DEFAULT\nEnd\n"},
				{"Data\\INI\\GameData.ini", kGameData},
				{"Data\\INI\\Default\\AIData.ini", kAIData},
				{"Data\\INI\\Multiplayer.ini", kMultiplayer},
				{"Data\\Scripts\\Scripts.lua", "-- synthetic"},
				{"Data\\Scripts\\ScriptEvents.xml", "<SageLuaScriptSection>\n</SageLuaScriptSection>\n"}});
	RetailObjectWorld world(c.fx.fsys);
	std::string error;
	REQUIRE_MESSAGE(world.load(&error), error);
	ArchiveW3DFileSource source(c.fx.fsys);
	WW3DAssetManager assets(source);
	MapObjectOptions opt;
	LiveGame game(world, c.fx.fsys, assets, opt);
	LiveGame::Options opts;
	opts.mapName = "synthetic";
	opts.mapScripts = 0;
	REQUIRE_MESSAGE(game.load(opts, &error), error);
	AudioAssetCache cache(&c.fx.fsys, 1 << 20);
	SimulatedAudioDevice device(&cache);
	AudioManager mgr(c.ini, cache, device, RandomAlgorithm::RotWK_GameDat_LCG, 1);
	LiveGameAudio client(game, mgr);
	auto &link = game.logic().largeGroupAudio();
	link.enable(0);
	auto a = c.member(95, 95, 2, true), b = c.member(105, 105, 1, false);
	a.frame = b.frame = 1;
	link.notify(a);
	link.notify(b);
	link.notify(b);
	client.runLargeGroupAudio();
	// RW 0x971326 / RW 0x971FA7: only the doubly shifted grid reaches four. The old object-ID/single-grid sink starts nothing.
	const auto groups = client.largeGroups();
	REQUIRE(groups.size() == 1);
	CHECK(groups.begin()->first.find("Crowd/Feet/") == 0);
	CHECK(groups.begin()->second.first.x == 100);
	CHECK(groups.begin()->second.first.y == 100);
}

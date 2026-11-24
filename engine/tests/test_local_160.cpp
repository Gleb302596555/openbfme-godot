// OpenBFME unit tests (Local 160, fork issue 927): script engine core conditions from RW_BINARY_MAP.tsv:
//   1. PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT (condition 161, RW 0x007E6C7C, callback RW 0x007E6C0F,
//      unit filter RW 0x006618E6, absent-object fallback RW 0x007E6CE5 -> distSq = -1.0f RW 0x00BD19DC).
//   2. HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS (condition 186, RW 0x007E6017, msvcr71!ceil RW 0x00BD0588,
//      Eva::findEvent RW 0x005DDEA2, Eva::getLastReallyPlayedFrameForEvaEvent RW 0x005DCD4F).
// Synthetic tests only: no retail or mod files are required.
// GPL-3.0.

#include "doctest.h"

#include "IniTestUtil.h"
#include "ProdTestUtil.h"
#include "ScriptTestUtil.h"

#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameClient/Eva.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace prodtest;
using namespace scripttest;

namespace
{

const char kLocal160Ini[] =
	"Object L160Infantry\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10.0\n"
	"  GeometryHeight = 10.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"End\n"
	"Object L160Horde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HORDE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20.0\n"
	"  GeometryHeight = 10.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"  End\n"
	"End\n"
	"Object L160Inert\n"
	"  KindOf = PRELOAD INFANTRY INERT\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10.0\n"
	"  GeometryHeight = 10.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object L160Structure\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20.0\n"
	"  GeometryHeight = 20.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

const char kLocal160EvaIni[] =
	"AudioSettings\n"
	"  AudioRoot = Data\\Audio\n"
	"  SoundsFolder = Sounds\n"
	"  MusicFolder = Music\n"
	"  StreamingFolder = Speech\n"
	"  SoundsExtension = .wav\n"
	"End\n"
	"DialogEvent L160AlertSound\n"
	"  Filename = l160alert.wav\n"
	"  Volume = 100\n"
	"End\n"
	"NewEvaEvent L160UnderAttack\n"
	"  Priority = 50\n"
	"  TimeBetweenEventsMS = 1000\n"
	"  ExpirationTimeMS = 5000\n"
	"  SideSound\n"
	"    Side = Men\n"
	"    Sound = L160AlertSound\n"
	"  End\n"
	"End\n";

struct Local160Fx : ProdWorld
{
	std::vector<ScriptList> lists = std::vector<ScriptList>(3);
	Player *p0 = nullptr;
	Player *p1 = nullptr;

	Local160Fx()
	{
		p0 = players.findPlayerWithName("Alice");
		p1 = players.findPlayerWithName("Bob");
		REQUIRE(p0 != nullptr);
		REQUIRE(p1 != nullptr);
		load(kLocal160Ini);
		logic->settings().bodyThresholdsLoaded = true;
		logic->settings().unitDamagedThreshold = 0.65f;
		logic->settings().unitReallyDamagedThreshold = 0.4f;
		logic->partition().setRegion(0.0f, 0.0f, 1200.0f, 1200.0f);
	}

	Object *spawn(const char *tmplName, Team *team, float x, float y, float z = 0.0f)
	{
		const ThingTemplate *tt = logic->things().findTemplate(tmplName);
		REQUIRE_MESSAGE(tt != nullptr, tmplName);
		Object *o = logic->newObject(tt, team, ObjectStatusMaskType{});
		REQUIRE(o != nullptr);
		const Coord3D pos{ x, y, z };
		o->setPosition(&pos);
		logic->partition().registerObject(*o);
		logic->partition().update();
		return o;
	}

	bool evalScriptCond(ScriptCondition c, int sideIdx = 1)
	{
		auto s = std::make_unique<Script>();
		s->name = "L160Check";
		s->isActive = true;
		s->isOneShot = false;
		s->easy = s->normal = s->hard = true;
		s->orConditions.push_back({ { std::move(c) } });
		s->actions.push_back(act("SET_FLAG", { sp(5, "L160Fired"), ip(8, 1) }));

		lists[(size_t)sideIdx].items.clear();
		lists[(size_t)sideIdx].items.push_back(item(std::move(s)));
		std::vector<ScriptEngine::SideScripts> sides(3);
		const char *names[] = { "", "Alice", "Bob" };
		for (size_t i = 0; i < 3; ++i)
		{
			sides[i].sideName = names[i];
			sides[i].lists.push_back(&lists[i]);
		}
		logic->scriptEngine().newGame(sides, nullptr, nullptr, false);
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->getName().empty())
			{
				logic->scriptEngine().objectNamed(*o);
			}
		}
		logic->scriptEngine().update();
		const bool *f = logic->scriptEngine().findFlag("Alice/L160Fired");
		return f != nullptr && *f;
	}
};

std::vector<std::uint8_t> makeSilentWav(double ms)
{
	const std::uint32_t rate = 22050;
	const std::uint32_t frames = (std::uint32_t)(ms * rate / 1000.0);
	std::vector<std::uint8_t> out;
	auto u32 = [&](std::uint32_t v) {
		for (int i = 0; i < 4; ++i)
		{
			out.push_back((std::uint8_t)(v >> (8 * i)));
		}
	};
	auto u16 = [&](std::uint16_t v) {
		out.push_back((std::uint8_t)v);
		out.push_back((std::uint8_t)(v >> 8));
	};
	auto tag = [&](const char *t) { out.insert(out.end(), t, t + 4); };
	const std::uint32_t dataBytes = frames * 2;
	tag("RIFF");
	u32(36 + dataBytes);
	tag("WAVE");
	tag("fmt ");
	u32(16);
	u16(1);
	u16(1);
	u32(rate);
	u32(rate * 2);
	u16(2);
	u16(16);
	tag("data");
	u32(dataBytes);
	out.resize(out.size() + dataBytes, 0);
	return out;
}

} // namespace

// RW 0x007E6C7C (evaluatePlayerHasNumberUnitsDistanceFromObject), RW 0x007E6C0F, RW 0x006618E6, RW 0x007E6CE5 -> RW 0x00BD19DC
TEST_CASE("LOCAL-160 PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT: KindOf filter, 3D distance, and absent-object fallback (RW 0x007E6C7C)")
{
	Local160Fx fx;
	Team *aliceTeam = fx.p0->getDefaultTeam();
	REQUIRE(aliceTeam != nullptr);

	Object *home = fx.spawn("L160Structure", aliceTeam, 100.0f, 100.0f, 0.0f);
	home->setName("HomeBase");

	// Exactly on the 2D boundary (dx=100, dy=0, dz=0 -> distSq == 10000): NOT strictly greater (RW 0x007E6C62 comiss/jbe)
	fx.spawn("L160Infantry", aliceTeam, 200.0f, 100.0f, 0.0f);
	// Same 2D distance (100), but dz=10 -> 3D distSq = 10100 > 10000: counts!
	fx.spawn("L160Infantry", aliceTeam, 200.0f, 100.0f, 10.0f);
	// Far away (dx=300): counts!
	fx.spawn("L160Infantry", aliceTeam, 400.0f, 100.0f, 0.0f);
	// Excluded by RW 0x006618E6 negative KindOf mask {STRUCTURE, INERT, HORDE} even though far away:
	fx.spawn("L160Horde", aliceTeam, 500.0f, 100.0f, 0.0f);
	fx.spawn("L160Inert", aliceTeam, 500.0f, 100.0f, 0.0f);
	fx.spawn("L160Structure", aliceTeam, 500.0f, 100.0f, 0.0f);

	auto makeCond = [](const char *player, int op, int count, float dist, const char *named) {
		return cond("PLAYER_HAS_NUMBER_UNITS_DISTANCE_FROM_OBJECT",
			{ sp(11, player), ip(6, op), ip(0, count), rp(dist), sp(14, named) });
	};

	// Expect exactly 2 units strictly farther than 100.0 in 3D (the dz=10 one and the dx=300 one)
	CHECK(fx.evalScriptCond(makeCond("Alice", 2 /* op EQ */, 2, 100.0f, "HomeBase")));
	CHECK_FALSE(fx.evalScriptCond(makeCond("Alice", 2 /* op EQ */, 3, 100.0f, "HomeBase")));
	CHECK(fx.evalScriptCond(makeCond("Alice", 3 /* op GE */, 2, 100.0f, "HomeBase")));
	CHECK_FALSE(fx.evalScriptCond(makeCond("Alice", 4 /* op GT */, 2, 100.0f, "HomeBase")));

	// Absent named object (RW 0x007E6CE5): origin=(0,0,0), distSq=-1.0f (RW 0x00BD19DC), so ALL 3 matching infantry units count!
	CHECK(fx.evalScriptCond(makeCond("Alice", 2 /* op EQ */, 3, 100.0f, "MissingObject")));
	CHECK(fx.evalScriptCond(makeCond("Alice", 3 /* op GE */, 3, 9999.0f, "MissingObject")));
	CHECK_FALSE(fx.evalScriptCond(makeCond("Alice", 2 /* op EQ */, 0, 100.0f, "MissingObject")));

	// Neighbor check: Bob has 0 matching units
	CHECK(fx.evalScriptCond(makeCond("Bob", 2 /* op EQ */, 0, 100.0f, "HomeBase")));
}

// RW 0x007E6017 (evaluateHasEvaEventPlayedInLastNSeconds), RW 0x00BD0588 (ceil), RW 0x005DCD4F
TEST_CASE("LOCAL-160 HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS: ceil(fps*sec) logic-frame window (RW 0x007E6017)")
{
	Local160Fx fx;
	initest::Fixture audioFx;
	const std::vector<std::uint8_t> wav = makeSilentWav(200.0);
	audioFx.mount({ { "data\\audio\\speech\\l160alert.wav", std::string((const char *)wav.data(), wav.size()) } });
	AudioIniState audioIni;
	audioIni.registerBlocks(audioFx.env.blocks);
	const std::string err = initest::loadError(audioFx.env, "eva.ini", kLocal160EvaIni);
	REQUIRE_MESSAGE(err.empty(), err);
	AudioAssetCache cache(&audioFx.fsys, 4u << 20);
	SimulatedAudioDevice device(&cache);
	AudioManager audioMgr(audioIni, cache, device, RandomAlgorithm::ZH_CarryChain, 7);
	Eva eva(audioIni, audioMgr);
	eva.setLocalSide("Men");
	eva.setLocalPlayerIndex(fx.p0->getPlayerIndex());

	Eva *savedEva = AudioApi::currentEva();
	AudioApi::installEva(&eva);
	struct Guard
	{
		Eva *prev;
		~Guard() { AudioApi::installEva(prev); }
	} guard{ savedEva };

	auto makeCond = [](const char *evName, float seconds) {
		return cond("HAS_EVA_EVENT_PLAYED_IN_LAST_N_SECONDS", { sp(0, evName), rp(seconds) });
	};

	// Unknown event and never-played event both return false (RW 0x007E6049 / 0x007E605E)
	CHECK_FALSE(fx.evalScriptCond(makeCond("NoSuchEvent", 10.0f)));
	CHECK_FALSE(fx.evalScriptCond(makeCond("L160UnderAttack", 10.0f)));

	// Advance logic to frame 10 and play the Eva event at logicFrame=10
	for (int i = 0; i < 10; ++i)
	{
		for (int ph = 1; ph <= 6; ++ph) { fx.logic->update(ph); }
	}
	REQUIRE(fx.logic->getFrame() == 10);
	const int evIdx = eva.findEvent("L160UnderAttack");
	REQUIRE(evIdx == 22);
	REQUIRE(eva.reportEvent(evIdx, nullptr, 1000.0));
	eva.update(1000.0, fx.logic->getFrame());
	std::uint32_t playedFrame = 0;
	REQUIRE(eva.lastPlayedFrame(evIdx, &playedFrame));
	CHECK(playedFrame == 10);

	// 1.5s * 5 FPS = 7.5 -> ceil(7.5) = 8 frames (RW 0x007E602A msvcr71!ceil)
	// At frame 10: elapsed = 0 <= 8 -> true
	CHECK(fx.evalScriptCond(makeCond("L160UnderAttack", 1.5f)));

	// Advance 8 frames to frame 18: elapsed = 8 <= 8 -> still true
	for (int i = 0; i < 8; ++i)
	{
		for (int ph = 1; ph <= 6; ++ph) { fx.logic->update(ph); }
	}
	REQUIRE(fx.logic->getFrame() == 18);
	CHECK(fx.evalScriptCond(makeCond("L160UnderAttack", 1.5f)));
	// With 1.4s -> ceil(7.0) = 7 frames: elapsed = 8 > 7 -> false
	CHECK_FALSE(fx.evalScriptCond(makeCond("L160UnderAttack", 1.4f)));

	// Advance 1 more frame to frame 19: elapsed = 9 > 8 -> false
	for (int ph = 1; ph <= 6; ++ph) { fx.logic->update(ph); }
	REQUIRE(fx.logic->getFrame() == 19);
	CHECK_FALSE(fx.evalScriptCond(makeCond("L160UnderAttack", 1.5f)));
}

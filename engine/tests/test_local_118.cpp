// OpenBFME unit tests for Local 118 (fork issue 802): LargeGroupAudioUpdate uncited functions and getDisabledTypesToProcess.
// Every expectation is derived from RotWK 2.01 game.dat instructions (RW 0x... cited per test), DONOR BFME2 1.06 (0x004AB7C8 .. 0x004ABB4B),
// and Ennorath 1.9.8.3 INI data (data/ini/largegroupaudio.ini, data/ini/object/goodfaction/units/men/gondorfighter.ini).

#include "doctest.h"

#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "IniTestUtil.h"
#include "LogicTestUtil.h"

#include "Common/Audio/AudioAssetCache.h"
#include "Common/Audio/AudioIni.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/INI.h"
#include "GameClient/LargeGroupAudioGrid.h"
#include "GameClient/LiveGame.h"
#include "GameClient/LiveGameAudio.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LargeGroupAudioLink.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/LargeGroupAudioUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>
#include <vector>

namespace ModelCondition
{
int indexOf(const std::string &name);
}

namespace
{
struct LgaCapture118 : LargeGroupAudioSink
{
	std::vector<LargeGroupAudioEvent> events;
	void onLargeGroupAudioEvent(const LargeGroupAudioEvent &e) override { events.push_back(e); }
};

struct LgaGridSink118 : LargeGroupAudioSink
{
	const LargeGroupAudioMap *map = nullptr;
	LargeGroupAudioGrid *grid = nullptr;
	const LargeGroupAudioLink *link = nullptr;
	void onLargeGroupAudioEvent(const LargeGroupAudioEvent &e) override
	{
		if (map && grid && !map->sounds.empty() && link)
		{
			grid->notify(*map, map->sounds.front(), e, link->gateFrame());
		}
	}
};

const char kLgaObjects118[] =
	"Object Grunt118\n"
	"  KindOf = INFANTRY\n"
	"  Behavior = LargeGroupAudioUpdate ModuleTag_LGAU\n"
	"    Key = Orc Unit\n"
	"    UnitWeight = 2\n"
	"  End\n"
	"End\n";

const LargeGroupAudioUpdateModuleData *findLgaData118(const ThingTemplate &tt)
{
	for (const auto &n : tt.behaviorModules().nuggets())
	{
		if (const auto *d = dynamic_cast<const LargeGroupAudioUpdateModuleData *>(n.data.get()))
		{
			return d;
		}
	}
	return nullptr;
}

LargeGroupAudioUpdate *findLgaModule118(Object &o)
{
	for (const auto &m : o.modules())
	{
		if (auto *u = dynamic_cast<LargeGroupAudioUpdate *>(m.get()))
		{
			return u;
		}
	}
	return nullptr;
}

bool hasConditionBit118(const ModelConditionMask &mask, int bitIndex)
{
	return bitIndex >= 0 && (mask[(unsigned)bitIndex >> 5] & (1u << ((unsigned)bitIndex & 31u))) != 0u;
}
} // namespace

// RW 0x008AF0D0 (update interface RW 0xC6B408 slot 1; DONOR BFME2 0x004AB9D5): LargeGroupAudioUpdate::getDisabledTypesToProcess copies
// DISABLEDMASK_ALL (RW 0xDE8B90) via RW 0x007B3CE0, and fillEvent packages the 11 member interface getters (RW 0xC6B3D0 slots 0..10:
// RW 0x008AF17C, RW 0x008AF18E, RW 0x009D39B1, RW 0x008AF1A5, RW 0x008F1161, RW 0x008AF1AE, RW 0x008AF1B7, RW 0x008AF1BF, RW 0x008AF1C6,
// RW 0x008A4A91, RW 0x006DA64A).
TEST_CASE("LOCAL-118: LargeGroupAudioUpdate::getDisabledTypesToProcess returns DISABLEDMASK_ALL so disabled members wake, draw RNG, and notify (RW 0x008AF0D0)")
{
	logictest::LogicWorld f;
	LargeGroupAudioUpdate::registerClass(f.w.modules);
	const std::string err = f.w.load(kLgaObjects118, INI_LOAD_OVERWRITE, "lga118.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	LgaCapture118 sink;
	LargeGroupAudioLink &link = f.logic->largeGroupAudio();
	link.setSink(&sink);
	link.enable(f.logic->getFrame()); // RW 0x0060D4D3
	const UnsignedInt gate = f.logic->getFrame();

	Object *o = f.make("Grunt118");
	REQUIRE(o != nullptr);
	LargeGroupAudioUpdate *m = findLgaModule118(*o);
	REQUIRE(m != nullptr);

	// RW 0x008AF0D0: copies DISABLEDMASK_ALL (RW 0xDE8B90)
	CHECK(m->getDisabledTypesToProcess() == (DisabledMaskType)0xFFFFFFFFu);

	// Advance past the initial post-gate wake so storedFrame == current frame.
	while (sink.events.empty() && f.logic->getFrame() < gate + 10)
	{
		f.logic->runLogicFrame();
	}
	REQUIRE(sink.events.size() == 1);
	const std::int32_t firstStoredFrame = m->storedFrame();
	CHECK(firstStoredFrame == (std::int32_t)f.logic->getFrame());

	// Disable the object (DISABLED_HELD) and change both its model condition (STUNNED) and its position while disabled.
	const int kStunned = ModelCondition::indexOf("STUNNED");
	const Coord3D oldPos = *o->getPosition();
	Coord3D newPos = oldPos;
	newPos.x += 45.0f;
	newPos.y += 15.0f;
	o->setDisabled(DISABLED_HELD, UPDATE_SLEEP_FOREVER);
	CHECK(o->getDisabledMask() == (DisabledMaskType)(1u << DISABLED_HELD));
	o->setModelConditionState(kStunned, true);
	o->setPosition(&newPos);

	f.logic->random().enableCallLog(true);
	f.logic->random().clearCallLog();
	while (sink.events.size() == 1 && f.logic->getFrame() < (UnsignedInt)firstStoredFrame + 6u)
	{
		f.logic->runLogicFrame();
	}

	// Because getDisabledTypesToProcess() is DISABLEDMASK_ALL (RW 0x008AF0D0), GameLogic still wakes LargeGroupAudioUpdate::update() (RW 0x008AF246)
	// while the object is DISABLED_HELD: it draws logic RNG (RW 0x008AEEC3) and sends UPDATE (RW 0x0060D5CB) with the member interface state.
	size_t lgaDraws = 0;
	for (const auto &c : f.logic->random().callLog())
	{
		if (c.file == "LargeGroupAudioUpdate.cpp" && c.line == 0xA7)
		{
			++lgaDraws;
		}
	}
	CHECK(lgaDraws == 1u);
	REQUIRE(sink.events.size() == 2);
	const LargeGroupAudioEvent &ev = sink.events.back();
	CHECK(ev.kind == LargeGroupAudioEvent::UPDATE);
	CHECK(ev.storedX == oldPos.x);                               // RW 0x008AF17C (slot 0)
	CHECK(ev.storedY == oldPos.y);
	CHECK(ev.x == newPos.x);                                     // RW 0x008AF18E (slot 1)
	CHECK(ev.y == newPos.y);
	CHECK_FALSE(hasConditionBit118(ev.storedConditions, kStunned)); // RW 0x009D39B1 (slot 2)
	CHECK(hasConditionBit118(ev.conditions, kStunned));          // RW 0x008AF1A5 (slot 3)
	CHECK(ev.weight == 2);                                       // RW 0x008AF1B7 (slot 6)
	CHECK(ev.keys == &findLgaData118(*f.w.get("Grunt118"))->m_keys); // RW 0x008AF1BF (slot 7)
	CHECK_FALSE(ev.stealthed);                                   // RW 0x008AF1C6 (slot 8)
	CHECK_FALSE(ev.storedStealthed);                             // RW 0x008A4A91 (slot 9)
	CHECK((std::int32_t)ev.storedFrame == firstStoredFrame);     // RW 0x006DA64A (slot 10)
	CHECK(m->storedFrame() == (std::int32_t)ev.frame);
	link.setSink(nullptr);
}

// Neighbor behavior: RW 0x008AF0D0 + RW 0x008AF246 + LargeGroupAudioGrid::notify (RW 0x007EEF0D): when a disabled member gains an excluded
// model condition (STUNNED), its update notification removes its weight from the grid and stops the playing cell sound below StopThreshold.
TEST_CASE("LOCAL-118: LargeGroupAudioGrid drops a disabled member that becomes STUNNED and stops the cell below StopThreshold (RW 0x008AF0D0, RW 0x008AF246)")
{
	initest::Fixture audioFx;
	AudioIniState audioIni;
	audioIni.registerBlocks(audioFx.env.blocks);
	const std::string audioErr = initest::loadError(
		audioFx.env,
		"lga_grid118.ini",
		"AudioEvent DefaultSoundEffect\n Type = DEFAULT\nEnd\n"
		"AudioEvent OrcMarchLoop\n Sounds = march\n Control = loop\nEnd\n"
		"LargeGroupAudioMap OrcMarchMap\n"
		"  Sound March\n"
		"    Sound = OrcMarchLoop\n"
		"    Key = Orc\n"
		"  End\n"
		"  ExcludedModelConditionFlags = STUNNED DYING\n"
		"  Size = 200\n"
		"  StartThreshold = 4\n"
		"  StopThreshold = 3\n"
		"End\n");
	REQUIRE_MESSAGE(audioErr.empty(), audioErr);
	const LargeGroupAudioMap *map = audioIni.largeGroupAudio.find("OrcMarchMap");
	REQUIRE(map != nullptr);

	LargeGroupAudioGrid grid(map->size, 1000.0f, 1000.0f);
	logictest::LogicWorld f;
	LargeGroupAudioUpdate::registerClass(f.w.modules);
	REQUIRE(f.w.load(kLgaObjects118, INI_LOAD_OVERWRITE, "lga118.ini").empty());

	LargeGroupAudioLink &link = f.logic->largeGroupAudio();
	LgaGridSink118 sink;
	sink.map = map;
	sink.grid = &grid;
	sink.link = &link;
	link.setSink(&sink);
	link.enable(f.logic->getFrame());

	Object *a = f.make("Grunt118");
	Object *b = f.make("Grunt118");
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	const Coord3D pos{ 250.0f, 250.0f, 0.0f };
	a->setPosition(&pos);
	b->setPosition(&pos);

	// Step 6 frames so both modules wake after the gate frame and add their weight (2 + 2 = 4 >= StartThreshold 4).
	for (int i = 0; i < 6; ++i)
	{
		f.logic->runLogicFrame();
	}
	bool allocated = false;
	grid.advance(map->startThreshold, map->stopThreshold, f.logic->getFrame(), allocated);
	CHECK(grid.members == 2);
	CHECK(grid.playing.size() == 1);

	// Disable unit `b` (DISABLED_PARALYZED) and set STUNNED (excluded by OrcMarchMap).
	const int kStunned = ModelCondition::indexOf("STUNNED");
	b->setDisabled(DISABLED_PARALYZED, UPDATE_SLEEP_FOREVER);
	b->setModelConditionState(kStunned, true);

	for (int i = 0; i < 6; ++i)
	{
		f.logic->runLogicFrame();
	}
	allocated = false;
	grid.advance(map->startThreshold, map->stopThreshold, f.logic->getFrame(), allocated);
	CHECK(grid.members == 1);
	CHECK(grid.playing.empty());
	link.setSink(nullptr);
}

// Ennorath scenario: GondorFighter (data/ini/object/goodfaction/units/men/gondorfighter.ini:998-1000: Key = Humanoid_Male Man Man_Male Unit Infantry
// Gondor_Soldier, UnitWeight = 1) and data/ini/largegroupaudio.ini (3_2_SmallGroupMoving lines 1654-1727: Sound = SoldierGroupMoveLoop,
// RequiredModelConditionFlags = MOVING, StartThreshold = 3, StopThreshold = 2; SmallGroupCelebrating lines 281-302: Sound = ManGroupSmallCheer,
// RequiredModelConditionFlags = EMOTION_CELEBRATING, ExcludedModelConditionFlags = STUNNED ..., StartThreshold = 4, StopThreshold = 3).
TEST_CASE("LOCAL-118 ENNORATH scenario: GondorFighter group marching near the camera plays SoldierGroupMoveLoop and stops ManGroupSmallCheer when stunned while disabled (RW 0x008AF0D0, RW 0x008AF246)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-118 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	retailtest::Mount *mount = world.shared.mount;
	REQUIRE(mount != nullptr);

	const ThingTemplate *fighterTmpl = world.shared.world->things().findTemplate("GondorFighter");
	REQUIRE(fighterTmpl != nullptr);
	const LargeGroupAudioUpdateModuleData *fighterLga = findLgaData118(*fighterTmpl);
	REQUIRE(fighterLga != nullptr);
	CHECK(fighterLga->m_keys == std::vector<std::string>{ "Humanoid_Male", "Man", "Man_Male", "Unit", "Infantry", "Gondor_Soldier" });
	CHECK(fighterLga->m_unitWeight == 1);
	CHECK(fighterLga->m_timeBetweenUpdatesMin == 3u);
	CHECK(fighterLga->m_timeBetweenUpdatesVariation == 1u);

	INIEnvironment env;
	env.fileSystem = mount->fs.get();
	AudioIniState ini;
	ini.registerBlocks(env.blocks);
	INI reader(env);
	ini.loadAll(reader);
	ini.loadLargeGroupAudio(reader);

	const LargeGroupAudioMap *moveMap = ini.largeGroupAudio.find("3_2_SmallGroupMoving");
	REQUIRE(moveMap != nullptr);
	CHECK(moveMap->startThreshold == 3);
	CHECK(moveMap->stopThreshold == 2);

	const LargeGroupAudioMap *cheerMap = ini.largeGroupAudio.find("SmallGroupCelebrating");
	REQUIRE(cheerMap != nullptr);
	CHECK(cheerMap->startThreshold == 4);
	CHECK(cheerMap->stopThreshold == 3);

	AudioAssetCache cache(mount->fs.get(), 32u << 20);
	SimulatedAudioDevice device(&cache);
	AudioManager mgr(ini, cache, device, RandomAlgorithm::RotWK_GameDat_LCG, 7);
	LiveGameAudio client(*game.game, mgr);

	const Coord3D nearCam{ 1200.0f, 1200.0f, 0.0f };
	mgr.setListenerPosition(nearCam, Coord3D{ 0.0f, 1.0f, 0.0f });

	std::vector<Object *> fighters;
	std::string error;
	for (int i = 0; i < 5; ++i)
	{
		Object *u = game.game->createObject("GondorFighter", game.index(), Coord3D{ 1200.0f + (float)(i * 8), 1200.0f, 0.0f }, 0.0f, &error);
		REQUIRE_MESSAGE(u != nullptr, error);
		LargeGroupAudioUpdate *mod = findLgaModule118(*u);
		REQUIRE(mod != nullptr);
		CHECK(mod->getDisabledTypesToProcess() == (DisabledMaskType)0xFFFFFFFFu);
		fighters.push_back(u);
	}

	const int kMoving = ModelCondition::indexOf("MOVING");
	const int kCelebrating = ModelCondition::indexOf("EMOTION_CELEBRATING");
	const int kStunned = ModelCondition::indexOf("STUNNED");

	// 1. Marching near the camera: order the 5 GondorFighters to march to (1500, 1200) so AIUpdate sets MOVING, exceeding StartThreshold = 3 of 3_2_SmallGroupMoving.
	for (Object *u : fighters)
	{
		REQUIRE(u->getAIUpdateInterface() != nullptr);
		u->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 1500.0f, 1200.0f, 0.0f }, CMD_FROM_PLAYER);
	}
	for (int f = 0; f < 10; ++f)
	{
		game.logic().runLogicFrame();
		client.runLargeGroupAudio();
	}
	auto hasGroupMap = [&](const std::string &mapPrefix) {
		for (const auto &kv : client.largeGroups())
		{
			if (kv.first.find(mapPrefix + "/") == 0)
			{
				return true;
			}
		}
		return false;
	};
	CHECK(hasGroupMap("3_2_SmallGroupMoving"));

	// 2. Stop marching and celebrate: 5 GondorFighters with EMOTION_CELEBRATING start SmallGroupCelebrating (StartThreshold = 4).
	for (Object *u : fighters)
	{
		u->getAIUpdateInterface()->aiIdle(CMD_FROM_PLAYER);
		u->setModelConditionState(kMoving, false);
		u->setModelConditionState(kCelebrating, true);
	}
	for (int f = 0; f < 8; ++f)
	{
		game.logic().runLogicFrame();
		client.runLargeGroupAudio();
	}
	CHECK_FALSE(hasGroupMap("3_2_SmallGroupMoving"));
	CHECK(hasGroupMap("SmallGroupCelebrating"));

	// 3. Stun and disable 3 of the 5 celebrating fighters (DISABLED_HELD + STUNNED, which SmallGroupCelebrating excludes):
	// RW 0x008AF0D0 lets their LargeGroupAudioUpdate::update() run while disabled, dropping qualified count 5 -> 2 (< StopThreshold = 3).
	for (int i = 0; i < 3; ++i)
	{
		fighters[i]->setDisabled(DISABLED_HELD, UPDATE_SLEEP_FOREVER);
		fighters[i]->setModelConditionState(kStunned, true);
	}
	for (int f = 0; f < 8; ++f)
	{
		game.logic().runLogicFrame();
		client.runLargeGroupAudio();
	}
	CHECK_FALSE(hasGroupMap("SmallGroupCelebrating"));
}

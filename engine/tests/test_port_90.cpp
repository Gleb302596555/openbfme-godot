// PORT-90: synthetic INI; expectations from RW 0x86754C / 0x865F73 and
// RW 0x8658A3 / 0x865932 (one sound per container per frame, container ID).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/GameAudio.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
struct AudioRig
{
	initest::Fixture fx;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> manager;
	ContainParseHooks saved;
	AudioRig() : saved(TheContainParseHooks())
	{
		fx.mount({ { "data\\audio\\sounds\\synthetic.wav", "x" } });
		ini.registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "sounds.ini",
			"AudioSettings\n  AudioRoot = Data\\Audio\n  SoundsFolder = Sounds\n  SoundsExtension = wav\n  SampleCount2D = 2\n  SampleCount3D = 3\n  StreamCount = 3\n  MinSampleVolume = 10\nEnd\n"
			"AudioEvent Port90Enter\n  Sounds = synthetic\n  Type = world everyone\nEnd\n"
			"AudioEvent Port90Exit\n  Sounds = synthetic\n  Type = world everyone\nEnd\n");
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&fx.fsys, 1024u * 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		manager = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::RotWK_GameDat_LCG, 1u);
		manager->enableEventLog(true);
		AudioApi::install(manager.get());
		TheContainParseHooks().audioEventExists = [](const std::string &n) { return n == "Port90Enter" || n == "Port90Exit"; };
	}
	~AudioRig()
	{
		TheContainParseHooks() = saved;
		AudioApi::install(nullptr);
	}
	unsigned count(const std::string &event, ObjectID id)
	{
		unsigned n = 0;
		for (const AudioLogEntry &e : manager->takeEventLog())
			if (!e.play && e.event == event && e.objectId == id) ++n;
		return n;
	}
};

std::string objectIni(const char *name, const char *module, bool sounds)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE IMMOBILE\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 20\n  GeometryMinorRadius = 20\n  GeometryHeight = 30\n"
		"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n"
		"  Behavior = " + module + " ModuleTag_Contain\n"
		"    ContainMax = 10\n" + (std::string(module) == "HordeTransportContain" ? "    Slots = 10\n" : "") +
		"    AllowAlliesInside = Yes\n    AllowOwnPlayerInsideOverride = Yes\n" +
		(sounds ? "    EnterSound = Port90Enter\n    ExitSound = Port90Exit\n" : "") +
		"  End\nEnd\n";
}

void loadObjects(combattest::CombatWorld &w)
{
	const std::string ini = objectIni("Port90Garrison", "HordeGarrisonContain", true) +
		objectIni("Port90Silent", "HordeGarrisonContain", false) + objectIni("Port90Transport", "HordeTransportContain", true);
	const std::string err = w.w.load(ini, INI_LOAD_OVERWRITE, "port90.ini");
	REQUIRE_MESSAGE(err.empty(), err);
}
} // namespace

TEST_CASE("PORT-90 garrison: a horde entering and leaving requests one sound for the container")
{
	AudioRig audio;
	combattest::CombatWorld w;
	loadObjects(w);
	Object *tower = w.unit("Port90Garrison", 'A', 300, 300);
	Object *horde = w.unit("SwordHorde", 'A', 400, 300);
	Object *other = w.unit("SwordHorde", 'A', 500, 300);
	REQUIRE(horde->getContain()->getContainCount() == 6u);
	w.frames(1); // RW 0x86804E / 0x868051 initialize last frames to zero
	audio.manager->takeEventLog();
	ContainModuleInterface *c = tower->getContain();
	REQUIRE(c);
	REQUIRE(c->addToContain(horde));
	REQUIRE(c->addToContain(other));
	CHECK(audio.count("Port90Enter", tower->getID()) == 1u); // RW 0x8658C8: not per member or horde in the same frame
	c->removeFromContain(horde);
	c->removeFromContain(other);
	CHECK(audio.count("Port90Exit", tower->getID()) == 1u); // RW 0x86594E: independent exit frame
	w.frames(1);
	REQUIRE(c->addToContain(horde));
	CHECK(audio.count("Port90Enter", tower->getID()) == 1u);
}

TEST_CASE("PORT-90 garrison members rejoin without replaying the module enter sound")
{
	AudioRig audio;
	combattest::CombatWorld w;
	loadObjects(w);
	Object *tower = w.unit("Port90Garrison", 'A', 300, 300);
	Object *horde = w.unit("SwordHorde", 'A', 400, 300);
	ContainModuleInterface *hc = horde->getContain();
	HordeContainInterface *hi = hc->getHordeContainInterface();
	REQUIRE(hi);
	const std::vector<Object *> members(hc->getContainedItemsList()->begin(), hc->getContainedItemsList()->end());
	REQUIRE(members.size() == 6u);
	w.frames(1);
	audio.manager->takeEventLog();
	REQUIRE(tower->getContain()->addToContain(horde));
	CHECK(audio.count("Port90Enter", tower->getID()) == 1u);
	w.frames(1);
	audio.manager->takeEventLog();
	for (Object *member : members)
	{
		hi->releaseMemberForGarrison(member);
		REQUIRE(tower->getContain()->addToContain(member));
	}
	// RW 0x87CD96 invokes the member callback, not slot 0x54; only the horde's own base add played the module event.
	CHECK(audio.count("Port90Enter", tower->getID()) == 0u);
}

TEST_CASE("PORT-90 missing fields do not request a sound")
{
	AudioRig audio;
	combattest::CombatWorld w;
	loadObjects(w);
	Object *tower = w.unit("Port90Silent", 'A', 300, 300);
	Object *horde = w.unit("SwordHorde", 'A', 400, 300);
	w.frames(1);
	audio.manager->takeEventLog();
	REQUIRE(tower->getContain()->addToContain(horde));
	tower->getContain()->removeFromContain(horde);
	for (const AudioLogEntry &e : audio.manager->takeEventLog()) CHECK(e.objectId != tower->getID());
}

TEST_CASE("PORT-90 horde transport uses the same module sounds")
{
	AudioRig audio;
	combattest::CombatWorld w;
	loadObjects(w);
	Object *transport = w.unit("Port90Transport", 'A', 300, 300);
	Object *horde = w.unit("SwordHorde", 'A', 400, 300);
	w.frames(1);
	audio.manager->takeEventLog();
	REQUIRE(transport->getContain()->addToContain(horde));
	CHECK(audio.count("Port90Enter", transport->getID()) == 1u);
	transport->getContain()->removeFromContain(horde);
	CHECK(audio.count("Port90Exit", transport->getID()) == 1u);
}

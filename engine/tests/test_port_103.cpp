// OpenBFME. GPL-3.0. PORT-103 (issue 263), synthetic INI only.
// Oracles: RW 0x8901BE NORMAL gate / 0x890201 sound start; 0x88FE61..93
// event(data+0x18, object ID), TheAudio slot 0x64, kept handle; 0x8900C7
// quench and 0x88FDDD destructor -> 0x88FBCA stop/clear; 0x890AB3..AED
// BURNED is not a quench until the independent AflameDuration deadline.
#include "doctest.h"
#include "LogicTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/LogicModules.h"

#include <thread>

namespace
{
struct BurningWorld
{
	// Audio outlives the logic (including module destruction); restore globals last.
	std::function<bool(const std::string &)> savedLookup = TheContainParseHooks().audioEventExists;
	initest::Fixture files;
	AudioIniState ini;
	std::unique_ptr<AudioAssetCache> cache;
	std::unique_ptr<SimulatedAudioDevice> device;
	std::unique_ptr<AudioManager> audio;
	std::unique_ptr<logictest::LogicWorld> world;

	explicit BurningWorld(const std::string &sound = "    BurningSoundName = TestCrackle\n", bool setBurned = true)
	{
		ini.registerBlocks(files.env.blocks);
		const std::string err = initest::loadError(files.env, "audio.ini",
			"AudioSettings\n  SampleCount2D = 8\n  SampleCount3D = 8\n  StreamCount = 2\nEnd\n"
			"AudioEvent TestCrackle\n  Sounds = synthetic\n  Control = loop\n  Type = world everyone\nEnd\n");
		REQUIRE_MESSAGE(err.empty(), err);
		cache = std::make_unique<AudioAssetCache>(&files.fsys, 1024u);
		device = std::make_unique<SimulatedAudioDevice>(cache.get());
		audio = std::make_unique<AudioManager>(ini, *cache, *device, RandomAlgorithm::RotWK_GameDat_LCG, 1u);
		audio->enableEventLog(true);
		AudioApi::install(audio.get());
		TheContainParseHooks().audioEventExists = [this](const std::string &name) { return audio->isValidAudioEvent(name); };
		world = std::make_unique<logictest::LogicWorld>();
		REQUIRE_MESSAGE(world->loadError.empty(), world->loadError);
		LogicModules::registerAll(world->w.modules);
		world->logic->settings().unitDamagedThreshold = 0.5f;
		world->logic->settings().unitReallyDamagedThreshold = 0.25f;
		world->logic->settings().bodyThresholdsLoaded = true;
		const std::string load = world->w.load(
			"Object BurningProbe\n  KindOf = STRUCTURE IMMOBILE\n"
			"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n"
			"  Behavior = FlammableUpdate ModuleTag_Fire\n    AflameDuration = 2000\n"
			"    BurnedDelay = 400\n    FlameDamageLimit = 20\n" + sound +
			(setBurned ? "    SetBurnedStatus = Yes\n" : "    SetBurnedStatus = No\n") + "  End\nEnd\n",
			INI_LOAD_OVERWRITE, "port103.ini");
		REQUIRE_MESSAGE(load.empty(), load);
	}
	~BurningWorld()
	{
		world.reset();
		AudioApi::drainDeferred();
		AudioApi::install(nullptr);
		TheContainParseHooks().audioEventExists = savedLookup;
	}
	Object *make() { return world->make("BurningProbe", world->teamOf("Alice")); }
	FlammableUpdate *fire(Object *o)
	{
		for (const auto &m : o->modules())
		{
			if (auto *f = dynamic_cast<FlammableUpdate *>(m.get())) { return f; }
		}
		throw std::runtime_error("no FlammableUpdate");
	}
	void frames(int n) { while (n-- > 0) { world->logic->runLogicFrame(); } }
	std::uint32_t handle(ObjectID id) { return AudioApi::heldSound(AudioApi::HELD_FLAMMABLE_LOOP, id); }
};
}

TEST_CASE("PORT-103 ignition requests one object-attached sound; BURNED does not stop it before aflame ends")
{
	BurningWorld w;
	Object *o = w.make();
	FlammableUpdate *f = w.fire(o);
	CHECK(w.handle(o->getID()) == 0u);
	f->tryToIgnite();
	const auto handle = w.handle(o->getID());
	REQUIRE(handle >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	auto log = w.audio->takeEventLog();
	REQUIRE(log.size() == 1u); // RW 0x890201, not one event per update
	CHECK(log[0].event == "TestCrackle");
	CHECK(log[0].objectId == o->getID()); // RW 0x88FE61, not a fixed position
	CHECK(log[0].drawableId == 0u);
	CHECK(log[0].outcome == "queued");
	f->tryToIgnite();
	CHECK(w.handle(o->getID()) == handle);
	CHECK(w.audio->takeEventLog().empty());
	w.frames(4); // BurnedDelay 400 ms = 2 frames; AflameDuration 2000 ms = 10
	CHECK(o->testStatus(11)); // BURNED, RW 0x890AB9..BF
	CHECK(o->testStatus(10)); // still AFLAME
	CHECK(w.handle(o->getID()) == handle);
	CHECK(w.audio->takeEventLog().empty());
	w.frames(8);
	CHECK_FALSE(o->testStatus(10));
	CHECK(w.handle(o->getID()) == 0u);
	CHECK(w.audio->pendingRequests() == 0u); // stop really reached the manager
	f->tryToIgnite(); // burned gate remains, no second sound
	CHECK(w.audio->takeEventLog().empty());
}

TEST_CASE("PORT-103 healing quenches the loop and a new NORMAL ignition starts only once")
{
	BurningWorld w("    BurningSoundName = TestCrackle\n", false);
	Object *o = w.make();
	FlammableUpdate *f = w.fire(o);
	// RW 0x8904B8 damage depletes FlameDamageLimit then calls the ignition.
	DamageInfo hit;
	hit.m_input.m_amount = 20;
	hit.m_input.m_damageType = 6;
	o->attemptDamage(hit);
	REQUIRE(f->status() == FlammableUpdate::FS_AFLAME);
	REQUIRE(w.handle(o->getID()) >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	w.audio->takeEventLog();
	DamageInfo healing;
	healing.m_input.m_amount = 19;
	f->onHealing(healing);
	CHECK(w.handle(o->getID()) >= AHSV_FirstHandle); // RW 0x890196 jb // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	healing.m_input.m_amount = 1;
	f->onHealing(healing);
	CHECK(w.handle(o->getID()) == 0u); // RW 0x89019B -> quench
	CHECK(w.audio->pendingRequests() == 0u);
	CHECK(f->status() == FlammableUpdate::FS_NORMAL);
	f->tryToIgnite();
	f->tryToIgnite();
	CHECK(w.audio->takeEventLog().size() == 1u);
	CHECK(w.handle(o->getID()) >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
}

TEST_CASE("PORT-103 absent or NoSound field is silent and keeps the burning lifecycle")
{
	for (const std::string sound : { std::string(), std::string("    BurningSoundName = NoSound\n") })
	{
		BurningWorld w(sound);
		Object *o = w.make();
		w.fire(o)->tryToIgnite();
		CHECK(o->testStatus(10));
		CHECK(w.handle(o->getID()) == 0u);
		w.frames(12);
		CHECK_FALSE(o->testStatus(10));
		CHECK(o->testStatus(11));
		CHECK(w.audio->takeEventLog().empty());
		CHECK(w.audio->pendingRequests() == 0u);
	}
}

TEST_CASE("PORT-103 destructor stops its loop without stopping another object's loop or another audio slot")
{
	BurningWorld w;
	Object *a = w.make(), *b = w.make();
	const ObjectID aID = a->getID(), bID = b->getID();
	w.fire(a)->tryToIgnite();
	w.fire(b)->tryToIgnite();
	AudioApi::postHeldSound(AudioApi::HELD_MOVE_LOOP, "TestCrackle", aID, aID);
	REQUIRE(w.handle(aID) >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	const auto bHandle = w.handle(bID);
	REQUIRE(bHandle >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	const auto moveHandle = AudioApi::heldSound(AudioApi::HELD_MOVE_LOOP, aID);
	REQUIRE(moveHandle >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	w.world->logic->destroyObject(a);
	w.frames(1);
	CHECK(w.handle(aID) == 0u); // RW 0x88FDDD, no surviving sound of the deleted module
	CHECK(w.handle(bID) == bHandle);
	CHECK(AudioApi::heldSound(AudioApi::HELD_MOVE_LOOP, aID) == moveHandle);
	AudioApi::stopHeldSound(AudioApi::HELD_MOVE_LOOP, aID);
}

TEST_CASE("PORT-103 logic worker orders start and stop on the audio owner queue")
{
	BurningWorld w;
	Object *o = w.make();
	std::thread worker([&] { w.fire(o)->tryToIgnite(); });
	worker.join();
	CHECK(w.audio->takeEventLog().empty());
	CHECK(w.handle(o->getID()) == 0u);
	AudioApi::drainDeferred();
	CHECK(w.audio->takeEventLog().size() == 1u);
	CHECK(w.handle(o->getID()) >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	std::thread stop([&] { w.frames(12); });
	stop.join();
	CHECK(w.handle(o->getID()) >= AHSV_FirstHandle); // still awaiting owner drain // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	AudioApi::drainDeferred();
	CHECK(w.handle(o->getID()) == 0u);
	CHECK(w.audio->pendingRequests() == 0u);
}

TEST_CASE("PORT-103 native snapshot restores burning audio without changing the saved simulation layout")
{
	BurningWorld w;
	Object *o = w.make();
	FlammableUpdate *f = w.fire(o);
	XferSave normal;
	f->xfer(normal);
	f->tryToIgnite();
	XferSave burning;
	f->xfer(burning);
	CHECK(normal.data().size() == burning.data().size());
	w.frames(12);
	CHECK(w.handle(o->getID()) == 0u);
	w.audio->takeEventLog();
	XferLoad load(burning.data());
	f->xfer(load);
	CHECK(f->status() == FlammableUpdate::FS_AFLAME);
	CHECK(w.handle(o->getID()) >= AHSV_FirstHandle); // tolerance: device handles are opaque; values below AHSV_FirstHandle are failure/sentinel codes (RW 0x45C754).
	CHECK(w.audio->takeEventLog().size() == 1u);
	XferLoad loadNormal(normal.data());
	f->xfer(loadNormal);
	CHECK(w.handle(o->getID()) == 0u);
	CHECK(w.audio->pendingRequests() == 0u);
}

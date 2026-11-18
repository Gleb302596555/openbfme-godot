// OpenBFME. GPL-3.0. PORT-234 (issue #557): real producers, synthetic INI and a recording audio manager.
// The names deliberately differ from the slots. No game archives or renderer are needed.
#include "doctest.h"
#include "ProdTestUtil.h"
#include "MoveTestUtil.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/AI/AIWorld.h"
#include "SyntheticHudTestUtil.h"
#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Audio/SimulatedAudioDevice.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/Eva.h"
#include "GameClient/GUI/GameWindowManager.h"
#include "GameClient/GUI/StillImageLoadScreen.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/BuildCommands.h"
#include "GameLogic/Damage.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Module/GateModules.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/ObjectHelper.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/System/InvisibilityManager.h"
#include "GameLogic/System/ShroudManager.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <thread>

namespace
{
struct Audio : AudioOwnerResolver
{
	initest::Fixture fx;
	AudioIniState ini;
	AudioAssetCache cache;
	SimulatedAudioDevice device{ &cache };
	std::unique_ptr<AudioManager> mgr;
	bool ownerPresent = true;
	Audio(std::initializer_list<const char *> slots, const std::string &extra = {}, unsigned budget = 65536) : cache(&fx.fsys, budget)
	{
		// A mono 8-bit PCM file with one second of silence. Cache misses/decodes below are real.
		std::string wav = "RIFF";
		auto le = [&](unsigned v, int bytes) { while (bytes--) { wav.push_back(static_cast<char>(v & 255)); v >>= 8; } };
		le(8036, 4); wav += "WAVEfmt "; le(16, 4); le(1, 2); le(1, 2); le(8000, 4); le(8000, 4); le(1, 2); le(8, 2);
		wav += "data"; le(8000, 4); wav.append(8000, char(128));
		fx.mount({ { "data\\audio\\sounds\\bell.wav", wav }, { "data\\audio\\sounds\\end.wav", wav }, { "data\\audio\\sounds\\other.wav", wav }, { "data\\audio\\tracks\\music.mp3", "x" } });
		device.setFileLength("Data\\Audio\\Tracks\\music.mp3", 60000.0);
		ini.registerBlocks(fx.env.blocks);
		std::string source = "AudioSettings\n AudioRoot = Data\\Audio\n DefaultSoundVolume = 100%\n DefaultMusicVolume = 100%\n SoundsFolder = Sounds\n SoundsExtension = wav\n MusicFolder = Tracks\n"
			" SampleCount2D = 64\n SampleCount3D = 64\n MillisecondsPriorToPlayingToReadSoundFile = 70\n StreamCount = 4\n MinSampleVolume = 2%\nEnd\n";
		std::string misc = "MiscAudio\n";
		for (const char *s : slots)
		{
			const std::string name = std::string("Test") + s;
			source += "AudioEvent " + name + "\n Sounds = bell\n Type = world everyone\n MaxRange = 1000\nEnd\n";
			misc += std::string(" ") + s + " = " + name + "\n";
		}
		const auto err = initest::loadError(fx.env, "audio234.ini", source + misc + "End\n" + extra);
		REQUIRE_MESSAGE(err.empty(), err);
		mgr = std::make_unique<AudioManager>(ini, cache, device, RandomAlgorithm::RotWK_GameDat_LCG, 7);
		mgr->update(0.0);
		mgr->setOwnerResolver(this);
		mgr->enableEventLog(true);
		AudioApi::install(mgr.get());
	}
	~Audio() { AudioApi::installEva(nullptr); AudioApi::install(nullptr); }
	bool objectPosition(std::uint32_t, Coord3D *p) const override { *p = { 5, 6, 7 }; return ownerPresent; }
	bool drawablePosition(std::uint32_t id, Coord3D *p) const override { return objectPosition(id, p); }
	std::vector<AudioLogEntry> requests()
	{
		std::vector<AudioLogEntry> out;
		for (const auto &e : mgr->takeEventLog()) if (!e.play && !e.volume) out.push_back(e);
		return out;
	}
	void one(const std::string &slot, ObjectID object, int player = -1)
	{
		auto r = requests(); REQUIRE(r.size() == 1);
		CHECK(r[0].event == "Test" + slot); CHECK(r[0].objectId == object); CHECK(r[0].player == player);
	}
};
} // namespace

TEST_CASE("PORT-234 money: only actual nonzero sounding transfers, owner index but no object or position")
{
	Audio a({ "MoneyDepositSound", "MoneyWithdrawSound" });
	Money money; ScoreKeeper score;
	money.setPlayerIndex(3);
	money.deposit(0, &score); money.deposit(2, &score, false); money.withdraw(1, &score, false);
	CHECK(a.requests().empty());
	// RW 0x7B18B8, RW 0x7B17EF: requested from the logic worker in balance order; withdraw first clamps to cash.
	std::uint32_t actual = 0;
	std::thread worker([&] { money.deposit(11, &score); actual = money.withdraw(60, &score); money.withdraw(1, &score); });
	worker.join(); CHECK(a.requests().empty()); AudioApi::drainDeferred();
	const auto r = a.requests(); REQUIRE(r.size() == 2);
	CHECK(r[0].event == "TestMoneyDepositSound"); CHECK(r[1].event == "TestMoneyWithdrawSound");
	for (const auto &e : r) { CHECK(e.player == 3); CHECK(e.objectId == 0); CHECK_FALSE(e.positional); }
	CHECK(actual == 12); CHECK(score.moneyEarned() == 13); CHECK(score.moneySpent() == 13); CHECK(money.countMoney() == 0);
}

TEST_CASE("PORT-234 EMP and UNDERPOWERED share two edges; UNMANNED is not an edge, DRONE is silent")
{
	prodtest::ProdWorld w;
	w.load("Object Both\n KindOf = STRUCTURE MACHINE\nEnd\nObject Cart\n KindOf = MACHINE\nEnd\nObject Flyer\n KindOf = DRONE\nEnd\n");
	Audio a({ "BuildingDisabled", "BuildingReenabled", "VehicleDisabled", "VehicleReenabled", "SplatterVehiclePilotsBrain" });
	for (bool building : { true, false })
	{
		Object *o = w.make(building ? "Both" : "Cart", w.teamOf("Alice"));
		Coord3D pos{ 21, 32, 43 }; o->setPosition(&pos);
		o->setDisabled(DISABLED_EMP, 0); // RW 0x6907F1: deadline must be in the future
		o->setDisabled(DISABLED_EMP, 10); o->setDisabled(DISABLED_EMP, 20); o->setDisabled(DISABLED_UNDERPOWERED, 20);
		o->clearDisabled(DISABLED_EMP); o->clearDisabled(DISABLED_EMP); o->clearDisabled(DISABLED_UNDERPOWERED);
		o->setDisabled(DISABLED_UNMANNED, 10); o->setDisabled(DISABLED_UNMANNED, 10);
		const auto r = a.requests(); REQUIRE(r.size() == 4); // RW 0x690843 before the same-deadline return
		CHECK(r[0].event == (building ? "TestBuildingDisabled" : "TestVehicleDisabled"));
		CHECK(r[1].event == (building ? "TestBuildingReenabled" : "TestVehicleReenabled"));
		CHECK(r[2].event == "TestSplatterVehiclePilotsBrain"); CHECK(r[3].event == r[2].event);
		for (const auto &e : r) { CHECK(e.objectId == 0); CHECK(e.player == -1); CHECK(e.positional); CHECK(e.position.z == pos.z); }
	}
	Object *drone = w.make("Flyer"); drone->setDisabled(DISABLED_UNMANNED, 10); drone->setDisabled(DISABLED_EMP, 10); CHECK(a.requests().empty());
}

TEST_CASE("PORT-234 defection: capture tick belongs to the new owner, phase edges and expiry ding to the object")
{
	prodtest::ProdWorld w; w.load("Object Flag\n KindOf = STRUCTURE\nEnd\n");
	Audio a({ "DefectorTimerTickSound", "DefectorTimerDingSound" });
	Object *o = w.make("Flag", w.teamOf("Bob")); o->friend_bindToClient(&w.hooks);
	o->setCapturedTeam(w.teamOf("Alice"), 10); // RW 0x6997D0..0x699847
	a.one("DefectorTimerTickSound", o->getID(), w.alice()->getPlayerIndex());
	o->setCapturedTeam(w.teamOf("Alice")); CHECK(a.requests().empty());
	w.frames(3); CHECK(a.requests().empty());
	auto *h = dynamic_cast<DefectionHelper *>(o->findModule("DefectionHelper")); REQUIRE(h);
	XferSave saved; h->xfer(saved); REQUIRE(saved.data().front() == 2);
	StateHasher before; h->crc(before);
	w.frames(4); a.one("DefectorTimerTickSound", o->getID()); // RW 0x8E3A42: passed even phase 2, not every frame
	XferLoad restore(saved.data()); h->xfer(restore); StateHasher after; h->crc(after); CHECK(after.value() == before.value());
	// A version 1 stream ends after the flash byte and resets phase, even when loading into an active helper.
	auto v1 = saved.data(); v1[0] = 1; v1.resize(v1.size() - sizeof(float)); XferLoad old(v1); h->xfer(old);
	StateHasher reset; h->crc(reset); CHECK(reset.value() != before.value());
	w.frames(5); a.one("DefectorTimerDingSound", o->getID()); CHECK_FALSE(o->isUndetectable());
	o->setInvulnerableTime(2); w.frames(3); CHECK(a.requests().empty()); // flash = false
	o->setCapturedTeam(w.teamOf("Bob"), 9); (void)a.requests(); o->setStatus(13, true); w.frames(12);
	CHECK_FALSE(o->isUndetectable()); CHECK(a.requests().empty()); // RW 0x8E39CC, interrupted without ding
}

TEST_CASE("PORT-234 reward crate collisions: four different receivers; deposit precedes the money cue")
{
	prodtest::ProdWorld w;
	w.load("Object Picker\n KindOf = STRUCTURE\n Body = ActiveBody ModuleTag_B\n MaxHealth = 100\n End\nEnd\nObject Gift\nEnd\n");
	w.logic->settings().bodyThresholdsLoaded = true;
	w.logic->settings().unitDamagedThreshold = 0.65f; w.logic->settings().unitReallyDamagedThreshold = 0.3f;
	Object *picker = w.make("Picker", w.teamOf("Alice")); Object *friendObj = w.make("Picker", w.teamOf("Alice"));
	Object *enemy = w.make("Picker", w.teamOf("Bob")); const Coord3D pos{ 30, 70, 0 }; picker->setPosition(&pos);
	pathtest::SyntheticTerrain terrain(32, 32); AIWorldConfig config; config.pathfind = pathtest::testConfig();
	AIWorld ai(*w.logic, config, w.w.fx.env.macros); ai.attach(); ai.newMap(terrain);
	ShroudManager shroud(*w.logic); shroud.init(32, 0); shroud.setExtent(0, 0, 256, 256); shroud.attach();
	Audio a({ "CrateMoney", "MoneyDepositSound", "CrateHeal", "CrateFreeUnit", "CrateShroud" });
	auto crate = [&](const std::string &name, const std::string &module, const std::string &fields = "") {
		w.load("Object " + name + "\n Behavior = " + module + " ModuleTag_C\n BuildingPickup = Yes\n" + fields + " End\nEnd\n");
		Object *obj = w.make(name); auto *c = dynamic_cast<CrateCollide *>(obj->findModule(module)); REQUIRE(c); return c;
	};
	auto collide = [&](CrateCollide *c) { c->onCollide(picker, nullptr, nullptr); };
	CrateCollide *cash = crate("Cash", "MoneyCrateCollide", " MoneyProvided = 15\n");
	cash->getObject()->setInvulnerableTime(3); collide(cash); CHECK(a.requests().empty()); // RW 0x8C0915: crate +458 bit 1
	cash->getObject()->setInvulnerableTime(0); collide(cash); collide(cash);
	auto r = a.requests(); REQUIRE(r.size() == 2); CHECK(r[0].event == "TestMoneyDepositSound"); CHECK(r[0].player == w.alice()->getPlayerIndex());
	CHECK(r[1].event == "TestCrateMoney"); CHECK(r[1].objectId == picker->getID()); CHECK(r[1].player == -1);
	collide(crate("EmptyCash", "MoneyCrateCollide")); a.one("CrateMoney", picker->getID());
	friendObj->getBodyModule()->setInitialHealth(20); enemy->getBodyModule()->setInitialHealth(20);
	collide(crate("Heal", "HealCrateCollide")); // RW 0x8C0A02, all owned objects; fixed position, not the crate's or an ID
	r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].event == "TestCrateHeal"); CHECK(r[0].positional); CHECK(r[0].position.y == 70); CHECK(r[0].objectId == 0);
	CHECK(friendObj->getBodyModule()->getHealth() == 100); CHECK(enemy->getBodyModule()->getHealth() == 20);
	collide(crate("Fog", "ShroudCrateCollide")); a.one("CrateShroud", picker->getID());
	CHECK(shroud.getCellStatus(w.alice()->getPlayerIndex(), 5, 5) == CELLSHROUD_FOGGED);
	CrateCollide *bad = crate("UnknownGift", "UnitCrateCollide", " UnitName = MissingUnit\n UnitCount = 1\n"); collide(bad); CHECK_FALSE(bad->everExecuted()); CHECK(a.requests().empty());
	collide(crate("NoUnits", "UnitCrateCollide", " UnitName = Gift\n UnitCount = 0\n")); a.one("CrateFreeUnit", picker->getID());
	collide(crate("Units", "UnitCrateCollide", " UnitName = Gift\n UnitCount = 2\n")); a.one("CrateFreeUnit", picker->getID());
	int units = 0; for (auto *o = w.logic->getFirstObject(); o; o = o->getNextObject()) if (o->getTemplate() == w.w.get("Gift")) { ++units; CHECK(o->getTeam() == picker->getTeam()); }
	CHECK(units == 2); shroud.detach();
}

TEST_CASE("PORT-234 placement: after the assistant even on refusal; no cheer for selection, no sound for an unaffordable order")
{
	prodtest::ProdWorld w; w.load("Object Builder\n KindOf = DOZER BASE_FOUNDATION\nEnd\nObject Wall\n BuildCost = 120\nEnd\n");
	BuildCommands builds; GameLogicDispatch dispatch(*w.logic); builds.registerHandlers(dispatch);
	Object *builder = w.make("Builder", w.teamOf("Alice")); w.alice()->selection().push_back(builder->getID());
	Audio a({ "BuildingPlacementSound", "WallPlacementSound" });
	const int player = w.alice()->getPlayerIndex(); const Coord3D pos{ 60, 30, 0 };
	auto send = [&](int type) {
		GameMessage m(type, player); m.appendIntegerArgument(w.w.get("Wall")->getTemplateID()); m.appendLocationArgument(pos);
		if (type == MSG_WALL_HUB_CONSTRUCT_SPAN) { m.appendLocationArgument(pos); m.appendIntegerArgument(0); m.appendObjectIDArgument(builder->getID()); }
		else m.appendRealArgument(0);
		dispatch.dispatch(m);
	};
	for (int type : { MSG_FOUNDATION_CONSTRUCT, MSG_DOZER_CONSTRUCT, MSG_WALL_HUB_CONSTRUCT_SPAN })
	{
		send(type); a.one(type == MSG_WALL_HUB_CONSTRUCT_SPAN ? "WallPlacementSound" : "BuildingPlacementSound", builder->getID(), player);
	}
	w.alice()->setOrderMode(2); send(MSG_WALL_HUB_CONSTRUCT_SPAN); send(MSG_DOZER_CONSTRUCT); CHECK(a.requests().empty());
	w.alice()->setOrderMode(0); w.alice()->getMoney()->withdraw(w.alice()->getMoney()->countMoney(), false);
	send(MSG_FOUNDATION_CONSTRUCT); send(MSG_DOZER_CONSTRUCT); send(MSG_WALL_HUB_CONSTRUCT_SPAN); CHECK(a.requests().empty()); // RW 0x6AAA2E
}

TEST_CASE("PORT-234 gate: one held loop per motion, removal before either endpoint and on death")
{
	std::string ini; for (const char *s : { "Opening", "Closing", "Opened", "Closed" }) ini += std::string("AudioEvent ") + s + "\n Sounds = bell\n Type = world everyone\nEnd\n";
	Audio a({}, ini);
	struct ResetLookup { decltype(ContainParseHooks::audioEventExists) saved = TheContainParseHooks().audioEventExists;
		~ResetLookup() { TheContainParseHooks().audioEventExists = saved; } } reset;
	TheContainParseHooks().audioEventExists = [&](const std::string &name) { return a.mgr->isValidAudioEvent(name); };
	prodtest::ProdWorld w; w.load("Object Door\n KindOf = STRUCTURE\n Behavior = GateOpenAndCloseBehavior ModuleTag_G\n OpenByDefault = No\n"
		" ResetTimeInMilliseconds = 800\n TimeBeforePlayingOpenSound = 200\n SoundOpeningGateLoop = Opening\n SoundClosingGateLoop = Closing\n"
		" SoundFinishedOpeningGate = Opened\n SoundFinishedClosingGate = Closed\n End\nEnd\n");
	Object *o = w.make("Door", w.teamOf("Alice")); auto *g = GateOpenAndCloseBehavior::findGate(*o); REQUIRE(g);
	auto held = [&] { return AudioApi::heldSound(AudioApi::HELD_GATE_LOOP, o->getID()); };
	for (bool opening : { true, false })
	{
		if (opening) { g->open(); g->open(); } else { g->close(); g->close(); }
		auto r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].event == (opening ? "Opening" : "Closing")); CHECK(r[0].objectId == o->getID()); CHECK(held() == r[0].handle); CHECK(a.mgr->isCurrentlyPlaying(held()));
		w.frames(7); r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].event == (opening ? "Opened" : "Closed")); CHECK(r[0].objectId == o->getID()); CHECK(held() == 0);
	}
	g->open(); (void)a.requests(); o->friend_onDie({}); g->update(); CHECK(held() == 0); CHECK(a.requests().empty()); // RW 0x89CD4A
}

TEST_CASE("PORT-234 GUI window: disabled hit only on the sounding pass; disabled siblings do not mask enabled ones")
{
	Audio a({ "DisabledButtonClicked" }); GameWindowManager windows;
	GameWindow *parent = windows.winCreate(nullptr, WIN_STATUS_ENABLED, 10, 10, 90, 90, nullptr);
	GameWindow *live = windows.winCreate(parent, WIN_STATUS_ENABLED, 5, 5, 20, 20, nullptr);
	GameWindow *dead = windows.winCreate(parent, 0, 5, 5, 20, 20, nullptr); // prepended: hit first
	REQUIRE(parent); REQUIRE(live); REQUIRE(dead);
	CHECK(parent->winPointInChild(16, 16, false, false) == live); CHECK(a.requests().empty());
	CHECK(parent->winPointInChild(16, 16, false, true) == live); a.one("DisabledButtonClicked", 0); // RW 0x715B30
	CHECK(parent->winPointInChild(16, 16, true, true) == dead); CHECK(a.requests().empty());
	dead->winHide(true); CHECK(parent->winPointInChild(16, 16, false, true) == live); CHECK(a.requests().empty());
}

TEST_CASE("PORT-234 LAN cheer: network meta posts one tactical sound and one cheer order; offline is silent")
{
	synthetichud::Rig h; Audio a({ "AllCheerSound" });
	for (bool net : { false, true })
	{
		h.commands.reset(); h.input->ui().setNetworkGame(net);
		h.input->stream().append(CMSG_META_ALL_CHEER); h.input->flush(); // RW 0x820128..0x820186
		int n = 0; for (const auto &m : h.commands.messages()) if (m.getType() == MSG_DO_CHEER) ++n;
		CHECK(n == int(net)); if (net) a.one("AllCheerSound", 0); else CHECK(a.requests().empty());
	}
}

TEST_CASE("PORT-234 stealth: the reveal producers post once, with the detector or revealed unit selected by relationship")
{
	prodtest::ProdWorld w; w.load("Object Agent\n KindOf = INFANTRY\n Behavior = StealthUpdate ModuleTag_S\n End\nEnd\nObject ObserverUnit\nEnd\n");
	ClientEventRecorder recorder(*w.logic); w.logic->setClientHooks(&recorder);
	struct Unbind { GameLogic &logic; ~Unbind() { logic.setClientHooks(nullptr); for (Object *o = logic.getFirstObject(); o; o = o->getNextObject()) o->friend_bindToClient(nullptr); } } unbind{ *w.logic };
	ArchiveW3DFileSource source(w.w.fx.fsys); WW3DAssetManager assets(source); DrawableManager draw(assets, *w.logic);
	std::vector<std::string> labels; draw.setRevealMessageHandler([&](const char *s) { labels.push_back(s); });
	Audio a({ "StealthDiscoveredSound", "StealthNeutralizedSound" }); Object *observer = w.make("ObserverUnit", w.teamOf("Alice"));
	const int local = w.alice()->getPlayerIndex();
	auto reveal = [&](const char *owner, int mode, Object *detector, bool obscured = false) {
		Object *o = w.make("Agent", w.teamOf(owner)); auto *s = StealthUpdate::of(*o); REQUIRE(s);
		REQUIRE(o->clientHooks() == &recorder); draw.applyEvents(recorder.take(false));
		auto *d = draw.findByObject(o->getID()); REQUIRE(d);
		if (!obscured) d->setSyncedRecord(nullptr, nullptr); // no local shroud, presented before the reveal
		s->markAsDetected(10, mode, detector, false); s->markAsDetected(10, mode, detector, false); // RW 0x7767A9: an extension is silent
		draw.applyEvents(recorder.take(false)); return o;
	};
	Object *own = reveal("Alice", 2, observer); a.one("StealthNeutralizedSound", own->getID(), local);
	reveal("Alice", 1, observer); CHECK(a.requests().empty());
	reveal("Bob", 2, observer, true); CHECK(a.requests().empty()); // +440 hides the enemy notice, not the owner/ally notice
	reveal("Bob", 1, observer); a.one("StealthDiscoveredSound", observer->getID(), local);
	reveal("Bob", 2, nullptr); CHECK(a.requests().empty());
	w.alice()->setPlayerRelationship(w.players.findPlayerWithName("Bob"), ALLIES);
	Object *ally = reveal("Bob", 2, observer); a.one("StealthDiscoveredSound", ally->getID(), local);
	reveal("Bob", 1, observer); CHECK(a.requests().empty());
	CHECK(labels == std::vector<std::string>{"MESSAGE:StealthNeutralized", "MESSAGE:StealthDiscovered"});
	// The other producer: reveal an active invisibility nugget. The manager does not need a StealthUpdate on the target.
	Object *target = w.make("ObserverUnit", w.teamOf("Alice")); w.frames(1);
	InvisibilityNugget nugget; nugget.invisibilityType = InvisibilityNugget::STEALTH;
	w.logic->invisibility().applyNugget(target, 50, nugget);
	REQUIRE(w.logic->invisibility().markDetected(target, observer, 5, 2)); // RW 0x81C32C -> 0x81ACD7
	draw.applyEvents(recorder.take(false)); a.one("StealthNeutralizedSound", target->getID(), local);
}

TEST_CASE("PORT-234 death EVA: no client polling, victim position, neutral selects enemy, self and FADED silent")
{
	std::string events;
	for (const char *name : { "Own", "Ally", "Enemy" }) events += std::string("NewEvaEvent ") + name + "\n ExpirationTimeMS = 2000\n"
		" SideSound\n  Side = Men\n  Sound = TestMoneyDepositSound\n End\nEnd\n";
	Audio a({ "MoneyDepositSound" }, events); Eva eva(a.ini, *a.mgr); eva.setLocalSide("Men"); AudioApi::installEva(&eva);
	prodtest::ProdWorld w; w.load("Object Fallen\n EvaEventDieOwner = Own\n EvaEventDieAlly = Ally\n EvaEventDieEnemy = Enemy\nEnd\n");
	for (const char *branch : { "Own", "Ally", "Enemy" })
	{
		eva.reset(); eva.setLocalPlayerIndex(w.alice()->getPlayerIndex());
		Object *o = w.make("Fallen", w.teamOf(std::string(branch) == "Own" ? "Alice" : "Bob"));
		w.alice()->setPlayerRelationship(w.players.findPlayerWithName("Bob"), std::string(branch) == "Ally" ? ALLIES : NEUTRAL);
		const Coord3D pos{ 5, 60, -1 }; o->setPosition(&pos); o->friend_onDie({}); // RW 0x6990DD..0x699140
		CHECK(eva.isEventAboutToPlay(eva.findEvent(branch))); eva.update(10);
		auto r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].positional); CHECK(r[0].position.y == pos.y);
	}
	const auto prev = AudioApi::deathReports(); Object *self = w.make("Fallen", w.teamOf("Alice")); DieModuleInterface::Event die;
	die.sourceId = self->getID(); self->friend_onDie(die); die.sourceId = 0; die.deathType = DEATH_FADED;
	w.make("Fallen", w.teamOf("Alice"))->friend_onDie(die); CHECK(AudioApi::deathReports() == prev); CHECK(a.requests().empty());
}

TEST_CASE("PORT-234 voice: actual current hero rank breaks the tie; only toggle-stance in planning replaces the button")
{
	Audio a({ "PlanningModeOrderGiven" }, "AudioEvent SelectVoice\n Sounds = bell\n Type = world everyone\nEnd\nAudioEvent ButtonVoice\n Sounds = bell\n Type = world everyone\nEnd\n");
	ExperienceLevelSystem levels; struct Restore { ExperienceLevelSystem *old = TheExperienceLevelSystem; ~Restore() { TheExperienceLevelSystem = old; } } restore;
	TheExperienceLevelSystem = &levels; prodtest::ProdWorld w; levels.registerBlocks(w.w.fx.env.blocks);
	w.load("ExperienceLevel First\n TargetNames = Hero\n RequiredExperience = 1\n Rank = 1\nEnd\n"
		"ExperienceLevel Second\n TargetNames = Hero\n RequiredExperience = 10\n Rank = 2\nEnd\nObject Hero\n KindOf = HERO INFANTRY SELECTABLE\n VoiceSelect = SelectVoice\nEnd\n");
	Object *first = w.make("Hero", w.teamOf("Alice")); Object *second = w.make("Hero", w.teamOf("Alice"));
	REQUIRE(second->getExperienceTracker()->gainExpForLevel(1, false));
	AudioApiVoiceSink sink; UnitVoiceResponse voice(*w.logic, sink);
	REQUIRE(voice.pickAndPlay({first->getID(), second->getID()}, MSG_CREATE_SELECTED_GROUP));
	auto r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].objectId == second->getID()); CHECK(r[0].event == "SelectVoice"); // RW 0x8DF106
	UnitVoiceResponse::Info info; info.commandSound = "ButtonVoice";
	w.alice()->setOrderMode(2); // SET_STANCE 0x3A, auto ability etc. keep their own sound
	REQUIRE(voice.pickAndPlay({first->getID()}, UnitVoiceResponse::VOICE_EVENT_STANCE, &info)); r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].event == "ButtonVoice");
	info.toggleStanceButton = true; REQUIRE(voice.pickAndPlay({first->getID()}, UnitVoiceResponse::VOICE_EVENT_STANCE, &info));
	a.one("PlanningModeOrderGiven", first->getID(), w.alice()->getPlayerIndex()); // RW 0x8DFAFF..0x8DFB5F
}

TEST_CASE("PORT-234 still load screen: explicit voice before music, film still takes only the save slot; teardown owns both handles")
{
	Audio a({}, "AudioEvent Narration\n Sounds = bell\n Type = ui everyone\nEnd\nMusicTrack MissionMusic\n Filename = music.mp3\nEnd\n"
		"MusicTrack FilmMusic\n Filename = music.mp3\nEnd\nMiscAudio\n SaveFileLoadMusic = FilmMusic\nEnd\n");
	const auto rng = [](int, int) { return 2; };
	const auto explicitPic = chooseLoadScreen(0, 0, false, "Picture", rng), film = chooseLoadScreen(3, 0, false, "Picture", rng);
	StillImageLoadAudio screen; screen.start(explicitPic.filmStill, "Narration", "MissionMusic");
	auto r = a.requests(); REQUIRE(r.size() == 2); CHECK(r[0].event == "Narration"); CHECK(r[1].event == "MissionMusic"); // RW 0x757DC7
	REQUIRE(a.mgr->isCurrentlyPlaying(r[0].handle)); REQUIRE(a.mgr->isCurrentlyPlaying(r[1].handle)); screen.stop(); a.mgr->update(67);
	CHECK_FALSE(a.mgr->isCurrentlyPlaying(r[0].handle)); CHECK_FALSE(a.mgr->isCurrentlyPlaying(r[1].handle));
	screen.start(film.filmStill, "Narration", "MissionMusic"); r = a.requests(); REQUIRE(r.size() == 1); CHECK(r[0].event == "FilmMusic");
	screen.stop(); screen.start(false, "Unknown", ""); CHECK(a.requests().empty()); // a missing explicit slot is not the default
}

TEST_CASE("PORT-234 request queue: missing owner admission, strict preload window, no extra play, refused loop may become audible")
{
	Audio a({}, ""
		"AudioEvent Hit\n Sounds = bell\n Type = world everyone\nEnd\nAudioEvent Floor\n Sounds = bell\n MinVolume = 2%\n Type = world everyone\nEnd\n"
		"AudioEvent Later\n Sounds = bell\n Limit = 1\n Delay = 70 70\n Type = world everyone\nEnd\n"
		"AudioEvent Forever\n Sounds = bell\n Delay = 35 35\n Control = loop\n Type = world everyone\nEnd\n");
	a.ownerPresent = false; CHECK(a.mgr->playSoundForObject("Hit", 10) == AHSV_NoSound); // RW 0x45CB92
	auto floor = a.mgr->playSoundForObject("Floor", 10); REQUIRE(floor == AHSV_FirstHandle + 1); a.mgr->removeAudioEvent(floor); // RW 0x452741: >= bypass
	a.ownerPresent = true; auto one = a.mgr->playSoundForObject("Later", 12); REQUIRE(one == floor + 1);
	a.mgr->update(34); CHECK(a.cache.stats().decodes == 0); // RW 0x461AC5: equality is outside the window
	a.mgr->update(67); CHECK(a.cache.stats().decodes == 1); CHECK(a.mgr->report().played == 0); // prefetched, not a voice
	a.mgr->update(101); CHECK(a.cache.stats().decodes == 1); CHECK(a.mgr->report().played == 1);
	a.ownerPresent = false; const auto loop = a.mgr->playSoundForObject("Forever", 15); REQUIRE(loop == one + 1);
	a.mgr->update(201); CHECK(a.mgr->isCurrentlyPlaying(loop)); CHECK(a.mgr->report().played == 1);
	a.ownerPresent = true; a.mgr->update(301); CHECK(a.mgr->report().played == 2); // same request survives an unresolved position, RW 0x461C34..0x461C6E
}

TEST_CASE("PORT-234 dozer: only the active phantom's truncated paid cost is placement credit")
{
	// RW 0x77B0B6 -> 0x88BF1A -> 0x88D0C6: do not count a completed building or the advertised cost.
	movetest::MoveWorld w(60, 60,
		"Object Porter\n KindOf = INFANTRY DOZER\n Behavior = DozerAIUpdate ModuleTag_AI\n End\n"
		" LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 40\n End\nEnd\n"
		"Object Site\n KindOf = STRUCTURE\n BuildCost = 120\n Behavior = GettingBuiltBehavior ModuleTag_GB\n End\nEnd\n");
	CommandStore commands;
	struct RestoreStore { CommandStore *old; ~RestoreStore() { TheCommandStore = old; } } restore{ TheCommandStore };
	TheCommandStore = &commands;
	commands.setThingFactory(&w.w.things);
	w.logic->settings().buildRulesLoaded = true;
	w.logic->productionSettings() = prodtest::defaultSettings();
	w.buildMap(); w.frames(3);
	Player *p = w.players.findPlayerWithName("Alice");
	Object *porter = w.spawn("Porter", 50, 50, 0, w.teamOf("Alice"));
	auto *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface()); REQUIRE(dozer);
	const ThingTemplate *site = w.w.get("Site");
	Object *phantom = dozer->construct(*site, { 200, 200, 0 }, 0, *p); REQUIRE(phantom);
	REQUIRE(dozer->unplacedStructure() == phantom->getID()); REQUIRE(phantom->getBuildCostPaid() == 120);
	p->getMoney()->withdraw(p->getMoney()->countMoney(), false);
	BuildCommands builds; builds.registerHandlers(w.dispatcher); p->selection().push_back(porter->getID());
	GameMessage m(MSG_DOZER_CONSTRUCT, p->getPlayerIndex());
	m.appendIntegerArgument(site->getTemplateID()); m.appendLocationArgument({ -50, -50, 0 }); m.appendRealArgument(0);
	Audio a({ "BuildingPlacementSound" });
	phantom->setBuildCostPaid(119.75f); w.dispatcher.dispatch(m); CHECK(a.requests().empty());
	phantom->setBuildCostPaid(120.25f); w.dispatcher.dispatch(m); a.one("BuildingPlacementSound", porter->getID(), p->getPlayerIndex());
	REQUIRE(dozer->unplacedStructure() == phantom->getID()); // refused inside the assistant, cue still follows it
}

TEST_CASE("PORT-234 read-ahead: only the current attack or decay is pinned, cancellation releases it without starting a voice")
{
	// RW 0x461A83 / 0x6DB9B9 / 0x4A7C71. One decoded WAV fits this budget. Looking up another
	// sample must not invalidate the request's reference. Cancelling must not leave that reference in the LRU.
	Audio a({}, "AudioEvent AttackFirst\n Attack = bell\n Sounds = other\n Delay = 65 65\nEnd\n"
		"AudioEvent DecayOnly\n Decay = end\n Delay = 65 65\nEnd\n", 16000);
	const auto h = a.mgr->playSound("AttackFirst"); REQUIRE(a.mgr->isCurrentlyPlaying(h));
	a.mgr->update(34); CHECK(a.device.log().empty()); REQUIRE(a.cache.stats().decodes == 1);
	const std::string dir = "Data\\Audio\\Sounds\\";
	std::weak_ptr<const AudioDecode::DecodedAudio> attack = a.cache.decoded(dir + "bell.wav", nullptr);
	REQUIRE(a.cache.stats().decodes == 1); REQUIRE(a.cache.decoded(dir + "other.wav", nullptr).get() != nullptr); CHECK_FALSE(attack.expired());
	a.mgr->removeAudioEvent(h); REQUIRE_FALSE(a.mgr->isCurrentlyPlaying(h));
	REQUIRE(a.cache.decoded(dir + "end.wav", nullptr).get() != nullptr); CHECK(attack.expired());
	a.mgr->update(101); CHECK(a.device.log().empty());
	const auto decay = a.mgr->playSound("DecayOnly"); REQUIRE(a.mgr->isCurrentlyPlaying(decay));
	a.mgr->update(135); CHECK(a.device.log().empty()); CHECK(a.cache.stats().decodes == 3);
	a.mgr->update(168); const auto &log = a.device.log(); REQUIRE(log.size() == 1);
	CHECK(log[0].kind == SimulatedAudioDevice::LogEntry::Start); CHECK(log[0].file == dir + "end.wav"); CHECK(a.cache.stats().decodes == 3);
}

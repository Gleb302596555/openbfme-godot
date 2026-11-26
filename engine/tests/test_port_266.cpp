// OpenBFME. GPL-3.0. PORT-266: synthetic GameData tests, no game files.
#include "doctest.h"
#include "LogicTestUtil.h"
#include "GameClient/TreeTopple.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "GameNetwork/NetPacket.h"
#include "Common/System/SaveGame/SaveGameFile.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ObjectFilter.h"
#include "GameLogic/ObjectFilterMatch.h"

namespace
{
std::string gameData(const std::string &rows)
{
	return "GameData\n"
		" ForceModelsToFollowTimeOfDay = Yes\n"
		" ForceModelsToFollowWeather = No\n"
		" UnitDamagedThreshold = 0.5\n"
		" UnitReallyDamagedThreshold = 0.25\n"
		" DefaultStartingCash = 700\n" + rows + "End\n";
}
}

TEST_CASE("PORT-266 GameData tree fade switch and filter are parsed")
{
	GameLogicSettings settings;
	std::string error;
	// RW 0x644148 (call 0x6441A5), table rows 0xBFF690 and 0xC00C20; filter parser RW 0x76392F.
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(
		" AllowTreeFading = Yes\n TreeFadeObjectFilter = ANY +INFANTRY -HERO\n"), settings, &error), error);
	CHECK(settings.allowTreeFading);
	REQUIRE(settings.treeFadeObjectFilter.get() != nullptr);
	CHECK(settings.treeFadeObjectFilter->rule == ObjectFilter::RULE_ANY);
	KindOfMaskType infantry{};
	BitFlagsSet(infantry, 8); // INFANTRY
	CHECK(ObjectFilterMatch::allowsKindOf(*settings.treeFadeObjectFilter, infantry));
	CHECK_FALSE(ObjectFilterMatch::allowsKindOf(*settings.treeFadeObjectFilter, KindOfMaskType{}));
	CHECK(settings.unappliedFields.count("GameData.AllowTreeFading") == 0);
	CHECK(settings.unappliedFields.count("GameData.TreeFadeObjectFilter") == 0);
}

TEST_CASE("PORT-266 omitted switch keeps retail constructor default and later block overrides")
{
	GameLogicSettings settings;
	std::string error;
	// RW 0x6429AD: the constructor stores zero at +0x40; unnamed filter is not synthesized.
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(""), settings, &error), error);
	CHECK_FALSE(settings.allowTreeFading);
	CHECK(settings.treeFadeObjectFilter.get() == nullptr);
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" AllowTreeFading = Yes\n"), settings, &error), error);
	CHECK(settings.allowTreeFading);
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" AllowTreeFading = No\n"), settings, &error), error);
	CHECK_FALSE(settings.allowTreeFading);
}

// This is a partition/filter integration baseline, not a test of the missing tree fade runtime (S-7438).
TEST_CASE("PORT-266 retail query inputs use 2D bounding circle and null filter player")
{
	logictest::LogicWorld world;
	REQUIRE_MESSAGE(world.loadError.empty(), world.loadError);
	const std::string error = world.w.load(
		"Object InfantryProbe\n KindOf = INFANTRY\n Geometry = CYLINDER\n GeometryMajorRadius = 3\n GeometryHeight = 10\nEnd\n"
		"Object HeroProbe\n KindOf = INFANTRY HERO\n Geometry = CYLINDER\n GeometryMajorRadius = 3\n GeometryHeight = 10\nEnd\n");
	REQUIRE_MESSAGE(error.empty(), error);
	world.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	auto place = [&](const char *name, const Coord3D &p) {
		Object *o = world.make(name, world.teamOf("Alice"));
		o->setPosition(&p);
		return o;
	};
	// RW 0x4ED508 -> mode 1 -> table RW 0xDBDAF8 -> RW 0xA3AE50: distance from 2D bounding-circle edge, not 3D centre.
	Object *edge = place("InfantryProbe", Coord3D{ 112.0f, 100.0f, 500.0f }); // edge 9, within 10 even at high Z
	Object *outside = place("InfantryProbe", Coord3D{ 114.0f, 100.0f, 0.0f }); // edge 11
	Object *excluded = place("HeroProbe", Coord3D{ 100.0f, 100.0f, 0.0f });
	world.logic->runLogicFrame(); // update moved partition entries
	GameLogicSettings settings;
	std::string parseError;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" TreeFadeObjectFilter = ANY +INFANTRY -HERO\n"), settings, &parseError), parseError);
	REQUIRE(settings.treeFadeObjectFilter.get() != nullptr);
	// RW 0x4ED508 initializes wrapper +0xC to null; RW 0x66122D passes that as the reference player.
	PartitionFilterFn filter([&](Object &o) { return ObjectFilterMatch::allows(*world.logic, *settings.treeFadeObjectFilter, o, nullptr); });
	const Coord3D center{ 100.0f, 100.0f, 0.0f };
	const PartitionHits hits = world.logic->partition().iterateObjectsInRange(center, 10.0f, FROM_BOUNDINGSPHERE_2D, { &filter }, ITER_FASTEST);
	REQUIRE(hits.size() == 1);
	CHECK(hits[0].object == edge);
	CHECK(hits[0].object != outside);
	CHECK(hits[0].object != excluded);
	const PartitionHits wrong3d = world.logic->partition().iterateObjectsInRange(center, 10.0f, FROM_CENTER_3D, { &filter }, ITER_FASTEST);
	CHECK(wrong3d.size() == 0);
	// RW 0x763543: relationships need both players. The separate local-alliance check is not the filter reference.
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" TreeFadeObjectFilter = ALL SAME_PLAYER\n"), settings, &parseError), parseError);
	REQUIRE(settings.treeFadeObjectFilter.get() != nullptr);
	CHECK_FALSE(ObjectFilterMatch::allows(*world.logic, *settings.treeFadeObjectFilter, *edge, nullptr));
	CHECK(ObjectFilterMatch::allows(*world.logic, *settings.treeFadeObjectFilter, *edge, edge->getControllingPlayer()));
}

// Client-only alpha persistence prerequisite; runtime tree fade is not connected yet (S-7438).
TEST_CASE("PORT-266 client tree alpha snapshot round trip and old default")
{
	W3DTreeDrawModuleData data;
	TreeTopple original;
	original.add(11, Coord3D{}, &data);
	original.add(12, Coord3D{}, &data, true); // a shrub is not in W3DTreeBuffer
	original.add(13, Coord3D{}, &data);
	NetByteWriter fixture;
	fixture.u32(1); fixture.u32(2);
	fixture.u32(0); fixture.u32(11); fixture.u32(170); fixture.u32(105);
	fixture.u32(2); fixture.u32(13); fixture.u32(201); fixture.u32(255);
	std::string error;
	REQUIRE_MESSAGE(original.loadFadeSnapshot(fixture.data(), &error), error);
	CHECK(original.trees()[0].fadeCurrent == 170);
	CHECK(original.trees()[0].fadeTarget == 105);
	CHECK(original.trees()[1].fadeCurrent == 255);
	const auto snapshot = original.saveFadeSnapshot();
	CHECK(snapshot == fixture.data());
	// v3 carries actual TreeTopple alpha bytes without changing the independent simulation snapshot.
	SaveGameFile file;
	file.header.profile = ProfileIdentity::fromText("data test\n");
	file.header.executionProfile = "enhanced";
	file.header.buildVersion = "v";
	file.header.engineId = "e";
	file.snapshot = { 1, 2, 3 };
	file.clientSnapshot = snapshot;
	SaveGameFile decoded;
	REQUIRE_MESSAGE(SaveGameFile::parse(file.encode(), decoded, &error), error);
	CHECK(decoded.snapshot == file.snapshot);
	CHECK(decoded.clientSnapshot == snapshot);
	TreeTopple loaded;
	loaded.add(11, Coord3D{}, &data);
	loaded.add(12, Coord3D{}, &data, true);
	loaded.add(13, Coord3D{}, &data);
	REQUIRE_MESSAGE(loaded.loadFadeSnapshot(decoded.clientSnapshot, &error), error);
	CHECK(loaded.trees()[0].fadeCurrent == 170);
	CHECK(loaded.trees()[0].fadeTarget == 105);
	CHECK(loaded.trees()[2].fadeCurrent == 201);
	CHECK(loaded.trees()[2].fadeTarget == 255);
	std::vector<std::uint8_t> bad = snapshot;
	bad.back() ^= 1; // an opaque word alone is valid; damage the record identity instead
	bad[12] ^= 1;
	CHECK_FALSE(loaded.loadFadeSnapshot(bad, &error));
	CHECK(loaded.trees()[0].fadeCurrent == 170); // failed load is atomic
	REQUIRE(loaded.loadFadeSnapshot({}, &error)); // v1/v2 no client section: RW 0x4EC93A initializes both to 255
	CHECK(loaded.trees()[0].fadeCurrent == 255);
	CHECK(loaded.trees()[0].fadeTarget == 255);
}

namespace
{
// a world with two objects' templates, Alice (the local player) and Bob; a TREE record at (100, 100) of a type with FadeTarget 105, FadeDistance 10 and FadeRate 40
struct FadeRig
{
	logictest::LogicWorld world;
	W3DTreeDrawModuleData data;
	TreeTopple trees;
	FadeRig(const std::string &filterRow)
	{
		REQUIRE_MESSAGE(world.loadError.empty(), world.loadError);
		const std::string error = world.w.load(
			"Object InfantryProbe\n KindOf = INFANTRY\n Geometry = CYLINDER\n GeometryMajorRadius = 3\n GeometryHeight = 10\nEnd\n"
			"Object HeroProbe\n KindOf = INFANTRY HERO\n Geometry = CYLINDER\n GeometryMajorRadius = 3\n GeometryHeight = 10\nEnd\n");
		REQUIRE_MESSAGE(error.empty(), error);
		world.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
		world.players.setLocalPlayer(world.players.findPlayerWithName("Alice"));
		data.m_fadeTarget = 105;   // RW 0x5C
		data.m_fadeDistance = 10.0f; // RW 0x60
		data.m_fadeRate = 40;      // RW 0x58
		trees.add(1, Coord3D{ 100.0f, 100.0f, 0.0f }, &data);
		trees.add(2, Coord3D{ 100.0f, 100.0f, 0.0f }, &data, true); // a shrub is not in the tree buffer: never fades
		std::string parseError;
		REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" AllowTreeFading = Yes\n" + filterRow), world.logic->settings(), &parseError), parseError);
	}
	Object *place(const char *name, const char *player, float x)
	{
		Object *o = world.make(name, world.teamOf(player));
		const Coord3D p{ x, 100.0f, 0.0f };
		o->setPosition(&p);
		world.logic->runLogicFrame(); // the partition takes the moved entries
		return o;
	}
	void run(int ticks) { for (int i = 0; i < ticks; ++i) trees.advance(TreeTopple::kUpdateSeconds * 1.01); }
};
}

// RW 0x4ED6F1 .. 0x4ED7F2 (target) and RW 0x4EBA54 .. 0x4EBA94 (the step: FadeRate per update, clamped at the target)
TEST_CASE("PORT-266 an object inside FadeDistance sets the target to FadeTarget and the opacity steps towards it at FadeRate")
{
	FadeRig rig(" TreeFadeObjectFilter = ANY +INFANTRY -HERO\n");
	Object *probe = rig.place("InfantryProbe", "Alice", 112.0f); // the bounding circle's edge is 9 away: within 10
	CHECK(rig.trees.updateFadeTargets(*rig.world.logic) == 1);
	CHECK(rig.trees.trees()[0].fadeTarget == 105);
	CHECK(rig.trees.trees()[1].fadeTarget == 255); // the shrub record is not part of the tree buffer
	CHECK(rig.trees.trees()[0].fadeCurrent == 255);
	rig.run(1);
	CHECK(rig.trees.trees()[0].fadeCurrent == 215); // 255 - 40
	rig.run(1);
	CHECK(rig.trees.trees()[0].fadeCurrent == 175);
	rig.run(2);
	CHECK(rig.trees.trees()[0].fadeCurrent == 105); // 135 - 40 = 95 is clamped at the target
	CHECK(rig.trees.takeFadeChanged().size() == 4);
	rig.run(1);
	CHECK(rig.trees.trees()[0].fadeCurrent == 105);
	CHECK(rig.trees.takeFadeChanged().empty());
	CHECK(rig.trees.trees()[1].fadeCurrent == 255);
	// the object leaves: the target is 255 again and the opacity climbs back (RW 0x4ED780), clamped at 255
	const Coord3D away{ 300.0f, 100.0f, 0.0f };
	probe->setPosition(&away);
	rig.world.logic->runLogicFrame();
	CHECK(rig.trees.updateFadeTargets(*rig.world.logic) == 1);
	CHECK(rig.trees.trees()[0].fadeTarget == 255);
	rig.run(3);
	CHECK(rig.trees.trees()[0].fadeCurrent == 225); // 105 + 3 * 40 = 225
	rig.run(1);
	CHECK(rig.trees.trees()[0].fadeCurrent == 255); // 265 is clamped
}

TEST_CASE("PORT-266 outside the radius, a filtered-out object and the switch off leave the tree opaque")
{
	{
		FadeRig rig(" TreeFadeObjectFilter = ANY +INFANTRY -HERO\n");
		rig.place("InfantryProbe", "Alice", 114.0f); // the edge is 11 away
		CHECK(rig.trees.updateFadeTargets(*rig.world.logic) == 0);
		CHECK(rig.trees.trees()[0].fadeTarget == 255);
		rig.place("HeroProbe", "Alice", 100.0f); // inside, but the filter's -HERO refuses it
		CHECK(rig.trees.updateFadeTargets(*rig.world.logic) == 0);
		CHECK(rig.trees.trees()[0].fadeTarget == 255);
		rig.run(2);
		CHECK(rig.trees.trees()[0].fadeCurrent == 255);
	}
	{
		// RW 0x4ED68E / RW 0x4ED79B / RW 0x4EBA5E: with the switch off nothing is queried and the stored opacities stay as they are (they are not reset to 255)
		FadeRig rig(" TreeFadeObjectFilter = ANY +INFANTRY -HERO\n");
		rig.place("InfantryProbe", "Alice", 105.0f);
		REQUIRE(rig.trees.updateFadeTargets(*rig.world.logic) == 1);
		rig.run(1);
		REQUIRE(rig.trees.trees()[0].fadeCurrent == 215);
		std::string parseError;
		REQUIRE_MESSAGE(GameLogicSettingsLoader::scanGameData(gameData(" AllowTreeFading = No\n"), rig.world.logic->settings(), &parseError), parseError);
		CHECK(rig.trees.updateFadeTargets(*rig.world.logic) == 0);
		CHECK_FALSE(rig.trees.fadeSwitch());
		rig.run(5);
		CHECK(rig.trees.trees()[0].fadeCurrent == 215);
		CHECK(rig.trees.trees()[0].fadeTarget == 105);
	}
}

// RW 0x4ED70A .. 0x4ED76F: the candidate tests
TEST_CASE("PORT-266 a stealthed object fades a tree only when detected or allied to the local player")
{
	FadeRig rig(" TreeFadeObjectFilter = ANY +INFANTRY\n");
	GameLogic &logic = *rig.world.logic;
	Object *mine = rig.place("InfantryProbe", "Alice", 100.0f);
	Object *enemy = rig.place("InfantryProbe", "Bob", 100.0f);
	const unsigned stealthed = (unsigned)CombatNames::status("STEALTHED");
	const unsigned detected = (unsigned)CombatNames::status("DETECTED");
	CHECK(TreeTopple::fadeCandidateAccepted(logic, *enemy)); // not stealthed: counts, enemy or not (RW 0x4ED737 -> RW 0x4ED7DD)
	enemy->setStatus(stealthed, true);
	CHECK_FALSE(TreeTopple::fadeCandidateAccepted(logic, *enemy)); // stealthed, undetected, an enemy of the local player: RW 0x4ED76C
	enemy->setStatus(detected, true);
	CHECK(TreeTopple::fadeCandidateAccepted(logic, *enemy)); // detected (RW 0x4ED749)
	enemy->setStatus(detected, false);
	mine->setStatus(stealthed, true);
	CHECK(TreeTopple::fadeCandidateAccepted(logic, *mine)); // stealthed but allied to the local player (RW 0x4ED76C: relationship 2)
	// the invisibility model conditions (RW 0x68FC06) send every candidate to the alliance test, stealth status or not
	enemy->setStatus(stealthed, false);
	enemy->setModelConditionState(CombatNames::modelCondition("INVISIBLE_STEALTH"), true);
	CHECK_FALSE(TreeTopple::fadeCandidateAccepted(logic, *enemy));
	enemy->setModelConditionState(CombatNames::modelCondition("INVISIBLE_STEALTH"), false);
	enemy->setModelConditionState(CombatNames::modelCondition("INVISIBLE_CAMOUFLAGE"), true);
	CHECK_FALSE(TreeTopple::fadeCandidateAccepted(logic, *enemy));
	mine->setStatus(stealthed, false);
	mine->setModelConditionState(CombatNames::modelCondition("INVISIBLE_CAMOUFLAGE"), true);
	CHECK(TreeTopple::fadeCandidateAccepted(logic, *mine));
}

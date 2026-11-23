// OpenBFME unit tests. GPL-3.0.
// PORT-292: the campaign record's horde overload name, from RotWK 2.01.
#include "doctest.h"
#include "LogicTestUtil.h"
#include "GameLogic/CampaignCarryover.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

#include <string>

namespace
{
std::string hordeObject(const std::string &name, const std::string &kind, const std::string &module, const std::string &body)
{
	return "Object " + name + "\n"
		" KindOf = " + kind + "\n"
		" Body = ImmortalBody BodyTag\n  MaxHealth = 1\n End\n"
		" Behavior = " + module + " ContainTag\n" + body + " End\n"
		"End\n";
}

std::string winningRecord(const std::string &ini, const std::string &objectName)
{
	logictest::LogicWorld world;
	world.logic->settings().bodyThresholdsLoaded = true;
	LogicModules::registerAll(world.w.modules);
	const std::string error = world.w.load(ini, INI_LOAD_OVERWRITE, "synthetic.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *object = world.make(objectName, world.teamOf("Alice"), INVALID_ID, false);
	REQUIRE(object);
	object->setCampaignCarryover(true);
	CampaignCarryover::State state;
	CampaignCarryover::onCurrentMissionWon(*world.logic, world.players, state);
	REQUIRE(state.records.size() == 1);
	CHECK(state.records[0].quantity == 1);
	return state.records[0].templateName;
}
}

TEST_CASE("PORT-292: non-empty overload template is recorded for infantry and cavalry hordes")
{
	// RW 0x6917BB -> slot +0x274 RW 0x86D6FD: nonempty +0x280 replaces the horde's own name.
	CHECK(winningRecord(hordeObject("P292Infantry", "HORDE", "HordeContain", "  LivingWorldOverloadTemplate = P292Replacement\n"), "P292Infantry") == "P292Replacement");
	CHECK(winningRecord(hordeObject("P292Cavalry", "HORDE", "HorseHordeContain", "  LivingWorldOverloadTemplate = P292HorseReplacement\n"), "P292Cavalry") == "P292HorseReplacement");
}

TEST_CASE("PORT-292: empty or missing horde interface falls back and HERO takes precedence")
{
	// RW 0x6918C7..0x6918DE: omitted / empty answer and missing interface use the object's template.
	CHECK(winningRecord(hordeObject("P292Plain", "HORDE", "HordeContain", ""), "P292Plain") == "P292Plain");
	CHECK(winningRecord("Object P292NoContain\n KindOf = HORDE\n Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\nEnd\n", "P292NoContain") == "P292NoContain");
	// An ordinary non-hero non-horde keeps its own template as well.
	CHECK(winningRecord("Object P292Ordinary\n KindOf = INFANTRY\n Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\nEnd\n", "P292Ordinary") == "P292Ordinary");
	// RW 0x6917DF branches to HERO before HORDE, even when both kind bits are present.
	CHECK(winningRecord(hordeObject("P292Hero", "HERO HORDE", "HordeContain", "  LivingWorldOverloadTemplate = P292Ignored\n"), "P292Hero") == "P292Hero");
}

TEST_CASE("PORT-292: the recorded overload survives campaign serialization and selects the next spawned template")
{
	logictest::LogicWorld world;
	world.logic->settings().bodyThresholdsLoaded = true;
	LogicModules::registerAll(world.w.modules);
	const std::string ini = hordeObject("P292Source", "HORDE", "HordeContain", "  LivingWorldOverloadTemplate = P292Next\n")
		+ hordeObject("P292Next", "HORDE", "HordeContain", "");
	const std::string error = world.w.load(ini, INI_LOAD_OVERWRITE, "synthetic.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *source = world.make("P292Source", world.teamOf("Alice"), INVALID_ID, false);
	REQUIRE(source);
	source->setCampaignCarryover(true);
	CampaignCarryover::State won;
	CampaignCarryover::onCurrentMissionWon(*world.logic, world.players, won);
	REQUIRE(won.records.size() == 1);
	CHECK(won.records[0].templateName == "P292Next"); // RW 0x6917BB -> RW 0x86D6FD

	CampaignCarryover::State loaded;
	std::string why;
	REQUIRE_MESSAGE(CampaignCarryover::State::parse(won.serialize(), loaded, &why), why);
	REQUIRE(loaded.records.size() == 1);
	CHECK(loaded.records[0].templateName == "P292Next");
	const CampaignCarryover::SpawnResult spawned = CampaignCarryover::addCarryoverStuffToMap(*world.logic, world.players, loaded, {});
	REQUIRE(spawned.objects.size() == 1);
	CHECK(static_cast<const ThingTemplate *>(spawned.objects[0]->getTemplate())->getName() == "P292Next"); // RW 0x780172
	CHECK(spawned.objects[0]->isCampaignCarryover());
	CHECK(loaded.records.empty());
	REQUIRE(loaded.used.size() == 1);
	CHECK(loaded.used[0].templateName == "P292Next");
}

// OpenBFME. GPL-3.0. PORT-266: synthetic GameData tests, no game files.
#include "doctest.h"
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

// OpenBFME unit tests. GPL-3.0.
// LOCAL-182 (fork issue 963): retail 2.01 constructor defaults for CloudBreakSpecialPowerModuleData (RW 0x008C8C53)
// and TaintSpecialPowerModuleData (RW 0x008C8F71).

#include "doctest.h"
#include "IniTestUtil.h"

#include "Common/INI.h"
#include "GameLogic/Module/SpellBookPowers.h"

#include <string>

using namespace initest;

// Expectations from RotWK 2.01 game.dat RW 0x008C8C53:
//   +0x7C (m_radius)        = *(float*)0x00BD83D8 = 10.0f
//   +0x88 (m_objectSpacing) = *(float*)0x00BD88D8 = 100.0f
TEST_CASE("LOCAL-182 CloudBreakSpecialPowerModuleData retail 2.01 defaults (RW 0x008C8C53)")
{
	CloudBreakSpecialPowerModuleData data;
	CHECK(data.m_radius == 10.0f);
	CHECK(data.m_objectSpacing == 100.0f);

	// Neighboring check: INI parsing without radius/spacing keeps retail defaults; explicit values override them.
	CloudBreakSpecialPowerModuleData omitted;
	{
		Fixture fx;
		fx.env.blocks.registerBlock("CloudBreakBlock", [&omitted](INI *ini) {
			MultiIniFieldParse mp;
			CloudBreakSpecialPowerModuleData::buildFieldParse(mp);
			ini->initFromINIMulti(&omitted, mp);
		});
		const std::string err = loadError(fx.env, "local182_cloudbreak_omitted.ini",
			"CloudBreakBlock\n  SunbeamObject = TestSunbeam\nEnd\n", INI_LOAD_OVERWRITE);
		REQUIRE(err.empty());
	}
	CHECK(omitted.m_sunbeamObject == "TestSunbeam");
	CHECK(omitted.m_radius == 10.0f);
	CHECK(omitted.m_objectSpacing == 100.0f);

	CloudBreakSpecialPowerModuleData overridden;
	{
		Fixture fx;
		fx.env.blocks.registerBlock("CloudBreakBlock", [&overridden](INI *ini) {
			MultiIniFieldParse mp;
			CloudBreakSpecialPowerModuleData::buildFieldParse(mp);
			ini->initFromINIMulti(&overridden, mp);
		});
		const std::string err = loadError(fx.env, "local182_cloudbreak_override.ini",
			"CloudBreakBlock\n  SunbeamObject = TestSunbeam\n  SunbeamObjectSpacing = 250.0\nEnd\n", INI_LOAD_OVERWRITE);
		REQUIRE(err.empty());
	}
	CHECK(overridden.m_objectSpacing == 250.0f);

	// Neighboring weather classes keep their own defaults untouched.
	DarknessSpecialPowerModuleData darkness;
	FreezingRainSpecialPowerModuleData freezing;
	CHECK(darkness.m_radius == 0.0f);
	CHECK(freezing.m_radius == 0.0f);
}

// Expectations from RotWK 2.01 game.dat RW 0x008C8F71:
//   +0x80 (m_taintRadius) = *(float*)0x00BD83D8 = 10.0f
TEST_CASE("LOCAL-182 TaintSpecialPowerModuleData retail 2.01 default radius (RW 0x008C8F71)")
{
	TaintSpecialPowerModuleData data;
	CHECK(data.m_taintRadius == 10.0f);

	// Neighboring check: INI parsing without TaintRadius preserves 10.0f; explicit TaintRadius overrides it.
	TaintSpecialPowerModuleData omitted;
	{
		Fixture fx;
		fx.env.blocks.registerBlock("TaintBlock", [&omitted](INI *ini) {
			MultiIniFieldParse mp;
			TaintSpecialPowerModuleData::buildFieldParse(mp);
			ini->initFromINIMulti(&omitted, mp);
		});
		const std::string err = loadError(fx.env, "local182_taint_omitted.ini",
			"TaintBlock\n  TaintObject = TestTaint\nEnd\n", INI_LOAD_OVERWRITE);
		REQUIRE(err.empty());
	}
	CHECK(omitted.m_taintObject == "TestTaint");
	CHECK(omitted.m_taintRadius == 10.0f);

	TaintSpecialPowerModuleData overridden;
	{
		Fixture fx;
		fx.env.blocks.registerBlock("TaintBlock", [&overridden](INI *ini) {
			MultiIniFieldParse mp;
			TaintSpecialPowerModuleData::buildFieldParse(mp);
			ini->initFromINIMulti(&overridden, mp);
		});
		const std::string err = loadError(fx.env, "local182_taint_override.ini",
			"TaintBlock\n  TaintObject = TestTaint\n  TaintRadius = 175.0\nEnd\n", INI_LOAD_OVERWRITE);
		REQUIRE(err.empty());
	}
	CHECK(overridden.m_taintRadius == 175.0f);
}

// OpenBFME unit tests. GPL-3.0.
// LOCAL-182 (fork issue 963): retail 2.01 constructor defaults for CloudBreakSpecialPowerModuleData (RW 0x008C8C53)
// and TaintSpecialPowerModuleData (RW 0x008C8F71).

#include "doctest.h"

#include "Common/INI/INI.h"
#include "Common/INI/MultiIniFieldParse.h"
#include "GameLogic/Module/SpellBookPowers.h"

#include <sstream>

// Expectations from RotWK 2.01 game.dat RW 0x008C8C53:
//   +0x7C (m_radius)        = *(float*)0x00BD83D8 = 10.0f
//   +0x88 (m_objectSpacing) = *(float*)0x00BD88D8 = 100.0f
TEST_CASE("LOCAL-182 CloudBreakSpecialPowerModuleData retail 2.01 defaults (RW 0x008C8C53)")
{
	CloudBreakSpecialPowerModuleData data;
	CHECK(data.m_radius == 10.0f);
	CHECK(data.m_objectSpacing == 100.0f);

	// Neighboring check: INI parsing without radius/spacing keeps retail defaults; explicit values override them.
	MultiIniFieldParse parse;
	CloudBreakSpecialPowerModuleData::buildFieldParse(parse);

	CloudBreakSpecialPowerModuleData omitted;
	{
		std::istringstream ss("SunbeamObject = TestSunbeam
End
");
		INI ini;
		ini.loadStream(ss, "local182_cloudbreak_omitted.ini", INI_LOAD_OVERWRITE);
		ini.initFromINIMulti(&omitted, parse);
	}
	CHECK(omitted.m_sunbeamObject == "TestSunbeam");
	CHECK(omitted.m_radius == 10.0f);
	CHECK(omitted.m_objectSpacing == 100.0f);

	CloudBreakSpecialPowerModuleData overridden;
	{
		std::istringstream ss("SunbeamObject = TestSunbeam
SunbeamObjectSpacing = 250.0
End
");
		INI ini;
		ini.loadStream(ss, "local182_cloudbreak_override.ini", INI_LOAD_OVERWRITE);
		ini.initFromINIMulti(&overridden, parse);
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
	MultiIniFieldParse parse;
	TaintSpecialPowerModuleData::buildFieldParse(parse);

	TaintSpecialPowerModuleData omitted;
	{
		std::istringstream ss("TaintObject = TestTaint
End
");
		INI ini;
		ini.loadStream(ss, "local182_taint_omitted.ini", INI_LOAD_OVERWRITE);
		ini.initFromINIMulti(&omitted, parse);
	}
	CHECK(omitted.m_taintObject == "TestTaint");
	CHECK(omitted.m_taintRadius == 10.0f);

	TaintSpecialPowerModuleData overridden;
	{
		std::istringstream ss("TaintObject = TestTaint
TaintRadius = 175.0
End
");
		INI ini;
		ini.loadStream(ss, "local182_taint_override.ini", INI_LOAD_OVERWRITE);
		ini.initFromINIMulti(&overridden, parse);
	}
	CHECK(overridden.m_taintRadius == 175.0f);
}

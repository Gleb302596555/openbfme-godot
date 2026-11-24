// OpenBFME. GPL-3.0.
// LOCAL-169: RainOfFireUpdateModuleData (RW 0x8AF6A7, RW 0x8AF701, RW 0x8AF7F7, RW 0x8AF835, RW 0x8AF891)
// and ArmorStore::update (RW 0x5D8C69).
// Expectations come directly from RotWK 2.01 game.dat disassembly.

#include "doctest.h"
#include "IniTestUtil.h"

#include "Common/INI.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Armor.h"
#include "GameLogic/Module/RainOfFireUpdate.h"

#include <string>

using namespace initest;

TEST_CASE("LOCAL-169 RainOfFireUpdate: field parse, defaults, setDarkness, and xfer (RW 0x8AF6A7, RW 0x8AF701, RW 0x8AF7F7, RW 0x8AF835)")
{
	// Defaults from RW 0x8AF6A7
	RainOfFireUpdateModuleData defaults;
	CHECK(defaults.m_startRainTime == 150u);
	CHECK(defaults.m_darknessFadeTime == 120u);
	CHECK(defaults.m_rainEmitterHeight == 100.0f);
	CHECK(defaults.m_darknessLevel == 0.25f);
	CHECK(defaults.m_jitterRadius == 0.0f);
	CHECK(defaults.m_dpsMin == 1.0f);
	CHECK(defaults.m_dpsMax == 1.0f);
	CHECK(defaults.m_dpsRampupTime == 0.0f);
	CHECK(defaults.m_rainOffset.x == 0.0f);
	CHECK(defaults.m_rainOffset.y == 0.0f);

	// Parse all 9 fields of RW 0xC6B618 via RW 0x8AF701
	const std::string iniText =
		"RainBlock\n"
		"  StartRainTime = 3000\n"
		"  DarknessFadeTime = 2000\n"
		"  RainEmitterHeight = 250.5\n"
		"  DarknessLevel = 0.5\n"
		"  JitterRadius = 42.0\n"
		"  DPSMin = 10.0\n"
		"  DPSMax = 50.0\n"
		"  DPSRampupTime = 4000\n"
		"  RainOffset = X:12.5 Y:-7.5\n"
		"End\n";
	RainOfFireUpdateModuleData parsed;
	{
		Fixture fx;
		fx.env.blocks.registerBlock("RainBlock", [&parsed](INI *ini) {
			MultiIniFieldParse mp;
			RainOfFireUpdateModuleData::buildFieldParse(mp);
			ini->initFromINIMulti(&parsed, mp);
		});
		const std::string err = loadError(fx.env, "rain.ini", iniText, INI_LOAD_OVERWRITE);
		REQUIRE(err.empty());
	}

	CHECK(parsed.m_startRainTime == 15u); // 3000 ms at 5 fps = 15 frames
	CHECK(parsed.m_darknessFadeTime == 10u);
	CHECK(parsed.m_rainEmitterHeight == 250.5f);
	CHECK(parsed.m_darknessLevel == 0.5f);
	CHECK(parsed.m_jitterRadius == 42.0f);
	CHECK(parsed.m_dpsMin == 10.0f);
	CHECK(parsed.m_dpsMax == 50.0f);
	CHECK(parsed.m_dpsRampupTime == 20.0f); // 4000 ms at 5 fps = 20 frames
	CHECK(parsed.m_rainOffset.x == 12.5f);
	CHECK(parsed.m_rainOffset.y == -7.5f);

	// Constructor DPS increment + setDarkness formula (RW 0x8AF712, RW 0x8AF7F7)
	RainOfFireUpdate mod(nullptr, &parsed);
	CHECK(mod.dpsIncrement() == 2.0f);
	mod.release(0.5f);
	mod.setDarkness(0.5f);
	// RW 0x8AF812..0x8AF828: 1.0 - (m_darknessLevel * m_scale * amount) = 1.0 - (0.5 * 0.5 * 0.5) = 0.875
	CHECK(mod.effectiveDarkness() == 0.875f);

	// Xfer round-trip (RW 0x8AF835) and loadPostProcess (RW 0x8AF891)
	XferSave saver;
	mod.xfer(saver);
	RainOfFireUpdate restored(nullptr, &parsed);
	XferLoad loader(saver.data());
	restored.xfer(loader);
	restored.loadPostProcess();
	CHECK(restored.darkness() == 0.5f);
	CHECK(restored.scale() == 0.5f);
	CHECK(restored.dpsIncrement() == 2.0f);
	CHECK(restored.effectiveDarkness() == 0.875f);
}

TEST_CASE("LOCAL-169 ArmorStore: update applies pending INI_LOAD_RELOAD overrides (RW 0x5D8C69)")
{
	ArmorStore store;
	ArmorStore *prev = TheArmorStore;
	TheArmorStore = &store;

	Fixture fx;
	fx.env.blocks.registerBlock("Armor", [](INI *ini) { ArmorStore::parseArmorDefinitionGlobal(ini); });

	const std::string baseIni =
		"Armor TestArmor\n"
		"  Armor = DEFAULT 100%\n"
		"  DamageScalar = 100%\n"
		"End\n";
	REQUIRE(loadError(fx.env, "armor_base.ini", baseIni, INI_LOAD_OVERWRITE).empty());
	REQUIRE(store.findArmorTemplate("TestArmor") != nullptr);
	CHECK(store.findArmorTemplate("TestArmor")->m_damageScalar == 1.0f);
	CHECK_FALSE(store.update()); // no pending overrides yet

	const std::string reloadIni =
		"Armor TestArmor\n"
		"  Armor = DEFAULT 50%\n"
		"  DamageScalar = 75%\n"
		"End\n";
	REQUIRE(loadError(fx.env, "armor_reload.ini", reloadIni, INI_LOAD_RELOAD).empty());
	// Before update(), the original template is still active and flagged overridden (RW 0x5D8D7D)
	CHECK(store.isOverridden("TestArmor"));
	CHECK(store.overrideCount() == 1u);
	CHECK(store.findArmorTemplate("TestArmor")->m_damageScalar == 1.0f);

	// RW 0x5D8C69: update() swaps pending overrides into m_templates and clears pending list
	CHECK(store.update());
	CHECK(store.overrideCount() == 0u);
	CHECK_FALSE(store.isOverridden("TestArmor"));
	CHECK(store.findArmorTemplate("TestArmor")->m_damageScalar == 0.75f);
	CHECK(store.findArmorTemplate("TestArmor")->getCoefficient(0) == 0.5f);
	CHECK_FALSE(store.update()); // idempotent once drained

	TheArmorStore = prev;
}

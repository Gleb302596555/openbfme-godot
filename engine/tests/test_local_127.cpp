// OpenBFME unit tests. GPL-3.0.
// LOCAL-127 (fork issue 811): Ennorath 1.9.8.5 paged options menu, option hotkeys, and online delay fix.
// Sources of expectations:
// - PYSAGE 1.9.8.5 .optui 0xF23000 (28-entry gadget table 0xF25960, 135-entry key table 0xF24640, hooks RW 0x5DA8F0, 0x5ED37A, 0x639FEF, 0x6E573C, 0x9204D9, 0x920602, 0x9212BA)
// - PYSAGE 1.9.8.5 .optfix 0xF30000 (key name labels and default suffix formatting)
// - PYSAGE 1.9.8.5 .keycmd 0xF31020 (synthetic MetaMapRec dispatch for custom hotkeys)
// - PYSAGE 1.9.8.5 .dlyfix 0xF35000 (Options::ETDelayFix, NetworkDelayFix, hooks RW 0x632535, 0x632A9B, in-place RW 0x62ECC8/0x632606/0x632633/0x63264C/0x632AD1)
// - PYSAGE 1.9.8.5 .hotkeys 0xF38000 / .keydef 0xF39000 (hooks RW 0x5DA8F5, 0x5DA910, 0x812C89, 0x812EDA, 0x81FAAB; defaults Builder=B, MatchType=E)
// - Ennorath 1.9.8.5 apt/Options.big (Options.apt ETPage0..4/9, ETBegin/ETCancel/ETReset, Options::ET* gadgets)

#include "doctest.h"
#include "AptRetail.h"

#include "Common/ArchiveFile.h"
#include "Common/ModEngineFeatures.h"
#include "GameClient/EnnorathOptions.h"
#include "GameClient/GUI/AptGadgetLayer.h"
#include "GameClient/GUI/AptScreens/AptScreenFactories.h"
#include "GameClient/GUI/AptScreens/AptSimpleScreens.h"
#include "GameClient/GUI/Gadgets.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/OptionPreferences.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{

struct ScopedModEngineReset
{
	~ScopedModEngineReset() { ModEngineFeatures::reset(); }
};

void activateEnnorath1985()
{
	std::string err;
	REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
}

} // namespace

TEST_CASE("LOCAL-127 profile: Ennorath 1.9.8.5 profile activates options-pages, option-hotkeys, online-delay-fix; reset returns retail 2.01")
{
	// Sources: PYSAGE 1.9.8.5 .optui / .optfix / .hotkeys / .keydef / .keycmd / .dlyfix (md5 fc32e4c4142b5c607b90972eaf448400)
	ScopedModEngineReset guard;
	ModEngineFeatures::reset();
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionsPages));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOnlineDelayFix));

	activateEnnorath1985();
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionsPages));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathOnlineDelayFix));
	CHECK(std::string(ModEngineFeatures::name(ModEngineFeature::EnnorathOptionsPages)) == "ennorath.options-pages");
	CHECK(std::string(ModEngineFeatures::name(ModEngineFeature::EnnorathOptionHotkeys)) == "ennorath.option-hotkeys");
	CHECK(std::string(ModEngineFeatures::name(ModEngineFeature::EnnorathOnlineDelayFix)) == "ennorath.online-delay-fix");

	ModEngineFeatures::reset();
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionsPages));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOnlineDelayFix));
}

TEST_CASE("LOCAL-127 options state: load, normalize B/E defaults, duplicate hotkey priority, save, and default label formatting")
{
	// Sources: PYSAGE 1.9.8.5 .optui 0xF2325C, 0xF23C91, 0xF23D0A; .keydef 0xF39035, 0xF391E6; .optfix 0xF30000; .dlyfix 0xF3511C, 0xF35189
	OptionPreferences prefs;
	prefs.parse(
		"ETFrameRate = 30\n"
		"ETMouseSensitivity = 75\n"
		"ETKeyBuilder = 2\n"       // B -> normalizes to 0 (.keydef 0xF39043)
		"ETKeyMatchType = 5\n"     // E -> normalizes to 0 (.keydef 0xF39052)
		"ETKeyCameraReset = 7\n"   // G
		"ETKeyStop = 7\n"          // G (later row wins over CameraReset on resolveDuplicateHotkeys)
		"NetworkDelayFix = 0\n");

	EnnorathOptionsState state;
	state.loadFromPreferences(&prefs);
	CHECK(state.values[0] == 30);
	CHECK(state.values[1] == 75);
	CHECK(state.values[EnnorathOptions::kEntryBuilder] == 0);
	CHECK(state.values[EnnorathOptions::kEntryMatchType] == 0);
	CHECK(state.values[6] == 7); // ETKeyCameraReset
	CHECK(state.values[8] == 7); // ETKeyStop
	CHECK_FALSE(state.delayFix);

	state.resolveDuplicateHotkeys();
	CHECK(state.values[6] == 0); // earlier row cleared (.optui 0xF23CD7)
	CHECK(state.values[8] == 7); // later row kept

	OptionPreferences saved;
	state.saveToPreferences(saved, true, true, true);
	CHECK(saved.get("ETFrameRate") == "30");
	CHECK(saved.get("ETMouseSensitivity") == "75");
	CHECK(saved.get("ETKeyCameraReset") == "0");
	CHECK(saved.get("ETKeyStop") == "7");
	CHECK(saved.get("ETKeyMatchType") == "0"); // ETKeyBuildPath writes ETKeyMatchType
	CHECK(saved.get("NetworkDelayFix") == "0");

	// Default labels (.optfix 0xF30000 -> .keydef 0xF391E6)
	CHECK(EnnorathOptions::formatDefaultKeyLabel(EnnorathOptions::kEntryBuilder, nullptr, nullptr) == u"B (default)");
	CHECK(EnnorathOptions::formatDefaultKeyLabel(EnnorathOptions::kEntryMatchType, nullptr, nullptr) == u"E (default)");
	CHECK(EnnorathOptions::formatDefaultKeyLabel(6, nullptr, nullptr) == u"Middle Button (default)");
}

TEST_CASE("LOCAL-127 delay-fix: network frame counts with profile ON vs OFF (.dlyfix 0xF35000 .. 0xF3506B)")
{
	// Sources: PYSAGE 1.9.8.5 .dlyfix 0xF35000 .. 0xF3506B (hooks RW 0x632535, 0x632A9B) + .optui 0xF238A4 .. 0xF238FE (in-place RW 0x62ECC8..0x632AD1)
	ScopedModEngineReset guard;

	// Profile OFF: retail 2.01 (divisor 5, stepLimit 6, caps 6 / 10)
	ModEngineFeatures::reset();
	{
		const auto r = EnnorathDelayFix::compute(true, 30, true);
		CHECK_FALSE(r.delayFixActive);
		CHECK(r.logicTickDivisor == 5);
		CHECK(r.stepLimit == 6);
		CHECK(r.frameCountCap == 6);
		CHECK(r.frameCountCapMin10 == 10);
	}

	// Profile ON: delay fix enabled (default) vs disabled in LAN/Online vs offline
	activateEnnorath1985();
	{
		const auto on30 = EnnorathDelayFix::compute(true, 30, true);
		CHECK(on30.delayFixActive);
		CHECK(on30.logicTickDivisor == 8);
		CHECK(on30.stepLimit == 3);
		CHECK(on30.frameCountCap == 6);
		CHECK(on30.frameCountCapMin10 == 10);

		const auto on60 = EnnorathDelayFix::compute(true, 60, true);
		CHECK(on60.delayFixActive);
		CHECK(on60.logicTickDivisor == 20);
		CHECK(on60.stepLimit == 3);
		CHECK(on60.frameCountCap == 12);
		CHECK(on60.frameCountCapMin10 == 12);

		const auto offNet60 = EnnorathDelayFix::compute(false, 60, true);
		CHECK_FALSE(offNet60.delayFixActive);
		CHECK(offNet60.logicTickDivisor == 5);
		CHECK(offNet60.stepLimit == 12);

		const auto offOffline30 = EnnorathDelayFix::compute(false, 30, false);
		CHECK(offOffline30.delayFixActive); // .dlyfix 0xF35010: only turns off in LAN/Online
		CHECK(offOffline30.logicTickDivisor == 8);
		CHECK(offOffline30.stepLimit == 3);
	}
}

TEST_CASE("LOCAL-127 Ennorath 1.9.8.5 Options.apt: paged visibility (ETPage0..4/9), ETReset, ETFrameRate display-only, and Save")
{
	// Sources: Ennorath 1.9.8.5 apt/Options.big (Options.apt), PYSAGE 1.9.8.5 .optui 0xF233B7 / 0xF23558 / 0xF23BF3, .dlyfix 0xF35070 / 0xF35102 / 0xF35189
	ScopedModEngineReset guard;
	const char *ennRoot = std::getenv("OPENBFME_ENNORATH_INSTALL");
	if (!ennRoot || !*ennRoot || !std::filesystem::exists(std::filesystem::path(ennRoot) / "apt" / "Options.big"))
	{
		MESSAGE("SKIP: OPENBFME_ENNORATH_INSTALL with apt/Options.big not present");
		return;
	}
	OPENBFME_REQUIRE_RETAIL(mount);
	// Overlay Ennorath 1.9.8.5's apt/Options.big on top of retail RotWK
	std::string mountErr;
	const std::string optBig = (std::filesystem::path(ennRoot) / "apt" / "Options.big").string();
	auto optArch = mount.fs.openArchiveFile(optBig, &mountErr);
	REQUIRE_MESSAGE(optArch != nullptr, mountErr);
	mount.fs.mountArchive(std::move(optArch), optBig, true);

	activateEnnorath1985();

	const std::string file = (std::filesystem::temp_directory_path() / "openbfme-local127-options-test.ini").string();
	{
		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		out << "StaticGameLOD = High\nETFrameRate = 60\nETMouseSensitivity = 100\nNetworkDelayFix = 1\n";
	}
	OptionPreferences prefs;
	REQUIRE(prefs.load(file));

	AptArchiveFileSource source(mount.fs);
	RecordingShellServices services;
	ShellEnvironment environment;
	environment.options = &prefs;
	environment.optionsFile = file;
	environment.openbfmeOptionsMenu = true; // must be suppressed by EnnorathOptionsPages so mod's paged Options.apt runs natively
	AptScreenFactoryTable factories;
	registerAptScreenFactories(factories);
	GadgetSkinData skins;
	std::string error;
	REQUIRE_MESSAGE(loadGadgetSkinData(mount.fs, skins, &error), error);

	auto wm = std::make_unique<WindowManager>(source, services);
	auto layer = std::make_unique<AptGadgetLayer>(*wm, source, skins);
	layer->registerComponents();
	auto shell = std::make_unique<Shell>(*wm, factories, services, environment);
	wm->setShell(shell.get());
	wm->init();
	auto tick = [&](int n) {
		for (int i = 0; i < n; ++i)
		{
			wm->update(33);
		}
	};
	tick(2);
	shell->push("MainMenu.apt");
	tick(5);
	AptOptionsScreen::open(*shell, true, true, true, false);
	tick(30);

	AptOptionsScreen *options = dynamic_cast<AptOptionsScreen *>(shell->top());
	REQUIRE(options != nullptr);
	CHECK_FALSE(options->menuMode());
	CHECK(options->pageState() == 1);

	// Page 0 (Graphics): Detail/Brightness/HealthBars visible, other pages' gadgets hidden
	REQUIRE(wm->invokeCallback("AptOptions::ETPage0", ""));
	CHECK(options->etPage() == 0);
	CHECK_FALSE(options->etGadgetHidden("Detail"));
	CHECK_FALSE(options->etGadgetHidden("Brightness"));
	CHECK_FALSE(options->etGadgetHidden("HealthBars"));
	CHECK(options->etGadgetHidden("Resolution"));
	CHECK(options->etGadgetHidden("ETFrameRate"));
	CHECK(options->etGadgetHidden("MusicVolume"));
	CHECK(options->etGadgetHidden("ETKeyCameraReset"));
	CHECK(options->etGadgetHidden("ETDelayFix"));

	// Page 1 (Screen): Resolution & ETFrameRate visible, Page 0 hidden
	REQUIRE(wm->invokeCallback("AptOptions::ETPage1", ""));
	CHECK(options->etPage() == 1);
	CHECK(options->etGadgetHidden("Detail"));
	CHECK_FALSE(options->etGadgetHidden("Resolution"));
	CHECK_FALSE(options->etGadgetHidden("ETFrameRate"));
	REQUIRE(options->gadget("ETFrameRate") != nullptr);
	CHECK(GadgetComboBoxGetLength(options->gadget("ETFrameRate")) == 2);
	GadgetComboBoxSetSelectedPos(options->gadget("ETFrameRate"), 0, false); // 30 FPS (display-only)

	// Page 2 (Sound)
	REQUIRE(wm->invokeCallback("AptOptions::ETPage2", ""));
	CHECK(options->etPage() == 2);
	CHECK(options->etGadgetHidden("ETFrameRate"));
	CHECK_FALSE(options->etGadgetHidden("MusicVolume"));
	CHECK(options->etGadgetHidden("ETMouseSensitivity"));

	// Page 3 (Hotkeys)
	REQUIRE(wm->invokeCallback("AptOptions::ETPage3", ""));
	CHECK(options->etPage() == 3);
	CHECK(options->etGadgetHidden("MusicVolume"));
	CHECK_FALSE(options->etGadgetHidden("ETMouseSensitivity"));
	CHECK_FALSE(options->etGadgetHidden("ETKeyCameraReset"));
	CHECK_FALSE(options->etGadgetHidden("ETKeyBuildPath"));
	REQUIRE(options->gadget("ETMouseSensitivity") != nullptr);
	CHECK(GadgetComboBoxGetLength(options->gadget("ETMouseSensitivity")) == 3);
	GadgetComboBoxSetSelectedPos(options->gadget("ETMouseSensitivity"), 1, false); // 75%
	REQUIRE(options->gadget("ETKeyCameraReset") != nullptr);
	CHECK(GadgetComboBoxGetLength(options->gadget("ETKeyCameraReset")) == EnnorathOptions::kKeyChoiceCount);
	GadgetComboBoxSetSelectedPos(options->gadget("ETKeyCameraReset"), 7, false); // G

	// Page 4 (Network)
	REQUIRE(wm->invokeCallback("AptOptions::ETPage4", ""));
	CHECK(options->etPage() == 4);
	CHECK(options->etGadgetHidden("ETKeyCameraReset"));
	CHECK_FALSE(options->etGadgetHidden("OnlineIp"));
	CHECK_FALSE(options->etGadgetHidden("ETDelayFix"));
	REQUIRE(options->gadget("ETDelayFix") != nullptr);
	CHECK(GadgetCheckBoxIsChecked(options->gadget("ETDelayFix")));
	GadgetCheckBoxSetChecked(options->gadget("ETDelayFix"), false);

	// Page 9 (Advanced): all basic page gadgets hidden
	REQUIRE(wm->invokeCallback("AptOptions::ETPage9", ""));
	CHECK(options->etPage() == 9);
	CHECK(options->etGadgetHidden("Detail"));
	CHECK(options->etGadgetHidden("ETDelayFix"));

	// ETReset resets combos to defaults (60 FPS, 100%, key 0) and re-checks ETDelayFix
	REQUIRE(wm->invokeCallback("AptOptions::ETPage4", ""));
	REQUIRE(wm->invokeCallback("AptOptions::ETReset", ""));
	int fpsSel = -1, sensSel = -1, keySel = -1;
	GadgetComboBoxGetSelectedPos(options->gadget("ETFrameRate"), &fpsSel);
	GadgetComboBoxGetSelectedPos(options->gadget("ETMouseSensitivity"), &sensSel);
	GadgetComboBoxGetSelectedPos(options->gadget("ETKeyCameraReset"), &keySel);
	CHECK(fpsSel == 1);  // 60 FPS
	CHECK(sensSel == 2); // 100%
	CHECK(keySel == 0);  // Default
	CHECK(GadgetCheckBoxIsChecked(options->gadget("ETDelayFix")));

	// Set new values and Save: verify Options.ini gets all updated values and ETFrameRate does not alter OpenBFME FPS
	GadgetComboBoxSetSelectedPos(options->gadget("ETFrameRate"), 0, false);        // 30 FPS (display-only)
	GadgetComboBoxSetSelectedPos(options->gadget("ETMouseSensitivity"), 1, false); // 75%
	GadgetComboBoxSetSelectedPos(options->gadget("ETKeyCameraReset"), 7, false);   // G
	GadgetCheckBoxSetChecked(options->gadget("ETDelayFix"), false);
	REQUIRE(wm->invokeCallback("AptOptions::Save", ""));
	tick(30);

	OptionPreferences reloaded;
	REQUIRE(reloaded.load(file));
	CHECK(reloaded.get("ETFrameRate") == "30");
	CHECK(reloaded.get("ETMouseSensitivity") == "75");
	CHECK(reloaded.get("ETKeyCameraReset") == "7");
	CHECK(reloaded.get("NetworkDelayFix") == "0");
	for (const auto &applied : services.appliedOptions)
	{
		CHECK(applied.first != "ETFrameRate"); // owner 2026-11-15: OpenBFME keeps its own frame rate
	}

	shell.reset();
	layer.reset();
	std::remove(file.c_str());
}

// OpenBFME. GPL-3.0.
// Local 125 (fork issue 809): global settings readers — ControlBarScheme, Mouse tooltip, Upgrade StrategicIcon, ScoredKillEvaAnnouncer.
// Expectations from RotWK 2.01 game.dat (RW addresses cited per case).

#include "doctest.h"
#include "ProdTestUtil.h"

#include "Common/INI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarScheme.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
void loadSchemesFromText(ControlBarSchemeStore &store, const std::string &text)
{
	INIEnvironment env;
	store.registerBlock(env.blocks);
	INI ini(env);
	ini.loadMemory("controlbarscheme.ini", std::vector<std::uint8_t>(text.begin(), text.end()), INI_LOAD_OVERWRITE);
}
} // namespace

// RW 0x7206D5 (ControlBarScheme::init -> 0x720704 BuildUpClockColor -> RW 0x8DBE7E; 0x720796..0x7207C6 ScreenCreationRes multiplier)
// RW 0x7216B6 (ControlBarSchemeManager::setControlBarSchemeByPlayerTemplate -> 0x72174E..0x72175D picks largest ScreenCreationRes.x)
TEST_CASE("LOCAL-125 ControlBarScheme: BuildUpClockColor and ScreenCreationRes (RW 0x7206D5 / 0x7216B6)")
{
	const std::string ini =
		"ControlBarScheme MenLow\n"
		"  ScreenCreationRes = X:800 Y:600\n"
		"  Side = Men\n"
		"  BuildUpClockColor = R:10 G:20 B:30 A:128\n"
		"End\n"
		"ControlBarScheme MenHigh\n"
		"  ScreenCreationRes = X:1024 Y:768\n"
		"  Side = Men\n"
		"  BuildUpClockColor = R:40 G:80 B:120 A:200\n"
		"End\n";

	ControlBarSchemeStore store;
	loadSchemesFromText(store, ini);

	// RW 0x72174E..0x72175D: among schemes for the same side, largest ScreenCreationRes.x wins
	const ControlBarScheme *chosen = store.findSchemeBySide("Men");
	REQUIRE(chosen != nullptr);
	CHECK(chosen->m_name == "MenHigh");
	CHECK(chosen->m_screenCreationRes.x == 1024);
	CHECK(chosen->m_screenCreationRes.y == 768);

	prodtest::ProdWorld f;
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *f.logic, nullptr, view, ui, stream, &f.commands, MouseSettings{} };
	ctx.schemes = &store;
	ControlBar bar(ctx);
	bar.setDisplaySize(2048, 1536);
	REQUIRE(bar.setSchemeBySide("Men"));

	// RW 0x720704 -> RW 0x8DBE7E: BuildUpClockColor applied to ControlBar
	CHECK(bar.buildUpClockColor() == chosen->m_buildUpClockColor);
	// RW 0x7207B1 / 0x7207C1: display / ScreenCreationRes multiplier
	CHECK(bar.schemeMultiplierX() == 2.0f);
	CHECK(bar.schemeMultiplierY() == 2.0f);
}

#include "GameClient/GUI/CursorTooltip.h"

// RW 0x5EE1A6 (Mouse::setCursorTooltip): TooltipBackgroundColor (+0x12BC), UseTooltipAltTextColor (+0x12E0),
// UseTooltipAltBackColor (+0x12E1), AdjustTooltipAltColor (+0x12E2).
TEST_CASE("LOCAL-125 Mouse tooltip: TooltipBackgroundColor and alt-color flags (RW 0x5EE1A6)")
{
	const std::string mouseIni =
		"Mouse\n"
		"  TooltipFontName = Arial\n"
		"  TooltipFontSize = 10\n"
		"  TooltipFontIsBold = No\n"
		"  TooltipDelayTime = 100\n"
		"  TooltipTextColor = R:220 G:220 B:220 A:255\n"
		"  TooltipBackgroundColor = R:20 G:30 B:40 A:127\n"
		"  UseTooltipAltTextColor = Yes\n"
		"  UseTooltipAltBackColor = Yes\n"
		"  AdjustTooltipAltColor = Yes\n"
		"End\n";

	CursorTooltipSettings s;
	std::string err;
	REQUIRE(CursorTooltipSettings::scan(mouseIni, "", s, &err));
	CHECK(s.backgroundColor == 0x7F141E28u);
	CHECK(s.useAltTextColor == true);
	CHECK(s.useAltBackColor == true);
	CHECK(s.adjustAltColor == true);

	// RW 0x5EE1F7..0x5EE29A with AdjustTooltipAltColor=Yes and alt=(0.0, 0.5, 1.0):
	// text: ((c + 1.0) * 127.5) -> (127, 191, 255), alpha 0xFF
	// back: (c * 127.5)         -> (0, 63, 127),    alpha 0x7F
	CursorTooltip tip;
	tip.setTooltipWithAltColor(u"Hello", 0, 0.0f, 0.5f, 1.0f, s);
	CHECK(tip.activeTextColor() == 0xFF7FBFFFu);
	CHECK(tip.activeBackgroundColor() == 0x7F003F7Fu);

	// RW 0x5EE230 / 0x5EE28B with AdjustTooltipAltColor=No: direct * 255.0
	s.adjustAltColor = false;
	tip.setTooltipWithAltColor(u"Hello", 0, 0.0f, 0.5f, 1.0f, s);
	CHECK(tip.activeTextColor() == 0xFF007FFFu);
	CHECK(tip.activeBackgroundColor() == 0x7F007FFFu);
}

#include "Common/Upgrade.h"

// RW 0x66F58E / 0x66F5B6 (UpgradeTemplate::cacheButtonImage -> +0x90) and WoTR readers RW 0x9FFC42 / 0xA08500
TEST_CASE("LOCAL-125 Upgrade StrategicIcon: resolves War of the Ring icons (RW 0x66F58E / 0x9FFC42)")
{
	const std::string iniText =
		"Upgrade Upgrade_TestArmor\n"
		"  Type = OBJECT\n"
		"  StrategicIcon = UIHeavyArmor\n"
		"End\n"
		"Upgrade Upgrade_TestNoIcon\n"
		"  Type = OBJECT\n"
		"End\n"
		"Upgrade Upgrade_TestBlades\n"
		"  Type = OBJECT\n"
		"  StrategicIcon = UIForgedBlades\n"
		"End\n";

	UpgradeCenter center;
	UpgradeCenter *prev = TheUpgradeCenter;
	TheUpgradeCenter = &center;
	INIEnvironment env;
	center.registerBlock(env.blocks);
	INI ini(env);
	ini.loadMemory("upgrade.ini", std::vector<std::uint8_t>(iniText.begin(), iniText.end()), INI_LOAD_OVERWRITE);
	TheUpgradeCenter = prev;

	const std::vector<std::string> icons = center.strategicIconsForUpgrades(
		{ "Upgrade_TestArmor", "Upgrade_TestNoIcon", "Upgrade_Unknown", "Upgrade_TestBlades" });
	REQUIRE(icons.size() == 2);
	CHECK(icons[0] == "UIHeavyArmor");
	CHECK(icons[1] == "UIForgedBlades");
}

#include "Common/Audio/AudioIni.h"
#include "Common/Audio/ScoredKillEvaAnnouncer.h"
#include "Common/XferSave.h"
#include "Common/XferLoad.h"

// RW 0x8256E4 (addMoreTrackersForNewPlayers) & RW 0x825995 (update) & RW 0x961353 (ScoredKillTracker::accepts):
// exercises CountOnlyKillsByLocalPlayer, CountOnlyKillsAgainstLocalPlayer, MinimumCountForAnnouncement,
// MaximumTimeForAnnouncementMS and xfer round-trip.
TEST_CASE("LOCAL-125 ScoredKillEvaAnnouncer: thresholds, player masks, expiry and xfer (RW 0x8256E4 / 0x825995)")
{
	const std::string evaText =
		"NewEvaEvent TestKillStreak\n"
		"  Priority = 5\n"
		"End\n"
		"ScoredKillEvaAnnouncer LocalSlayer\n"
		"  EvaEvent = TestKillStreak\n"
		"  CountOnlyKillsByLocalPlayer = Yes\n"
		"  CountOnlyKillsAgainstLocalPlayer = No\n"
		"  MinimumCountForAnnouncement = 2\n"
		"  MaximumTimeForAnnouncementMS = 1000\n"
		"  ObjectFilter = ALL\n"
		"End\n"
		"ScoredKillEvaAnnouncer LocalVictim\n"
		"  EvaEvent = TestKillStreak\n"
		"  CountOnlyKillsByLocalPlayer = No\n"
		"  CountOnlyKillsAgainstLocalPlayer = Yes\n"
		"  MinimumCountForAnnouncement = 1\n"
		"  MaximumTimeForAnnouncementMS = 1000\n"
		"  ObjectFilter = ALL\n"
		"End\n";

	AudioEventInfoStore audioStore;
	EvaEventStore evaStore;
	INIEnvironment env;
	env.blocks.registerBlock("NewEvaEvent", [&](INI *ini) { evaStore.parseNew(ini, audioStore, false); });
	env.blocks.registerBlock("ScoredKillEvaAnnouncer", [&](INI *ini) { evaStore.parseScoredKillAnnouncer(ini); });
	INI ini(env);
	ini.loadMemory("eva.ini", std::vector<std::uint8_t>(evaText.begin(), evaText.end()), INI_LOAD_OVERWRITE);

	REQUIRE(evaStore.announcers().size() == 2);
	const int eventIdx = evaStore.findIndex("TestKillStreak", false);
	REQUIRE(eventIdx == EvaEventStore::kPredefinedCount);

	prodtest::ProdWorld w;
	w.w.load(
		"Object Local125TestUnit\n"
		"  KindOf = INFANTRY\n"
		"End\n");

	Player *p0 = w.logic->players().getNthPlayer(0);
	Player *p1 = w.logic->players().getNthPlayer(1);
	REQUIRE(p0 != nullptr);
	REQUIRE(p1 != nullptr);

	Object *victimOfP0 = w.make("Local125TestUnit", p1->getDefaultTeam());
	Object *victimOfP1 = w.make("Local125TestUnit", p0->getDefaultTeam());
	REQUIRE(victimOfP0 != nullptr);
	REQUIRE(victimOfP1 != nullptr);
	const Coord3D p0Pos{ 10.0f, 20.0f, 0.0f };
	victimOfP0->setPosition(&p0Pos);
	const Coord3D p1Pos{ 30.0f, 40.0f, 0.0f };
	victimOfP1->setPosition(&p1Pos);

	ScoredKillEvaAnnouncerController ctrl;
	ctrl.loadFromStore(evaStore);
	ctrl.setLocalPlayerIndex(0);

	// Initialize trackers via first update (RW 0x825995)
	auto fired0 = ctrl.update(*w.logic, 0);
	CHECK(fired0.empty());

	// 1st kill by local player (p0 kills p1's unit): below threshold 2 for LocalSlayer, not against local player
	ctrl.onObjectKilled(*w.logic, 0, *victimOfP0, 0);
	auto fired1 = ctrl.update(*w.logic, 0);
	CHECK(fired1.empty());

	// 2nd kill by local player within 1000ms window: meets MinimumCountForAnnouncement=2 -> fires TestKillStreak
	ctrl.onObjectKilled(*w.logic, 0, *victimOfP0, 0);
	int reportedEvent = -1;
	Coord3D reportedPos{};
	auto fired2 = ctrl.update(*w.logic, 0, [&](int ev, const Coord3D *pos) {
		reportedEvent = ev;
		if (pos) reportedPos = *pos;
	});
	REQUIRE(fired2.size() == 1);
	CHECK(reportedEvent == eventIdx);
	CHECK(reportedPos.x == 10.0f);
	CHECK(reportedPos.y == 20.0f);

	// Save & load xfer round-trip preserves tracked kills
	XferSave xs;
	ctrl.xfer(xs);
	ScoredKillEvaAnnouncerController restored;
	restored.loadFromStore(evaStore);
	XferLoad xl(xs.data());
	restored.xfer(xl);
	CHECK(restored.announcers()[0].trackers.size() == 1);
	CHECK(restored.announcers()[0].trackers[0].count() == 2);

	// Advance past MaximumTimeForAnnouncementMS (1000ms = 5 frames at 5fps): kills expire
	for (int i = 0; i < 10; ++i) w.logic->update(1);
	auto firedAfterExpiry = ctrl.update(*w.logic, 0);
	CHECK(firedAfterExpiry.empty());

	// Kill against local player (p1 kills p0's unit): triggers LocalVictim (MinimumCountForAnnouncement=1)
	ctrl.onObjectKilled(*w.logic, 1, *victimOfP1, 0);
	auto firedVictim = ctrl.update(*w.logic, 0);
	REQUIRE(firedVictim.size() == 1);
	CHECK(firedVictim[0].evaEvent == eventIdx);
	CHECK(firedVictim[0].position.x == 30.0f);
}

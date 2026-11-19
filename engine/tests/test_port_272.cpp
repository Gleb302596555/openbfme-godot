// OpenBFME. PORT-272: the two lines actually offered to Godot, not the labels in the control bar.
// Synthetic map, INIs, translations and APT button. No installed game, movie, font, or renderer. GPL-3.0.
#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "IniTestUtil.h"
#include "MapTestUtil.h"

#include "Common/Player.h"
#include "GameClient/GUI/AptScreens/AptDisconnectScreen.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/GUI/GameTextSource.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/InGameHud.h"
#include "GameClient/LiveGame.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
const std::string content = "_level1.CommandButtons.Command2.content";
constexpr int x = 205, y = 165; // inside the authored 10 x 10 hit mesh

apttest::TestMovie movie()
{
	using namespace apttest;
	TestMovie m;
	m.addCharacter(0);
	const auto art = m.addCharacter(m.addShape(0, 0, 10, 10, 1));
	const auto press = program(m, [](Asm &a) { fscmd(a, content + "_OnPress"); });
	const auto button = m.addCharacter(m.addButton({ { 15, art } }, { { 0x04, 0, press } })); // OverUp -> OverDown
	const auto state = program(m, [](Asm &a) { a.defineFunction("SetState", { "state" }, [](Asm &) {}); });
	const auto slot = m.addCharacter(m.addSprite({ { m.addPlaceItem(placeChar(button, 1, "hit")), m.addActionItem(state) } }));
	m.addExport("TestSlot", slot);
	const auto create = program(m, [](Asm &a) {
		a.defineFunction("CreateContent", { "type", "name" }, [](Asm &f) {
			f.pushByte(1).pushString("content").pushString("TestSlot").pushByte(3).getStringVar("this").pushString("attachMovie").op(APT_OP_EA_CALLMETHODPOP);
			f.getStringVar("extern").pushString("_level1.CommandButtons.Command2_ContentName").setStringMember("TestSlot");
		});
	});
	const auto frame = m.addCharacter(m.addSprite({ { m.addActionItem(create) } }));
	auto pad = placeChar(frame, 1, "Command2");
	pad.flags |= APT_PLACE_HASMATRIX;
	pad.translation[0] = 200;
	pad.translation[1] = 160;
	const auto buttons = m.addCharacter(m.addSprite({ { m.addPlaceItem(pad) } }));
	const auto helpFns = program(m, [](Asm &a) {
		for (const char *fn : { "SampleContentWidth", "Hide", "Show" }) a.defineFunction(fn, {}, [](Asm &) {});
	});
	const auto help = m.addCharacter(m.addSprite({ { m.addActionItem(helpFns) } }));
	const auto init = program(m, [](Asm &a) {
		fscmd(a, "AptPalantir::OnInitialized");
		a.getURL("FSCommand:PalantirCommandUI::OnButtonFrameLoaded", "index=1&name=CommandButtons.Command2");
	});
	m.setRootFrames({ { m.addPlaceItem(placeChar(buttons, 1, "CommandButtons")), m.addPlaceItem(placeChar(help, 2, "helpBox")), m.addActionItem(init) } });
	return m;
}

// Small, unmounted LiveGame. Unrelated required settings only let the real HUD boot.
std::string flatMap()
{
	using namespace maptest;
	Toc t;
	W heights;
	heights.i32(16).i32(16).i32(0).i32(1).i32(16).i32(16).i32(256);
	for (int i = 0; i < 256; ++i) heights.u16(0);
	Bytes chunks = cat(chunk(t, "HeightMapData", 5, heights.b), chunk(t, "WorldInfo", 1, D(t).i("weather", 0).bytes()));
	D side(t), team(t);
	side.s("playerName", "TestPlayer").b("playerIsHuman", true);
	team.s("teamName", "teamTestPlayer").s("teamOwner", "TestPlayer").b("teamIsSingleton", true);
	chunks = cat(chunks, chunk(t, "SidesList", 6, W().u8(1).i32(1).raw(side.bytes()).i32(0).b));
	chunks = cat(chunks, chunk(t, "Teams", 1, W().i32(1).raw(team.bytes()).b));
	const Bytes map = file(t, chunks);
	return { map.begin(), map.end() };
}

std::string gameData()
{
	std::string data = R"(GameData
 ForceModelsToFollowTimeOfDay = No
 ForceModelsToFollowWeather = No
 UnitDamagedThreshold = 0.5
 UnitReallyDamagedThreshold = 0.2
 DefaultStartingCash = 50
 TerrainResourceCellSize = 12
 PartitionCellSize = 16
 PowerLimit = 80
 ResourceBonusMultiplier = 1
 ResourceMultiplierLimit = 1
 MultiPlayMoneyMult = MP1:1
 UnlookPersistDuration = 500
 ShroudColor = R:0 G:0 B:0
 FogAlpha = 100
 ShroudAlpha = 180
 StealthFriendlyOpacity = 40%
 MovementPenaltyDamageState = DAMAGED
)";
	for (const std::string side : { "Good", "Evil" })
	{
		data += side + "CommandPointLimit = 80\n" + side + "CommandPoints = 8 80\n" + side + "CommandPointsAI = 8 80\n" + side + "CommandPointsBonus = 0\n";
		for (int i = 2; i <= 8; ++i) data += side + "CommandPointsMP" + std::to_string(i) + " = 8 80\n";
	}
	for (const std::string field : { "PathfindCellsPerFrame", "CellsAdjustDestination", "CellsAdjustHordeMeleeDestination", "CellsPatchPath", "CellsFindPathLimit",
	                                 "CellsFindAttackPath", "CellsFindMeleeEngagementLocation", "CellsAdjustToMeleeDestination", "CellsAdjustToPossibleDestination", "CellsToExamineTowardsGoal" })
		data += "Max" + field + " = 32\n";
	return data + "End\n";
}

std::string fontSettings()
{
	std::string data = "InGameUI\n SelectNearestBuilderCycleTimeOut = 200\n";
	for (const std::string role : { "Name", "Cost", "Shortcut", "Description" })
	{
		const std::string k = " HelpBox" + role;
		data += k + "Font = Toy\n" + k + "PointSize = 10\n" + k + "Bold = No\n" + k + "Color = R:240 G:240 B:240 A:255\n";
	}
	return data + "End\n";
}

struct Text : GameTextSource
{
	const std::map<std::string, std::u16string> table = {
		{ "Test:Start", u"Resume watch" }, { "Test:Stop", u"Cancel watch" },
		{ "Test:StartTip", u"Look for targets." }, { "Test:StopTip", u"Wait for orders." }
	};
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		const auto it = table.find(label);
		if (it == table.end()) return false;
		out = it->second;
		return true;
	}
};

struct Hud
{
	initest::Fixture files;
	RetailObjectWorld world{ files.fsys };
	ArchiveW3DFileSource w3d{ files.fsys };
	WW3DAssetManager assets{ w3d };
	MapObjectOptions mapOptions;
	LiveGame game{ world, files.fsys, assets, mapOptions };
	PinholeView view;
	MetaMap keys;
	apttest::MemorySource movies;
	RecordingShellServices services;
	ShellEnvironment environment;
	AptScreenFactoryTable factories;
	WindowManager windows{ movies, services };
	Shell shell{ windows, factories, services, environment };
	Text text;
	std::unique_ptr<InGameHud> hud;
	Object *siege = nullptr;
	int now = 0;

	explicit Hud(const std::string &titles = "Test:Start Test:Stop", const std::string &tips = "Test:StartTip Test:StopTip")
	{
		files.mount({ { "Maps\\test\\test.map", flatMap() }, { "Data\\INI\\GameData.ini", gameData() },
			{ "Data\\INI\\Default\\AIData.ini", "AIData\n EnableRepulsors = No\n RepulsedDistance = 2\n WadeWaterDepth = 1\n DeepWaterDepth = 3\n HordesWaitForHordes = No\n MeleeApproachTolerance = 1\n MeleeApproachDist = 1\n CastleSiegeStandBackDistance = 2\n AttackUsesLineOfSight = No\nEnd\n" },
			{ "Data\\INI\\Multiplayer.ini", "MultiplayerSettings\n InitialCreditsVeryLow = 50\n InitialCreditsLow = 50\n InitialCreditsMedium = 50\n InitialCreditsHigh = 50\n InitialCreditsVeryHigh = 50\n UseShroud = No\nEnd\n" },
			{ "Data\\INI\\Default\\SoundEffects.ini", "AudioEvent DefaultSoundEffect\n Volume = 60\n Type = ui everyone\n Type = +DEFAULT\nEnd\n" },
			{ "Data\\Scripts\\Scripts.lua", "-- no events\n" }, { "Data\\Scripts\\ScriptEvents.xml", "<SageLuaScriptSection>\n</SageLuaScriptSection>\n" },
			{ "Data\\INI\\InGameUI.ini", fontSettings() }, { "language.ini", "Language\nEnd\n" },
			{ "window\\controlbar.wnd", "WINDOW\n NAME = \"ControlBar.wnd:ButtonCommand\";\n TOOLTIPDELAY = 32;\nEND\n" },
			{ "Data\\INI\\Default\\SubsystemLegendExpansion1.ini", "LoadSubsystem TheThingFactory\n InitFile = Data\\INI\\Toy.ini\nEnd\n" },
			{ "Data\\INI\\Toy.ini", "Object ToySiege\n KindOf = SELECTABLE MACHINE\n CommandSet = ToySet\nEnd\n" },
			{ "Data\\INI\\CommandSet.ini", "CommandSet ToySet\n 2 = ToyOrder\nEnd\n" },
			{ "Data\\INI\\CommandButton.ini", "CommandButton ToyOrder\n Command = TOGGLE_NO_AUTO_ACQUIRE\n Options = TOGGLE_IMAGE_ON_WEAPON\n"
				" InPalantir = Yes\n Radial = No\n ButtonImage = ToyStart ToyStop\n TextLabel = " + titles + "\n DescriptLabel = " + tips + "\nEnd\n" } });
		std::string error;
		REQUIRE_MESSAGE(world.load(&error), error);
		LiveGame::Options options;
		options.mapName = "test";
		options.mapScripts = 0;
		REQUIRE_MESSAGE(game.load(options, &error), error);

		apttest::TestMovie empty;
		empty.setSize(1024, 768, 1); // even the shortest test update drains the posted APT transition
		empty.setRootFrames({ {} });
		movies.add("AptLevel0", empty);
		auto palantir = movie();
		movies.add("Palantir", palantir);
		factories.registerFactory("Palantir.apt", [](AptScreenContext &c) { return std::make_unique<AptPalantir>(c.windows, c.shell); });
		windows.init();
		shell.push("Palantir.apt", true);
		windows.setMinimumFrameTime(false);
		windows.update(0);
		hud = std::make_unique<InGameHud>(InGameHud::Config{ game, world, files.fsys, services, view, MouseSettings{}, keys, &text, &windows, &shell });
		REQUIRE_MESSAGE(hud->boot(&error), error);
		hud->setWindowSize(1024, 768);
		REQUIRE(hud->helpBoxSettings().loaded);
		REQUIRE(windows.invokeCallback("AptPalantir::OnHelpBoxLoaded", "_level1.helpBox"));
		Player *local = game.players().getLocalPlayer();
		REQUIRE(local);
		siege = game.createObject("ToySiege", local->getPlayerIndex(), { 25, 25, 0 }, 0, &error);
		REQUIRE_MESSAGE(siege, error);
		local->selection() = { siege->getID() };
		hud->input().ui().selectObject(siege->getID());
		frame(); // demand the empty clip's width, then sample it in helpBoxRender
		REQUIRE(windows.invokeCallback(content + "_OnInitialized", ""));
		frame(); // width accepted; arc ready for hit-testing
	}

	// One client update and one native draw, as in the shell-hosted HUD. Nothing here re-offers a provider or looks up a button by name.
	std::vector<std::string> frame(int ms = 16)
	{
		now += ms;
		windows.update(ms);
		hud->update(ms / 1000.0);
		std::vector<HelpDrawOp> ops;
		hud->helpBoxRender(20, 20, 240, 80, ops);
		std::vector<std::string> lines;
		for (const auto &op : ops) if (op.kind == HelpDrawOp::TEXT) lines.push_back(op.text);
		return lines;
	}
	void hover()
	{
		hud->mouseMove(x, y, 0);
		REQUIRE(frame().empty());
		REQUIRE(windows.apt().input().currentButton());
		REQUIRE(windows.apt().input().currentButton()->targetPath() == content + ".hit");
		REQUIRE(frame(32).empty()); // RW 0x807848, equality at 0x8078ED still waits
	}
	void mouse(bool down) { hud->mouseButton(HudInput::Button::Left, down, x, y, 0, now); }
	std::string image() const
	{
		const auto *im = hud->palantir()->imageFor(content + "_Image");
		REQUIRE(im);
		return im->image;
	}
	unsigned noAuto() const
	{
		const int bit = ObjectTemplateInfoBuilder::objectStatusIndex("NO_AUTO_ACQUIRE");
		REQUIRE(bit == 0x17); // RW 0x75D22E: the auto-acquire arm pushes this exact status at 0x75D2CF
		return static_cast<unsigned>(bit);
	}
};

void checkLines(const std::vector<std::string> &lines, const std::string &title, const std::string &description)
{
	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == title);
	CHECK(lines[1] == description);
}
} // namespace

TEST_CASE("PORT-272 the first HUD draw after a Palantir press changes both lines, without a second hover")
{
	Hud f;
	f.hover();
	checkLines(f.frame(1), "Cancel watch", "Wait for orders."); // RW 0x75D22E: status clear, invert 0 -> index 1
	REQUIRE(f.image() == "ToyStop");
	AptButtonInst *target = f.windows.apt().input().currentButton();
	for (bool holdingFire : { true, false })
	{
		// Real APT transition -> _OnPress -> dropProvider + pressButton. No mouseMove from here on.
		f.mouse(true);
		const auto firstDraw = f.frame(1); // less than TOOLTIPDELAY; no intervening logic tick
		CHECK(f.windows.apt().input().currentButton() == target);
		CHECK(f.image() == (holdingFire ? "ToyStart" : "ToyStop"));
		CHECK(f.siege->testStatus(f.noAuto()) != holdingFire); // this draw is the click prediction, not the later logic confirmation
		const char *title = holdingFire ? "Resume watch" : "Cancel watch";
		const char *desc = holdingFire ? "Look for targets." : "Wait for orders.";
		checkLines(firstDraw, title, desc);
		const auto &orders = f.game.commands().messages();
		CHECK(std::count_if(orders.begin(), orders.end(), [](const GameMessage &m) { return m.getType() == MSG_TOGGLE_NO_AUTO_ACQUIRE; }) == 1);
		REQUIRE(f.game.stepFrame() == 1);
		CHECK(f.siege->testStatus(f.noAuto()) == holdingFire);
		f.mouse(false);
		checkLines(f.frame(40), title, desc);
	}
	CHECK(f.hud->helpBox().helpsShown() == 3); // one initial composition and one per change, not one per update
}

TEST_CASE("PORT-272 the already shown provider follows a change to either effective label alone")
{
	bool sameTitle = false;
	SUBCASE("only the description changes") { sameTitle = true; }
	SUBCASE("only the title changes") { sameTitle = false; }
	Hud f(sameTitle ? "Test:Stop Test:Stop" : "Test:Start Test:Stop", sameTitle ? "Test:StartTip Test:StopTip" : "Test:StopTip Test:StopTip");
	f.hover();
	checkLines(f.frame(1), "Cancel watch", "Wait for orders.");
	// No press, dropProvider, or new APT event. The authoritative object changes while the hover is consumed (RW 0x8078CA).
	f.siege->setStatus(f.noAuto(), true);
	for (int i = 0; i < 4; ++i)
		checkLines(f.frame(), sameTitle ? "Cancel watch" : "Resume watch", sameTitle ? "Look for targets." : "Wait for orders.");
	CHECK(f.image() == "ToyStart");
	CHECK(f.hud->helpBox().helpsShown() == 2);
}

TEST_CASE("PORT-272 leaving then returning is a new hover even while the old box is hiding")
{
	Hud f;
	f.hover();
	checkLines(f.frame(1), "Cancel watch", "Wait for orders.");
	f.hud->mouseMove(x + 50, y, 0);
	f.frame(); // nobody marked this frame
	REQUIRE(f.windows.apt().input().currentButton() == nullptr);
	f.siege->setStatus(f.noAuto(), true);
	f.hud->mouseMove(x, y, 0);
	checkLines(f.frame(1), "Cancel watch", "Wait for orders."); // RW 0x92E2AC still draws the outgoing box in state 4
	CHECK(f.image() == "ToyStart");
	checkLines(f.frame(32), "Cancel watch", "Wait for orders.");
	checkLines(f.frame(1), "Resume watch", "Look for targets.");
	CHECK(f.hud->helpBox().helpsShown() == 2);
}

TEST_CASE("PORT-272 a changed visible command cannot bypass the disconnect guard")
{
	Hud f;
	f.hover();
	checkLines(f.frame(1), "Cancel watch", "Wait for orders.");
	{
		AptDisconnectScreen disconnect(f.windows, f.shell);
		f.siege->setStatus(f.noAuto(), true);
		checkLines(f.frame(), "Cancel watch", "Wait for orders.");
		CHECK(f.image() == "ToyStart");
	}
	// RW 0x807848 consumes the request at 0x807971 before testing TheDisconnectMenu at 0x807982.
	checkLines(f.frame(40), "Cancel watch", "Wait for orders.");
	CHECK(f.hud->helpBox().helpsShown() == 1);
}

// OpenBFME. GPL-3.0. PORT-239: asset-free player-command regression coverage.
#include "doctest.h"

#include "GameClient/GUI/AptScreens/InGameCommandButtonSubMenu.h"

#include <string>
#include <vector>

namespace
{
using Menu = InGameCommandButtonSubMenu;

void makeContents(Menu &menu)
{
	for (int i = 0; i < menu.itemCount(); ++i)
	{
		menu.onButtonFrameLoaded("index=" + std::to_string(i) + "&name=_level5.Frame" + std::to_string(i));
	}
	menu.update([](int) { return true; }, nullptr);
}
} // namespace

TEST_CASE("PORT-239 stance submenu: tactical owner notifications do not close another menu")
{
	// Verdict/neighbour coverage, not a failing-before-fix claim. RW 0x93023D ->
	// RW 0x9D353B installs vtable 0xC8C46C: owner slots +4/+8 are RW 0x63F3BF ret.
	// RW 0x9D32F9 sets open, then calls +4; it does not implement exclusivity.
	// The strategic HUD's derived listener must not be imported into Palantir.
	std::vector<std::string> calls;
	auto call = [&](const std::string &path, const std::string &fn, const std::vector<std::string> &) {
		calls.push_back(path + "." + fn);
		return true;
	};
	Menu first(5, "subMenu0", call), second(5, "subMenu1", call);
	REQUIRE(first.addItem(1));
	REQUIRE(second.addItem(2));
	REQUIRE(first.open());
	REQUIRE(second.open());
	CHECK(first.isOpen());
	CHECK(second.isOpen());
	CHECK_FALSE(first.closeRequested());
	CHECK_FALSE(second.closeRequested());
	const std::vector<std::string> shown{ "subMenu0.Show", "subMenu1.Show" };
	CHECK(calls == shown);

	// RW 0x9D2F21 invokes the empty owner +8 after clearing this menu only.
	second.hide();
	CHECK(first.isOpen());
	CHECK_FALSE(second.isOpen());
	CHECK(first.itemCount() == 1);
	CHECK(second.itemCount() == 0);
	const std::vector<std::string> hidden{ "subMenu0.Show", "subMenu1.Show", "subMenu1.Hide" };
	CHECK(calls == hidden);
}

TEST_CASE("PORT-239 stance submenu: fade completion releases contents once in reverse order")
{
	// RW 0x9D3235 ignores premature OnClosed, clears fading before releasing
	// records from count-1 to zero, clears open, then calls the empty owner +8.
	// RW 0x9D2F92 consumes closeRequested and does not restart an active fade.
	std::vector<std::string> calls;
	std::vector<int> dropped;
	Menu menu(5, "subMenu0", [&](const std::string &, const std::string &fn, const std::vector<std::string> &) {
		calls.push_back(fn);
		return true;
	});
	for (int value : { 3, 1, 2 })
	{
		REQUIRE(menu.addItem(value));
	}
	menu.setDropItemContent([&](const Menu::Item &item) {
		CHECK_FALSE(menu.fading());
		CHECK(menu.isOpen()); // RW 0x9D3277 clears open after releasing records.
		dropped.push_back(item.value);
	});
	REQUIRE(menu.open());
	makeContents(menu);
	menu.onClosed();
	CHECK(menu.isOpen());
	CHECK(dropped.empty());
	menu.requestClose();
	menu.update(nullptr, nullptr);
	CHECK(menu.fading());
	CHECK_FALSE(menu.closeRequested());
	menu.requestClose();
	menu.update(nullptr, nullptr);
	const std::vector<std::string> faded{ "Show", "FadeOut" };
	CHECK(calls == faded);
	menu.onClosed();
	CHECK_FALSE(menu.isOpen());
	CHECK_FALSE(menu.fading());
	CHECK(menu.itemCount() == 0);
	const std::vector<int> reverse{ 2, 1, 3 };
	CHECK(dropped == reverse);
	menu.onClosed();
	menu.hide();
	CHECK(dropped == reverse);
	CHECK(calls == faded);
}

TEST_CASE("PORT-239 stance submenu: failed Show still opens and Hide cancels fading")
{
	// RW 0x9D32F9 sets open unconditionally at 0x9D332A after the movie call.
	// RW 0x9D2F21 Hide drops records, then clears fading/open; the later movie
	// OnClosed has no effects (RW 0x9D3235's initial fading guard).
	std::vector<std::string> calls;
	int drops = 0;
	Menu menu(5, "subMenu0", [&](const std::string &, const std::string &fn, const std::vector<std::string> &) {
		calls.push_back(fn);
		return false;
	});
	REQUIRE(menu.addItem(1));
	menu.setDropItemContent([&](const Menu::Item &) { ++drops; });
	CHECK_FALSE(menu.open());
	CHECK(menu.isOpen());
	makeContents(menu);
	menu.requestClose();
	menu.update(nullptr, nullptr);
	CHECK(menu.fading());
	menu.hide();
	CHECK_FALSE(menu.isOpen());
	CHECK_FALSE(menu.fading());
	CHECK(menu.itemCount() == 0);
	CHECK(drops == 1);
	menu.onClosed();
	menu.hide();
	CHECK(drops == 1);
	const std::vector<std::string> expected{ "Show", "FadeOut", "Hide" };
	CHECK(calls == expected);
}

// These tests exercise the real INI parser and translator; no game files or
// replay/planning UI are needed to put the already-existing Player state in mode 2.
#include "SyntheticHudTestUtil.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "GameClient/MessageStream/MetaEvent.h"

namespace
{
struct MetaProbe
{
	MetaMap map;
	InGameUI ui;
	MessageStream stream;
	HudContext context;
	MetaEventTranslator translator;

	MetaProbe(synthetichud::Rig &world, const std::string &ini)
		: context{ *world.logic, nullptr, world.view, ui, stream, nullptr, { 15, 5, 150 } }, translator(context, map)
	{
		INIEnvironment env;
		map.registerBlocks(env);
		INI parser(env);
		parser.loadMemory("port239-commandmap.ini", std::vector<std::uint8_t>(ini.begin(), ini.end()), INI_LOAD_OVERWRITE);
	}

	std::vector<ClientMessage> key(int code, int flags, MessageDisposition *disposition = nullptr)
	{
		ClientMessage raw((flags & KEY_STATE_UP) ? CMSG_RAW_KEY_UP : CMSG_RAW_KEY_DOWN);
		raw.appendInteger(code);
		raw.appendInteger(flags);
		const auto result = translator.translate(raw);
		if (disposition) *disposition = result;
		return stream.propagate();
	}
};

std::string binding(int mask)
{
	std::string text = "CommandMap STOP\n Key = KEY_F11\n Transition = DOWN\n Modifiers = NONE\n";
	if (mask)
	{
		text += " UseableIn =";
		if (mask & 1) text += " SHELL";
		if (mask & 2) text += " GAME";
		if (mask & 4) text += " PLANNING";
		text += "\n";
	}
	return text + "End\n";
}
} // namespace

TEST_CASE("PORT-239 CommandMap: planning-only key emits exactly one meta in order mode two")
{
	// RW 0x5DA83D: 0x5DA8E5 compares Player +0x770 to 2, 0x5DA8ED selects
	// mask 6, 0x5DA910 admits PLANNING before 0x5DA982 emits the matched meta.
	// This specific assertion fails before the shared usable-mask fix.
	synthetichud::Rig world;
	Player *local = world.players.getLocalPlayer();
	REQUIRE(local);
	MetaProbe probe(world, binding(4));
	probe.translator.setClientFrame(1);
	local->setOrderMode(2);
	MessageDisposition disposition;
	auto messages = probe.key(KEY_F11, KEY_STATE_DOWN, &disposition);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].type() == CMSG_META_STOP);
	CHECK(disposition == MessageDisposition::Destroy);
	CHECK(probe.translator.metasMade() == 1);
	// A matched autorepeat is consumed without emitting another command.
	CHECK(probe.key(KEY_F11, KEY_STATE_DOWN | KEY_STATE_AUTOREPEAT, &disposition).empty());
	CHECK(disposition == MessageDisposition::Destroy);
	CHECK(probe.translator.metasMade() == 1);
	// Read mode live: Alt/queued (1) is NOT planning; returning to mode 2 works.
	local->setOrderMode(1);
	CHECK(probe.key(KEY_F11, KEY_STATE_DOWN, &disposition).empty());
	CHECK(disposition == MessageDisposition::Keep);
	local->setOrderMode(2);
	messages = probe.key(KEY_F11, KEY_STATE_DOWN);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].type() == CMSG_META_STOP);
}

TEST_CASE("PORT-239 CommandMap: usable masks preserve shell neutral-player and frame-zero rules")
{
	// RW 0x5DA83D, 0x5DA8C3..0x5DA913: shell exclusively uses 1; game
	// uses 2 or 6. At frame zero ONLY the exact GAME-only record is suppressed
	// (0x5DA8FA compares for equality, not a bit test). Covers all 8 INI masks.
	synthetichud::Rig world;
	Player *local = world.players.getLocalPlayer();
	REQUIRE(local);
	for (int mask = 0; mask != 8; ++mask)
	for (int mode : { 0, 1, 2 })
	for (bool shell : { false, true })
	for (bool useNeutral : { false, true })
	for (unsigned frame : { 0u, 1u })
	{
		CAPTURE(mask); CAPTURE(mode); CAPTURE(shell); CAPTURE(useNeutral); CAPTURE(frame);
		local->setOrderMode(mode);
		// The public null setter selects Neutral, not a null local player.
		world.players.setLocalPlayer(useNeutral ? nullptr : local);
		REQUIRE(world.players.getLocalPlayer() == (useNeutral ? world.players.getNeutralPlayer() : local));
		MetaProbe probe(world, binding(mask));
		probe.translator.setShellActive(shell);
		probe.translator.setClientFrame(frame);
		bool expected = shell ? (mask & 1) != 0 :
			((mask & 2) != 0 || (!useNeutral && mode == 2 && (mask & 4) != 0));
		if (mask == 2 && frame == 0) expected = false;
		MessageDisposition disposition;
		const auto messages = probe.key(KEY_F11, KEY_STATE_DOWN, &disposition);
		CHECK(messages.size() == (expected ? 1u : 0u));
		CHECK(disposition == (expected ? MessageDisposition::Destroy : MessageDisposition::Keep));
		if (!messages.empty()) CHECK(messages[0].type() == CMSG_META_STOP);
	}
	world.players.setLocalPlayer(local);
}

TEST_CASE("PORT-239 CommandMap: planning modifier-only transitions share the same usable filter")
{
	// RW 0x5DA83D, 0x5DA910 filters before KEY_NONE matching at 0x5DA915;
	// 0x5DA96C emits once on each matching modifier transition.
	synthetichud::Rig world;
	Player *local = world.players.getLocalPlayer();
	REQUIRE(local);
	MetaProbe probe(world,
		"CommandMap STOP\n Key = KEY_NONE\n Transition = DOWN\n Modifiers = CTRL\n UseableIn = PLANNING\nEnd\n"
		"CommandMap SCATTER\n Key = KEY_NONE\n Transition = UP\n Modifiers = CTRL\n UseableIn = PLANNING\nEnd\n");
	probe.translator.setClientFrame(1);
	local->setOrderMode(2);
	auto messages = probe.key(KEY_LCTRL, KEY_STATE_DOWN | KEY_STATE_LCONTROL);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].type() == CMSG_META_STOP);
	CHECK(probe.key(KEY_LCTRL, KEY_STATE_DOWN | KEY_STATE_LCONTROL | KEY_STATE_AUTOREPEAT).empty());
	messages = probe.key(KEY_LCTRL, KEY_STATE_UP);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].type() == CMSG_META_SCATTER);
	CHECK(probe.translator.metasMade() == 2);
	// Opening the shell must suppress planning bindings even in mode 2.
	probe.translator.setShellActive(true);
	CHECK(probe.key(KEY_LCTRL, KEY_STATE_DOWN | KEY_STATE_LCONTROL).empty());
	CHECK(probe.key(KEY_LCTRL, KEY_STATE_UP).empty());
	CHECK(probe.translator.metasMade() == 2);
}

#include "IniTestUtil.h"
#include "GameClient/MessageStream/CommandMapList.h"

namespace
{
const char *const kPort239Legend = "Data\\INI\\Default\\SubsystemLegendExpansion1.ini";

const CommandMapEntry *findKey(const std::vector<CommandMapEntry> &rows, const std::string &name)
{
	for (const auto &row : rows) if (row.name == name) return &row;
	return nullptr;
}
} // namespace

TEST_CASE("PORT-239 CommandMap loading: all legend files then paths then language override")
{
	// GameEngine::init registers TheMetaMap at 0x63C1F3, then separately loads
	// CommandMap.ini at 0x63C506. RW 0x5B4B9A processes InitFiles before InitPaths.
	// Call the existing production list reader so this test also compiles with
	// the old loader, which misses SyntheticExtra.ini and SyntheticKeys entirely.
	initest::Fixture files({
		{ kPort239Legend, "LoadSubsystem TheMetaMap\n Loader = INI\n InitFile = Data\\INI\\CommandMap.ini\n InitFile = SyntheticExtra.ini\n InitPath = SyntheticKeys\nEnd\n" },
		{ "Data\\INI\\CommandMap.ini", "CommandMap STOP\n Key = KEY_A\n Transition = DOWN\n UseableIn = GAME\nEnd\n" },
		{ "SyntheticExtra.ini", "CommandMap STOP\n Key = KEY_B\n Transition = UP\nEnd\nCommandMap SCATTER\n Key = KEY_F9\n UseableIn = GAME\nEnd\n" },
		{ "SyntheticKeys\\One.ini", "CommandMap STOP\n Key = KEY_C\nEnd\nCommandMap SCATTER\n Key = KEY_F10\nEnd\n" },
		{ "CommandMap.ini", "CommandMap STOP\n Key = KEY_Z\nEnd\n" }
	});
	std::vector<CommandMapEntry> rows;
	std::string error;
	REQUIRE_MESSAGE(ListCommandMap(files.fsys, rows, &error), error);
	REQUIRE(rows.size() == 2);
	const auto *stop = findKey(rows, "STOP");
	const auto *scatter = findKey(rows, "SCATTER");
	REQUIRE(stop);
	REQUIRE(scatter);
	CHECK(stop->key == "KEY_Z"); // root override last
	CHECK(stop->transition == TRANSITION_UP); // the second InitFile was read
	CHECK(stop->game); // unspecified fields survive each overwrite
	CHECK(scatter->key == "KEY_F10"); // InitPath overrides the earlier InitFile
	CHECK(scatter->game);
}

TEST_CASE("PORT-239 CommandMap loading: no legend entry does not invent a default path")
{
	// RW 0x5B4B9A returns false for no entry; TheMetaMap's registration passed
	// no fallback paths (call sites 0x63C1D5..0x63C1D8). The direct root load
	// still happens. A broken decoy base file must never be opened.
	initest::Fixture files({
		{ kPort239Legend, "LoadSubsystem UnrelatedSyntheticSubsystem\n Loader = INI\nEnd\n" },
		{ "Data\\INI\\CommandMap.ini", "NotARealBlock Broken\nEnd\n" },
		{ "CommandMap.ini", "CommandMap STOP\n Key = KEY_F12\n UseableIn = GAME\nEnd\n" }
	});
	std::vector<CommandMapEntry> rows;
	std::string error;
	REQUIRE_MESSAGE(ListCommandMap(files.fsys, rows, &error), error);
	REQUIRE(rows.size() == 1);
	CHECK(rows[0].name == "STOP");
	CHECK(rows[0].key == "KEY_F12");
}

TEST_CASE("PORT-239 CommandMap loading: missing or malformed manifest inputs fail loudly")
{
	// Same loader chain as above, with the port's existing strict INI error
	// contract: a broken declared file must not fall back to the old hardcoded
	// base and language pair and pretend the installation loaded successfully.
	initest::FileList tree{
		{ "Data\\INI\\CommandMap.ini", "CommandMap STOP\n Key = KEY_S\nEnd\n" },
		{ "CommandMap.ini", "CommandMap STOP\n Key = KEY_S\nEnd\n" }
	};
	SUBCASE("missing manifest") {}
	SUBCASE("malformed manifest") { tree.push_back({ kPort239Legend, "NotARealBlock Broken\nEnd\n" }); }
	SUBCASE("missing declared input")
	{
		tree.push_back({ kPort239Legend, "LoadSubsystem TheMetaMap\n InitFile = MissingKeys.ini\nEnd\n" });
	}
	SUBCASE("malformed second declared input")
	{
		tree.push_back({ kPort239Legend, "LoadSubsystem TheMetaMap\n InitFile = Data\\INI\\CommandMap.ini\n InitFile = BrokenKeys.ini\nEnd\n" });
		tree.push_back({ "BrokenKeys.ini", "CommandMap STOP\n Key = KEY_NOT_REAL\nEnd\n" });
	}
	initest::Fixture files(tree);
	std::vector<CommandMapEntry> rows;
	std::string error;
	CHECK_FALSE(ListCommandMap(files.fsys, rows, &error));
	CHECK_FALSE(error.empty());
}

TEST_CASE("PORT-239 CommandMap loading: other-edition paths remain unchanged")
{
	// Neighbour coverage, not a RotWK binary claim: preserve the existing
	// BFME1 base-only and BFME2 base-plus-root contracts when no expansion
	// legend is mounted. The decoy default legend must not be parsed here.
	bool bfme2 = false;
	SUBCASE("BFME1 base only") {}
	SUBCASE("BFME2 root override") { bfme2 = true; }
	initest::FileList tree{
		{ "Data\\INI\\Default\\SubsystemLegend.ini", "NotARealBlock Decoy\nEnd\n" },
		{ "Data\\INI\\CommandMap.ini", "CommandMap STOP\n Key = KEY_A\n UseableIn = GAME\nEnd\n" },
		{ "CommandMap.ini", bfme2 ? "CommandMap STOP\n Key = KEY_Z\nEnd\n" : "NotARealBlock Decoy\nEnd\n" }
	};
	if (bfme2) tree.push_back({ "Data\\INI\\AwardSystem.ini", "" });
	initest::Fixture files(tree);
	std::vector<CommandMapEntry> rows;
	std::string error;
	REQUIRE_MESSAGE(ListCommandMap(files.fsys, rows, &error), error);
	REQUIRE(rows.size() == 1);
	CHECK(rows[0].key == (bfme2 ? "KEY_Z" : "KEY_A"));
	CHECK(rows[0].game);
}

TEST_CASE("PORT-239 CommandMap loading: shared macros survive the temporary legend registry")
{
	// The production INI environment's macro lifetime must span the temporary
	// manifest registry and both command-file loads, without retaining a
	// callback referring to the destroyed temporary SubsystemLegend.
	initest::Fixture files({
		{ "Before.ini", "#define PORT239_BEFORE 11\n" },
		{ kPort239Legend, "#define PORT239_MANIFEST 22\nLoadSubsystem TheMetaMap\n InitFile = SyntheticKeys.ini\nEnd\n" },
		{ "SyntheticKeys.ini", "#define PORT239_KEYS 33\nCommandMap STOP\n Key = KEY_S\nEnd\n" },
		{ "CommandMap.ini", "#define PORT239_ROOT 44\n" }
	});
	INI before(files.env);
	before.load("Before.ini", INI_LOAD_OVERWRITE);
	MetaMap map;
	map.registerBlocks(files.env);
	std::string error;
	REQUIRE_MESSAGE(map.loadInstalled(files.env, &error), error);
	CHECK(files.env.macros.find("PORT239_BEFORE") != nullptr);
	CHECK(files.env.macros.find("PORT239_MANIFEST") != nullptr);
	CHECK(files.env.macros.find("PORT239_KEYS") != nullptr);
	CHECK(files.env.macros.find("PORT239_ROOT") != nullptr);
	CHECK(files.env.blocks.find("LoadSubsystem") == nullptr);
}

#include "AptPlayerTestUtil.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"

namespace
{
// Only supplies presentation input. Does not pretend to model CommandBar
// availability or retail APT scripts; the production callback is under test.
struct Port239Palantir
{
	apttest::MemorySource movies;
	RecordingShellServices services;
	ShellEnvironment environment;
	AptScreenFactoryTable factories;
	std::unique_ptr<WindowManager> windows;
	std::unique_ptr<Shell> shell;

	Port239Palantir()
	{
		using namespace apttest;
		TestMovie level0;
		level0.setRootFrames({ {} });
		movies.add("AptLevel0", level0);
		TestMovie movie;
		movie.addCharacter(0);
		const auto contentAction = program(movie, [](Asm &a) {
			a.defineFunction("SetState", { "state" }, [](Asm &) {});
			a.defineFunction("SetAutoAbilityOverlayState", { "state" }, [](Asm &) {});
		});
		const auto content = movie.addCharacter(movie.addSprite({ { movie.addActionItem(contentAction) } }));
		movie.addExport("SyntheticButton", content);
		const auto frameAction = program(movie, [](Asm &a) {
			a.defineFunction("CreateContent", { "type", "name" }, [](Asm &f) {
				f.pushByte(4).pushString("content").pushString("SyntheticButton").pushByte(3).getStringVar("this")
					.pushString("attachMovie").op(APT_OP_EA_CALLMETHODPOP);
				f.getStringVar("extern").pushString("_level1.Frame_ContentName").setStringMember("SyntheticButton");
			});
			a.defineFunction("DeleteContent", {}, [](Asm &) {});
		});
		const auto frame = movie.addCharacter(movie.addSprite({ { movie.addActionItem(frameAction) } }));
		const auto rootAction = program(movie, [](Asm &a) {
			for (const char *name : { "SetPalantirFrameState", "SetPlayerButtonsState", "EnablePlayerMagicButton", "SetPlayerFaction",
				"ShowCommandInterface", "HideCommandInterface", "HideRankInterface" })
				a.defineFunction(name, {}, [](Asm &) {});
			a.getURL("FSCommand:AptPalantir::OnInitialized", "");
			a.getURL("FSCommand:PalantirCommandUI::OnButtonFrameLoaded", "index=0&name=Frame");
		});
		const auto menuAction = program(movie, [](Asm &a) {
			a.defineFunction("Show", { "count" }, [](Asm &) {});
			a.defineFunction("Hide", {}, [](Asm &) {});
			a.defineFunction("FadeOut", {}, [](Asm &) {});
		});
		const auto menu = movie.addCharacter(movie.addSprite({ { movie.addActionItem(menuAction) } }));
		movie.setRootFrames({ { movie.addPlaceItem(placeChar(frame, 1, "Frame")), movie.addPlaceItem(placeChar(menu, 2, "SubMenu")),
			movie.addActionItem(rootAction) } });
		movies.add("Palantir", movie);
		factories.registerFactory("Palantir.apt", [](AptScreenContext &c) { return std::make_unique<AptPalantir>(c.windows, c.shell); });
		windows = std::make_unique<WindowManager>(movies, services);
		shell = std::make_unique<Shell>(*windows, factories, services, environment);
		windows->init();
		shell->push("Palantir.apt", true);
		for (int i = 0; i < 3; ++i) { windows->update(33); shell->update(); }
	}
	AptPalantir *palantir() { return dynamic_cast<AptPalantir *>(shell->top()); }
};
} // namespace

TEST_CASE("PORT-239 ordinary Palantir left press: disabled clip states sound once and do not dispatch")
{
	// Existing-main verdict, not a failing-before-fix claim. RW entry 0x9D285E
	// compares states 1/3/5 and calls the disabled cue once, otherwise jumps to
	// RW 0x9D2814's notification. Active/_up and nonpressable/_static are not
	// rejected by this native gate; the movie decides whether to send a press.
	synthetichud::Rig world;
	InGameUI ui;
	MessageStream stream;
	HudContext context{ *world.logic, nullptr, world.view, ui, stream, nullptr, {} };
	ControlBar bar(context);
	Port239Palantir movie;
	AptPalantir *palantir = movie.palantir();
	REQUIRE(palantir);
	REQUIRE(palantir->initialized());
	REQUIRE(palantir->level() == 1);
	CommandButton command;
	command.m_name = "SyntheticOrdinaryPress";
	ControlBarButton shown;
	shown.slot = 7;
	shown.position = 0;
	shown.button = &command;
	shown.image = "SyntheticImage";
	// Non-const bar owns this vector. Like test_hudpassive1's movie fixture,
	// inject presentation input without adding a production test-only setter.
	auto &buttons = const_cast<std::vector<ControlBarButton> &>(bar.palantirButtons());
	int dispatches = 0;
	palantir->setButtonHandler([&](int slot, bool arc) { CHECK(slot == 7); CHECK(arc); ++dispatches; });
	movie.windows->setPressButton(0);
	for (ButtonState state : { ButtonState::Restricted, ButtonState::CantAfford, ButtonState::Purchased, ButtonState::NotReady,
		ButtonState::Enabled, ButtonState::Active })
	{
		CAPTURE(static_cast<int>(state));
		shown.state = state;
		buttons = { shown };
		palantir->sync(bar);
		movie.services.sounds.clear();
		dispatches = 0;
		REQUIRE(movie.windows->invokeCallback("_level1.Frame.content_OnPress", ""));
		const bool disabled = state == ButtonState::Restricted || state == ButtonState::CantAfford ||
			state == ButtonState::Purchased || state == ButtonState::NotReady;
		CHECK(dispatches == (disabled ? 0 : 1));
		if (disabled)
		{
			REQUIRE(movie.services.sounds.size() == 1);
			CHECK(movie.services.sounds[0] == "Gui_PalantirCommandButtonDisabledClick");
		}
		else CHECK(movie.services.sounds.empty());
	}
	command.m_options = COMMAND_OPTION_NONPRESSABLE;
	shown.state = ButtonState::Enabled;
	buttons = { shown };
	palantir->sync(bar);
	movie.services.sounds.clear();
	dispatches = 0;
	REQUIRE(movie.windows->invokeCallback("_level1.Frame.content_OnPress", ""));
	CHECK(dispatches == 1); // clip state 4 is not 1/3/5
	CHECK(movie.services.sounds.empty());
	// A released/unloaded frame cannot keep dispatching its previous slot.
	REQUIRE(movie.windows->invokeCallback("PalantirCommandUI::OnButtonFrameUnloaded", "index=0"));
	dispatches = 0;
	movie.windows->invokeCallback("_level1.Frame.content_OnPress", "");
	CHECK(dispatches == 0);
	CHECK(movie.services.sounds.empty());
}


TEST_CASE("PORT-239 stance toggle: live context relationship not disabled clip state decides the press")
{
	// RW entry 0x96FAC6: 0x96FAF9 bypasses absent object; otherwise 0x6A8803
	// requires local and either inactive (0x6AAC52) or team relationship ALLIES
	// (0x6ADBEB). This controller never calls the ordinary disabled-clip gate.
	synthetichud::Rig world;
	Object *object = world.place("TestFighter", 100, 100, "Bob");
	REQUIRE(object);
	Player *local = world.players.getLocalPlayer();
	REQUIRE(local);
	InGameUI ui;
	MessageStream stream;
	HudContext context{ *world.logic, nullptr, world.view, ui, stream, nullptr, {} };
	ControlBar bar(context);
	Port239Palantir movie;
	AptPalantir *palantir = movie.palantir();
	REQUIRE(palantir);
	REQUIRE(palantir->initialized());
	REQUIRE(movie.windows->invokeCallback("PalantirCommandUI::OnSubMenuLoaded", "index=0&name=SubMenu"));
	REQUIRE(movie.windows->invokeCallback("PalantirCommandUI::OnToggleFlashLoaded", "index=0&name=Flash"));
	CommandButton toggle;
	toggle.m_name = "SyntheticToggleStance";
	toggle.m_command = GUI_COMMAND_TOGGLE_STANCE;
	toggle.m_stances = { 3, 1, 2, 1 };
	ControlBarButton shown;
	shown.slot = 7;
	shown.position = 0;
	shown.button = &toggle;
	shown.image = "SyntheticStance";
	auto &buttons = const_cast<std::vector<ControlBarButton> &>(bar.palantirButtons());
	int requests = 0, ordinaryPresses = 0;
	palantir->setButtonHandler([&](int, bool) { ++ordinaryPresses; });
	// Same entry and live-context callback as InGameHud, without a worker thread.
	palantir->setToggleStanceHandler([&](int position) {
		++requests;
		palantir->pressToggleStance(position, context);
	});
	movie.windows->setPressButton(0);
	for (Relationship relationship : { ENEMIES, NEUTRAL, ALLIES })
	{
		for (bool defeated : { false, true })
		{
			for (bool missingObject : { false, true })
			{
				CAPTURE(static_cast<int>(relationship));
				CAPTURE(defeated);
				CAPTURE(missingObject);
				local->setTeamRelationship(object->getTeam(), relationship);
				local->setDefeated(defeated);
				AptPalantir::LocalState state;
				state.context = true;
				state.contextObject = missingObject ? INVALID_ID : object->getID();
				palantir->setLocalState(state);
				for (ButtonState clipState : { ButtonState::Enabled, ButtonState::Restricted, ButtonState::NotReady })
				{
					CAPTURE(static_cast<int>(clipState));
					shown.state = clipState;
					buttons = { shown };
					palantir->sync(bar);
					REQUIRE(palantir->isToggleStanceContent(0));
					const auto *menu = palantir->subMenu(0);
					REQUIRE(menu);
					palantir->hideStanceMenu(0);
					movie.services.sounds.clear();
					requests = 0;
					REQUIRE(movie.windows->invokeCallback("_level1.Frame.content_OnPress", ""));
					CHECK(requests == 1);
					CHECK(ordinaryPresses == 0);
					const bool allowed = missingObject || defeated || relationship == ALLIES;
					CHECK(menu->isOpen() == allowed);
					CHECK(menu->itemCount() == (allowed ? 3 : 0));
					CHECK(movie.services.sounds.empty());
					if (allowed)
					{
						// Permission is re-read on the close press, not latched at open.
						if (!missingObject)
						{
							local->setDefeated(false);
							local->setTeamRelationship(object->getTeam(), ENEMIES);
							movie.windows->invokeCallback("_level1.Frame.content_OnPress", "");
							CHECK_FALSE(menu->closeRequested());
							local->setDefeated(defeated);
							local->setTeamRelationship(object->getTeam(), relationship);
						}
						movie.windows->invokeCallback("_level1.Frame.content_OnPress", "");
						CHECK(menu->closeRequested());
						CHECK(menu->itemCount() == 3);
					}
					CHECK(stream.propagate().empty()); // opening/closing issues no stance order
					palantir->hideStanceMenu(0);
				}
			}
		}
	}
	CHECK(palantir->stanceErrors().empty());
}

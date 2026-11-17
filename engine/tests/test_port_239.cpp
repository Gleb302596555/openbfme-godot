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

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

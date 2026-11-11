// PORT-107 (issue #267): Object UpgradeCameo1..5 — RotWK 2.01's only reader, pinned; no slot display exists.
//
// RotWK addresses (game.dat, caveat S-001):
//   field names RW 0xC25B44 (UpgradeCameo1) .. RW 0xC25B04 (UpgradeCameo5); Object field table entries
//   RW 0xDA41F8 .. RW 0xDA4238; parser RW 0x42EE5E (AsciiString), template offset +0x7C .. +0x8C.
//   The only reachable reader: ProductionUpdate::completeUpgrade RW 0x8A22D5 .. 0x8A2328 — the selected
//   object's template's five names each go through TheUpgradeCenter->findUpgrade (RW 0x66F5E5); the
//   completed upgrade among them writes TheControlBar RW 0xDE7744 + 0x28 = 1 (RW 0x8A2328). The refresh
//   RW 0x71FD81 gates evaluateContextUI RW 0x71EBDA on +0x28 and clears it at RW 0x71EBED.
//   No display: ControlBar.wnd:UnitUpgrade%d windows (ControlBar +0x8C .. +0x9C) are made RW 0x71E566
//   (WIN_STATUS_USE_OVERLAY_STATES, no image) and their only reader, setPortraitByImage RW 0x71C56A ..
//   0x71C60D (it HIDES them in both branches), has no caller in 2.01 (no E8 / imm32 reference at all).
//   BFME2 1.06 is the same readable C++: ProductionUpdateQueue.cpp:1943 (the dirty check) and
//   ControlBar.cpp:1779 / 2157 / 2165 (create + hide) — no show/lit code anywhere.
//
// These tests pin: the five fields parse from a synthetic INI; the completed-upgrade decision marks the
// bar dirty exactly as RW 0x8A22D5 .. 0x8A2328; upgrades outside the list (and no selection) do not;
// the refresh consumes the mark (RW 0x71EBED). No engine behaviour beyond the ported reader changed.
// See docs/ennorath/port107-upgradecameo-rw.md; docs/STOPS.md S-7294. GPL-3.0.

#include "doctest.h"

#include "EconTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char objects[] = R"(
Object P107Archer
 KindOf = SELECTABLE INFANTRY
 UpgradeCameo1 = Upgrade_P107FireArrows
 UpgradeCameo2 = Upgrade_P107PoisonTips
End
Object P107Plain
 KindOf = SELECTABLE INFANTRY
End
)";

struct Fixture
{
	econtest::Fx game{objects};
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext context{*game.logic, nullptr, view, ui, stream, nullptr, {}, {}};
	ControlBar bar{context};

	Fixture()
	{
		game.players.setLocalPlayer(game.player("Alice"));
		ui.setLogic(game.logic.get());
		ui.setMaxSelectCount(50);
		REQUIRE(game.w.load(
			"Upgrade Upgrade_P107FireArrows\n Type = OBJECT\nEnd\n"
			"Upgrade Upgrade_P107PoisonTips\n Type = OBJECT\nEnd\n"
			"Upgrade Upgrade_P107Unrelated\n Type = OBJECT\nEnd\n")
				.empty());
	}
	Object *make(const char *name)
	{
		return game.makeAt(name, "Alice", 100.0f, 100.0f);
	}
	void select(Object *o)
	{
		ui.deselectAll(false);
		ui.selectObject(o->getID());
	}
	const UpgradeTemplate *upgrade(const char *name)
	{
		const UpgradeTemplate *u = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
		REQUIRE(u);
		return u;
	}
};
} // namespace

TEST_CASE("PORT-107 the template parses UpgradeCameo1..5 (field rows RW 0xDA41F8 .. RW 0xDA4238)")
{
	Fixture f;
	const ThingTemplate *tt = f.make("P107Archer")->getTemplate();
	REQUIRE(tt);
	const ThingTemplate *t = tt->getFinalOverride();
	const char *names[] = { "Upgrade_P107FireArrows", "Upgrade_P107PoisonTips" };
	for (int i = 0; i < 5; ++i)
	{
		const FieldValue *v = t->findField("UpgradeCameo" + std::to_string(i + 1));
		if (i < 2)
		{
			REQUIRE(v);
			const std::string *s = std::get_if<std::string>(v);
			REQUIRE(s);
			CHECK(*s == names[i]);
		}
		else if (v)
		{
			// slots 3..5 absent in the INI: the row exists and stores nothing (RW keeps an empty string there)
			const std::string *s = std::get_if<std::string>(v);
			REQUIRE(s);
			CHECK(s->empty());
		}
	}
	// a template without the fields: no value, absent stays absent
	const ThingTemplate *plain = f.make("P107Plain")->getTemplate();
	REQUIRE(plain);
	for (int i = 0; i < 5; ++i)
	{
		CHECK(plain->getFinalOverride()->findField("UpgradeCameo" + std::to_string(i + 1)) == nullptr);
	}
}

TEST_CASE("PORT-107 a completed upgrade in the selected template's cameos marks the bar dirty (RW 0x8A22D5 .. 0x8A2328)")
{
	Fixture f;
	Object *unit = f.make("P107Archer");
	f.select(unit);
	CHECK_FALSE(f.bar.dirty());
	// slot 1
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107FireArrows"));
	CHECK(f.bar.dirty());
	f.bar.update(); // the refresh consumes the mark
	CHECK_FALSE(f.bar.dirty());
	// slot 2
	const unsigned versionBefore = f.bar.version();
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107PoisonTips"));
	CHECK(f.bar.dirty());
	// the mark alone changes nothing the Palantir reads: no redraw bump, no command set change
	CHECK(f.bar.version() == versionBefore);
	CHECK(f.bar.commandSetName().empty());
}

TEST_CASE("PORT-107 a completed upgrade outside the cameos, or without a selection, does not mark dirty")
{
	Fixture f;
	Object *unit = f.make("P107Archer");
	f.select(unit);
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107Unrelated"));
	CHECK_FALSE(f.bar.dirty());
	// no selection: RW 0x8A22E3 / 0x8A22F1 take the null path and never write +0x28
	f.ui.deselectAll(false);
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107FireArrows"));
	CHECK_FALSE(f.bar.dirty());
	// a template without the fields (absent rows take the same null path)
	Object *plain = f.make("P107Plain");
	f.select(plain);
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107FireArrows"));
	CHECK_FALSE(f.bar.dirty());
	// a null upgrade never marks
	f.select(unit);
	f.bar.noteUpgradeCompleted(nullptr);
	CHECK_FALSE(f.bar.dirty());
}

TEST_CASE("PORT-107 the control bar refresh consumes the dirty mark (RW 0x71EBED)")
{
	Fixture f;
	Object *unit = f.make("P107Archer");
	f.select(unit);
	f.bar.noteUpgradeCompleted(f.upgrade("Upgrade_P107FireArrows"));
	REQUIRE(f.bar.dirty());
	f.bar.update();
	CHECK_FALSE(f.bar.dirty());
	// a second refresh keeps it clear; the selection's own state is untouched by the mark
	f.bar.update();
	CHECK_FALSE(f.bar.dirty());
	CHECK(f.bar.sourceObject() == unit->getID());
}

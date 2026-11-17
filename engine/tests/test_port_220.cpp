// OpenBFME unit tests for PORT-220 (issue #529): Object + 0x45B, the single-use command flag — getCommandAvailability's common gate and
// processCommandUI's writer. Synthetic world only: no RotWK or Ennorath file is mounted, no drawable, no wall clock.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001), read linearly with tools/mod/rw.py:
//   * the gate is RW 0x942826 `cmp byte ptr [ebx + 0x45B], 0` / RW 0x94282D `jne 0x9438C8` inside getCommandAvailability (RW 0x942733), after the
//     script availability call (RW 0x942490, skipped for command 0x18 by RW 0x94280D) and before the experience tracker's gate (RW 0x942833).
//     RW 0x9438C8 is the function's `disabled` exit: availability 0, which the port shows as ButtonState::Restricted. It runs for every command,
//     so one press of a SINGLE_USE_COMMAND button grays the whole command set of that object.
//   * a whole-.text linear scan (capstone, skipdata) finds five accesses to [reg + 0x45B] and no others: the constructor's clear RW 0x699C88
//     (with + 0x45A and + 0x45C, before `call 0x691BAB`), the setter RW 0x940397 (`mov byte ptr [ecx + 0x45B], 1` / `ret`; `rw.py xref 0x940397`
//     finds no rel32 caller), the accessor RW 0x9422C2 (`mov al, byte ptr [ecx + 0x45B]` / `ret`), this gate, and the writer below. Nothing ever
//     clears the flag again, so the gray buttons stay gray.
//   * the writer is RW 0x94066B `mov byte ptr [ebx + 0x45B], 1` in processCommandUI (the function starts at RW 0x940435; its stack guard is the
//     `mov eax, 0xBAC326` / `call 0xA3CEF0` pair getCommandAvailability has too). RW 0x940625 calls getCommandAvailability(button, 0, object,
//     &percent, 0); results 1 and 2 jump to RW 0x940656, which clears the button's + 0xF8, skips a null object (RW 0x94065D) and tests
//     RW 0x940665 `test byte ptr [esi + 0x1E], 2` — bit 17 of the button's Options dword (+ 0x1C). RW 0xDA4C88's option name table gives index 17
//     as SINGLE_USE_COMMAND (index 30 is ON_GROUND_ONLY, index 26 MOUNTED_ONLY). Results 6 and 7 leave through RW 0x9406DA / 0x9405F1 and write
//     nothing.
//   * the flag is simulation state, so Object::xfer carries it (v6) and Object::crcOmittingModules hashes it when set.
// DONOR (corroboration only, never retail proof): BFME2 gamelogic/object.h:402 .. 403 names the pair markSingleUseCommandUsed /
// hasSingleUseCommandBeenUsed, ObjectDoXfer.cpp:392 puts the same byte at + 0x43B and line 556 transfers it, and
// ControlBarRappellerCount.cpp:445 returns COMMAND_RESTRICTED from it between the same two gates.

#include "doctest.h"
#include "MoveTestUtil.h"
#include "HudTestUtil.h" // PinholeView

#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/Object/Object.h"

#include <vector>

namespace
{
// A unit with a four-button command set: a plain STOP, a SINGLE_USE_COMMAND STOP, a STOP that needs rank 5 (RW 0x942833's gate, after the flag's)
// and a MOUNTED_ONLY STOP (RW 0x9427BD's gate, before it). The bar is driven the way the HUD drives it: select, update, read the states, press.
struct SingleUseRig
{
	movetest::MoveWorld world{ 10, 10 };
	CommandStore store;
	CommandStore *savedStore = nullptr;
	PinholeView view{ { 0, -400, 500 }, { 0, 0, 0 }, 1024, 768, 0.7f };
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *world.logic, world.ai.get(), view, ui, stream, &store, {} };
	ControlBar bar{ ctx };
	CommandButton plain, single, ranked, mountedOnly;
	Object *unit = nullptr;
	Object *other = nullptr;

	SingleUseRig()
	{
		savedStore = TheCommandStore;
		TheCommandStore = &store;
		store.setThingFactory(&world.w.things);
		REQUIRE(world.w.load("Object P220Unit\n KindOf = SELECTABLE INFANTRY\n CommandSet = P220Set\nEnd\n").empty());
		plain.m_name = "P220Plain";
		plain.m_command = GUI_COMMAND_STOP;
		plain.m_inPalantir = true;
		single.m_name = "P220Single";
		single.m_command = GUI_COMMAND_STOP;
		single.m_inPalantir = true;
		single.m_options = COMMAND_OPTION_SINGLE_USE_COMMAND;
		ranked.m_name = "P220Ranked";
		ranked.m_command = GUI_COMMAND_STOP;
		ranked.m_inPalantir = true;
		ranked.m_requireLevel = 5;
		mountedOnly.m_name = "P220Mounted";
		mountedOnly.m_command = GUI_COMMAND_STOP;
		mountedOnly.m_inPalantir = true;
		mountedOnly.m_options = COMMAND_OPTION_MOUNTED_ONLY;
		CommandSet *set = store.newDynamicCommandSet("P220Set");
		REQUIRE(set);
		set->setDynamicButton(0, &plain);
		set->setDynamicButton(1, &single);
		set->setDynamicButton(2, &ranked);
		set->setDynamicButton(3, &mountedOnly);
		unit = world.make("P220Unit", world.teamOf("Alice"));
		REQUIRE(unit);
		other = world.make("P220Unit", world.teamOf("Alice"));
		REQUIRE(other);
		world.players.setLocalPlayer(unit->getControllingPlayer());
		REQUIRE(unit->getExperienceTracker()); // RW 0x942833's gate needs a tracker; the constructor makes one (RW 0x69A606)
		select(unit);
	}
	~SingleUseRig() { TheCommandStore = savedStore; }

	void select(Object *o)
	{
		// PORT-221 (issue #531) gives a selection of two live controllable objects its own context (populateMultiSelect RW 0x94475A), which shows only
		// the OK_FOR_MULTI_SELECT buttons; this rig reads one object's command set, so the selection is replaced, not extended.
		ui.mirrorSelection({ o->getID() });
		bar.update();
	}
	ButtonState stateOf(const CommandButton &b) const
	{
		for (const std::vector<ControlBarButton> *list : { &bar.palantirButtons(), &bar.sideButtons(), &bar.offBarButtons() })
		{
			for (const ControlBarButton &cb : *list)
			{
				if (cb.button == &b)
				{
					return cb.state;
				}
			}
		}
		return ButtonState::Hidden; // the bar does not show the button at all
	}
};
} // namespace

TEST_CASE("PORT-220: a set single-use flag grays every button of the object's command set (RW 0x942826)")
{
	SingleUseRig r;
	// the neighbouring gates before the change: the plain and the single-use buttons are usable, the rank 5 button is hidden (RW 0x942833) and the
	// MOUNTED_ONLY button of an unmounted unit is restricted (RW 0x9427BD)
	REQUIRE(r.stateOf(r.plain) == ButtonState::Enabled);
	REQUIRE(r.stateOf(r.single) == ButtonState::Enabled);
	REQUIRE(r.stateOf(r.ranked) == ButtonState::Hidden);
	REQUIRE(r.stateOf(r.mountedOnly) == ButtonState::Restricted);
	CHECK_FALSE(r.unit->hasSingleUseCommandBeenUsed());

	r.unit->markSingleUseCommandUsed(); // RW 0x940397
	r.bar.update();
	// the gate is common to every command: the button without the option grays too, and it wins over the later tracker gate (hidden -> restricted)
	CHECK(r.stateOf(r.plain) == ButtonState::Restricted);
	CHECK(r.stateOf(r.single) == ButtonState::Restricted);
	CHECK(r.stateOf(r.ranked) == ButtonState::Restricted);
	// RW 0x9427BD's gate runs first, so the mounted button keeps its own result
	CHECK(r.stateOf(r.mountedOnly) == ButtonState::Restricted);
	// the flag is the object's, not the player's or the bar's: another object of the same player keeps its usable buttons
	r.select(r.other);
	CHECK(r.stateOf(r.plain) == ButtonState::Enabled);
	CHECK(r.stateOf(r.single) == ButtonState::Enabled);
}

TEST_CASE("PORT-220: pressing a SINGLE_USE_COMMAND button sets the flag, a plain press does not (RW 0x94066B)")
{
	SingleUseRig r;
	REQUIRE(r.bar.pressButton(0, true)); // the plain STOP: MSG_DO_STOP goes out, no write
	CHECK(r.stream.pending() == 1);
	CHECK_FALSE(r.unit->hasSingleUseCommandBeenUsed());
	CHECK(r.stateOf(r.plain) == ButtonState::Enabled);

	REQUIRE(r.bar.pressButton(1, true)); // the SINGLE_USE_COMMAND STOP: availability 1 (RW 0x940625) and bit 17 (RW 0x940665) -> RW 0x94066B
	CHECK(r.unit->hasSingleUseCommandBeenUsed());
	r.bar.update();
	CHECK(r.stateOf(r.single) == ButtonState::Restricted);
	CHECK(r.stateOf(r.plain) == ButtonState::Restricted);
	// the second press is refused by the gate the first press set: processCommandUI never reaches a restricted button (RW 0x940435's state gate)
	const int messages = r.stream.pending();
	CHECK_FALSE(r.bar.pressButton(1, true));
	CHECK(r.stream.pending() == messages);
}

TEST_CASE("PORT-220: the flag is simulation state — the object's hash and its snapshot carry it")
{
	SingleUseRig r;
	StateHasher before;
	r.unit->crc(before);
	XferSave clear;
	r.unit->xfer(clear); // the snapshot of an object that never pressed such a button
	r.unit->markSingleUseCommandUsed();
	StateHasher after;
	r.unit->crc(after);
	CHECK(before.value() != after.value()); // hashed only when set: an untouched object keeps its hash
	XferSave flagged;
	r.unit->xfer(flagged);

	SingleUseRig fresh; // the same synthetic world again: its objects are created in the same order, so the ids and templates match
	REQUIRE(fresh.unit->getID() == r.unit->getID());
	REQUIRE(fresh.other->getID() == r.other->getID());
	CHECK_FALSE(fresh.unit->hasSingleUseCommandBeenUsed());
	XferLoad loadFlagged(flagged.data());
	fresh.unit->xfer(loadFlagged);
	CHECK(fresh.unit->hasSingleUseCommandBeenUsed());
	fresh.bar.update();
	CHECK(fresh.stateOf(fresh.plain) == ButtonState::Restricted);
	XferLoad loadClear(clear.data());
	fresh.unit->xfer(loadClear);
	CHECK_FALSE(fresh.unit->hasSingleUseCommandBeenUsed());
	fresh.bar.update();
	CHECK(fresh.stateOf(fresh.plain) == ButtonState::Enabled);
}

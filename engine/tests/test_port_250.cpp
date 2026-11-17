// PORT-250 (fork issue 578): a SPECIAL_POWER button whose power no module of the object carries.
//
// RotWK's command availability (RW 0x942733) sends SPECIAL_POWER (0x18) and SPECIAL_POWER_TOGGLE / FROM_COMMAND_CENTER to the arm RW 0x943343.
// The arm's gates, read with rw.py dis:
//   RW 0x943343  cmp byte [button + 0x106], 0        ShowButton; 0 -> RW 0x942C5D (eax = 3, hidden)
//   RW 0x94334A  esi = [button + 0x44]               the SpecialPowerTemplate resolved at INI load; null -> RW 0x942C5D (hidden)
//   RW 0x94335E  call RW 0x68B678 -> owner; null or RW 0x6AAC52 (not active) -> hidden
//   RW 0x94337F..0x9433D1  the first owned RequiredScience (RW 0x6A0713 / 0x6AC207); none -> hidden; RW 0x6AC25F / 0x6AC287
//                          (a disabled or hidden science) -> RW 0x9438C8 (eax = 0, disabled)
//   RW 0x94341F  RW 0x75CDC4 (Object + 0x94) with the template's mask at +0x64 -> RW 0x9438C8 (disabled)
//   RW 0x943439..0x943480 the UnitCost gate of this arm (see the test below)
//   RW 0x943486  RW 0x68C26D (Object::findModule by SpecialPowerTemplate): the object's own modules only, no horde member search
//   RW 0x9434A9  the SpecialAbilityUpdate lookup (RW 0x68C461 on the final override's + 0x1C, the power type) and its chain name (RW 0x851371)
//   RW 0x9434F7  no power module: esi == 0, the CommandTrigger loop RW 0x94351A .. 0x943562 (each trigger's button + 0x14 must be 0x18, its
//                + 0x44 the power) and every exit of it jumps to RW 0x942FF1 -> `xor eax, eax; inc eax` = 1 = AVAILABLE
// So RotWK answers 1, available (the button is drawn live and a press sends MSG_DO_SPECIAL_POWER, which no module answers: nothing happens),
// for a power that exists but that no module of the object carries; and 3, hidden, for a button whose power is not a SpecialPowerTemplate at all.
// The port's availability already answers exactly that (ControlBar.cpp, the SPECIAL_POWER arm), which is what these tests hold.
//
// What the port did not have is the arm's UnitCost gate (RW 0x943439 .. 0x943480): the template's final override (RW 0x688D3C) + 0x80 (UnitCost);
// with a cost the object's contain (Object + 0x258, slot 0x7C) horde member count (slot 0x188, RW 0x943477) must reach it, else availability 0
// (disabled). ActionManager already runs the same test for a press (RW 0x82E000 / 0x82D605), so without the arm's copy the player saw a live button
// that did nothing: the mod's Angmar Necromancer powers (SpecialAbilityNecroWellOfSouls UnitCost = 4, SoulFreeze 3, FellStrength 3, BlackIce 3,
// Devastation 2, FrostArmor 4, and the Bloody Sacrifice pair) are spent from his horde.
// See docs/ennorath/PORT250_RW_ANALYSIS.md, docs/ennorath/night_watch.d/PORT-250.md. GPL-3.0.

#include "doctest.h"

#include "EconTestUtil.h"
#include "IniTestUtil.h"

#include "Common/INI.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
// the buttons of the case: RW 0x943343's three answers
const char buttons[] = R"(
CommandButton Command_P250_Well
  Command = SPECIAL_POWER
  Options = NEED_TARGET_POS
  SpecialPower = SpecialAbilityP250Well
  ButtonImage = UCCommon_Well
  TextLabel = CONTROLBAR:P250Well
  DescriptLabel = CONTROLBAR:P250WellDesc
  InPalantir = Yes
End
CommandButton Command_P250_Screech
  Command = SPECIAL_POWER
  Options = CONTEXTMODE_COMMAND
  SpecialPower = SpecialAbilityP250Screech
  ButtonImage = UCCommon_Screech
  TextLabel = CONTROLBAR:P250Screech
  DescriptLabel = CONTROLBAR:P250ScreechDesc
  InPalantir = Yes
End
CommandButton Command_P250_Ghost
  Command = SPECIAL_POWER
  SpecialPower = SpecialAbilityP250NoTemplate
  ButtonImage = UCCommon_Ghost
  TextLabel = CONTROLBAR:P250Ghost
  DescriptLabel = CONTROLBAR:P250GhostDesc
  InPalantir = Yes
End
CommandSet P250Set
  1 = Command_P250_Well
  2 = Command_P250_Screech
  3 = Command_P250_Ghost
End
)";

// the powers: Well has a cost (3 members), Screech has none; SpecialAbilityP250NoTemplate is deliberately not defined
const char powerIni[] = R"(
SpecialPower SpecialAbilityP250Well
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10000
  UnitCost = 3
  UnitCostDeathType = 3
End
SpecialPower SpecialAbilityP250Screech
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10000
End
)";

// P250HordeOne: one member for a power that costs three (RW 0x943439 answers 0); P250HordeFour: enough. Both hordes carry the power's module
// (the way Ennorath's Harad slave and HobbitPikeman hordes do), the Screech button has no module anywhere (the NO-MODULE row of the matrix)
const char objects[] = R"(
Object P250Member
  KindOf = SELECTABLE INFANTRY
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
End
Object P250HordeOne
  KindOf = SELECTABLE HORDE INFANTRY
  CommandSet = P250Set
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Behavior = HordeAIUpdate ModuleTag_AI
  End
  Behavior = HordeContain ModuleTag_Contain
    RankInfo = RankNumber:1 UnitType:P250Member Position:X:0 Y:0 Position:X:0 Y:10 Position:X:0 Y:20 Position:X:0 Y:30
    InitialPayload = P250Member 1
  End
  Behavior = SpecialPowerModule ModuleTag_Well
    SpecialPowerTemplate = SpecialAbilityP250Well
  End
End
ChildObject P250HordeFour P250HordeOne
  Behavior = HordeContain ModuleTag_Contain
    RankInfo = RankNumber:1 UnitType:P250Member Position:X:0 Y:0 Position:X:0 Y:10 Position:X:0 Y:20 Position:X:0 Y:30
    InitialPayload = P250Member 4
  End
End
)";

// the bar over a synthetic world, command store and special power store (no game files)
struct Fixture
{
	econtest::Fx game{ objects };
	CommandStore store;
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext context;
	ControlBar bar;

	Fixture()
		: context{ *game.logic, nullptr, view, ui, stream, &store, {}, {} }
		, bar{ context }
	{
		TheSpecialPowerStore = &powers;
		store.setThingFactory(&game.w.things);
		TheCommandStore = &store;
		game.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		game.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		game.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		const std::string powerErr = game.w.load(powerIni);
		REQUIRE_MESSAGE(powerErr.empty(), powerErr);
		const std::string buttonErr = game.w.load(buttons);
		REQUIRE_MESSAGE(buttonErr.empty(), buttonErr);
		game.players.setLocalPlayer(game.player("Alice"));
		ui.setLogic(game.logic.get());
		ui.setMaxSelectCount(50);
	}
	~Fixture()
	{
		TheCommandStore = nullptr;
		TheSpecialPowerStore = savedPowers;
	}
	// the state of the arc's button whose power is `power` (null: no such button on the bar)
	ButtonState stateOf(Object *o, const char *power)
	{
		ui.deselectAll(false);
		ui.selectObject(o->getID());
		bar.update();
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_specialPowerName == power)
			{
				return b.state;
			}
		}
		return ButtonState::Hidden;
	}
	bool onBar(Object *o, const char *power)
	{
		ui.deselectAll(false);
		ui.selectObject(o->getID());
		bar.update();
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_specialPowerName == power)
			{
				return true;
			}
		}
		return false;
	}
};
} // namespace

TEST_CASE("PORT-250 the arm's UnitCost gate: a power that costs more members than the horde carries is disabled (RW 0x943439 .. 0x943480, count RW 0x943477)")
{
	Fixture f;
	// the fixture's data: the power costs three members, the template has the module, the horde carries one
	const SpecialPowerTemplate *well = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityP250Well");
	REQUIRE(well);
	CHECK(well->getFinalOverride()->m_unitCost == 3);

	Object *one = f.game.makeAt("P250HordeOne", "Alice", 100.0f, 100.0f);
	REQUIRE(one);
	REQUIRE(f.onBar(one, "SpecialAbilityP250Well")); // the button is on the bar: the module is there, ShowButton is set
	// RW 0x943439: with a cost, the contain's horde interface count (slot 0x188) below it answers 0 = disabled (the grey button)
	CHECK(f.stateOf(one, "SpecialAbilityP250Well") == ButtonState::Restricted);

	// the neighbour: four members reach the cost, and the same button of the same module is live
	Object *four = f.game.makeAt("P250HordeFour", "Alice", 200.0f, 100.0f);
	REQUIRE(four);
	REQUIRE(f.onBar(four, "SpecialAbilityP250Well"));
	CHECK(f.stateOf(four, "SpecialAbilityP250Well") == ButtonState::Enabled);
}

TEST_CASE("PORT-250 a power no module carries is available, as RW 0x9434F7 .. 0x943564 -> RW 0x942FF1 answers 1")
{
	Fixture f;
	Object *horde = f.game.makeAt("P250HordeFour", "Alice", 100.0f, 100.0f);
	REQUIRE(horde);
	// the NO-MODULE row: SpecialAbilityP250Screech is a template, no module of the horde carries it; RW falls through to 1 = available
	REQUIRE(f.onBar(horde, "SpecialAbilityP250Screech"));
	CHECK(f.stateOf(horde, "SpecialAbilityP250Screech") == ButtonState::Enabled);
	// the neighbour with a module stays live on the same object (the module exists, the cost is met)
	CHECK(f.stateOf(horde, "SpecialAbilityP250Well") == ButtonState::Enabled);
}

TEST_CASE("PORT-250 a button whose power is no SpecialPowerTemplate at all is hidden (RW 0x94334A / 0x943356 -> RW 0x942C5D answers 3)")
{
	Fixture f;
	Object *horde = f.game.makeAt("P250HordeFour", "Alice", 100.0f, 100.0f);
	REQUIRE(horde);
	// the button is in the CommandSet and RW answers 3 for it: + 0x44 is null, so the arm hides before any module search. The port's arc does not
	// carry a hidden button (ControlBar::update skips ButtonState::Hidden when it fills the Palantir), so the button is not on it at all
	CHECK(!f.onBar(horde, "SpecialAbilityP250NoTemplate"));
	// the same object, the same bar: the two buttons whose power exists are on it, the NO-MODULE one live
	CHECK(f.onBar(horde, "SpecialAbilityP250Screech"));
	CHECK(f.stateOf(horde, "SpecialAbilityP250Screech") == ButtonState::Enabled);
	CHECK(f.stateOf(horde, "SpecialAbilityP250Well") == ButtonState::Enabled);
}

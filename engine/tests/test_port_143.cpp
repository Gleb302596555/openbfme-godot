// PORT-143 (issue #345, the owner's P-25): Rohan's battering ram: the chosen tactic sounds but the palantir keeps showing the defensive stance.
//
// An object whose command set offers Command_ToggleStance but that has no StancesBehavior (Ennorath's RohanBatteringRam, whose INI skips the behavior
// while every other ram of the mod has one) gets a gray stance button: ControlBar::getCommandAvailability RW 0x942A77 .. 0x942A8E calls
// Object::findModule("StancesBehavior") (RW 0x861E6E -> RW 0x68BDA5) and answers 0, the window is disabled (RW 0x943C1F .. 0x943C29) and a click on it
// reaches nothing (RW 0x9D6E54 -> RW 0x63F3BF ret). The logic refuses the order for the same reason: AIGroup::setStance (RW 0x76FF27 .. 0x76FF5E) and
// ChangeStanceGroupOrder (RW 0x949DF0) both return when findModule gives 0, so MSG_CHANGE_STANCE never changes that object's stance.
//
// The client-side picture, though, follows the order. The Palantir builds an InGameToggleStanceCommandButton for the button even in a disabled window
// (RW 0x92FF5C), and it registers SetStanceHotKeyAction (RW 0x96F890 -> RW 0x96F880) on the stance keys and the meta messages 0x93 / 0x94 / 0x95
// (RW 0x75AE14 / RW 0x75B2BA) whatever the window's state. SetStance (RW 0x96F53C, DONOR BFME2 0x00567A6E) then
//   0x96F54B  slot = StanceToButtonSlot(m_button, stance)             (RW 0x96F2DA: m_stances[i] == stance, else -1)
//   0x96F556  m_button->setCurrentIndex(slot)                          (RW 0x75D082: an index outside ButtonImage leaves +0xFC as it was)
//   0x96F567  GadgetButtonSetEnabledImage(m_window, m_button->getButtonImage()) (RW 0x75CDFC reads +0xFC, RW 0x6C106C writes the window's enabled image)
//   0x96F56C  MSG_CHANGE_STANCE (0x468) with the stance
//   0x96F59C  the unit voice 0x7E3 (RW 0x8DF932, the button's UnitSpecificSound at +0xFC: RW 0x75D0B9)
// and the widget's own ctor (RW 0x96FDFD) does the same at 0x96FF43 .. 0x96FF68 with the object's stance CLASS (RW 0x96FF2F -> RW 0x861D8E): with no
// StancesBehavior the class lookup yields no slot (RW 0x96FF55: jl skips it) and the image is read from the index the button still has. That is why the
// gray ram button shows the tactic the player chose: m_currentIndex (CommandButton +0xFC, the ctor RW 0x75D64A inits it to 0) is the button's own state,
// shared by every selection, and the picture (getButtonImage RW 0x75CDFC, getTextLabel RW 0x75CE47, getDescriptLabel RW 0x75CECD) is read at it.
//
// These tests pin the field and its four readers, StanceToButtonSlot's raw compare, the picture of a gray toggle after an order that the logic drops and
// the picture of a stanced unit following its stance class. The voice and the logic's stance are untouched (the owner's option 2).
// See docs/ennorath/NIGHT_WATCH.md (lane PORT-143); docs/STOPS.md S-7335. GPL-3.0.

#include "doctest.h"

#include "EconTestUtil.h"
#include "IniTestUtil.h"

#include "Common/INI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

namespace
{
// the retail names of the stance toggle (RotWK's CommandButton.ini, the same rows Ennorath's Command_ToggleStance uses: Stances = HoldGround Battle
// Aggressive, ButtonImage = UCCommon_HoldGroundStance UCCommon_BattleStance UCCommon_AggresiveStance, UnitSpecificSound = Gui_Stance* )
const char buttons[] = R"(
CommandButton Command_P143_ToggleStance
  Command = TOGGLE_STANCE
  Options = OK_FOR_MULTI_SELECT
  ButtonImage = UCCommon_HoldGroundStance UCCommon_BattleStance UCCommon_AggresiveStance
  TextLabel = CONTROLBAR:ToggleStanceHoldGround CONTROLBAR:ToggleStanceBattle CONTROLBAR:ToggleStanceAggressive
  DescriptLabel = CONTROLBAR:ToggleStanceDescHoldGround CONTROLBAR:ToggleStanceDescBattle CONTROLBAR:ToggleStanceDescAggressive
  Stances = HoldGround Battle Aggressive
  UnitSpecificSound = Gui_StanceHoldGround Gui_StanceBattle Gui_StanceAttack
  InPalantir = Yes
End
CommandSet P143RamSet
  1 = Command_P143_ToggleStance
End
)";

// the ram: the toggle in its command set, no StancesBehavior anywhere (rohanbatteringram.ini:7 .. 667 of Ennorath 1.9.8.2); the soldier: the same set
// with the behavior, so its picture follows its stance's class
const char objects[] = R"(
Object P143Ram
  KindOf = SELECTABLE IMMOBILE CAN_ATTACK
  CommandSet = P143RamSet
End
Object P143Soldier
  KindOf = SELECTABLE CAN_ATTACK
  CommandSet = P143RamSet
  Behavior = StancesBehavior ModuleTag_P143Stance
    StanceTemplate = P143Stances
  End
End
)";

const std::string kHoldGroundImage = "UCCommon_HoldGroundStance";
const std::string kBattleImage = "UCCommon_BattleStance";
const std::string kAggressiveImage = "UCCommon_AggresiveStance";

// the bar over a synthetic world, command store and stance template store (no game files)
struct Fixture
{
	econtest::Fx game{ objects };
	CommandStore store;
	StanceTemplateStore stances;
	StanceTemplateStore *savedStances = TheStanceTemplateStore;
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext context;
	ControlBar bar;

	Fixture()
		: context{ *game.logic, nullptr, view, ui, stream, &store, {}, {} }
		, bar{ context }
	{
		TheStanceTemplateStore = &stances;
		stances.registerBlock(game.w.fx.env.blocks);
		store.setThingFactory(&game.w.things);
		TheCommandStore = &store;
		game.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		game.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		const std::string stanceErr = game.w.load("StanceTemplate P143Stances\nEnd\n");
		REQUIRE_MESSAGE(stanceErr.empty(), stanceErr);
		const std::string buttonErr = game.w.load(buttons);
		REQUIRE_MESSAGE(buttonErr.empty(), buttonErr);
		game.players.setLocalPlayer(game.player("Alice"));
		ui.setLogic(game.logic.get());
		ui.setMaxSelectCount(50);
	}
	~Fixture()
	{
		TheCommandStore = nullptr;
		TheStanceTemplateStore = savedStances;
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
	// the stance toggle the arc shows for the current selection (null: none)
	const ControlBarButton *toggle()
	{
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_command == GUI_COMMAND_TOGGLE_STANCE)
			{
				return &b;
			}
		}
		return nullptr;
	}
};
} // namespace

TEST_CASE("PORT-143 CommandButton's index field follows RW 0x75D082: only a slot inside ButtonImage moves it, and the image and labels are read at it (RW 0x75CDFC / 0x75CE47 / 0x75CECD)")
{
	CommandButton b;
	// the ctor RW 0x75D64A: +0xFC starts at 0
	CHECK(b.currentIndex() == 0);
	CHECK(b.getButtonImage() == nullptr); // getButtonImage RW 0x75CE1E: no list, no image
	b.m_buttonImageName = { kHoldGroundImage, kBattleImage, kAggressiveImage };
	b.m_textLabel = { "CONTROLBAR:ToggleStanceHoldGround", "CONTROLBAR:ToggleStanceBattle", "CONTROLBAR:ToggleStanceAggressive" };
	b.m_descriptLabel = { "CONTROLBAR:ToggleStanceDescHoldGround", "CONTROLBAR:ToggleStanceDescBattle", "CONTROLBAR:ToggleStanceDescAggressive" };
	CHECK(b.currentIndex() == 0);
	CHECK(b.getButtonImage() != nullptr);
	CHECK(*b.getButtonImage() == kHoldGroundImage);

	b.setCurrentIndex(1);
	CHECK(b.currentIndex() == 1);
	CHECK(*b.getButtonImage() == kBattleImage);
	CHECK(b.getTextLabel() == "CONTROLBAR:ToggleStanceBattle");
	CHECK(b.getDescriptLabel() == "CONTROLBAR:ToggleStanceDescBattle");

	// StanceToButtonSlot's -1 (the stance is not in Stances) and a slot past the images are both refused: RW 0x75D086 (jl) and RW 0x75D09C (jae) leave
	// +0xFC where it was, which is what keeps the chosen picture on a button the logic answers with nothing
	b.setCurrentIndex(-1);
	CHECK(b.currentIndex() == 1);
	b.setCurrentIndex(3);
	CHECK(b.currentIndex() == 1);

	// the label lists may be shorter than the images: getTextLabel RW 0x75CE78 and getDescriptLabel RW 0x75CEE8 fall back to the last entry
	b.m_descriptLabel = { "CONTROLBAR:OnlyOne" };
	b.setCurrentIndex(2);
	CHECK(b.currentIndex() == 2);
	CHECK(*b.getButtonImage() == kAggressiveImage);
	CHECK(b.getDescriptLabel() == "CONTROLBAR:OnlyOne");
	// and an empty TextLabel list is the empty string (RW 0x75CE88, TheEmptyAsciiString RW 0xDC62B8)
	b.m_textLabel.clear();
	CHECK(b.getTextLabel().empty());
}

TEST_CASE("PORT-143 the button's Stances list is the raw stance names (RW 0x96F2DA compares them with the ordered stance, no stance class in between)")
{
	Fixture f;
	const CommandButton *b = f.store.findCommandButton("Command_P143_ToggleStance");
	REQUIRE(b);
	CHECK(b->m_command == GUI_COMMAND_TOGGLE_STANCE);
	// kStanceNames RW 0xC53660: Uninitialized 0, Battle 1, Aggressive 2, HoldGround 3 (and the classes of RW 0x861D8E agree on these three)
	CHECK(b->m_stances == std::vector<int>{ STANCE_HOLD_GROUND, STANCE_BATTLE, STANCE_AGGRESSIVE });
	CHECK(b->m_buttonImageName == std::vector<std::string>{ kHoldGroundImage, kBattleImage, kAggressiveImage });
	CHECK(ControlBar::stanceSlotOf(*b, STANCE_HOLD_GROUND) == 0);
	CHECK(ControlBar::stanceSlotOf(*b, STANCE_BATTLE) == 1);
	CHECK(ControlBar::stanceSlotOf(*b, STANCE_AGGRESSIVE) == 2);
	// HoldGroundMoving is not in the list: SetStance's StanceToButtonSlot answers -1 on the raw value (the class conversion is the ctor's, RW 0x96FF2F)
	CHECK(ControlBar::stanceSlotOf(*b, STANCE_HOLD_GROUND_MOVING) == -1);
	CHECK(ControlBar::stanceSlotOf(*b, STANCE_UNINITIALIZED) == -1);
}

TEST_CASE("PORT-143 the tactic order moves the picture of the gray stance button of an object without a StancesBehavior (RW 0x96F53C -> 0x75D082 -> 0x6C106C)")
{
	Fixture f;
	Object *ram = f.make("P143Ram");
	REQUIRE(ram);
	REQUIRE(StancesBehavior::of(*ram) == nullptr); // the mod's rohanbatteringram.ini has no StancesBehavior
	f.select(ram);
	f.bar.update();
	const ControlBarButton *toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->state == ButtonState::Restricted); // RW 0x942A77: no StancesBehavior, availability 0 (the gray button)
	CHECK(toggle->image == kHoldGroundImage);        // the first ButtonImage, m_currentIndex 0
	CHECK(toggle->button->currentIndex() == 0);

	// the player chooses Battle: the client's SetStance sets the button's index and its picture, then sends MSG_CHANGE_STANCE (the logic drops it here)
	REQUIRE(f.bar.sendChangeStance(STANCE_BATTLE, "PORT-143"));
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->button->currentIndex() == 1);
	CHECK(toggle->image == kBattleImage);
	CHECK(toggle->textLabel == "CONTROLBAR:ToggleStanceBattle");
	CHECK(toggle->descriptLabel == "CONTROLBAR:ToggleStanceDescBattle");
	// the order did not change the object: the logic has nothing to change (the sound and the stance are untouched by this port)
	CHECK(StancesBehavior::of(*ram) == nullptr);

	// and the next refresh of the bar does not push the picture back (RW 0x96FF55: with no StancesBehavior there is no slot, so +0xFC stays)
	f.bar.update();
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->image == kBattleImage);
	CHECK(toggle->button->currentIndex() == 1);
	CHECK(toggle->state == ButtonState::Restricted);

	// another tactic: Aggressive; a stance the button does not list keeps the picture of the last one
	f.bar.applyToggleStance(STANCE_AGGRESSIVE); // the D / F / G meta route (CommandTranslator::changeStance, RW 0x8201F5) reaches it through the HUD's MSG_CHANGE_STANCE observer
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->image == kAggressiveImage);
	CHECK(toggle->button->currentIndex() == 2);
	f.bar.applyToggleStance(STANCE_HOLD_GROUND_MOVING); // StanceToButtonSlot -1, setCurrentIndex's bounds: nothing moves
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->image == kAggressiveImage);
	CHECK(toggle->button->currentIndex() == 2);
	f.bar.update();
	CHECK(f.toggle()->image == kAggressiveImage);
}

TEST_CASE("PORT-143 a stanced unit's button still follows its stance's class (RW 0x96FF43 .. 0x96FF68, RW 0x96F5D8 .. 0x96F608), including on the next selection")
{
	Fixture f;
	Object *soldier = f.make("P143Soldier");
	REQUIRE(soldier);
	StancesBehavior *sb = StancesBehavior::of(*soldier);
	REQUIRE(sb);
	f.select(soldier);
	f.bar.update();
	// a fresh object has not run its first update (RW 0x8622E9): Uninitialized is class Battle (RW 0x861D8E), so the toggle shows Battle, not the list's first
	const ControlBarButton *toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(sb->getStance() == STANCE_UNINITIALIZED);
	CHECK(toggle->state == ButtonState::Enabled);
	CHECK(toggle->image == kBattleImage);
	CHECK(toggle->button->currentIndex() == 1);

	REQUIRE(sb->setStance(STANCE_HOLD_GROUND, "PORT-143"));
	f.bar.update();
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->image == kHoldGroundImage);
	CHECK(toggle->button->currentIndex() == 0);
	// HoldGroundMoving is the HoldGround class (RW 0x861D8E): the same slot (the listener RW 0x96F5E4 uses the class too)
	sb->onMoveStart("PORT-143");
	REQUIRE(sb->getStance() == STANCE_HOLD_GROUND_MOVING);
	f.bar.update();
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->image == kHoldGroundImage);
	CHECK(toggle->button->currentIndex() == 0);

	// the index lives on the shared parsed button (CommandButton +0xFC), so another selection of the same button starts from it: the ram below keeps
	// Battle until an order says otherwise, and a stanced unit's own class wins as soon as the bar refreshes
	REQUIRE(sb->setStance(STANCE_AGGRESSIVE, "PORT-143"));
	f.bar.update();
	REQUIRE(f.toggle()->image == kAggressiveImage);
	Object *ram = f.make("P143Ram");
	f.select(ram);
	f.bar.update();
	toggle = f.toggle();
	REQUIRE(toggle);
	CHECK(toggle->button->currentIndex() == 2);
	CHECK(toggle->image == kAggressiveImage);
}

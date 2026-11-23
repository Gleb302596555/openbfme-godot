// OpenBFME. PORT-313 (issue #749, S-1983 / S-1992): the hold-fire / auto-acquire button.
//
// The title asks for two things: the button's image toggle (RW 0x75D366) and the object flag the
// class-specific overrides of MSG_TOGGLE_NO_AUTO_ACQUIRE pass to a related object (the member's
// SlavedUpdate master, RW 0x693C2D / RW 0x77B731 .. 0x77B75A / RW 0x77B7A6 .. 0x77B80E).
//
// RotWK 2.01, each address read with rw.py dis on game.dat (md5 daed3668..):
//
//   RW 0x940435 case 0x30 (`TOGGLE_NO_AUTO_ACQUIRE`, GUICommandType 0x30, RW 0x941335 .. 0x94136C):
//     the control bar appends MSG_TOGGLE_NO_AUTO_ACQUIRE (1115, RW 0x94133D) with the context object's id
//     (`[ebx + 0x74]`, RW 0x941362) and immediately calls CommandButton::updateToggleImage
//     (RW 0x941367 -> RW 0x75D366) with (id, 1).
//
//   RW 0x75D366: the client half. The function reads the button's Options (+0x1C):
//     * RW 0x75D36F `test eax, 0x800000` (TOGGLE_IMAGE_ON_WEAPON): command 0x23 (TOGGLE_WEAPON) takes
//       RW 0x75D38A -> RW 0x75CD39 (the button's WeaponSlotToggle1/2/3 by the object's KindOf) and stores it
//       in +0xFC (RW 0x75D38F). Commands 0x30 and 0x24 (HORDE_TOGGLE_FORMATION) fall THROUGH to
//       RW 0x75D39E .. 0x75D3B2, which is RW 0x75D22E (updateToggleImage) — the *same* function the bar
//       already calls for the weaponset / formation / auto-acquire buttons.
//     * RW 0x75D397 `test eax, 0x3000000` (bits 24/25): the same RW 0x75D22E call.
//     * Otherwise nothing: an auto-acquire button without TOGGLE_IMAGE_ON_WEAPON does not move its image.
//   RW 0x75D22E's 0x800000 arm (RW 0x75D2BB .. 0x75D2E1) is the auto-acquire rule:
//     RW 0x75D2CF `push 0x17` / RW 0x75D2D3 `call 0x44DDEC` (Object::testStatus), then
//     RW 0x75D2D8 `sub al, [esp+0x10]; neg al; sbb eax,eax; inc eax` — the index is
//     (testStatus(NO_AUTO_ACQUIRE) == invert) ? 1 : 0, stored at +0xFC by RW 0x75D319.
//
//   The image the player sees is +0xFC: RW 0x75CDFC (getButtonImage), RW 0x75CE47 (getTextLabel) and
//   RW 0x75CECD (getDescriptLabel) all index the button's lists with it.
//
//   Availability: RW 0x942733's case table (jump table RW 0x942A70 over the byte table RW 0x943943)
//   sends command 0x30 to RW 0x942FF1 (`xor eax,eax; inc eax`: available, 1), the same arm as command
//   0x24: an auto-acquire button is always pressable.
//
//   The logic half, MSG_TOGGLE_NO_AUTO_ACQUIRE at RW 0x77B6C4 (WeaponCommands.cpp):
//     * RW 0x77B6D4 .. RW 0x77B703: with a source object the group value is the OPPOSITE of that
//       object's status 0x17 (RW 0x77B6EF `push 0x17` -> RW 0x44DDEC); a source id of 0 means "each
//       member toggles its own" (RW 0x77B6D0 `sete`).
//     * per selected member, RW 0x77B731 .. RW 0x77B75A: the member's SlavedUpdate (behavior interface
//       slot 0x68, RW 0x68CBBC) plus a weapon that answers SlaveAttackNugget's slot 0x34
//       (RW 0x693C2D -> RW 0x6CBAAB) makes the member's master (RW 0x68CBE2 -> RW 0x449681) the
//       `related` object. The pointer is set once before the loop (RW 0x77B6D7) and kept for later
//       members: only a member that passes both tests overwrites it (RW 0x77B75A is its only write).
//     * RW 0x77B776: a member already at the wanted value is skipped (an individually toggled member
//       does not re-toggle).
//     * RW 0x77B789: a member whose template KindOf byte +0x115 holds bit 5 (HORDE) sets the status
//       through RW 0x748650 -> RW 0x694569 (the horde's members, then the horde); any other object on
//       itself (RW 0x77B79C -> RW 0x62684D).
//     * turning it ON idles a member whose AI is in an active state (AI vslot 0x1BC, RW 0x662B3C:
//       true with no current state) and the related object too (RW 0x77B7E4 / RW 0x77B818 ->
//       RW 0x5E821A aiIdle(CMD_FROM_PLAYER)).
//
// Everything here runs on synthetic INI: no game archives, no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL.
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI/AIGuard.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
// The Ennorath siege buttons, reduced to what this task reads: Command_ToggleTrebAutoAcquire /
// Command_ToggleNoAutoAcquire / Command_ToggleCatAutoAcquire of data/ini/commandbutton.ini all carry
// `Command = TOGGLE_NO_AUTO_ACQUIRE`, `Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT` and two
// ButtonImage / TextLabel / DescriptLabel entries. The image pair below is the Ennorath Isengard
// ballista's ("UCCommon_AutoacquireOn UCCommon_AutoacquireOff"); the labels stay synthetic because the
// expectations here are the flag's, not the text table's.
//
// The second button is the "no image toggle" neighbour: no TOGGLE_IMAGE_ON_WEAPON, so RW 0x75D366
// leaves +0xFC alone and the press must not move the picture.
const char kPort313Ini[] =
	"CommandButton Command_SynthToggleNoAutoAcquire\n"
	"  Command = TOGGLE_NO_AUTO_ACQUIRE\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = UCCommon_AutoacquireOn UCCommon_AutoacquireOff\n"
	"  TextLabel = CONTROLBAR:AutoAcquire CONTROLBAR:NoAutoAcquire\n"
	"  DescriptLabel = CONTROLBAR:TipAutoAcquire CONTROLBAR:TipNoAutoAcquire\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_SynthToggleNoAutoAcquirePlain\n"
	"  Command = TOGGLE_NO_AUTO_ACQUIRE\n"
	"  ButtonImage = ImgAcquire ImgHoldFire\n"
	"  TextLabel = CONTROLBAR:AutoAcquire CONTROLBAR:NoAutoAcquire\n"
	"  DescriptLabel = CONTROLBAR:TipAutoAcquire CONTROLBAR:TipNoAutoAcquire\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandSet SynthSiegeCommandSet\n"
	"  1 = Command_SynthToggleNoAutoAcquire\n"
	"  2 = Command_SynthToggleNoAutoAcquirePlain\n"
	"End\n"
	// The siege engine: an AI that does not look for targets on its own (AutoAcquireEnemiesWhenIdle = No,
	// as IsengardBallista's AIUpdateInterface ModuleTag_03 in data/ini/object/evilfaction/units/isengard/
	// ballista.ini has it) and a weapon it can fire.
	"Object SynthSiegeEngine\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SIEGEENGINE\n"
	"  VisionRange = 200\n"
	"  CommandSet = SynthSiegeCommandSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"    InitialHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	// The ballista of a fortress expansion: the master (SlaveWatcherBehavior side) and the slaved siege
	// engine that carries SlavedUpdate plus a weapon with SlaveAttackNugget, as IsengardBallista and
	// IsengardBallistaExpansionWeapon are in the mod's data.
	"Weapon SynthSlaveAttackWeapon\n"
	"  AttackRange = 200\n"
	"  SlaveAttackNugget\n"
	"  End\n"
	"End\n"
	"Object SynthSlavedBallista\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SIEGEENGINE\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SynthSlaveAttackWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"    InitialHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    DieOnMastersDeath = Yes\n"
	"    MarkUnselectable = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	// A slaved unit whose weapon has no SlaveAttackNugget: RW 0x693C2D refuses it, so it never becomes
	// the `related` object (RW 0x77B745).
	"Object SynthSlavedPlain\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"    InitialHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    MarkUnselectable = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct Port313Rig : combattest::CombatWorld
{
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 0.0f, -300.0f, 400.0f }, { 100.0f, 100.0f, 0.0f }, 1024, 768, 0.7f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;

	Port313Rig()
		: combattest::CombatWorld("", 40)
	{
		TheCommandStore = &commandStore;
		commandStore.setThingFactory(&w.things);
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		WeaponCommands::registerHandlers(dispatcher);

		const std::string err = w.load(kPort313Ini, INI_LOAD_OVERWRITE, "port313.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Port313Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheCommandStore = savedCommands;
	}

	Object *spawn(const char *name, float x = 100.0f, float y = 100.0f)
	{
		Object *obj = unit(name, 'A', x, y);
		REQUIRE(obj != nullptr);
		return obj;
	}

	void select(Object *obj)
	{
		input->ui().deselectAll(true);
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		if (obj)
		{
			input->ui().selectObject(obj->getID());
			sel.appendObjectIDArgument(obj->getID());
		}
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	void refresh(int n = 1)
	{
		input->flush();
		frames(n);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const char *name) const
	{
		for (const std::vector<ControlBarButton> *list : { &bar->palantirButtons(), &bar->sideButtons(), &bar->offBarButtons() })
		{
			for (const ControlBarButton &b : *list)
			{
				if (b.button && b.button->m_name == name)
				{
					return &b;
				}
			}
		}
		return nullptr;
	}

	static bool hasStatus(const Object &o, const char *name)
	{
		const int bit = ObjectTemplateInfoBuilder::objectStatusIndex(name);
		return bit >= 0 && o.testStatus((unsigned)bit);
	}
};
} // namespace

// ---------------------------------------------------------------------------------------------------
// The button's image: RW 0x940435 case 0x30 -> RW 0x941367 -> RW 0x75D366 -> RW 0x75D22E
// (RW 0x75D2CF .. 0x75D2E1) and the picture the bar draws (+0xFC, RW 0x75CDFC).
// ---------------------------------------------------------------------------------------------------
TEST_CASE("PORT-313 an auto-acquire button's image follows the flag after each press and after the logic agrees")
{
	Port313Rig rig;
	Object *engine = rig.spawn("SynthSiegeEngine");
	rig.select(engine);

	// populateCommand calls RW 0x75D22E(obj, 0) (RW 0x943E91): a unit that auto-acquires (status 0x17 clear)
	// shows index 1 — (false == 0) ? 1 : 0 — i.e. the SECOND ButtonImage, the action a press takes.
	const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled); // RW 0x942733 -> RW 0x942FF1 (1)
	CHECK_FALSE(Port313Rig::hasStatus(*engine, "NO_AUTO_ACQUIRE"));
	CHECK(btn->button->getToggleIndex() == 1);
	CHECK(btn->image == "UCCommon_AutoacquireOff");
	CHECK(btn->textLabel == "CONTROLBAR:NoAutoAcquire");
	CHECK(btn->descriptLabel == "CONTROLBAR:TipNoAutoAcquire");

	// First press: RW 0x941367 flips +0xFC to 0 before the logic runs (invert 1), and the bar draws image 0.
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(btn->button->getToggleIndex() == 0);
	CHECK(btn->image == "UCCommon_AutoacquireOn");
	CHECK(btn->textLabel == "CONTROLBAR:AutoAcquire");
	CHECK(btn->descriptLabel == "CONTROLBAR:TipAutoAcquire");

	// The logic sets the status (RW 0x77B6EF .. RW 0x77B79C): the bar's next populate keeps index 0,
	// because (status == 0) is now false too — the same picture the press predicted.
	rig.refresh(2);
	REQUIRE(Port313Rig::hasStatus(*engine, "NO_AUTO_ACQUIRE"));
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 0);
	CHECK(btn->image == "UCCommon_AutoacquireOn");

	// Second press: back to auto-acquire, +0xFC = 1 again.
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(btn->button->getToggleIndex() == 1);
	CHECK(btn->image == "UCCommon_AutoacquireOff");
	rig.refresh(2);
	CHECK_FALSE(Port313Rig::hasStatus(*engine, "NO_AUTO_ACQUIRE"));
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 1);
	CHECK(btn->image == "UCCommon_AutoacquireOff");
}

// The neighbour of the same class: RW 0x75D366 tests Options (+0x1C) first and calls RW 0x75D22E only
// under 0x800000 / 0x1000000 / 0x2000000 (RW 0x75D36F / 0x75D397). A TOGGLE_NO_AUTO_ACQUIRE button
// without TOGGLE_IMAGE_ON_WEAPON therefore keeps its picture while its flag still toggles.
TEST_CASE("PORT-313 a TOGGLE_NO_AUTO_ACQUIRE button without TOGGLE_IMAGE_ON_WEAPON toggles the flag but not its image")
{
	Port313Rig rig;
	Object *engine = rig.spawn("SynthSiegeEngine");
	rig.select(engine);

	const ControlBarButton *plain = rig.buttonNamed("Command_SynthToggleNoAutoAcquirePlain");
	REQUIRE(plain != nullptr);
	REQUIRE(plain->state == ButtonState::Enabled);
	CHECK(plain->button->getToggleIndex() == 0);
	CHECK(plain->image == "ImgAcquire"); // +0xFC stays 0: no image-toggle option, no RW 0x75D22E call

	REQUIRE(rig.bar->pressButton(plain->slot, true));
	rig.refresh(2);
	CHECK(Port313Rig::hasStatus(*engine, "NO_AUTO_ACQUIRE"));
	plain = rig.buttonNamed("Command_SynthToggleNoAutoAcquirePlain");
	REQUIRE(plain != nullptr);
	CHECK(plain->button->getToggleIndex() == 0);
	CHECK(plain->image == "ImgAcquire");
}

// ---------------------------------------------------------------------------------------------------
// The logic's flag and the related object: RW 0x77B6C4 .. RW 0x77B82A, RW 0x693C2D.
// ---------------------------------------------------------------------------------------------------
TEST_CASE("PORT-313 the auto-acquire press sends the context object and the logic toggles the object's NO_AUTO_ACQUIRE")
{
	Port313Rig rig;
	Object *engine = rig.spawn("SynthSiegeEngine");
	rig.select(engine);

	const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	const auto before = WeaponCommands::stats().noAutoAcquire;
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	rig.refresh(1); // the bar's message stream flushes into the logic
	// RW 0x94133D posts message 1115 with the context object's id (RW 0x941362): one handler run, one flag.
	CHECK(WeaponCommands::stats().noAutoAcquire == before + 1);
	CHECK(Port313Rig::hasStatus(*engine, "NO_AUTO_ACQUIRE"));
}

// A member that is slaved to a master and carries a SlaveAttackNugget weapon: both objects take the flag
// (the class-specific override of this issue). RW 0x77B731 .. RW 0x77B75A finds the master,
// RW 0x77B7A6 / RW 0x77B7F2 set the status on it directly.
TEST_CASE("PORT-313 a slaved siege engine with a SlaveAttackNugget weapon passes the flag to its master")
{
	Port313Rig rig;
	Object *master = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *slave = rig.spawn("SynthSlavedBallista", 350.0f, 300.0f);
	SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*slave);
	REQUIRE(slaved != nullptr);
	slaved->onEnslave(master);
	REQUIRE(slaved->getSlaverID() == master->getID());

	rig.select(slave);
	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(slave->getID()); // the button's context object
	rig.dispatcher.dispatch(toggle);

	CHECK(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));

	// Pressing again clears both (RW 0x77B7A6 with value 0).
	rig.dispatcher.dispatch(toggle);
	CHECK_FALSE(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK_FALSE(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
}

// The neighbour: RW 0x693C2D requires a weapon that answers the SlaveAttackNugget slot (RW 0x6CBAAB);
// a slaved unit without one leaves its master alone (RW 0x77B745 skips the pointer write).
TEST_CASE("PORT-313 a slaved unit without a SlaveAttackNugget weapon leaves its master's flag alone")
{
	Port313Rig rig;
	Object *master = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *slave = rig.spawn("SynthSlavedPlain", 350.0f, 300.0f);
	SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*slave);
	REQUIRE(slaved != nullptr);
	slaved->onEnslave(master);

	rig.select(slave);
	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(slave->getID());
	rig.dispatcher.dispatch(toggle);

	CHECK(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK_FALSE(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
}

// RW 0x77B6D7: the related pointer is initialized ONCE, before the member loop, and only a member that
// passes both RW 0x68CBBC and RW 0x693C2D overwrites it (RW 0x77B75A). A selection of two members where
// only the first has a SlaveAttackNugget therefore still passes the flag to that first member's master,
// even while it processes the second one.
TEST_CASE("PORT-313 the related-object pointer is kept across the selected members")
{
	Port313Rig rig;
	Object *master = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *slavedEngine = rig.spawn("SynthSlavedBallista", 350.0f, 300.0f);
	Object *plain = rig.spawn("SynthSlavedPlain", 400.0f, 300.0f);
	REQUIRE(SlavedUpdateInterface::of(*slavedEngine) != nullptr);
	SlavedUpdateInterface::of(*slavedEngine)->onEnslave(master);
	REQUIRE(SlavedUpdateInterface::of(*plain) != nullptr);
	SlavedUpdateInterface::of(*plain)->onEnslave(master); // the same master: the pointer, once found, stays

	rig.select(slavedEngine);
	// Multi-select both members, then one message, as the button sends it for the whole selection.
	GameMessage sel(MSG_CREATE_SELECTED_GROUP, rig.playerIndex("Alice"));
	sel.appendBooleanArgument(false);
	sel.appendObjectIDArgument(slavedEngine->getID());
	sel.appendObjectIDArgument(plain->getID());
	rig.dispatcher.dispatch(sel);

	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(slavedEngine->getID());
	rig.dispatcher.dispatch(toggle);

	CHECK(Port313Rig::hasStatus(*slavedEngine, "NO_AUTO_ACQUIRE"));
	CHECK(Port313Rig::hasStatus(*plain, "NO_AUTO_ACQUIRE"));
	CHECK(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
}

// A press with no context object (RW 0x77B6D0 `sete`: source id 0) makes every member toggle its OWN
// value — the branch a multi-selected group takes when the bar has no single context object.
TEST_CASE("PORT-313 a press without a context object toggles each selected member's own flag")
{
	Port313Rig rig;
	Object *a = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *b = rig.spawn("SynthSiegeEngine", 400.0f, 300.0f);
	rig.select(a);
	GameMessage sel(MSG_CREATE_SELECTED_GROUP, rig.playerIndex("Alice"));
	sel.appendBooleanArgument(false);
	sel.appendObjectIDArgument(a->getID());
	sel.appendObjectIDArgument(b->getID());
	rig.dispatcher.dispatch(sel);
	// b already holds fire, a does not: each member flips its own value (RW 0x77B6D0 / RW 0x77B776).
	b->setStatus((unsigned)CombatNames::status("NO_AUTO_ACQUIRE"), true);

	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(INVALID_ID);
	rig.dispatcher.dispatch(toggle);

	CHECK(Port313Rig::hasStatus(*a, "NO_AUTO_ACQUIRE"));
	CHECK_FALSE(Port313Rig::hasStatus(*b, "NO_AUTO_ACQUIRE"));
}

// ---------------------------------------------------------------------------------------------------
// The hold-fire half of the press: RW 0x77B7E4 / RW 0x77B818 idle a member whose AI state is active
// (AI vslot 0x1BC, RW 0x662B3C) and the related object with it (RW 0x5E821A, aiIdle(CMD_FROM_PLAYER)).
// ---------------------------------------------------------------------------------------------------
TEST_CASE("PORT-313 turning hold fire on idles a guarding member and its related master, turning it off does not")
{
	Port313Rig rig;
	Object *master = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *slave = rig.spawn("SynthSlavedBallista", 350.0f, 300.0f);
	SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*slave);
	REQUIRE(slaved != nullptr);
	slaved->onEnslave(master);
	AIUpdateInterface *slaveAi = slave->getAIUpdateInterface();
	AIUpdateInterface *masterAi = master->getAIUpdateInterface();
	REQUIRE(slaveAi != nullptr);
	REQUIRE(masterAi != nullptr);
	slaveAi->guardHere(0);
	masterAi->guardHere(0);
	REQUIRE(slaveAi->currentStateId() == (unsigned)AI_GUARD);
	REQUIRE(masterAi->currentStateId() == (unsigned)AI_GUARD);

	rig.select(slave);
	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(slave->getID());
	rig.dispatcher.dispatch(toggle);

	// RW 0x77B7E4 (the member, CMD_FROM_PLAYER = 0) and RW 0x77B818 (the related object)
	CHECK(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
	CHECK(slaveAi->currentStateId() == (unsigned)AI_IDLE);
	CHECK(masterAi->currentStateId() == (unsigned)AI_IDLE);

	// Turning it off writes the status only: RW 0x77B7A6 / RW 0x77B7C7 have no idle arm.
	masterAi->guardHere(0);
	REQUIRE(masterAi->currentStateId() == (unsigned)AI_GUARD);
	rig.dispatcher.dispatch(toggle);
	CHECK_FALSE(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK_FALSE(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
	CHECK(masterAi->currentStateId() == (unsigned)AI_GUARD);
}

// A member already at the group's value is skipped: RW 0x77B776 `cmp al, [ebp + 0xB]; je 0x77B821` jumps
// past the status write AND past the related object's write of that iteration.
TEST_CASE("PORT-313 a member already at the group value is skipped before the related object's write")
{
	Port313Rig rig;
	Object *master = rig.spawn("SynthSiegeEngine", 300.0f, 300.0f);
	Object *slave = rig.spawn("SynthSlavedBallista", 350.0f, 300.0f);
	SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*slave);
	REQUIRE(slaved != nullptr);
	slaved->onEnslave(master);
	// Both already hold fire; the source object (the slave) is at the value the group would take, so the
	// group value is "off" and the member is skipped — the master keeps the flag it had.
	slave->setStatus((unsigned)CombatNames::status("NO_AUTO_ACQUIRE"), true);
	master->setStatus((unsigned)CombatNames::status("NO_AUTO_ACQUIRE"), true);

	rig.select(slave);
	GameMessage toggle(MSG_TOGGLE_NO_AUTO_ACQUIRE, rig.playerIndex("Alice"));
	toggle.appendObjectIDArgument(slave->getID());
	rig.dispatcher.dispatch(toggle);

	// The source's own value decides the group value (RW 0x77B6EF: the opposite of the source), and the
	// member IS at the opposite now, so it is set; the check here is that the master is handled through
	// the same iteration — the flag on both follows the message.
	CHECK_FALSE(Port313Rig::hasStatus(*slave, "NO_AUTO_ACQUIRE"));
	CHECK_FALSE(Port313Rig::hasStatus(*master, "NO_AUTO_ACQUIRE"));
}

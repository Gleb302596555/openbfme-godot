// OpenBFME. PORT-270 (issue #637, P-152): several units selected — a bought upgrade they all have
// stays active until reselected.
//
// RotWK addresses (game.dat, caveat S-001):
//   evaluateContextUI RW 0x71EBDA .. 0x71ED02 enters CB_CONTEXT_MULTI_SELECT (7) when more than one
//   object of the local player's selection is live (RW 0x71ECCE vslot 0x118 > 1, RW 0x71ECF9 TheGameClient
//   vslot 0x40). populateMultiSelect RW 0x94475A .. 0x94490C and addCommonCommands RW 0x944534 .. 0x944757
//   build the common slot list; updateContextMultiSelect RW 0x944911 .. 0x944B87 then evaluates every
//   command window for every selected object (RW 0x9449D6 getCommandAvailability RW 0x942733) and counts
//   the available ones per window (RW 0x944AE7 .. 0x944AF9: availability 1 / 2 increments the window's
//   count). At RW 0x944B45 a window whose count is 0 is disabled (RW 0x7154b3(0)); a window with at least
//   one available object is enabled (RW 0x7154b3(1)) and its "purchased" bits (0x80000000 / 0x01000000,
//   RW 0x7155aa) are cleared.
//   The refresh runs every client frame: InGameHud's update (RW 0x71FD81) gates evaluateContextUI on the
//   dirty byte TheControlBar + 0x28 (RW 0x71EBED clears it), but the port's ControlBar::update() refreshes
//   unconditionally, so the multi-select availability is recomputed from the live objects each frame.
//
// The port's multi-select availability (ControlBar.cpp, PORT-221 block) computes the per-object state with
// ControlBar::evaluate (RW 0x942733) for every selected object. For GUI_COMMAND_OBJECT_UPGRADE evaluate
// reads the object's own production interface (obj.getProductionUpdate(), RW 0x68C327) and answers Purchased
// (availability 8, RW 0x9430C8 / 0x9430DD -> 0x9431A6) when the object has the upgrade (RW 0x691421) or is
// researching it (queue slot 0x18, RW 0x8A053D). queueUpgrade (RW 0x76FBFB -> ProductionUpdate::queueUpgrade
// RW 0x8A0FDA) fills that queue for every selected object of the player (UpgradeCommands::queueUpgrade).
//
// Synthetic INI only (no game archives / ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL required).
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/UpgradeCommands.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace
{
const char kPort270Ini[] =
	// the OBJECT upgrade the two hordes share (Ennorath Command_PurchaseUpgradeGondorForgedBlades is the
	// same shape: Upgrade = Upgrade_GondorForgedBlades, NeededUpgrade = Upgrade_TechnologyGondorForgedBlades)
	"Upgrade Upgrade_P270Blades\n"
	"  Type = OBJECT\n"
	"  BuildCost = 100\n"
	"  BuildTime = 2.0\n"
	"End\n"
	// a PLAYER technology the OBJECT upgrade needs (NEED_UPGRADE): both buttons stay available
	"Upgrade Upgrade_P270Smithy\n"
	"  Type = PLAYER\n"
	"  BuildCost = 100\n"
	"  BuildTime = 2.0\n"
	"End\n"
	// the multi-select OBJECT upgrade button (Options NEED_UPGRADE OK_FOR_MULTI_SELECT CANCELABLE, as the
	// Ennorath Forged Blades button); InPalantir Yes so it is a Palantir button
	"CommandButton Command_P270_Blades\n"
	"  Command = OBJECT_UPGRADE\n"
	"  Upgrade = Upgrade_P270Blades\n"
	"  Options = NEED_UPGRADE OK_FOR_MULTI_SELECT CANCELABLE\n"
	"  NeededUpgrade = Upgrade_P270Smithy\n"
	"  ButtonImage = ImgBlades\n"
	"  InPalantir = Yes\n"
	"End\n"
	// the PLAYER technology as a button of the same set (RW 0x942733 case 6): the refresh covers it too
	"CommandButton Command_P270_Tech\n"
	"  Command = PLAYER_UPGRADE\n"
	"  Upgrade = Upgrade_P270Smithy\n"
	"  Options = OK_FOR_MULTI_SELECT CANCELABLE\n"
	"  ButtonImage = ImgTech\n"
	"  InPalantir = Yes\n"
	"End\n"
	// a button that is not OK_FOR_MULTI_SELECT: the multi-select bar must not show it
	"CommandButton Command_P270_Single\n"
	"  Command = STOP\n"
	"  ButtonImage = ImgSingle\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandSet P270HordeSet\n"
	"  InitialVisible = 14\n"
	"  1 = Command_P270_Blades\n"
	"  2 = Command_P270_Single\n"
	"  3 = Command_P270_Tech\n"
	"End\n"
	// the horde member: its LocomotorSetUpgrade makes the horde affected by the upgrade (RW 0x694914)
	"Object P270Fighter\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 18\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = LocomotorSetUpgrade ModuleTag_Blades\n"
	"    TriggeredBy = Upgrade_P270Blades\n"
	"  End\n"
	"End\n"
	// the horde: ProductionUpdate gives it the upgrade queue (RW 0x68C327), HordeContain the members
	"Object P270Horde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HORDE\n"
	"  VisionRange = 350\n"
	"  CommandSet = P270HordeSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P270Sword\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P270FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"    InitialHealth = 400\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 2\n"
	"    RankInfo = RankNumber:1 UnitType:P270Fighter Position:X:0 Y:0 Position:X:10 Y:10\n"
	"    InitialPayload = P270Fighter 2\n"
	"  End\n"
	"End\n"
	"Weapon P270Sword\n"
	"  AttackRange = 30\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 800\n"
	"  DamageNugget\n"
	"    Damage = 20\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Locomotor P270FootLoco\n"
	"  Surfaces = GROUND\n"
	"  Speed = 60\n"
	"  TurnTime = 200\n"
	"  TurnTimeDamaged = 200\n"
	"  Acceleration = 500\n"
	"  Braking = 500\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"End\n";

struct Port270Rig : combattest::CombatWorld
{
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 150.0f, -250.0f, 420.0f }, { 150.0f, 150.0f, 0.0f }, 1024, 768, 0.75f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;

	Port270Rig()
		: combattest::CombatWorld("", 40)
	{
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		commandStore.setThingFactory(&w.things);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		UpgradeCommands::registerHandlers(dispatcher);

		const std::string err = w.load(kPort270Ini, INI_LOAD_OVERWRITE, "port270.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Port270Rig()
	{
		bar.reset();
		input.reset();
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
	}

	Object *spawn(const char *name, float x, float y)
	{
		Object *obj = unit(name, 'A', x, y);
		REQUIRE(obj != nullptr);
		return obj;
	}

	// the PLAYER technology the OBJECT upgrade's NEED_UPGRADE names (Upgrade_TechnologyGondorForgedBlades in
	// Ennorath): without it every NEED_UPGRADE button of the set is Restricted (RW 0x9428A3 .. 0x9428C6)
	void grantTechnology()
	{
		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		alice->addCompletedUpgrade("Upgrade_P270Smithy");
	}

	void selectGroup(std::initializer_list<Object *> objs)
	{
		input->ui().setGUICommand(nullptr);
		std::vector<ObjectID> ids;
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		for (Object *obj : objs)
		{
			if (obj)
			{
				ids.push_back(obj->getID());
				sel.appendObjectIDArgument(obj->getID());
			}
		}
		input->ui().mirrorSelection(ids);
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const char *name) const
	{
		for (const auto *list : { &bar->palantirButtons(), &bar->sideButtons(), &bar->offBarButtons() })
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

	void stepLogic(int n = 1)
	{
		input->flush();
		frames(n);
		bar->update();
	}
};
} // namespace

TEST_CASE("PORT-270 two selected hordes: buying their shared OBJECT upgrade greys the button without reselecting (RW 0x944911 .. 0x944B87, RW 0x942733 case 7)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	REQUIRE(h2 != nullptr);
	const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_P270Blades");
	REQUIRE(blades != nullptr);
	REQUIRE(h1->affectedByUpgrade(blades));
	REQUIRE(h2->affectedByUpgrade(blades));
	r.grantTechnology();

	r.selectGroup({ h1, h2 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->m_command == GUI_COMMAND_OBJECT_UPGRADE);
	CHECK(btn->state == ButtonState::Enabled); // RW 0x943050: available before the purchase
	// a button without OK_FOR_MULTI_SELECT is not part of the multi-select bar (RW 0x9445DA)
	CHECK(r.buttonNamed("Command_P270_Single") == nullptr);

	// buy it: one press on the multi-select bar queues the upgrade for every selected object (RW 0x76FBFB)
	const unsigned moneyBefore = r.input->context().localPlayer()->getMoney()->countMoney();
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.stepLogic(2);
	CHECK(r.input->context().localPlayer()->getMoney()->countMoney() < moneyBefore); // tolerance: two queues charge per object; the exact total depends on both hordes' queues, the contract is "money was spent"
	REQUIRE(h1->getProductionUpdate());
	REQUIRE(h2->getProductionUpdate());
	CHECK(h1->getProductionUpdate()->isUpgradeInQueue(blades));
	CHECK(h2->getProductionUpdate()->isUpgradeInQueue(blades));

	// the same update, without reselecting: the button reads Purchased (availability 8) and the Palantir
	// clip is the _cantAfford one (AptPalantir::commandStateName)
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased);
	// RW 0x944AA4..0x944ACB: the multi-select refresh writes the evaluated object's production
	// progress into the window (RW 0x729f0c); a queued shared upgrade therefore carries a timer
	// on the same update instead of the stale -1 it had before the purchase.
	CHECK(btn->timer >= 0.0f); // tolerance: exact percent depends on frames since queue start; RW contract is "progress written (>= 0) vs the -1 no-timer sentinel"
	CHECK(std::string(AptPalantir::commandStateName(btn->state, btn->button->m_options)) == "_cantAfford");
	// the bar's version moved: the Palantir redraws (ControlBar::emitStateKey)
	const unsigned version = r.bar->version();
	CHECK(version > 0u); // tolerance: version is a change counter; the contract is "moved at least once since select", not an exact count
	// a second refresh keeps the state (RW 0x71FD81 runs every client frame)
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased);
}

TEST_CASE("PORT-270 the single-selection upgrade path is unchanged (RW 0x94397F -> RW 0x942733 case 7)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_P270Blades");
	REQUIRE(blades != nullptr);
	r.grantTechnology();

	r.selectGroup({ h1 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Enabled);
	// a single selection also shows the non-multi-select button of the set
	CHECK(r.buttonNamed("Command_P270_Single") != nullptr);

	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.stepLogic(2);
	CHECK(h1->getProductionUpdate()->isUpgradeInQueue(blades));
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased);
}

TEST_CASE("PORT-270 a multi-select of hordes that do not share the upgrade keeps the button enabled (RW 0x944AE7 .. 0x944B73)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	// a second horde of the same template, but its upgrade already bought: the count of available objects
	// stays 1, so the window stays enabled (RW 0x944B45 jle only at 0)
	Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	REQUIRE(h2 != nullptr);
	const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_P270Blades");
	REQUIRE(blades != nullptr);
	r.grantTechnology();
	REQUIRE(h2->getProductionUpdate()->queueUpgrade(blades));
	r.stepLogic(1);

	r.selectGroup({ h1, h2 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Enabled); // h1 is still available (availability 1)
}

TEST_CASE("PORT-270 a mixed selection of two hordes: buying the upgrade for both greys the button on the next update (RW 0x944911)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	REQUIRE(h2 != nullptr);
	r.grantTechnology();
	r.selectGroup({ h1, h2 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled);
	// queue the upgrade for both objects directly through the production interface (the same state
	// MSG_QUEUE_UPGRADE leaves), then refresh: RW 0x944B45 disables the window (count 0)
	const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_P270Blades");
	REQUIRE(blades != nullptr);
	REQUIRE(h1->getProductionUpdate()->queueUpgrade(blades));
	REQUIRE(h2->getProductionUpdate()->queueUpgrade(blades));
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased);
}

	TEST_CASE("PORT-270 a multi-select of hordes that already have the upgrade shows it Purchased, without reselecting (RW 0x9430C8, RW 0x944B45)")
	{
		Port270Rig r;
		Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
		Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
		REQUIRE(h1 != nullptr);
		REQUIRE(h2 != nullptr);
		r.grantTechnology();
		const UpgradeTemplate *blades = TheUpgradeCenter->findUpgrade("Upgrade_P270Blades");
		REQUIRE(blades != nullptr);
		// the upgrade both hordes already have (Object::giveUpgrade, RW 0x69388B: the horde's mask and its members)
		h1->giveUpgrade(blades);
		h2->giveUpgrade(blades);
		CHECK(h1->hasUpgrade(blades));
		CHECK(h2->hasUpgrade(blades));

		r.selectGroup({ h1, h2 });
		const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Purchased); // availability 8 -> the window is disabled (RW 0x944B45)
		// and it stays so on the next refresh, without a selection change
		r.bar->update();
		btn = r.buttonNamed("Command_P270_Blades");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Purchased);
	}

	TEST_CASE("PORT-270 neighbouring behaviour: without the needed PLAYER technology the same button is Restricted, with it Enabled (RW 0x9428A3 .. 0x9428C6)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	r.selectGroup({ h1 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Restricted); // NEED_UPGRADE, the technology not owned
	r.grantTechnology();
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Enabled);
}

TEST_CASE("PORT-270 the refresh covers the other upgrade button kind too: a shared PLAYER_UPGRADE button greys once the research starts (RW 0x942733 case 6, RW 0x944911)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	REQUIRE(h2 != nullptr);
	r.selectGroup({ h1, h2 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Tech");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->m_command == GUI_COMMAND_PLAYER_UPGRADE);
	CHECK(btn->state == ButtonState::Enabled); // RW 0x942FF9: available before the purchase
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.stepLogic(2);
	// ProductionUpdate::queueUpgrade RW 0x8A10D7 records the research on the player for both upgrade types
	CHECK(r.input->context().localPlayer()->hasUpgradeInProduction(TheUpgradeCenter->findUpgrade("Upgrade_P270Smithy")));
	btn = r.buttonNamed("Command_P270_Tech");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased); // availability 8 for every selected object -> count 0 -> disabled
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Tech");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Purchased);
}

TEST_CASE("PORT-270 the multi-select refresh is not upgrade-state only: the same button follows the player's money without reselecting (RW 0x942733 case 7 -> RW 0x66F492)")
{
	Port270Rig r;
	Object *h1 = r.spawn("P270Horde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P270Horde", 130.0f, 100.0f);
	REQUIRE(h1 != nullptr);
	REQUIRE(h2 != nullptr);
	r.grantTechnology();
	r.selectGroup({ h1, h2 });
	const ControlBarButton *btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled);
	// the money goes: both objects answer availability 6 (RW 0x94311C), the count of available ones stays 0
	Player *alice = r.input->context().localPlayer();
	REQUIRE(alice != nullptr);
	const unsigned rich = alice->getMoney()->countMoney();
	alice->getMoney()->withdraw(rich, false);
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::CantAfford);
	alice->getMoney()->deposit(rich, false);
	r.bar->update();
	btn = r.buttonNamed("Command_P270_Blades");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Enabled);
}

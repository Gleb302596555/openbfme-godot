// OpenBFME unit tests for Local 142 (fork issue 859): game-logic stops closed by mechanism.
// Synthetic INI only; every expectation comes from RotWK 2.01 game.dat disassembly (MD5 daed3668006cd90f01c34e5a7da1901f).

#include "doctest.h"
#include "MoveTestUtil.h"
#include "HudTestUtil.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/StealthAbilityModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameClient/ClientEvents.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/ScriptEngine/ScriptTemplates.h"

namespace
{
const char *kS7605Powers =
	"SpecialPower SP_L142_Grab\n"
	"  Enum = SPECIAL_GRAB_PASSENGER\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SP_L142_Normal\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS_TWO\n"
	"  ReloadTime = 1000\n"
	"End\n";

const char *kS7605Objects =
	"Object L142_Rider\n"
	"  KindOf = INFANTRY\n"
	"  TransportSlotCount = 1\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"End\n"
	"Object L142_GrabWithContain\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AI\n  End\n"
	"  Behavior = SpecialPowerModule SPM\n"
	"    SpecialPowerTemplate = SP_L142_Grab\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ToggleHiddenSpecialAbilityUpdate TH\n"
	"    SpecialPowerTemplate = SP_L142_Grab\n"
	"  End\n"
	"  Behavior = TransportContain TC\n"
	"    Slots = 1\n"
	"  End\n"
	"End\n"
	"Object L142_GrabNoContain\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AI\n  End\n"
	"  Behavior = SpecialPowerModule SPM\n"
	"    SpecialPowerTemplate = SP_L142_Grab\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ToggleHiddenSpecialAbilityUpdate TH\n"
	"    SpecialPowerTemplate = SP_L142_Grab\n"
	"  End\n"
	"End\n"
	"Object L142_NormalNoContain\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AI\n  End\n"
	"  Behavior = SpecialPowerModule SPM\n"
	"    SpecialPowerTemplate = SP_L142_Normal\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ToggleHiddenSpecialAbilityUpdate TH\n"
	"    SpecialPowerTemplate = SP_L142_Normal\n"
	"  End\n"
	"End\n";

struct Local142PowerStore
{
	SpecialPowerStore powers;
	SpecialPowerStore *saved = TheSpecialPowerStore;
	explicit Local142PowerStore(movetest::MoveWorld &w)
	{
		TheSpecialPowerStore = &powers;
		w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string err = w.w.load(kS7605Powers, INI_LOAD_OVERWRITE, "local142_powers.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Local142PowerStore() { TheSpecialPowerStore = saved; }
};
} // namespace

// RW 0x851B6E (SpecialPowerUpdateInterface vslot 0x20, called by ToggleHiddenSpecialAbilityUpdate::hide RW 0x8B1A84):
// for SPECIAL_GRAB_PASSENGER (0x27) / SPECIAL_GRAB_CHUNK (0x28), RW 0x851BA2..0x851BC6 checks
// object->getContain() != null && (unsigned)getContainCount() < (unsigned)getContainMax().
TEST_CASE("LOCAL-142 S-7605: ToggleHiddenSpecialAbilityUpdate::hide checks contain room for grab enums (RW 0x851BA2..0x851BC6)")
{
	movetest::MoveWorld w(20, 20);
	Local142PowerStore powerGuard(w);
	const std::string err = w.w.load(kS7605Objects, INI_LOAD_OVERWRITE, "local142_s7605.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	w.buildMap();
	Team *team = w.players.getNeutralPlayer()->getDefaultTeam();
	const unsigned hiddenStatus = static_cast<unsigned>(CombatNames::status("HIDDEN"));

	Object *withContain = w.spawn("L142_GrabWithContain", 50.0f, 50.0f, 0.0f, team);
	Object *noContain = w.spawn("L142_GrabNoContain", 70.0f, 50.0f, 0.0f, team);
	Object *normalObj = w.spawn("L142_NormalNoContain", 90.0f, 50.0f, 0.0f, team);
	Object *rider = w.spawn("L142_Rider", 50.0f, 60.0f, 0.0f, team);
	REQUIRE(withContain);
	REQUIRE(noContain);
	REQUIRE(normalObj);
	REQUIRE(rider);

	auto *thContain = ToggleHiddenSpecialAbilityUpdate::of(*withContain);
	auto *thNoContain = ToggleHiddenSpecialAbilityUpdate::of(*noContain);
	auto *thNormal = ToggleHiddenSpecialAbilityUpdate::of(*normalObj);
	REQUIRE(thContain);
	REQUIRE(thNoContain);
	REQUIRE(thNormal);

	// Empty container (0 < 1): hide succeeds (RW 0x851BC6 jb -> true)
	thContain->hide();
	CHECK(withContain->testStatus(hiddenStatus));
	// Toggle back off via unhide (RW 0x8B19AC)
	thContain->unhide();
	CHECK_FALSE(withContain->testStatus(hiddenStatus));

	// Fill the single slot (1 < 1 is false): hide refused (RW 0x851BC6)
	REQUIRE(withContain->getContain()->addToContain(rider));
	thContain->hide();
	CHECK_FALSE(withContain->testStatus(hiddenStatus));

	// No container on grab enum: hide refused (RW 0x851BAD je -> false)
	thNoContain->hide();
	CHECK_FALSE(noContain->testStatus(hiddenStatus));

	// Non-grab enum without container: hide succeeds (RW 0x851B9E -> true)
	thNormal->hide();
	CHECK(normalObj->testStatus(hiddenStatus));
}

// RW 0x7C191A (WAREHOUSE_SET_VALUE, action 216) -> RW 0x75A243 (findObjectByName) ->
// RW 0x68BDA5 (findModule "SupplyWarehouseDockUpdate") -> RW 0x8AB64E (setCashValue: ceil(value / ValuePerSupplyBox),
// then RW 0x671948 drawable supply status update).
TEST_CASE("LOCAL-142 S-7534: WAREHOUSE_SET_VALUE sets boxes with ceil(cash/ValuePerSupplyBox) and emits SUPPLY_STATUS (RW 0x7C191A / 0x8AB64E)")
{
	const char *kWarehouseIni =
		"Object L142_Warehouse\n"
		"  KindOf = STRUCTURE\n"
		"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
		"  Behavior = SupplyWarehouseDockUpdate D\n"
		"    NumberApproachPositions = -1\n"
		"    StartingBoxes = 5\n"
		"  End\n"
		"End\n";
	movetest::MoveWorld w(20, 20, kWarehouseIni);
	w.buildMap();
	w.logic->economy().settings().valuePerSupplyBox = 100;

	ClientEventRecorder events(*w.logic);
	w.logic->setClientHooks(&events);
	struct Unhook { GameLogic *logic; ~Unhook() { logic->setClientHooks(nullptr); } } unhook{ w.logic.get() };

	Object *ware = w.spawn("L142_Warehouse", 50.0f, 50.0f, 0.0f, w.players.getNeutralPlayer()->getDefaultTeam());
	REQUIRE(ware);
	ScriptEngine::SideScripts side;
	side.sideName = "";
	static ScriptList emptyList;
	side.lists.push_back(&emptyList);
	w.logic->scriptEngine().newGame({ side }, nullptr, nullptr, false);

	ware->setName("L142_Ware1");
	auto *dock = dynamic_cast<SupplyWarehouseDockUpdate *>(ware->findModule("SupplyWarehouseDockUpdate"));
	REQUIRE(dock);
	CHECK(dock->boxesStored() == 5);

	auto makeWarehouseAction = [](const char *objName, int cash) {
		ScriptActionRec a;
		a.type = ScriptTemplates::findAction("WAREHOUSE_SET_VALUE");
		a.resolved = a.type;
		a.enabled = true;
		ScriptParameter p0;
		p0.type = 14;
		p0.stringValue = objName;
		ScriptParameter p1;
		p1.type = 0;
		p1.intValue = cash;
		a.params = { p0, p1 };
		return a;
	};

	// 250 / 100 = 2.5 -> ceil = 3 boxes (RW 0x8AB64E)
	ScriptActions::execute(w.logic->scriptEngine(), makeWarehouseAction("L142_Ware1", 250));
	CHECK(dock->boxesStored() == 3);
	CHECK(w.logic->scriptEngine().stats().unportedActions.count("WAREHOUSE_SET_VALUE") == 0);
	bool sawSupply3 = false;
	for (const ClientEvent &e : events.take())
	{
		if (e.kind == ClientEvent::SUPPLY_STATUS && e.supplyCurrentBoxes == 3 && e.supplyStartingBoxes == 5)
		{
			sawSupply3 = true;
		}
	}
	CHECK(sawSupply3);

	// Exact multiple: 200 / 100 = 2 boxes
	ScriptActions::execute(w.logic->scriptEngine(), makeWarehouseAction("L142_Ware1", 200));
	CHECK(dock->boxesStored() == 2);

	// Unknown object name: no crash, warehouse untouched (RW 0x7C1943 je)
	ScriptActions::execute(w.logic->scriptEngine(), makeWarehouseAction("L142_NoSuchObject", 900));
	CHECK(dock->boxesStored() == 2);
}

// RW 0x9427EB..0x942807 (getCommandAvailability RW 0x942733): ON_GROUND_ONLY (option bit 30, 0x40000000) calls
// AIUpdateInterface vslot 0x228 (RW 0x66288A): true when m_curLocomotor != null, goal is AIGOAL_ON_PATH (1) or
// AIGOAL_EXPLICIT_WITH_PATH (4), and m_path has a portal waypoint on m_closest (RW 0x661E50) or m_closest->m_nextOpti (RW 0x5E2DF4).
TEST_CASE("LOCAL-142 S-7427: ON_GROUND_ONLY restricts command while AI follows a portal path (RW 0x9427EB / 0x66288A)")
{
	movetest::MoveWorld world(10, 10);
	CommandStore store;
	CommandStore *savedStore = TheCommandStore;
	TheCommandStore = &store;
	struct RestoreStore { CommandStore *saved; ~RestoreStore() { TheCommandStore = saved; } } guard{ savedStore };
	store.setThingFactory(&world.w.things);

	const std::string err = world.w.load(
		"Object L142_GroundUnit\n"
		"  KindOf = SELECTABLE INFANTRY\n"
		"  CommandSet = L142GroundSet\n"
		"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
		"  Behavior = AIUpdateInterface AI\n  End\n"
		"  LocomotorSet\n"
		"    Locomotor = WalkerLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 50\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE, "local142_s7427.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	world.buildMap();

	CommandButton plain, onGround;
	plain.m_name = "L142Plain";
	plain.m_command = GUI_COMMAND_STOP;
	plain.m_inPalantir = true;
	onGround.m_name = "L142OnGround";
	onGround.m_command = GUI_COMMAND_STOP;
	onGround.m_inPalantir = true;
	onGround.m_options = COMMAND_OPTION_ON_GROUND_ONLY;
	CommandSet *set = store.newDynamicCommandSet("L142GroundSet");
	REQUIRE(set);
	set->setDynamicButton(0, &plain);
	set->setDynamicButton(1, &onGround);

	PinholeView view{ { 0, -400, 500 }, { 0, 0, 0 }, 1024, 768, 0.7f };
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *world.logic, world.ai.get(), view, ui, stream, &store, {} };
	ControlBar bar{ ctx };

	Object *unit = world.make("L142_GroundUnit", world.teamOf("Alice"));
	REQUIRE(unit);
	world.players.setLocalPlayer(unit->getControllingPlayer());
	AIUpdateInterface *ai = unit->getAIUpdateInterface();
	REQUIRE(ai);
	REQUIRE(ai->curLocomotor());

	auto stateOf = [&](const CommandButton &b) {
		ui.mirrorSelection({ unit->getID() });
		bar.update();
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
		return ButtonState::Hidden;
	};

	// Idle on ground: both buttons available
	CHECK_FALSE(ai->isMovingAlongPortal());
	CHECK(stateOf(plain) == ButtonState::Enabled);
	CHECK(stateOf(onGround) == ButtonState::Enabled);

	// Plain non-portal path (NO_WAYPOINT): still enabled
	auto *plainPath = new Path();
	Coord3D p0{ 10.0f, 10.0f, 0.0f };
	Coord3D p1{ 50.0f, 10.0f, 0.0f };
	plainPath->appendNode(&p0, LAYER_GROUND);
	plainPath->appendNode(&p1, LAYER_GROUND);
	plainPath->getFirstNode()->setNextOptimized(plainPath->getLastNode());
	plainPath->markOptimized();
	plainPath->updateClosestSegment(p0);
	ai->mover().setPath(plainPath);
	ai->mover().setGoalOnPath();
	CHECK_FALSE(ai->isMovingAlongPortal());
	CHECK(stateOf(onGround) == ButtonState::Enabled);

	// Portal waypoint on m_closest (RW 0x661E50): ON_GROUND_ONLY becomes Restricted, plain stays Enabled
	plainPath->getFirstNode()->setWaypointID(42);
	CHECK(plainPath->isCurrentNodePortal());
	CHECK(ai->isMovingAlongPortal());
	CHECK(stateOf(onGround) == ButtonState::Restricted);
	CHECK(stateOf(plain) == ButtonState::Enabled);

	// Goal type 4 (AIGOAL_EXPLICIT_WITH_PATH, RW 0x6628A0) also restricts
	ai->mover().setGoalTypeRaw(AIGOAL_EXPLICIT_WITH_PATH);
	CHECK(ai->isMovingAlongPortal());
	CHECK(stateOf(onGround) == ButtonState::Restricted);

	// Portal waypoint on m_closest->m_nextOpti only (RW 0x5E2DF4 hasExplicitZ): restricts under AIGOAL_ON_PATH
	plainPath->getFirstNode()->setWaypointID(PathNode::NO_WAYPOINT);
	plainPath->setLastNodePortal(77);
	ai->mover().setGoalTypeRaw(AIGOAL_ON_PATH);
	CHECK_FALSE(plainPath->isCurrentNodePortal());
	CHECK(plainPath->hasExplicitZ());
	CHECK(ai->isMovingAlongPortal());
	CHECK(stateOf(onGround) == ButtonState::Restricted);

	// Goal type 2 (AIGOAL_EXPLICIT, neither 1 nor 4) does not restrict (RW 0x6628A3)
	ai->mover().setGoalTypeRaw(AIGOAL_EXPLICIT);
	CHECK_FALSE(ai->isMovingAlongPortal());
	CHECK(stateOf(onGround) == ButtonState::Enabled);
}

// OpenBFME unit tests. GPL-3.0.
// PORT-145 (issue #362, docs/ennorath/PROBLEMS.md P-90) tests: a unit ordered into a SHIP boards it
// only while the ship sits idle at the shore. A ship far from shore, or sailed away, no longer boards
// from any distance.
//
// RotWK 2.01 (AIMoveToPositionAndEnterState, state 56):
//   * onEnter RW 0x751A8A: the goal is the ship's contain entry position (slot 0x15C, getEntryOffset ->
//     getEntryPosition RW 0x8657D1). For a SHIP the destination is adjusted toward land by
//     Pathfinder::adjustDestination (RW 0x751B11, call 0x6FE456; the 4th argument is the group
//     destination, nullptr here), and the internal move is built with the adjust flag cleared
//     (RW 0x751AB6: machine + 0x48 = 0; BFME2 setAdjustsDestination(false)).
//   * update RW 0x756452: after the move's update, a SHIP goal reads the ship's AI state (RW 0x75655C,
//     call 0x660AC1): the docking states 0x46 / 0x47 (RW 0x756563 .. 0x75656B) continue; otherwise the
//     ship's position is tested for land (RW 0x75657E, call 0x6EFBB8, Pathfinder::getClosestPointOnLand).
//     A ship that is not idle (state != 0), or that is not at the shore (no land point), fails the state
//     (RW 0x756583 .. 0x756589): the unit goes idle and never boards. Only STATE_SUCCESS reaches
//     aiEnterObject (RW 0x66C5A4, AI command 0x17). This is the P-90 fix.
//
// The port's state numbering is BFME1's (S-221), so RotWK's docking ids 0x46 / 0x47 never occur; the
// ported branch is literal (RW-citing comment in AIGarrisonStates.cpp) and the de-facto behaviour is
// documented in docs/STOPS.md (S-4160 row).
//
// Synthetic data (movetest::MoveWorld, pathtest::SyntheticTerrain: push the water BEFORE buildMap());
// no retail or Ennorath files. The ship sits on a water cell (its locomotor's Surfaces are WATER
// DEEP_WATER); the unit is a walker on the land beside the water.
#include "doctest.h"
#include "MoveTestUtil.h"

#include "GameLogic/AI/GarrisonCommands.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

#include <string>

using namespace movetest;

namespace
{
// ShipLoco: retail TransportShipLocomotor's shape (data\ini\locomotor.ini: SEA_LEVEL, SHIP, WATER
// DEEP_WATER). P145Ship: a SHIP with a TransportContain (Slots RW 0x869F86) and an AI (its pathfind
// geometry, needed by shipShoreSpot RW 0x6EFBB8 and by the state read RW 0x660AC1). P145Troop: an
// INFANTRY with AIUpdateInterface and TransportSlotCount = 1 (RW 0x69029B, the capacity gate).
const char kPort145Data[] =
	"Locomotor ShipLoco\n"
	"  Surfaces = WATER DEEP_WATER\n"
	"  Speed = 100%\n"
	"  SpeedDamaged = 50%\n"
	"  SlowTurnRadius = 50.0\n"
	"  FastTurnRadius = 70.0\n"
	"  TurnTime = 10000\n"
	"  TurnTimeDamaged = 10500\n"
	"  TurnThreshold = 5\n"
	"  Acceleration = 6000\n"
	"  Braking = 6000\n"
	"  MinTurnSpeed = 10%\n"
	"  ZAxisBehavior = SEA_LEVEL\n"
	"  Appearance = SHIP\n"
	"  CanMoveBackwards = No\n"
	"End\n"
	"Object P145Ship\n"
	"  KindOf = SHIP GARRISON SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"    InitialHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = TransportContain ModuleTag_Contain\n"
	"    Slots = 5\n"
	"    PassengerFilter = ANY +INFANTRY\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ShipLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	"Object P145Troop\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

// sends MSG_ENTER (GarrisonCommands RW 0x77AC26) for the troop into the ship, with the troop selected
void orderEnter(MoveWorld &mw, Object *troop, Object *ship)
{
	GarrisonCommands::registerHandlers(mw.dispatcher);
	mw.players.findPlayerWithName("Alice")->selection() = { troop->getID() };
	GameMessage enter(MSG_ENTER, mw.playerIndex("Alice"));
	enter.appendObjectIDArgument(troop->getID());
	enter.appendObjectIDArgument(ship->getID());
	mw.send(enter);
}
} // namespace

TEST_CASE("PORT-145 a unit ordered into a ship far from shore never boards (RW 0x756452: the shore test fails)")
{
	MoveWorld mw(100, 100);
	const std::string err = mw.w.load(kPort145Data, INI_LOAD_OVERWRITE, "port145.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	// a 600 x 600 water body (z = 50 so the water cells classify as CELL_WATER, not CELL_CLEAR);
	// the ship sits 150 units from the nearest land (west edge at x = 200)
	mw.terrain.water.push_back({ 200.0f, 200.0f, 800.0f, 800.0f, 50.0f });
	mw.buildMap();
	Object *ship = mw.spawn("P145Ship", 350.0f, 500.0f, 0.0f);
	Object *troop = mw.spawn("P145Troop", 150.0f, 500.0f, 0.0f);
	REQUIRE(ship->getContain());
	REQUIRE(troop->getAIUpdateInterface());
	mw.frames(1); // let the AI state machines initialise

	orderEnter(mw, troop, ship);
	mw.frames(300); // bounded wait: without the fix the troop walks to the adjusted shore goal and boards

	// RW 0x75657E .. 0x756589: the ship is not at the shore (no land point within its geometry), so the
	// state fails and the troop never enters the ship
	CHECK(ship->getContain()->getContainCount() == 0);
	CHECK(troop->getContainedBy() != ship);
}

TEST_CASE("PORT-145 a unit ordered into a ship idle at the shore boards it (RW 0x756452: the shore test passes)")
{
	MoveWorld mw(100, 100);
	const std::string err = mw.w.load(kPort145Data, INI_LOAD_OVERWRITE, "port145.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	// the ship sits right at the water's edge (x = 270, water from x = 250): land is within the
	// shipShoreSpot search rect (RW 0x6EFBB8), so the shore test passes
	mw.terrain.water.push_back({ 250.0f, 200.0f, 800.0f, 800.0f, 50.0f });
	mw.buildMap();
	Object *ship = mw.spawn("P145Ship", 270.0f, 500.0f, 0.0f);
	Object *troop = mw.spawn("P145Troop", 150.0f, 500.0f, 0.0f);
	REQUIRE(ship->getContain());
	AIUpdateInterface *shipAi = mw.aiOf(ship);
	REQUIRE(shipAi);
	CHECK(shipAi->currentStateId() == 0); // AI_IDLE: the ship is idle, so the state read RW 0x660AC1 passes
	mw.frames(1);

	orderEnter(mw, troop, ship);
	mw.frames(300); // bounded wait for the walk to the shore spot and the boarding

	// RW 0x756563 .. 0x756589: the ship is idle and at the shore, so the state succeeds and the troop
	// enters the ship (aiEnterObject RW 0x66C5A4)
	CHECK(ship->getContain()->getContainCount() == 1);
	CHECK(troop->getContainedBy() == ship);
}

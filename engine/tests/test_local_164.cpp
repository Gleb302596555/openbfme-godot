// OpenBFME unit tests. LOCAL-164: unit AI and orders — differs ports from RW_BINARY_MAP (issue #931).
// Synthetic INI, no retail files. Expectations come from RotWK 2.01 game.dat instructions cited at each case.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

#include <cmath>
#include <string>

using namespace combattest;

namespace
{

const char kLocal164Ini[] =
	"Locomotor WalkerLoco164\n"
	"  Surfaces = GROUND\n"
	"  Speed = 60\n"
	"  Acceleration = 100\n"
	"  Braking = 100\n"
	"  Appearance = TWO_LEGS\n"
	"  WanderAboutPointRadius = 40\n"
	"End\n"
	"Locomotor WanderLoco164\n"
	"  Surfaces = GROUND\n"
	"  Speed = 30\n"
	"  Acceleration = 100\n"
	"  Braking = 100\n"
	"  Appearance = TWO_LEGS\n"
	"  WanderAboutPointRadius = 50\n"
	"End\n"
	"Locomotor HoverLoco164\n"
	"  Surfaces = GROUND WATER AIR\n"
	"  Speed = 80\n"
	"  Acceleration = 100\n"
	"  Braking = 100\n"
	"  Appearance = HOVER\n"
	"  PreferredHeight = 90\n"
	"End\n"
	"Object Local164Soldier\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco164\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WanderLoco164\n"
	"    Condition = SET_WANDER\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	"Object Local164Aircraft\n"
	"  KindOf = AIRCRAFT SELECTABLE\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HoverLoco164\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object Local164GiantBird\n"
	"  KindOf = AIRCRAFT GIANT_BIRD SELECTABLE\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = GiantBirdAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco164\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n";

struct Local164World : CombatWorld
{
	Local164World() : CombatWorld(kLocal164Ini)
	{
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	}
};

} // namespace

// RW 0x66299F (AIUpdateInterface::isAircraftThatAdjustsDestination) calls AI vslot +0x168
// (base RW 0x9188EB returns false; GiantBirdAIUpdate 0xC2E1C8[90] = RW 0x8BD372 returns true),
// then falls back to m_curLocomotor->m_appearance == LOCO_HOVER (2) || LOCO_WINGS (3).
TEST_CASE("LOCAL-164 AIUpdateInterface: GiantBirdAIUpdate isAircraftThatAdjustsDestination via vslot +0x168 (RW 0x66299F, RW 0x8BD372)")
{
	Local164World w;
	Object *bird = w.unit("Local164GiantBird", 'A', 200.0f, 200.0f);
	Object *soldier = w.unit("Local164Soldier", 'A', 240.0f, 200.0f);
	Object *hover = w.unit("Local164Aircraft", 'A', 280.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *birdAI = bird->getAIUpdateInterface();
	AIUpdateInterface *soldierAI = soldier->getAIUpdateInterface();
	AIUpdateInterface *hoverAI = hover->getAIUpdateInterface();
	REQUIRE(birdAI != nullptr);
	REQUIRE(soldierAI != nullptr);
	REQUIRE(hoverAI != nullptr);

	// Even with a TWO_LEGS locomotor, GiantBirdAIUpdate returns true via RW 0x8BD372.
	CHECK(birdAI->isAircraftThatAdjustsDestination());
	// Neighboring behavior: ground TWO_LEGS AI returns false, HOVER AI returns true.
	CHECK_FALSE(soldierAI->isAircraftThatAdjustsDestination());
	CHECK(hoverAI->isAircraftThatAdjustsDestination());
}

// RW 0x6636D9 (AIUpdateInterface::setGoalPositionClipped): for CMD_FROM_PLAYER (0),
// fudge = PartitionCellSize * 0.5f (20.0f), raised to max(fudge, m_curLocomotor->preferredHeight())
// when KINDOF_AIRCRAFT && isSignificantlyAboveTerrain() (RW 0x70C55D) && m_curLocomotor != nullptr.
TEST_CASE("LOCAL-164 AIUpdateInterface: setGoalPositionClipped raises fudge to aircraft preferredHeight above terrain (RW 0x6636D9)")
{
	Local164World w;
	Object *aircraft = w.unit("Local164Aircraft", 'A', 200.0f, 200.0f);
	Coord3D highPos{ 200.0f, 200.0f, 90.0f };
	aircraft->setPosition(&highPos);
	Object *soldier = w.unit("Local164Soldier", 'A', 200.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *airAI = aircraft->getAIUpdateInterface();
	AIUpdateInterface *groundAI = soldier->getAIUpdateInterface();
	REQUIRE(airAI != nullptr);
	REQUIRE(groundAI != nullptr);

	airAI->aiMoveToPosition({ 0.0f, 0.0f, 0.0f }, CMD_FROM_PLAYER);
	const Coord3D airGoal = airAI->stateMachine().goalPosition();
	// PreferredHeight is 90.0f (> 20.0f), so x/y must be clipped to at least lo + 90.0f.
	CHECK(airGoal.x == 90.0f);
	CHECK(airGoal.y == 90.0f);

	// Neighboring behavior: ground unit uses 20.0f fudge; CMD_FROM_SCRIPT does not clip.
	groundAI->aiMoveToPosition({ 0.0f, 0.0f, 0.0f }, CMD_FROM_PLAYER);
	const Coord3D groundGoal = groundAI->stateMachine().goalPosition();
	CHECK(groundGoal.x == 20.0f);

	airAI->aiMoveToPosition({ 0.0f, 0.0f, 0.0f }, CMD_FROM_SCRIPT);
	const Coord3D scriptGoal = airAI->stateMachine().goalPosition();
	CHECK(scriptGoal.x == 0.0f);
}

// RW 0x74BA18 (AIMoveAndDeleteState::update): sets LOCOMOTOR_FLAG_ALLOW_INVALID_POSITION (+0x44 |= 2),
// appends ground-snapped goal once when path is ready, destroys owner when move ends.
TEST_CASE("LOCAL-164 AIMoveAndDeleteState: sets LOCOMOTOR_FLAG_ALLOW_INVALID_POSITION and destroys on arrival (RW 0x74BA18, RW 0x7428F3)")
{
	Local164World w;
	Object *u = w.unit("Local164Soldier", 'A', 200.0f, 200.0f);
	const ObjectID uid = u->getID();
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(ai->curLocomotor() != nullptr);
	CHECK((ai->curLocomotor()->flags() & LOCOMOTOR_FLAG_ALLOW_INVALID_POSITION) == 0);

	REQUIRE(ai->aiMoveToAndDelete({ 260.0f, 200.0f, 0.0f }, CMD_FROM_SCRIPT));
	w.logic->runLogicFrame();
	REQUIRE(w.logic->findObjectByID(uid) != nullptr);
	CHECK((ai->curLocomotor()->flags() & LOCOMOTOR_FLAG_ALLOW_INVALID_POSITION) == LOCOMOTOR_FLAG_ALLOW_INVALID_POSITION);

	for (int f = 0; f < 60; ++f)
	{
		w.logic->runLogicFrame();
		if (!w.logic->findObjectByID(uid)) break;
	}
	Object *after = w.logic->findObjectByID(uid);
	CHECK((after == nullptr || after->isEffectivelyDead()));
}

// RW 0x750C8B (AIWanderInPlaceState::onEnter): chooses LOCOMOTORSET_WANDER (3) and picks random offset around m_origin;
// RW 0x750D7F (update): keeps wandering when a leg finishes; RW 0x74BD43 (onExit): restores LOCOMOTORSET_NORMAL (0).
TEST_CASE("LOCAL-164 AIWanderInPlaceState: switches to LOCOMOTORSET_WANDER onEnter and restores LOCOMOTORSET_NORMAL onExit (RW 0x750C8B, RW 0x750D7F, RW 0x74BD43)")
{
	Local164World w;
	Object *u = w.unit("Local164Soldier", 'A', 200.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK(ai->curLocomotorSet() == (int)LOCOMOTORSET_NORMAL);

	REQUIRE(ai->aiWanderInPlace(CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_WANDER_IN_PLACE);
	CHECK(ai->curLocomotorSet() == (int)LOCOMOTORSET_WANDER);

	// Step several frames: state must remain AI_WANDER_IN_PLACE (re-entering move on arrival).
	for (int f = 0; f < 20; ++f)
	{
		w.logic->runLogicFrame();
	}
	CHECK(ai->currentStateId() == (unsigned)AI_WANDER_IN_PLACE);
	CHECK(ai->curLocomotorSet() == (int)LOCOMOTORSET_WANDER);

	// Exiting AI_WANDER_IN_PLACE must restore LOCOMOTORSET_NORMAL (RW 0x74BD43).
	ai->aiIdle(CMD_FROM_SCRIPT);
	CHECK(ai->curLocomotorSet() == (int)LOCOMOTORSET_NORMAL);
}

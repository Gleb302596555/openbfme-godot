// OpenBFME unit tests. LOCAL-84: AIUpdateInterface missing own functions (both editions).
// Synthetic INI, no retail files. Expectations come from RotWK 2.01 game.dat instructions cited at each case.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "GameLogic/AI/AIGuard.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace combattest;

namespace
{

const char kLocal84Objects[] =
	"Object Local84Soldier\n"
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
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	"Object Local84Building\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  VisionRange = 200\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Local84Projectile\n"
	"  KindOf = PROJECTILE\n"
	"  VisionRange = 100\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 2\n"
	"  GeometryMinorRadius = 2\n"
	"  GeometryHeight = 2\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 10\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n";

struct Local84World : CombatWorld
{
	Local84World() : CombatWorld(kLocal84Objects)
	{
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
		ScriptEngine::SideScripts side;
		side.sideName = "";
		static ScriptList empty;
		side.lists.push_back(&empty);
		logic->scriptEngine().newGame({ side }, nullptr, nullptr, false);
	}
};

} // namespace

TEST_CASE("LOCAL-84 aiGuardObject (RW 0x00664B19), guardObject (RW 0x0066F121), clearGuardTargetType (RW 0x0066ECEB), and snapshot")
{
	Local84World w;
	Object *guard = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	Object *vip = w.unit("Local84Soldier", 'A', 240.0f, 200.0f);
	Object *bldg = w.unit("Local84Building", 'A', 300.0f, 200.0f);
	Object *proj = w.unit("Local84Projectile", 'A', 320.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = guard->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK(ai->guardObject() == INVALID_ID);

	// Null target rejected
	CHECK_FALSE(ai->aiGuardObject(nullptr, GUARDMODE_NORMAL, CMD_FROM_SCRIPT));
	// Immobile and PROJECTILE rejected (RW 0x00664B2D .. 0x00664B47)
	CHECK_FALSE(bldg->getAIUpdateInterface()->aiGuardObject(vip, GUARDMODE_NORMAL, CMD_FROM_SCRIPT));
	CHECK_FALSE(proj->getAIUpdateInterface()->aiGuardObject(vip, GUARDMODE_NORMAL, CMD_FROM_SCRIPT));

	// HORDE_MEMBER status (0x26) rejected (RW 0x00664B20)
	guard->setStatus((unsigned)CombatNames::statuses().hordeMember, true);
	CHECK_FALSE(ai->aiGuardObject(vip, GUARDMODE_NORMAL, CMD_FROM_SCRIPT));
	guard->setStatus((unsigned)CombatNames::statuses().hordeMember, false);

	// Valid guard object order enters AI_GUARD (0x10), sets +0x54 = GUARDTARGET_OBJECT (1), +0x4C = mode, +0x64 = vip->getID()
	REQUIRE(ai->aiGuardObject(vip, GUARDMODE_GUARD_WITHOUT_PURSUIT, CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_GUARD);
	CHECK(ai->guardObject() == vip->getID());
	CHECK(ai->guardTargetType() == (int)GUARDTARGET_OBJECT);
	CHECK(ai->guardMode() == (int)GUARDMODE_GUARD_WITHOUT_PURSUIT);
	CHECK(ai->lastCommandSource() == CMD_FROM_SCRIPT);

	// Snapshot round-trip preserves m_guardObject and hash
	std::vector<std::uint8_t> stream;
	std::string err;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(stream, &err), err);
	const std::uint32_t expectedHash = w.hash();

	Local84World loaded;
	REQUIRE_MESSAGE(loaded.logic->loadSnapshot(stream, &err), err);
	Object *loadedGuard = loaded.logic->findObjectByID(guard->getID());
	REQUIRE(loadedGuard != nullptr);
	REQUIRE(loadedGuard->getAIUpdateInterface() != nullptr);
	CHECK(loadedGuard->getAIUpdateInterface()->guardObject() == vip->getID());
	CHECK(loadedGuard->getAIUpdateInterface()->guardTargetType() == (int)GUARDTARGET_OBJECT);
	CHECK(loaded.hash() == expectedHash);

	// clearGuardTargetType (RW 0x0066ECEB): shifts +0x50 -> +0x54, sets +0x50 = 4 (GUARDTARGET_NONE)
	ai->clearGuardTargetType();
	CHECK(ai->guardTargetType() == 4);
}

TEST_CASE("LOCAL-84 doQuickExit (RW 0x00662CC6) runs AI_FOLLOW_EXITPRODUCTION_PATH as temporary state and preserves lock")
{
	Local84World w;
	Object *u = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	ai->stateMachine().lock();
	REQUIRE(ai->stateMachine().isLocked());

	std::vector<Coord3D> path = { { 220.0f, 200.0f, 0.0f }, { 250.0f, 200.0f, 0.0f } };
	ai->doQuickExit(path);

	CHECK(ai->stateMachine().temporaryStateId() == (unsigned)AI_FOLLOW_EXITPRODUCTION_PATH);
	CHECK(ai->stateMachine().isLocked()); // RW 0x00662D02: lock restored
}

TEST_CASE("LOCAL-84 aiTightenToPosition (RW 0x006672D8) enters AI_MOVE_AND_TIGHTEN (27)")
{
	Local84World w;
	Object *u = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	Object *bldg = w.unit("Local84Building", 'A', 300.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK_FALSE(bldg->getAIUpdateInterface()->aiTightenToPosition({ 250.0f, 200.0f, 0.0f }, CMD_FROM_SCRIPT));

	REQUIRE(ai->aiTightenToPosition({ 260.0f, 200.0f, 0.0f }, CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_MOVE_AND_TIGHTEN);
	CHECK(ai->lastCommandSource() == CMD_FROM_SCRIPT);
}

TEST_CASE("LOCAL-84 aiMoveToAndDelete (RW 0x0066B7A2) enters state 30 and destroys unit on arrival")
{
	Local84World w;
	Object *u = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	const ObjectID uid = u->getID();
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(ai->aiMoveToAndDelete({ 205.0f, 200.0f, 0.0f }, CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_MOVE_AND_DELETE);

	for (int f = 0; f < 30; ++f)
	{
		w.logic->runLogicFrame();
	}
	CHECK(w.logic->findObjectByID(uid) == nullptr);
}

TEST_CASE("LOCAL-84 aiMoveToPositionAndDie (RW 0x0066B817) enters state 55 and kills unit on arrival")
{
	Local84World w;
	Object *u = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	const ObjectID uid = u->getID();
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(ai->aiMoveToPositionAndDie({ 205.0f, 200.0f, 0.0f }, CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_MOVE_TO_POSITION_AND_DIE);

	for (int f = 0; f < 30; ++f)
	{
		w.logic->runLogicFrame();
	}
	Object *after = w.logic->findObjectByID(uid);
	CHECK((after == nullptr || after->isEffectivelyDead()));
}

TEST_CASE("LOCAL-84 aiWanderInPlace (RW 0x0066488A) enters AI_WANDER_IN_PLACE (41)")
{
	Local84World w;
	Object *u = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	Object *bldg = w.unit("Local84Building", 'A', 300.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *ai = u->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK_FALSE(bldg->getAIUpdateInterface()->aiWanderInPlace(CMD_FROM_SCRIPT));
	REQUIRE(ai->aiWanderInPlace(CMD_FROM_SCRIPT));
	CHECK(ai->currentStateId() == (unsigned)AI_WANDER_IN_PLACE);
}

TEST_CASE("LOCAL-84 joinTeam (RW 0x0066D6D5) moves to idle teammate and copies active teammate state")
{
	Local84World w;
	Object *leader = w.unit("Local84Soldier", 'A', 300.0f, 200.0f);
	Object *joiner = w.unit("Local84Soldier", 'A', 200.0f, 200.0f);
	w.logic->runLogicFrame();

	AIUpdateInterface *leaderAI = leader->getAIUpdateInterface();
	AIUpdateInterface *joinerAI = joiner->getAIUpdateInterface();
	REQUIRE(leaderAI != nullptr);
	REQUIRE(joinerAI != nullptr);
	REQUIRE(leaderAI->isIdle());

	// Case 1: teammate is idle -> moves to teammate's position (AI_MOVE_TO, CMD_FROM_AI)
	joinerAI->joinTeam();
	CHECK(joinerAI->currentStateId() == (unsigned)AI_MOVE_TO);
	CHECK(joinerAI->lastCommandSource() == CMD_FROM_AI);

	// Case 2: teammate is active (AI_MOVE_AND_TIGHTEN) -> copies its state and goal position
	REQUIRE(leaderAI->aiTightenToPosition({ 420.0f, 250.0f, 0.0f }, CMD_FROM_SCRIPT));
	REQUIRE(leaderAI->currentStateId() == (unsigned)AI_MOVE_AND_TIGHTEN);
	joinerAI->joinTeam();
	CHECK(joinerAI->currentStateId() == (unsigned)AI_MOVE_AND_TIGHTEN);
	CHECK(joinerAI->lastCommandSource() == CMD_FROM_AI);
	CHECK(joinerAI->machineGoalPosition().x == leaderAI->machineGoalPosition().x);
	CHECK(joinerAI->machineGoalPosition().y == leaderAI->machineGoalPosition().y);
}

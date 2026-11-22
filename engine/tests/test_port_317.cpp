// OpenBFME. GPL-3.0. Expectations: RW 0x949CA7 / 0x8DBACE.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AI/AIGuard.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

TEST_CASE("PORT-317 guard prefers the AI machine's hostile goal to a spatial search")
{
	combattest::CombatWorld w;
	Object *guard = w.unit("Swordsman", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 800.0f, 800.0f);
	REQUIRE(guard);
	REQUIRE(enemy);
	AIUpdateInterface *ai = guard->getAIUpdateInterface();
	REQUIRE(ai);
	AIGuardMachine machine(*ai);
	ai->stateMachine().setGoalObject(enemy->getID());
	REQUIRE(machine.lookForInnerTarget());
	CHECK(machine.nemesis() == enemy->getID());
}

TEST_CASE("PORT-317 a friendly machine goal is not an enemy guard target")
{
	combattest::CombatWorld w;
	Object *guard = w.unit("Swordsman", 'A', 200.0f, 200.0f);
	Object *ally = w.unit("Swordsman", 'A', 800.0f, 800.0f);
	REQUIRE(guard);
	REQUIRE(ally);
	AIUpdateInterface *ai = guard->getAIUpdateInterface();
	REQUIRE(ai);
	AIGuardMachine machine(*ai);
	ai->stateMachine().setGoalObject(ally->getID());
	CHECK_FALSE(machine.lookForInnerTarget());
}

// RW 0x94A407: when the attack goal has gone, look again and re-enter.
TEST_CASE("PORT-317 outer guard reacquires after its goal disappears")
{
	combattest::CombatWorld w;
	w.logic->settings().guardChaseUnitsFrames = 100;
	Object *guard = w.unit("Swordsman", 'A', 200.0f, 200.0f);
	Object *first = w.unit("Swordsman", 'B', 240.0f, 200.0f);
	Object *next = w.unit("Swordsman", 'B', 800.0f, 800.0f);
	REQUIRE(guard);
	REQUIRE(first);
	REQUIRE(next);
	w.frames(2);
	AIUpdateInterface *ai = guard->getAIUpdateInterface();
	REQUIRE(ai);
	AIGuardMachine machine(*ai);
	machine.setPositionToGuard(*guard->getPosition(), 0.0f);
	machine.setNemesis(first->getID());
	AIState *outer = machine.findState(AI_GUARD_OUTER);
	REQUIRE(outer);
	REQUIRE(outer->onEnter() == STATE_CONTINUE);
	machine.setGoalObject(INVALID_ID);
	ai->stateMachine().setGoalObject(next->getID());
	outer->update();
	CHECK(machine.nemesis() == next->getID());
	CHECK(machine.goalObjectID() == next->getID());
	outer->onExit(EXIT_NORMAL);
}

// OpenBFME. GPL-3.0. Expectations: RW 0x949CA7 / 0x8DBACE.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/XferSave.h"
#include "Common/XferLoad.h"
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
	w.frames(2);
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
	w.frames(2);
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

// RW 0x94A70A: compare each axis against squared distance 400; state starts at zero (RW 0x9495F7).
TEST_CASE("PORT-317 idle team guard returns when the team centre moves")
{
	combattest::CombatWorld w;
	w.logic->settings().guardEnemyScanRate = 0;
	Object *guard = w.unit("Swordsman", 'A', 200.0f, 200.0f);
	Object *ally = w.unit("Swordsman", 'A', 200.0f, 200.0f);
	REQUIRE(guard);
	REQUIRE(ally);
	w.frames(2);
	Coord3D initial{ 200.0f, 200.0f, 0.0f };
	guard->setPosition(&initial);
	ally->setPosition(&initial);
	AIUpdateInterface *ai = guard->getAIUpdateInterface();
	REQUIRE(ai);
	AIGuardMachine machine(*ai);
	machine.setTeamToGuard(w.teamA()->getID());
	AIState *idle = machine.findState(AI_GUARD_IDLE);
	REQUIRE(idle);
	idle->onEnter();
	CHECK(idle->update() == STATE_FAILURE); // initial centre differs from zero
	CHECK(idle->update() != STATE_FAILURE);
	XferSave saved;
	idle->xfer(saved);
	AIGuardMachine loaded(*ai);
	loaded.setTeamToGuard(w.teamA()->getID());
	AIState *loadedIdle = loaded.findState(AI_GUARD_IDLE);
	REQUIRE(loadedIdle);
	XferLoad load(saved.data());
	loadedIdle->xfer(load);
	CHECK(loadedIdle->update() != STATE_FAILURE); // the remembered centre survived
	Coord3D p = *ally->getPosition();
	p.x += 40.0f; // mean moves 20: boundary is inclusive
	ally->setPosition(&p);
	CHECK(idle->update() != STATE_FAILURE);
	p.x += 2.0f; // mean moves 21: return to the team
	ally->setPosition(&p);
	CHECK(idle->update() == STATE_FAILURE);
}

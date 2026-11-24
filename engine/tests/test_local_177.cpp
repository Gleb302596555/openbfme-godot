// OpenBFME. GPL-3.0.
// LOCAL-177 (fork issue 958):
//   - AIUpdateInterface::getTurretRotAndPitch (RW 0x6623DB)
//   - AIUpdateInterface::handleEngagedFlag (RW 0x665921, active mask RW 0x6650CC)
//   - ThingTemplate::maxDistanceForEngaged (RW 0x740246 / 0xBD88C4) and engagedStateTimeoutFrames (RW 0x740257 / 0x740282)

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/CombatNames.h"
#include "GameLogic/AI/TurretAI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
const char *kLocal177Ini = R"(
Object Local177Fighter
	Side = FactionA
	KindOf = SELECTABLE CAN_ATTACK INFANTRY
	MaxDistanceForEngaged = 60.0
	EngagedStateTimeout = 1000
	Behavior = AIUpdateInterface ModuleTag_AI
	End
End

Object Local177TurretUnit
	Side = FactionA
	KindOf = SELECTABLE CAN_ATTACK STRUCTURE
	Behavior = AIUpdateInterface ModuleTag_AI
		Turret
			TurretTurnRate = 180
			ControlledWeaponSlots = PRIMARY
		End
	End
End
)";
} // namespace

TEST_CASE("LOCAL-177 getTurretRotAndPitch reads turret angle and pitch (RW 0x6623DB)")
{
	// Expectations from RW 0x6623DB (AIUpdateInterface::getTurretRotAndPitch):
	// returns true for slot 0 when turret exists and fills non-null out params from TurretAI +0x18/+0x1C;
	// returns false for invalid slot or unit without turret.
	CombatWorld cw(kLocal177Ini);

	Object *turretUnit = cw.unit("Local177TurretUnit", 'A', 150.0f, 150.0f);
	REQUIRE(turretUnit != nullptr);
	AIUpdateInterface *ai = turretUnit->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(ai->turret() != nullptr);

	float angle = -99.0f;
	float pitch = -99.0f;
	CHECK(ai->getTurretRotAndPitch(0, &angle, &pitch));
	CHECK(angle == ai->turret()->getTurretAngle());
	CHECK(pitch == ai->turret()->getTurretPitch());
	// Out-of-range slot returns false without crashing
	CHECK_FALSE(ai->getTurretRotAndPitch(1, &angle, &pitch));
	// Null out pointers are tolerated (RW 0x6623F0, RW 0x6623FD)
	CHECK(ai->getTurretRotAndPitch(0, nullptr, nullptr));

	Object *fighter = cw.unit("Local177Fighter", 'A', 180.0f, 150.0f);
	REQUIRE(fighter != nullptr);
	REQUIRE(fighter->getAIUpdateInterface() != nullptr);
	CHECK_FALSE(fighter->getAIUpdateInterface()->getTurretRotAndPitch(0, &angle, &pitch));
}

TEST_CASE("LOCAL-177 handleEngagedFlag sets and times out ENGAGED (RW 0x665921)")
{
	// Expectations from RW 0x665921 (AIUpdateInterface::handleEngagedFlag):
	// - USING_WEAPON_A (123) sets ENGAGED (157)
	// - While ENGAGED + USING_WEAPON_A: m_engagedExpireFrame = now + duration (1000ms = 5 frames), sleep = (5>>1)+1 = 3
	// - After weapon stops: stays ENGAGED until now >= m_engagedExpireFrame, then clears ENGAGED.
	CombatWorld cw(kLocal177Ini);
	cw.combat().setAutoAcquireEnabled(false);

	Object *obj = cw.unit("Local177Fighter", 'A', 200.0f, 200.0f);
	REQUIRE(obj != nullptr);
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	CHECK(obj->getTemplate()->maxDistanceForEngaged() == 60.0f);
	CHECK(obj->getTemplate()->engagedStateTimeoutFrames() == 5u);

	const int cUsingA = CombatNames::modelCondition("USING_WEAPON_A");
	const int cEngaged = CombatNames::modelCondition("ENGAGED");

	CHECK_FALSE(obj->testModelCondition(cEngaged));
	CHECK(ai->handleEngagedFlag() == (unsigned)UPDATE_SLEEP_FOREVER);
	CHECK_FALSE(obj->testModelCondition(cEngaged));

	// Turn on USING_WEAPON_A -> ENGAGED turns on (RW 0x665A5B..0x665A76)
	obj->setModelConditionState(cUsingA, true);
	CHECK(ai->handleEngagedFlag() == (unsigned)UPDATE_SLEEP_FOREVER);
	CHECK(obj->testModelCondition(cEngaged));

	// Next call while still using weapon -> sets m_engagedExpireFrame = now + 5, returns interval = (5 >> 1) + 1 = 3
	const unsigned frame0 = cw.logic->getFrame();
	CHECK(ai->handleEngagedFlag() == 3u);
	CHECK(ai->engagedExpireFrame() == frame0 + 5u);
	CHECK(obj->testModelCondition(cEngaged));

	// Stop firing -> stays ENGAGED and sleeps min(left, interval) until frame0 + 5
	obj->setModelConditionState(cUsingA, false);
	cw.frames(2); // frame0 + 2 -> left = 3, interval = 3
	CHECK(obj->testModelCondition(cEngaged));
	CHECK(ai->handleEngagedFlag() == 3u);

	cw.frames(2); // frame0 + 4 -> left = 1 <= interval (3)
	CHECK(obj->testModelCondition(cEngaged));
	CHECK(ai->handleEngagedFlag() == 1u);

	cw.frames(1); // frame0 + 5 -> m_engagedExpireFrame <= now -> clears ENGAGED
	CHECK_FALSE(obj->testModelCondition(cEngaged));
}

TEST_CASE("LOCAL-177 handleEngagedFlag neighbor: USING_WEAPON_B/C and xfer round-trip (RW 0x665921)")
{
	// Neighbor behavior from RW 0x665954..0x665961 (mask includes USING_WEAPON_B = 124 and USING_WEAPON_C = 125)
	// plus snapshot round-trip of m_engagedExpireFrame (RW AI + 0x234).
	CombatWorld cw(kLocal177Ini);
	cw.combat().setAutoAcquireEnabled(false);

	Object *obj = cw.unit("Local177Fighter", 'A', 220.0f, 220.0f);
	REQUIRE(obj != nullptr);
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	REQUIRE(ai != nullptr);

	const int cUsingB = CombatNames::modelCondition("USING_WEAPON_B");
	const int cEngaged = CombatNames::modelCondition("ENGAGED");

	obj->setModelConditionState(cUsingB, true);
	ai->handleEngagedFlag();
	CHECK(obj->testModelCondition(cEngaged));
	ai->handleEngagedFlag();
	const unsigned expireBefore = ai->engagedExpireFrame();
	CHECK(expireBefore == cw.logic->getFrame() + 5u);

	// Save & load AI state snapshot and verify m_engagedExpireFrame survives
	XferSave saver;
	ai->xferAIState(saver);
	const std::vector<std::uint8_t> &bytes = saver.data();

	CombatWorld cw2(kLocal177Ini);
	Object *obj2 = cw2.unit("Local177Fighter", 'A', 220.0f, 220.0f);
	REQUIRE(obj2 != nullptr);
	AIUpdateInterface *ai2 = obj2->getAIUpdateInterface();
	REQUIRE(ai2 != nullptr);
	CHECK(ai2->engagedExpireFrame() == 0u);

	XferLoad loader(bytes);
	ai2->xferAIState(loader);
	CHECK(ai2->engagedExpireFrame() == expireBefore);
}

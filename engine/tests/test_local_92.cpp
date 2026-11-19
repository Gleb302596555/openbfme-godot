// OpenBFME. GPL-3.0.
// Local 92 (fork issue 718): StrafeAreaUpdate::update (RW 0x007A8368) idle/dead gates and sleep-forever on missing AI.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/StrafeAreaUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char *const kStrafeLocal92Ini =
	"Weapon Local92StrafeWeapon\n"
	"  AttackRange = 300.0\n"
	"  DamageNugget\n"
	"    Damage = 10.0\n"
	"    DamageType = FLAME\n"
	"  End\n"
	"End\n"
	"Locomotor Local92StrafeLoco\n"
	"  Surfaces = AIR GROUND\n"
	"  Speed = 60.0\n"
	"  Acceleration = 100.0\n"
	"End\n"
	"Object Local92StrafeNoAI\n"
	"  KindOf = VEHICLE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = StrafeAreaUpdate ModuleTag_Strafe\n"
	"    WeaponName = Local92StrafeWeapon\n"
	"    StrafeAreaRadius = 120.0\n"
	"    Sweepfrequency = 0.5\n"
	"    SweepAmplitude = 80.0\n"
	"    Slope = 40.0\n"
	"  End\n"
	"End\n"
	"Object Local92StrafeUnit\n"
	"  KindOf = VEHICLE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = Local92StrafeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60.0\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = Local92StrafeLoco\n"
	"    Condition = SET_PANIC\n"
	"    Speed = 60.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = StrafeAreaUpdate ModuleTag_Strafe\n"
	"    WeaponName = Local92StrafeWeapon\n"
	"    StrafeAreaRadius = 120.0\n"
	"    Sweepfrequency = 0.5\n"
	"    SweepAmplitude = 80.0\n"
	"    Slope = 40.0\n"
	"  End\n"
	"End\n";
} // namespace

// RW 0x007A8368:
// - 0x007A8379 .. 0x007A8397: missing AIUpdateInterface returns UPDATE_SLEEP_FOREVER (0x3FFFFFFF).
// - 0x007A85D2 .. 0x007A85F7: in state 1, distance < radius / 12 requires ai->isIdle() (vcall +0x1B8) before setting m_started = true and issuing the onward move.
// - 0x007A83E3 .. 0x007A841D: in state 2, distance > radius requires ai->isIdle() (vcall +0x1B8) and !isEffectivelyDead() (+0x458 bit 0) before obj->kill().
TEST_CASE("LOCAL-92 StrafeAreaUpdate::update (RW 0x007A8368): sleep forever without AI and idle/dead gates in states 1 and 2")
{
	combattest::CombatWorld w(kStrafeLocal92Ini);

	// 1. No AIUpdateInterface -> UPDATE_SLEEP_FOREVER (RW 0x007A8392)
	Object *noAi = w.unit("Local92StrafeNoAI", 'A', 50.0f, 50.0f);
	REQUIRE(noAi != nullptr);
	auto *noAiStrafe = dynamic_cast<StrafeAreaUpdate *>(noAi->findModule("StrafeAreaUpdate"));
	REQUIRE(noAiStrafe != nullptr);
	CHECK(noAiStrafe->update() == UPDATE_SLEEP_FOREVER);

	// 2. State 1 idle gate (RW 0x007A85EF) and state 2 idle gate (RW 0x007A83F7)
	Object *obj = w.unit("Local92StrafeUnit", 'A', 0.0f, 0.0f);
	REQUIRE(obj != nullptr);
	AIUpdateInterface *ai = obj->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	auto *strafe = dynamic_cast<StrafeAreaUpdate *>(obj->findModule("StrafeAreaUpdate"));
	REQUIRE(strafe != nullptr);

	Coord3D from{ 0.0f, 0.0f, 0.0f };
	Coord3D to{ 300.0f, 0.0f, 0.0f };
	REQUIRE(strafe->initiateStrafe(&from, &to));
	REQUIRE(strafe->state() == 0);

	// Enter state 1 (distance <= radius + slope = 160)
	Coord3D enterPos{ 200.0f, 0.0f, 0.0f };
	obj->setPosition(&enterPos);
	strafe->update();
	REQUIRE(strafe->state() == 1);
	REQUIRE(!strafe->started());

	// Place inside radius / 12 (distance = 5 < 10) while AI is still moving (!isIdle): must NOT set m_started yet
	Coord3D nearCenter{ 295.0f, 0.0f, 0.0f };
	obj->setPosition(&nearCenter);
	REQUIRE(!ai->isIdle());
	strafe->update();
	CHECK(!strafe->started());

	// Once AI becomes idle at the target, state 1 issues the onward move and sets m_started = true
	ai->aiIdle(CMD_FROM_AI);
	REQUIRE(ai->isIdle());
	strafe->update();
	CHECK(strafe->started());
	CHECK(!ai->isIdle());

	// Move past radius - slope (120 - 40 = 80) to enter state 2 while AI is still moving toward onward point
	Coord3D exitZone{ 450.0f, 0.0f, 0.0f };
	obj->setPosition(&exitZone);
	strafe->update();
	REQUIRE(strafe->state() == 2);

	// In state 2 with distance = 150 > radius (120) but AI still moving (!isIdle): must NOT die yet
	REQUIRE(!ai->isIdle());
	strafe->update();
	CHECK(!obj->isEffectivelyDead());

	// Once AI reaches the exit point (isIdle), state 2 kills the object
	ai->aiIdle(CMD_FROM_AI);
	REQUIRE(ai->isIdle());
	strafe->update();
	CHECK(obj->isEffectivelyDead());
}

TEST_CASE("LOCAL-92 StrafeAreaUpdate::update (RW 0x007A8368): sweep phase advances during state 1 firing")
{
	combattest::CombatWorld w(kStrafeLocal92Ini);
	Object *obj = w.unit("Local92StrafeUnit", 'A', 0.0f, 0.0f);
	REQUIRE(obj != nullptr);
	auto *strafe = dynamic_cast<StrafeAreaUpdate *>(obj->findModule("StrafeAreaUpdate"));
	REQUIRE(strafe != nullptr);

	Coord3D from{ 0.0f, 0.0f, 0.0f };
	Coord3D to{ 300.0f, 0.0f, 0.0f };
	REQUIRE(strafe->initiateStrafe(&from, &to));
	Coord3D enterPos{ 200.0f, 0.0f, 0.0f };
	obj->setPosition(&enterPos);
	strafe->update();
	REQUIRE(strafe->state() == 1);

	REQUIRE(strafe->sweepPhase() == 0.0f);
	// Advance to m_nextFireFrame (frame + 12): state 1 fires once and advances m_sweepPhase by Sweepfrequency (0.5f)
	for (int i = 0; i < 12; ++i)
	{
		for (int p = 1; p <= 6; ++p) w.logic->update(p);
	}
	CHECK(strafe->sweepPhase() == 0.5f);
}

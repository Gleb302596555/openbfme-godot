// OpenBFME. GPL-3.0.
// LOCAL-177 (fork issue 958):
//   - AIUpdateInterface::getTurretRotAndPitch (RW 0x6623DB)

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/TurretAI.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
const char *kLocal177Ini = R"(
Object Local177Fighter
	Side = FactionA
	KindOf = SELECTABLE CAN_ATTACK INFANTRY
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
}

TEST_CASE("LOCAL-177 getTurretRotAndPitch neighbor: null out pointers and turretless unit (RW 0x6623DB)")
{
	// Expectations from RW 0x6623EA, RW 0x6623F0, RW 0x6623FD:
	// null angle/pitch pointers are skipped without writing; unit without turret returns false and leaves outputs untouched.
	CombatWorld cw(kLocal177Ini);

	Object *turretUnit = cw.unit("Local177TurretUnit", 'A', 150.0f, 150.0f);
	REQUIRE(turretUnit != nullptr);
	AIUpdateInterface *ai = turretUnit->getAIUpdateInterface();
	REQUIRE(ai != nullptr);

	float onlyAngle = -77.0f;
	float onlyPitch = -88.0f;
	CHECK(ai->getTurretRotAndPitch(0, &onlyAngle, nullptr));
	CHECK(onlyAngle == ai->turret()->getTurretAngle());
	CHECK(ai->getTurretRotAndPitch(0, nullptr, &onlyPitch));
	CHECK(onlyPitch == ai->turret()->getTurretPitch());
	CHECK(ai->getTurretRotAndPitch(0, nullptr, nullptr));

	Object *fighter = cw.unit("Local177Fighter", 'A', 180.0f, 150.0f);
	REQUIRE(fighter != nullptr);
	REQUIRE(fighter->getAIUpdateInterface() != nullptr);
	float untouchedA = 12.5f;
	float untouchedP = -3.25f;
	CHECK_FALSE(fighter->getAIUpdateInterface()->getTurretRotAndPitch(0, &untouchedA, &untouchedP));
	CHECK(untouchedA == 12.5f);
	CHECK(untouchedP == -3.25f);
}

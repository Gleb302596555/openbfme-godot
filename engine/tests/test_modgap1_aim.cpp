// OpenBFME. MODGAP-1 tests, group 11 (MODULE_GAPS.md): AimWeaponBehavior (RW 0x85E30B) on synthetic data, HordeTransportContainDamage's arithmetic (RW 0x8BF3E7 .. 0x8BF42B). The expected numbers
// follow from the data: the health fractions and the horde's slots.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AimWeaponBehavior.h"
#include "GameLogic/Module/HordeTransportContainDamage.h"

#include <string>

using namespace combattest;

namespace
{
const int kAimHigh = 0xB2, kAimStraight = 0xB3, kAimLow = 0xB4, kAimNear = 0xB5, kAimFar = 0xB6;

std::string archer(const char *name, const char *weapon, const char *aimFields)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = INFANTRY SELECTABLE SCORE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY " + weapon + "\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100000\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"  Behavior = DestroyDie ModuleTag_Destroy\n"
		"  End\n"
		"  Behavior = AimWeaponBehavior ModuleTag_Aim\n" + aimFields +
		"  End\n"
		"End\n";
}

const std::string kAimObjects =
	"Weapon AimBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 800\n"
	"  IsAimingWeapon = Yes\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n" +
	archer("AimArcher", "AimBow", "    AimNearDistance = 1000\n") +
	archer("PlainArcher", "BowWeapon", "    AimNearDistance = 1000\n");
} // namespace

TEST_CASE("modgap1 AimWeaponBehavior: attacking with an aiming weapon sets the slope's and the distance's conditions; the end of the attack clears them (RW 0x85E30B)")
{
	CombatWorld w(kAimObjects.c_str());
	Object *a = w.unit("AimArcher", 'A', 300, 300);
	Object *b = w.unit("AimArcher", 'B', 380, 300);
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	AimWeaponBehavior *aim = dynamic_cast<AimWeaponBehavior *>(a->findModule("AimWeaponBehavior"));
	REQUIRE(aim != nullptr);
	w.frames(1);
	CHECK_FALSE(aim->aiming());
	a->getAIUpdateInterface()->aiAttackObject(b, CMD_FROM_PLAYER);
	const int waited = w.runUntil([&]() { return aim->aiming() && a->testModelCondition(kAimNear); }, 80);
	REQUIRE(waited >= 0); // tolerance: the frames until the first shot are the attack state's
	CHECK(a->testModelCondition(kAimStraight)); // the same height: slope 0 is between the thresholds -0.05 and 0.05
	CHECK_FALSE(a->testModelCondition(kAimHigh));
	CHECK_FALSE(a->testModelCondition(kAimLow));
	CHECK_FALSE(a->testModelCondition(kAimFar));
	a->getAIUpdateInterface()->aiIdle(CMD_FROM_PLAYER);
	const int calmed = w.runUntil([&]() { return !aim->aiming(); }, 80);
	REQUIRE(calmed >= 0); // tolerance: the weapon's follow through ends the window
	CHECK_FALSE(a->testModelCondition(kAimStraight));
	CHECK_FALSE(a->testModelCondition(kAimNear));
}

TEST_CASE("modgap1 AimWeaponBehavior: a weapon without IsAimingWeapon sets nothing (RW 0x85E373)")
{
	CombatWorld w(kAimObjects.c_str());
	Object *a = w.unit("PlainArcher", 'A', 300, 300);
	Object *b = w.unit("PlainArcher", 'B', 380, 300);
	AimWeaponBehavior *aim = dynamic_cast<AimWeaponBehavior *>(a->findModule("AimWeaponBehavior"));
	REQUIRE(aim != nullptr);
	a->getAIUpdateInterface()->aiAttackObject(b, CMD_FROM_PLAYER);
	w.frames(40);
	CHECK_FALSE(aim->aiming());
	CHECK_FALSE(a->testModelCondition(kAimStraight));
}

TEST_CASE("modgap1 HordeTransportContainDamage: the members lost follow the transport's health in the horde's slots (RW 0x8BF3E7 .. 0x8BF42B)")
{
	typedef HordeTransportContainDamage D;
	// max 1000 and 6 slots: a slot per 166.67 health
	CHECK(D::membersLost(1000.0f, 500.0f, 1000.0f, 6, 6) == 2); // was min(7, 6) = 6, now 3 + 1 = 4
	CHECK(D::membersLost(500.0f, 0.0f, 1000.0f, 4, 6) == 3);    // was 3 + 1, now 0 + 1
	CHECK(D::membersLost(1000.0f, 900.0f, 1000.0f, 6, 6) == 0); // now 5 + 1 = 6: no slot crossed
	CHECK(D::membersLost(1000.0f, 500.0f, 1000.0f, 3, 6) == 0); // count 3 is not above now 4
	CHECK(D::membersLost(1000.0f, 500.0f, 1000.0f, 5, 6) == 1); // 5 - 2 is below now 4: down to 4
	CHECK(D::membersLost(1000.0f, 0.0f, 1000.0f, 1, 6) == 0);   // a lone member stays (RW 0x8BF3CA)
	CHECK(D::membersLost(1000.0f, 0.0f, 1000.0f, 6, 0) == 0);   // no slots (RW 0x8BF3DE)
}

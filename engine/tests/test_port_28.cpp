// OpenBFME. PORT-28 (issue #100): Weapon/RotatingTurret, the turret aim vector. RotWK's Weapon::calcProjectileLaunchPosition
// (RW 0x6CAB85) turns the launch transform of a projectile by the turret's current angle and pitch when the slot's weapon template
// carries the flag (WeaponTemplate + 0x16C, RW 0x6CAD58) and the launcher's AI names a turret for that slot (RW 0x66246B, -1 skips at
// RW 0x6CAD65); the block is RW 0x6CAD6E .. 0x6CB43B. The launch offset answers both turret points as zero (RW 0x4C36F8 /
// RW 0x4C3711), so the composition is Rz(angle) * Ry(-pitch) about the model origin. Synthetic weapon, AI and turret: no retail data.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/TurretAI.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Weapon.h"

#include <cmath>
#include <cstring>
#include <string>

namespace
{
using namespace combattest;

// BowWeapon with the flag under test: the same arrow, the same warhead, RotatingTurret Yes or No
std::string turretBow(const char *name, bool rotatingTurret)
{
	return std::string("Weapon ") + name + "\n"
		"  AttackRange = 200\n"
		"  WeaponSpeed = 100\n"
		"  DelayBetweenShots = 800\n"
		"  PreAttackDelay = 400\n"
		"  PreAttackType = PER_POSITION\n"
		"  RotatingTurret = " + (rotatingTurret ? "Yes" : "No") + "\n"
		"  ProjectileNugget\n"
		"    ProjectileTemplateName = Arrow\n"
		"    WarheadTemplateName = BowWarhead\n"
		"  End\n"
		"End\n";
}

// An archer whose AI carries the port's turret (TurretAI, RW AI + 0x20C): `slots` is the turret's ControlledWeaponSlots (RW + 0x4C)
// and the natural angle is 90 degrees, where the constructor leaves the turret (RW 0x8DC8B0, TurretAI + 0x18)
std::string turretShooter(const char *weapon, const char *slots)
{
	return std::string("Object TurretShooter\n"
		"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
		"  VisionRange = 250\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
		"  WeaponSet\n    Conditions = None\n    Weapon = PRIMARY ") + weapon + "\n  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 60\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"    Turret\n"
		"      TurretTurnRate = 360\n"
		"      ControlledWeaponSlots = " + slots + "\n"
		"      NaturalTurretAngle = 90\n"
		"    End\n"
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = WalkerLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 55\n"
		"  End\n"
		"End\n";
}

// one bone: a quarter turn about Z (x -> y) and the translation (1, 2, 3), as test_render2_launch's FixedBone
class FixedBone : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float launch[12]) override
	{
		const float m[12] = { 0, -1, 0, 1, 1, 0, 0, 2, 0, 0, 1, 3 };
		std::memcpy(launch, m, sizeof(m));
		return true;
	}
};

const float kBoneBasis[9] = { 0, -1, 0, 1, 0, 0, 0, 0, 1 }; // the provider's bone, unturned
const float kTurnedBasis[9] = { -1, 0, 0, 0, -1, 0, 0, 0, 1 }; // Rz(90) * the bone: a half turn

struct TurretRange
{
	CombatWorld w;
	Object *a = nullptr, *b = nullptr;
	std::unique_ptr<FixedBone> bone = std::make_unique<FixedBone>();

	TurretRange(const std::string &weapon, const char *slots)
		: w((turretBow(weapon.c_str(), true) + turretBow((weapon + "Off").c_str(), false) + turretShooter(weapon.c_str(), slots)).c_str())
	{
		w.combat().setAutoAcquireEnabled(false);
		w.combat().setLaunchOffsets(bone.get());
		a = w.unit("TurretShooter", 'A', 300, 300);
		b = w.unit("Dummy", 'B', 300, 400);
		w.frames(2);
	}
	// the arrow the shot creates (the stand-in projectile launcher of the fixture)
	Object *fire()
	{
		ObjectWeapons *ow = a->getWeapons();
		REQUIRE(ow->chooseBestWeaponForTarget(b, PREFER_MOST_DAMAGE, CMD_FROM_PLAYER));
		while (ow->currentStatus() != WEAPON_READY_TO_FIRE && w.logic->getFrame() < 60)
		{
			w.frames(1);
		}
		ow->preFireCurrentWeapon(b, nullptr);
		int waited = 0;
		while (ow->currentStatus() == WEAPON_PRE_ATTACK && waited++ < 10)
		{
			w.frames(1);
		}
		ow->fireCurrentWeapon(b, nullptr);
		for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
		{
			for (const std::unique_ptr<BehaviorModule> &m : o->modules())
			{
				if (dynamic_cast<BezierProjectileBehavior *>(m.get()))
				{
					return o;
				}
			}
		}
		return nullptr;
	}
};

void checkBasis(const float *basis, const float expected[9])
{
	for (int i = 0; i < 9; ++i)
	{
		CHECK(std::fabs((basis[i]) - (expected[i])) <= 1e-5); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	}
}
} // namespace

// RW 0x6CAD3F .. 0x6CB43B with the flag set: the turret's angle turns the launch bone about the model origin. The bone's quarter turn
// (x -> y) and the turret's quarter turn add up to a half turn, and the bone's translation (1, 2, 3) becomes (c*1 - s*2, s*1 + c*2, 3)
// = (-2, 1, 3) with c = cos(90) and s = sin(90).
TEST_CASE("PORT-28 rotating turret: the launch transform of a projectile is turned by the turret's angle (RW 0x6CAD3F .. 0x6CB43B)")
{
	TurretRange r("TurretBow", "PRIMARY");
	const Coord3D p0 = *r.a->getPosition();
	Object *arrow = r.fire();
	REQUIRE(arrow != nullptr);
	CHECK(r.w.combat().counters().turretedLaunches == 1); // RW 0x6CAD58 and RW 0x6CAD65 let the block run
	CHECK(std::fabs((arrow->getPosition()->x) - (p0.x - 2.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->y) - (p0.y + 1.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->z) - (p0.z + 3.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	checkBasis(arrow->getBasis(), kTurnedBasis);
}

// RW 0x6CAD58: the flag clear skips the block: the launch transform stays the launch bone's (the neighbouring behaviour).
TEST_CASE("PORT-28 without the flag the launch transform stays the launch bone's (RW 0x6CAD58 -> RW 0x6CB440)")
{
	TurretRange r("TurretBowOff", "PRIMARY");
	const Coord3D p0 = *r.a->getPosition();
	Object *arrow = r.fire();
	REQUIRE(arrow != nullptr);
	CHECK(r.w.combat().counters().turretedLaunches == 0);
	CHECK(std::fabs((arrow->getPosition()->x) - (p0.x + 1.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->y) - (p0.y + 2.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->z) - (p0.z + 3.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	checkBasis(arrow->getBasis(), kBoneBasis);
}

// RW 0x6CAD65: the flag set, but the turret of the AI controls another weapon slot (RW 0x66246B answers -1 for PRIMARY): skipped.
TEST_CASE("PORT-28 with the flag but no turret for the slot the launch transform stays the launch bone's (RW 0x6CAD65)")
{
	TurretRange r("TurretBow", "SECONDARY");
	const Coord3D p0 = *r.a->getPosition();
	Object *arrow = r.fire();
	REQUIRE(arrow != nullptr);
	CHECK(r.w.combat().counters().turretedLaunches == 0);
	CHECK(std::fabs((arrow->getPosition()->x) - (p0.x + 1.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->y) - (p0.y + 2.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	CHECK(std::fabs((arrow->getPosition()->z) - (p0.z + 3.0f)) <= 1e-4); // tolerance: cos / sin of the float 90 degrees (RW 0xA3CF90 / RW 0xA3CF84) are not exactly 0 and 1
	checkBasis(arrow->getBasis(), kBoneBasis);
}

// The two inputs of the block: the parsed template flag (RW 0x42E558 + 0x16C, the getter RW 0x6C9C50) and the AI's answer
// (RW 0x66246B: the index of the turret whose ControlledWeaponSlots hold the slot, with its angle (+ 0x18) and pitch (+ 0x1C)).
TEST_CASE("PORT-28 the template flag parses (RW 0x42E558 + 0x16C) and the AI names the turret of the slot (RW 0x66246B)")
{
	TurretRange r("TurretBow", "PRIMARY");
	const Weapon *on = r.a->getWeapons()->weaponInSlot(0);
	REQUIRE(on != nullptr);
	CHECK(on->getTemplate()->rotatingTurret());
	AIUpdateInterface *ai = r.a->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	REQUIRE(ai->turret() != nullptr);
	float angle = 0.0f, pitch = 0.0f;
	CHECK(ai->whichTurretForWeaponSlot(0, &angle, &pitch) == ai->turret()->whichTurret());
	CHECK(std::fabs((angle) - (1.5707964f)) <= 1e-5); // tolerance: NaturalTurretAngle 90 degrees parsed by the float RADS_PER_DEGREE (RW 0x42EE15) is 1.5707964f, not pi / 2
	CHECK(pitch == 0.0f);
	CHECK(ai->whichTurretForWeaponSlot(1, &angle, &pitch) == TURRET_INVALID); // SECONDARY is not on the turret
	CHECK(ai->whichTurretForWeaponSlot(-1, nullptr, nullptr) == TURRET_INVALID);
}

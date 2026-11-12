// OpenBFME unit tests. PORT-148 (fork issue 365): a GIANT_BIRD flyer ordered to attack a structure. GPL-3.0.
// Synthetic INI only; no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL.
//
// RW 0x76D995 routes attack-object commands 0xB/0xC/0x39 to the bird swoop (RW 0x76A139).
// RW 0x76DB18 (AIGiantBirdSwoopState::onEnter) chooses the weapon at RW 0x76DCB5 -> 0x68B619 -> 0x6C8A4E
// with criteria 2 and CMD_FROM_AI; RW 0x6C85CA gives a ground STRUCTURE the anti-mask GROUND|STRUCTURE.
// RW 0x76B668 (AIGiantBirdAttackState::onEnter) gates the shot by max(speed*3, 8.5) to the contact point
// (RW 0x76B744..0x76B794), then fires at RW 0x76B8CA -> 0x69200A. The weapon wrapper RW 0x69200A has no
// AttackRange gate of its own; it only refuses a missing/non-ready current weapon (RW 0x6CDCE7).
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
// Shape of the Ennorath flier WeaponSet (secondary plow preferred against STRUCTURE / MACHINE / HORDE /
// INFANTRY / CAVALRY, tertiary claw only against MONSTER, shared ready status), with synthetic numbers.
const char kPort148Fixture[] = R"ini(
Locomotor Port148FlightLoco
 Surfaces = AIR
 Appearance = GIANT_BIRD
 TurnTime = 1000
 Acceleration = 1000
 Braking = 1000
 PreferredHeight = 120
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Weapon Port148Plow
 AttackRange = 24.0
 AcceptableAimDelta = 5
 DelayBetweenShots = 4000
 FiringDuration = 4500
 MeleeWeapon = No
 CanSwoop = Yes
 ChaseWeapon = Yes
 RadiusDamageAffects = ENEMIES NOT_SIMILAR
 DamageNugget
  Damage = 10
  Radius = 40.0
  DelayTime = 0
  DamageType = SIEGE
  DeathType = NORMAL
 End
End
Weapon Port148Claw
 AttackRange = 20.0
 AcceptableAimDelta = 5
 DelayBetweenShots = 250
 ClipSize = 2
 ClipReloadTime = 2000
 RadiusDamageAffects = ENEMIES NOT_SIMILAR
 DamageNugget
  Damage = 10
  Radius = 0.0
  DelayTime = 0
  DamageType = SIEGE
  DeathType = NORMAL
 End
End
Object Port148Bird
 KindOf = HERO MONSTER SELECTABLE CAN_ATTACK GIANT_BIRD CANNOT_RETALIATE
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 WeaponSet
  Conditions = None
  ReadyStatusSharedWithinSet = Yes
  Weapon = SECONDARY Port148Plow
  PreferredAgainst = SECONDARY STRUCTURE MACHINE HORDE INFANTRY CAVALRY
  Weapon = TERTIARY Port148Claw
  OnlyAgainst = TERTIARY MONSTER
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = Yes ATTACK_BUILDINGS
  SpecialContactPoints = Swoop
 End
 LocomotorSet
  Locomotor = Port148FlightLoco
  Condition = SET_NORMAL
  Speed = 95
 End
 LocomotorSet
  Locomotor = Port148FlightLoco
  Condition = SET_SUPERSONIC
  Speed = 150
 End
 LocomotorSet
  Locomotor = Port148FlightLoco
  Condition = SET_WANDER
  Speed = 78
 End
 LocomotorSet
  Locomotor = Port148FlightLoco
  Condition = SET_PANIC
  Speed = 160
 End
End
Object Port148Fortress
 KindOf = STRUCTURE SELECTABLE IMMOBILE BASE_FOUNDATION BASE_SITE
 Geometry = BOX
 GeometryMajorRadius = 64
 GeometryMinorRadius = 64
 GeometryHeight = 50
 GeometryContactPoint = X:0 Y:0 Z:75 Swoop
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 1000
 End
End
Object Port148Unit
 KindOf = INFANTRY SELECTABLE
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 100
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 30
 End
End
)ini";

struct Port148Arena
{
	combattest::CombatWorld w;
	Object *bird = nullptr;
	GiantBirdAIUpdate *ai = nullptr;
	ObjectWeapons *weapons = nullptr;

	Port148Arena()
	{
		const std::string error = w.w.load(kPort148Fixture, INI_LOAD_OVERWRITE, "port148.ini");
		REQUIRE_MESSAGE(error.empty(), error);
		w.combat().setAutoAcquireEnabled(false);
		bird = w.unit("Port148Bird", 'A', 200.0f, 200.0f);
		REQUIRE(bird != nullptr);
		ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
		REQUIRE(ai != nullptr);
		weapons = bird->getWeapons();
		REQUIRE(weapons != nullptr);
		w.frames(20);
		Coord3D air{ 200.0f, 200.0f, 120.0f };
		bird->setPosition(&air);
	}
};

int strikeSlot(Port148Arena &a, Object *victim, int limit)
{
	REQUIRE(a.ai->aiAttackObject(victim, CMD_FROM_PLAYER));
	const unsigned long long before = a.ai->strikes();
	for (int frame = 0; frame < limit; ++frame)
	{
		a.w.frames(1);
		if (a.ai->strikes() > before)
		{
			return a.weapons->curSlot();
		}
	}
	return -1;
}
} // namespace

TEST_CASE("PORT-148 weapon choice: a ground structure takes the preferred plow, not the monster-only claw (RW 0x6C8A4E / 0x6C85CA)")
{
	Port148Arena a;
	Object *fortress = a.w.unit("Port148Fortress", 'B', 300.0f, 200.0f);
	Object *unit = a.w.unit("Port148Unit", 'B', 400.0f, 200.0f);
	REQUIRE(fortress != nullptr);
	REQUIRE(unit != nullptr);
	CHECK(a.weapons->chooseBestWeaponForTarget(fortress, PREFER_GRAB_OVER_DAMAGE, CMD_FROM_AI));
	CHECK(a.weapons->curSlot() == SECONDARY_WEAPON);
	CHECK(a.weapons->chooseBestWeaponForTarget(unit, PREFER_GRAB_OVER_DAMAGE, CMD_FROM_AI));
	CHECK(a.weapons->curSlot() == SECONDARY_WEAPON);
}

TEST_CASE("PORT-148 giant bird: an ordered attack on an enemy structure fires the plow from the air (RW 0x76DB18 / 0x76B668 / 0x69200A)")
{
	Port148Arena a;
	Object *fortress = a.w.unit("Port148Fortress", 'B', 300.0f, 200.0f);
	REQUIRE(fortress != nullptr);
	const int slot = strikeSlot(a, fortress, 400);
	MESSAGE("fortress strike slot " << slot << ", state " << a.ai->currentStateId() << ", health " << a.w.health(fortress) << ", strikes " << a.ai->strikes());
	CHECK(slot == SECONDARY_WEAPON);
	CHECK(a.w.health(fortress) == 990.0f);
}

TEST_CASE("PORT-148 giant bird: the neighbouring ordered attack on an enemy unit still fires (RW 0x76B668 / 0x69200A)")
{
	Port148Arena a;
	Object *unit = a.w.unit("Port148Unit", 'B', 300.0f, 200.0f);
	REQUIRE(unit != nullptr);
	const int slot = strikeSlot(a, unit, 400);
	MESSAGE("unit strike slot " << slot << ", state " << a.ai->currentStateId() << ", health " << a.w.health(unit) << ", strikes " << a.ai->strikes());
	CHECK(slot == SECONDARY_WEAPON);
	CHECK(a.w.health(unit) == 90.0f);
}

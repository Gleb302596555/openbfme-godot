// OpenBFME. GPL-3.0. Exercise the special-power interface and real weapon delivery, without retail files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/ObjectCreationList.h"

namespace
{
struct CastWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *saved = TheSpecialPowerStore;
	ObjectCreationListStore lists{ w.keys };
	ObjectCreationListStore *savedLists = TheObjectCreationListStore;

	CastWorld()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &lists;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		combat().setAutoAcquireEnabled(false);
	}
	~CastWorld()
	{
		TheSpecialPowerStore = saved;
		TheObjectCreationListStore = savedLists;
	}

	Object *caster(const std::string &effect, const char *kind = "INFANTRY", const char *abilityFields = "", bool ordinarySet = true, bool missing = false,
		const char *otherModule = "")
	{
		const std::string ini = std::string("SpecialPower CastPower\n Enum = SPECIAL_GENERAL_TARGETLESS\nEnd\n") +
			"Locomotor CastLoco\n Surfaces = GROUND\n Appearance = TWO_LEGS\n TurnTime = 2000\n TurnTimeDamaged = 2000\n End\n" +
			"Weapon CastWeapon\n AttackRange = 65\n MinimumAttackRange = 12.5\n RadiusDamageAffects = ENEMIES\n" + effect + "End\n" +
			"Object CastUnit\n KindOf = SELECTABLE " + kind + "\n Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 16\n" +
			(ordinarySet ? " WeaponSet\n  Conditions = None\n  Weapon = PRIMARY SwordWeapon\n End\n" : "") + otherModule +
			" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n Behavior = AIUpdateInterface AiTag\n End\n Behavior = PhysicsBehavior PhysicsTag\n End\n" +
			" LocomotorSet\n  Locomotor = CastLoco\n  Condition = SET_NORMAL\n  Speed = 55\n End\n" +
			" Behavior = SpecialPowerModule PowerTag\n  SpecialPowerTemplate = CastPower\n  UpdateModuleStartsAttack = Yes\n End\n" +
			" Behavior = WeaponFireSpecialAbilityUpdate AbilityTag\n  SpecialPowerTemplate = CastPower\n  SpecialWeapon = " + (missing ? "AbsentWeapon" : "CastWeapon") +
			"\n  StartAbilityRange = 100\n  IgnoreFacingCheck = Yes\n" + abilityFields + " End\nEnd\n";
		const std::string err = w.load(ini);
		REQUIRE_MESSAGE(err.empty(), err);
		return unit("CastUnit", 'A', 100, 100);
	}
	WeaponFireSpecialAbilityUpdate &ability(Object &caster)
	{
		auto *a = dynamic_cast<WeaponFireSpecialAbilityUpdate *>(caster.findModule("WeaponFireSpecialAbilityUpdate"));
		REQUIRE(a);
		return *a;
	}
	void cast(Object &source, Object *target = nullptr, const Coord3D *point = nullptr)
	{
		ability(source).initiateIntentToDoSpecialPower(powers.findSpecialPowerTemplate("CastPower"), target, point, 0, 0);
	}
};
const char damage[] = " DamageNugget\n  Damage = 17\n  Radius = 42\n  DamageType = UNRESISTABLE\n  DeathType = NORMAL\n End\n";
}

TEST_CASE("PORT-288 point admission uses the special weapon and caster edge, not StartAbilityRange or its primary slot")
{
	CastWorld w;
	bool immobile = false, missing = false;
	SUBCASE("mobile can approach far targets") {}
	SUBCASE("immobile with no ordinary weapon set") { immobile = true; }
	SUBCASE("missing special weapon refuses even a null point") { missing = true; }
	Object *o = w.caster(damage, immobile ? "IMMOBILE" : "INFANTRY", "", !immobile, missing);
	auto &ability = w.ability(*o);
	// RW 0x896156 -> RW 0x6CA525: 2D edge distance; radius 8. Minimum = 12.5 - 2.5.
	// IMMOBILE maximum = (65 - 2.5) - 2.5. Equality is accepted by both comparisons.
	const Coord3D near{ 117, 100, 500 }, min{ 118, 100, 500 }, max{ 168, 100, 500 }, far{ 169, 100, 500 };
	CHECK(ability.allowsSpecialPowerUse() == !missing);
	CHECK_FALSE(ability.allowsSpecialPowerUse(&near));
	CHECK(ability.allowsSpecialPowerUse(&min) == !missing);
	CHECK(ability.allowsSpecialPowerUse(&max) == !missing);
	CHECK(ability.allowsSpecialPowerUse(&far) == (!missing && !immobile));
}

TEST_CASE("PORT-288 unpacking tracks a moving victim before firing a directional weapon")
{
	CastWorld w;
	Object *o = w.caster(" DamageDealtAtSelfPosition = Yes\n DamageNugget\n  Damage = 17\n  Radius = 50\n  DamageArc = 60\n  DamageType = UNRESISTABLE\n  DeathType = NORMAL\n End\n",
		"INFANTRY", "  UnpackTime = 1200\n");
	Object *v = w.unit("Dummy", 'B', 135, 100);
	w.frames(2);
	o->setOrientation(0);
	w.cast(*o, v);
	auto &a = w.ability(*o);
	w.runUntil([&] { return a.packingState() == 2; }, 10);
	REQUIRE(a.packingState() == 2);
	Coord3D across{100, 135, 0};
	v->setPosition(&across);
	w.frames(1);
	// tolerance: RW 0x896370 -> RW 0x5E6810 takes one locomotor turn step toward +Y rather than snapping to pi/2.
	CHECK(o->getOrientation() > 0.0f);
	// tolerance: one CastLoco step is smaller than a quarter-turn.
	CHECK(o->getOrientation() < 1.56f);
	CHECK(a.shotsFired() == 0);
	w.runUntil([&] { return a.shotsFired() != 0; }, 48);
	CHECK(a.shotsFired() == 1);
	CHECK(w.health(v) == 83);
	REQUIRE(o->getWeapons());
	// RW 0x8961F7 -> RW 0x8E326D: an actual owner tracker sees the shot even in hard-reset mode.
	CHECK(o->getWeapons()->firingTracker().lastShotFrame() == a.ownedWeapon()->lastFireFrame());
}

TEST_CASE("PORT-288 targetless stomp delivers at the caster, without an ordinary WeaponSet")
{
	CastWorld w;
	Object *o = w.caster(damage, "INFANTRY", "  Instant = Yes\n", false);
	Object *near = w.unit("Dummy", 'B', 130, 100);
	Object *far = w.unit("Dummy", 'B', 230, 100);
	w.frames(2);
	REQUIRE_FALSE(o->getWeapons());
	w.cast(*o); // RW 0x8961F7: zero stored position -> forceFireWeapon at owner, no minimum-range rejection
	CHECK(w.health(near) == 83);
	CHECK(w.health(far) == 100);
}

TEST_CASE("PORT-288 a porter shot extinguishes the selected fire cell, not the caster's cell")
{
	CastWorld w;
	Object *o = w.caster(" FireLogicNugget\n  LogicType = DECREASE_BURN_RATE\n  Damage = 80\n  Radius = 12\n End\n");
	auto &fire = w.logic->fireLogic();
	fire.newMap(0, 0, 250, 250);
	const Coord3D pos{150, 100, 0};
	fire.changeBurnRateInArea(pos, 12, 20, false);
	fire.changeBurnRateInArea(*o->getPosition(), 12, 20, false);
	REQUIRE(fire.isCellBurning(15, 10));
	w.cast(*o, nullptr, &pos); // RW 0x8961F7 -> RW 0x6CF3D2 -> FireLogicNugget position effect RW 0x9120E6
	w.runUntil([&] { return w.ability(*o).shotsFired() == 1; }, 8);
	CHECK(w.ability(*o).shotsFired() == 1);
	CHECK_FALSE(fire.isCellBurning(15, 10));
	CHECK(fire.isCellBurning(10, 10));
}

TEST_CASE("PORT-288 object-targeted OCL shot creates one object")
{
	CastWorld w;
	REQUIRE(w.w.load("ObjectCreationList CastList\n CreateObject\n  ObjectNames = Dummy\n End\nEnd\n").empty());
	Object *o = w.caster(" WeaponOCLNugget\n  WeaponOCLName = CastList\n End\n");
	Object *target = w.unit("Dummy", 'B', 145, 100);
	const auto next = w.logic->getNextObjectID();
	w.cast(*o, target); // RW 0x8961F7 -> RW 0x6CF328 -> WeaponOCLNugget slot 5 (RW 0x90F858)
	w.runUntil([&] { return w.ability(*o).shotsFired() != 0; }, 8);
	REQUIRE(w.byId(next));
	CHECK(w.byId(next)->getTemplate() == target->getTemplate());
	CHECK(w.logic->getNextObjectID() == next + 1);
}

TEST_CASE("PORT-288 victim shot grabs the eligible passenger through the caster's contain")
{
	CastWorld w;
	REQUIRE(w.w.load("ChildObject CastPassenger Dummy\n TransportSlotCount = 1\nEnd\n").empty());
	Object *o = w.caster(" GrabNugget\n  ContainTargetOnEffect = Yes\n End\n", "TRANSPORT", "", true, false,
		" Behavior = TransportContain ContainTag\n  Slots = 1\n  PassengerFilter = ANY +INFANTRY\n  AllowEnemiesInside = Yes\n End\n");
	Object *target = w.unit("CastPassenger", 'B', 145, 100);
	w.cast(*o, target); // RW 0x910C27, RW 0x910C8A: slot 0x98 validates capacity, slot 0x9C adds the target
	w.runUntil([&] { return w.ability(*o).shotsFired() != 0; }, 8);
	CHECK(target->getContainedBy() == o);
}

TEST_CASE("PORT-288 leap uses the caster as projectile and delivers its landing weapon")
{
	CastWorld w;
	REQUIRE(w.w.load(std::string("Weapon CastLanding\n RadiusDamageAffects = ENEMIES\n") + damage + "End\n").empty());
	Object *o = w.caster(" WeaponSpeed = 125\n ProjectileSelf = Yes\n ProjectileNugget\n  WarheadTemplateName = CastLanding\n End\n", "INFANTRY", "", true, false,
		" Behavior = BezierProjectileBehavior FlightTag\n  FirstHeight = 40\n  SecondHeight = 15\n  FirstPercentIndent = 25%\n  SecondPercentIndent = 70%\n  CrushStyle = Yes\n  GroundHitWeapon = CastLanding\n End\n");
	Object *target = w.unit("Dummy", 'B', 168, 100);
	const Coord3D pos{152, 100, 0};
	w.cast(*o, nullptr, &pos); // RW 0x8961F7 -> RW 0x6CF3D2; projectile position slot selects the source for ProjectileSelf
	w.runUntil([&] { return w.combat().counters().projectilesLanded != 0; }, 60);
	CHECK(w.combat().counters().projectilesLanded == 1);
	CHECK_FALSE(o->isEffectivelyDead());
	CHECK(w.health(target) == 83);
}

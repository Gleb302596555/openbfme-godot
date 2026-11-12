// OpenBFME unit tests. PORT-136: synthetic, no game installation. GPL-3.0.
// Probe file (temporary diagnostics): the summoned giant bird of the spell book does not attack on its own.
#include "doctest.h"

#include <iomanip>

#include "CombatTestUtil.h"
#include "IniTestUtil.h"
#include "GameLogic/AI/AIClosestEnemy.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/Object.h"

namespace
{
// the real mod shape of GwaihirEagleSpell_Summoned (Ennorath 1.9.8.2): its KindOf list, VisionRange 450, AttackRange 20, the two DamageNuggets (FORCE,
// HERO), a MetaImpactNugget, the TransportContain grab and LifetimeUpdate; `extras` adds the two modules beside GiantBirdAIUpdate.
std::string eagleIni(bool extras)
{
	std::string s = R"ini(
Locomotor Port136BirdLoco
 Surfaces = AIR
 Appearance = GIANT_BIRD
 TurnTime = 1000
 Acceleration = 1000
 Braking = 1000
 PreferredHeight = 100
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Weapon Port136EagleAttack
 AttackRange = 20.0
 DelayBetweenShots = 4000
 AntiAirborneVehicle = Yes
 AntiAirborneMonster = Yes
 AntiAirborneInfantry = Yes
 RadiusDamageAffects = ENEMIES ALLIES SELF
 DamageNugget
  Damage = 50
  DamageType = FORCE
  DeathType = NORMAL
  Radius = 0
 End
End
Object Port136Eagle
 KindOf = PRELOAD CAN_ATTACK CAN_CAST_REFLECTIONS PASS_EXPERIENCE_TO_PRODUCER SCORE NO_FREEWILL_ENTER MONSTER SCARY CANNOT_RETALIATE SUMMONED
 VisionRange = 450
 ShroudClearingRange = 650
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 WeaponSet
  Conditions = None
  Weapon = PRIMARY Port136EagleAttack
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
  AutoAcquireEnemiesWhenIdle = Yes ATTACK_BUILDINGS
 End
)ini";
	if (extras)
	{
		s += R"ini( Behavior = StancesBehavior ModuleTag_Stances
  StanceTemplate = Port136Hero
 End
)ini";
		s += R"ini( Behavior = TransportContain ModuleTag_Contain
  Slots = 1
  PassengerBonePrefix = PassengerBone:B_Passenger KindOf:GRAB_AND_DROP
  DamagePercentToUnits = 0%
 End
 Behavior = LifetimeUpdate ModuleTag_Life
  MinLifetime = 120000
  MaxLifetime = 120000
 End
)ini";
	}
	s += R"ini( LocomotorSet
  Locomotor = Port136BirdLoco
  Condition = SET_NORMAL
  Speed = 120
 End
End
StanceTemplate Port136Hero
    Stance Aggressive
        MeleeBehavior = Amoeba
        End
    End
    Stance Battle
        MeleeBehavior = Swarm
        End
    End
    Stance HoldGround
        MeleeBehavior = HoldGround
        End
    End
End
Armor Port136EagleArmor
 Armor = DEFAULT 100%
 Armor = HERO 100%
 Armor = FORCE 100%
End
)ini";
	return s;
}

struct Outcome
{
	unsigned state = 0;
	float z = 0.0f;
	float health = 0.0f;
	bool withinRange = false;
	bool canAttackFromHere = false;
	std::string target;
};

Outcome probe(const char *label, bool extras, float horizontal, float birdZ, int frames)
{
	combattest::CombatWorld w;
	const std::string error = w.w.load(eagleIni(extras), INI_LOAD_OVERWRITE, "port136-probe.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
	Object *enemy = w.unit("Swordsman", 'B', 200.0f + horizontal, 200.0f);
	REQUIRE(bird);
	REQUIRE(enemy);
	auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
	REQUIRE(ai);
	if (birdZ != 0.0f)
	{
		Coord3D p = *bird->getPosition();
		p.z = birdZ;
		bird->setPosition(&p);
	}
	Outcome out;
	out.withinRange = bird->getWeapons()->isSlotWithinAttackRange(0, *enemy, 0.0f, true);
	out.canAttackFromHere = AI::canAttackFromHere(*bird, *enemy);
	Object *mood = ai->getNextMoodTarget(false, true);
	out.target = mood ? mood->getTemplate()->getName() : std::string("none");
	MESSAGE(label << ": playerTypeA=" << (int)w.playerOf('A')->getPlayerType() << " extras=" << extras << " horiz=" << horizontal << " z=" << birdZ << " withinRange=" << out.withinRange << " canAttackFromHere="
		      << out.canAttackFromHere << " mood(no cadence)=" << out.target);
	w.frames(frames);
	out.state = ai->currentStateId();
	out.z = bird->getPosition()->z;
	out.health = w.health(enemy);
	MESSAGE(label << ": after " << frames << " frames state=" << out.state << " z=" << out.z << " enemy health=" << out.health);
	return out;
}
}

TEST_CASE("PORT-136 probe D: the mod eagle shape, enemy at 20 horizontally")
{
	probe("D1", true, 20.0f, 100.0f, 60);
	probe("D2", false, 20.0f, 100.0f, 60);
	probe("D3", true, 20.0f, 0.0f, 60);
	CHECK(false);
}

TEST_CASE("PORT-136 probe F: the eagle with a StancesBehavior takes Battle and sits in AI_GUARD (RW 0x8622E9 -> 0x86223F -> 0x662A21 -> AIGuardState 16)")
{
	combattest::CombatWorld w;
	StanceTemplateStore store;
	store.registerBlock(w.w.fx.env.blocks);
	StanceTemplateStore *const saved = TheStanceTemplateStore;
	TheStanceTemplateStore = &store;
	struct Restore
	{
		StanceTemplateStore *v;
		~Restore() { TheStanceTemplateStore = v; }
	} restore{saved};
	const std::string error = w.w.load(eagleIni(true), INI_LOAD_OVERWRITE, "port136-stance.ini");
	REQUIRE_MESSAGE(error.empty(), error);
	REQUIRE(store.find("Port136Hero"));
	const float distances[] = { 400.0f, 300.0f, 200.0f, 100.0f };
	for (float d : distances)
	{
		Object *bird = w.unit("Port136Eagle", 'A', 200.0f, 200.0f);
		Object *enemy = w.unit("Swordsman", 'B', 200.0f + d, 200.0f);
		REQUIRE(bird);
		REQUIRE(enemy);
		auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
		REQUIRE(ai);
		StancesBehavior *sb = StancesBehavior::of(*bird);
		REQUIRE(sb);
		std::string trace;
		unsigned states[10] = { 0 };
		float hp[10] = { 0 };
		for (int tick = 0; tick < 10; ++tick)
		{
			w.frames(30);
			states[tick] = ai->currentStateId();
			hp[tick] = w.health(enemy);
			trace += std::to_string(states[tick]) + "/" + std::to_string(w.health(bird)) + " ";
		}
		{
			const float vr = AI::adjustedVisionRange(*bird, 3u);
			Object *closest = AI::findClosestEnemy(*bird, vr, AI::CAN_ATTACK | AI::AUTO_ACQUIRE, nullptr, nullptr, 0);
			MESSAGE("F d=" << d << " adjustedVisionRange=" << vr << " findClosestEnemy=" << (closest ? closest->getTemplate()->getName() : std::string("none")) << " autoAcquireFlags=" << ai->autoAcquireFlags());
		}
		MESSAGE("F d=" << d << ": stance=" << (int)sb->getStance() << " state/hp(300 frames, every 30)=" << trace << " enemy hp=" << w.health(enemy) << " bird z=" << bird->getPosition()->z << " bird hp=" << w.health(bird));
	}
	CHECK(false);
}

TEST_CASE("PORT-136 probe E: the mod eagle shape, enemy at 100/400 horizontally")
{
	probe("E1", true, 100.0f, 100.0f, 60);
	probe("E2", true, 400.0f, 100.0f, 120);
	CHECK(false);
}

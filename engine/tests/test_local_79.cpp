// OpenBFME unit tests. LOCAL-79 (issue #621, after Port 215 #505, P-124):
// Buhrdur (WildDuhrbur) and club trolls ordered onto an uncrushable hero stop at ~59.9 between centres
// and swing without damage when the victim's template defines PathfindDiameter = 40.0 (GondorBoromir in
// data/ini/object/goodfaction/units/men/boromir.ini, GondorIsildur in elven/isildur.ini), whereas against
// GondorBoromir_mod (PathfindDiameter = 25 in data/ini/object/goodfaction/units/gondor/gondorboromir.ini)
// or a hero without PathfindDiameter they close inside the 40-radius DamageNugget and deal damage.
//
// RotWK 2.01 facts:
// - RW 0x6CC653 -> RW 0x6CC07C (WeaponIsWithinAttackRange): for a MeleeWeapon (+0x125) against a target with
//   an AI and current locomotor (+0x260, +0x1F0), calls Pathfinder::meleeReach (RW 0x6ED843) and checks the
//   ground/wall layer rule (RW 0x6CC137 .. 0x6CC184).
// - RW 0x6ED843: computes range against the victim via RW 0x6CA935 -> 0x6CA701 -> 0x6CA69A -> 0x6C9F5B
//   (subtracts 2.5f at RW 0x6C9F71 from AttackRange * bonus: 30.0 - 2.5 = 27.5), converts to cells via
//   floor(range * 0.1 + 0.1) = 2 cells (RW 0x6ED8AB .. 0x6ED8CF), computes both footprints' size via
//   RW 0x6EAF79(attacker) at RW 0x6ED88C and RW 0x6EAF79(victim) at RW 0x6EDB1B, and tests inclusive box
//   overlap (RW 0x6EDB57 .. 0x6EDB69).
// - RW 0x6EAF79 (Pathfinder::footprintSize): starts with d = 2 * boundingCircleRadius (clamped to 20 when
//   10 < d < 20), then if ThingTemplate + 0x53C (PathfindDiameter) > 0.0f overwrites d = PathfindDiameter
//   (RW 0x6EAFD5 .. 0x6EAFE7), and returns floor(d * 0.1f + 0.3f) clamped to [1, 2 * maxR + 1].
// - RW 0x6CCBF9 .. 0x6CCC51 -> DamageNugget::doNugget RW 0x90DEF0 -> PartitionManager RW 0xA3C7D0 ->
//   RW 0xAD2CE0 -> RW 0xAD48F0: DamageDealtAtSelfPosition centers the blast at the attacker, and the
//   cylinder overlap test compares horizontal centre distance against R_nugget + R_geom_victim (using the
//   victim's geometry radius 8.0, not PathfindDiameter 40.0).
//
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"

#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

#include <cmath>
#include <string>

using namespace combattest;

namespace
{

const char kLocal79Objects[] =
	"Weapon L79TrollClubSwing\n"
	"  AttackRange = 30.0\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1400\n"
	"  PreAttackDelay = 600\n"
	"  PreAttackType = PER_SHOT\n"
	"  FiringDuration = 1400\n"
	"  DamageDealtAtSelfPosition = Yes\n"
	"  RadiusDamageAffects = ENEMIES NEUTRALS NOT_SIMILAR\n"
	"  DamageNugget\n"
	"    Damage = 150\n"
	"    Radius = 40.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"  MetaImpactNugget\n"
	"    ShockWaveAmount = 50.0\n"
	"    ShockWaveRadius = 55.0\n"
	"    ShockWaveTaperOff = 0.75\n"
	"    ShockWaveZMult = 1.20\n"
	"  End\n"
	"End\n"
	"Object L79ClubTroll\n"
	"  KindOf = HERO PRELOAD SELECTABLE MONSTER CAN_ATTACK ATTACK_NEEDS_LINE_OF_SIGHT PATH_THROUGH_EACH_OTHER SCORE HEAVY_MELEE_HITTER\n"
	"  VisionRange = 175\n"
	"  CrusherLevel = 1\n"
	"  CrushableLevel = 2\n"
	"  UseCrushAttack = No\n"
	"  PathfindDiameter = 40.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 22.0\n"
	"  GeometryHeight = 42.0\n"
	"  GeometryIsSmall = No\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY L79TrollClubSwing\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 4000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object L79HeroDefaultDiameter\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY PATH_THROUGH_EACH_OTHER SCORE HERO HEAVY_MELEE_HITTER\n"
	"  CrushableLevel = 2\n"
	"  ShockwaveResistance = 20\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8.0\n"
	"  GeometryMinorRadius = 8.0\n"
	"  GeometryHeight = 19.2\n"
	"  GeometryIsSmall = Yes\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object L79HeroDiameter25\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY PATH_THROUGH_EACH_OTHER SCORE HERO HEAVY_MELEE_HITTER\n"
	"  CrushableLevel = 2\n"
	"  PathfindDiameter = 25.0\n"
	"  ShockwaveResistance = 20\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8.0\n"
	"  GeometryMinorRadius = 8.0\n"
	"  GeometryHeight = 19.2\n"
	"  GeometryIsSmall = Yes\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object L79HeroDiameter40\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY PATH_THROUGH_EACH_OTHER SCORE HERO HEAVY_MELEE_HITTER\n"
	"  CrushableLevel = 2\n"
	"  PathfindDiameter = 40.0\n"
	"  ShockwaveResistance = 20\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8.0\n"
	"  GeometryMinorRadius = 8.0\n"
	"  GeometryHeight = 19.2\n"
	"  GeometryIsSmall = Yes\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

float dist2D(const Object &a, const Object &b)
{
	const float dx = a.getPosition()->x - b.getPosition()->x;
	const float dy = a.getPosition()->y - b.getPosition()->y;
	return std::sqrt(dx * dx + dy * dy);
}

} // namespace

TEST_CASE("LOCAL-79 synthetic: target PathfindDiameter 40 makes melee reach true at ~59.5 (4x4 footprint + 2 range cells) while default/25 diameter closes inside the 48.0 damage reach and deals damage (RW 0x6EAF79, RW 0x6ED843, RW 0x6CC07C)")
{
	// 1. Static reach comparison at identical coordinates (540.5, 300.5) vs (600.0, 300.0), dist ~59.502.
	{
		CombatWorld w(kLocal79Objects);
		w.combat().setAutoAcquireEnabled(false);

		Object *troll = w.unit("L79ClubTroll", 'A', 540.5f, 300.5f);
		Object *heroDef = w.unit("L79HeroDefaultDiameter", 'B', 600.0f, 300.0f);
		Object *hero25 = w.unit("L79HeroDiameter25", 'B', 600.0f, 300.0f);
		Object *hero40 = w.unit("L79HeroDiameter40", 'B', 600.0f, 300.0f);

		Pathfinder &pf = w.ai->pathfinder();
		const PathfindObject *pfTroll = w.ai->findObjectByID(troll->getID());
		const PathfindObject *pfDef = w.ai->findObjectByID(heroDef->getID());
		const PathfindObject *pf25 = w.ai->findObjectByID(hero25->getID());
		const PathfindObject *pf40 = w.ai->findObjectByID(hero40->getID());
		REQUIRE(pfTroll != nullptr);
		REQUIRE(pfDef != nullptr);
		REQUIRE(pf25 != nullptr);
		REQUIRE(pf40 != nullptr);

		// RW 0x6EAF79: footprintSize uses PathfindDiameter (+0x53C) when > 0, else 2*r (clamped to 20 for 10 < 2*r < 20).
		CHECK(pf.footprintSize(pfTroll) == 4);
		CHECK(pf.footprintSize(pfDef) == 2); // 2 * 8 = 16 -> 20 -> floor(20 * 0.1 + 0.3) = 2
		CHECK(pf.footprintSize(pf25) == 2);  // 25 -> floor(25 * 0.1 + 0.3) = 2
		CHECK(pf.footprintSize(pf40) == 4);  // 40 -> floor(40 * 0.1 + 0.3) = 4

		// RW 0x6C9F71 / 0x6ED8AB..0x6ED8CF: AttackRange 30 -> 30 - 2.5 = 27.5 -> floor(27.5 * 0.1 + 0.1) = 2 cells.
		CHECK_FALSE(pf.meleeReachCells(*pfTroll, *troll->getPosition(), *pfDef, 2));
		CHECK_FALSE(pf.meleeReachCells(*pfTroll, *troll->getPosition(), *pf25, 2));
		CHECK(pf.meleeReachCells(*pfTroll, *troll->getPosition(), *pf40, 2));

		CHECK_FALSE(troll->getWeapons()->isWithinAttackRange(*heroDef));
		CHECK_FALSE(troll->getWeapons()->isWithinAttackRange(*hero25));
		CHECK(troll->getWeapons()->isWithinAttackRange(*hero40));
	}

	// 2. Approach from 300 onto hero with PathfindDiameter = 40: stops at ~59.5, fires club (MetaImpactNugget hits, 0 damage).
	{
		CombatWorld w(kLocal79Objects);
		w.combat().setAutoAcquireEnabled(false);

		Object *troll = w.unit("L79ClubTroll", 'A', 300.0f, 300.5f);
		Object *hero40 = w.unit("L79HeroDiameter40", 'B', 600.0f, 300.0f);
		const float hp0 = w.health(hero40);

		troll->getAIUpdateInterface()->aiAttackObject(hero40, CMD_FROM_PLAYER);

		int stopFrame = -1;
		float stopDist = 0.0f;
		bool knockedBack = false;
		for (int f = 0; f < 60; ++f)
		{
			w.logic->runLogicFrame();
			if (stopFrame < 0 && troll->getWeapons()->isWithinAttackRange(*hero40))
			{
				stopFrame = f;
				stopDist = dist2D(*troll, *hero40);
			}
			if (hero40->getPosition()->x > 600.1f)
			{
				knockedBack = true;
			}
		}
		REQUIRE(stopFrame >= 0); // tolerance: reach frame depends on locomotor acceleration steps
		CHECK(stopDist > 48.0f); // tolerance: stops outside DamageNugget 40 + victim geometry radius 8 = 48.0
		CHECK(stopDist < 63.0f); // tolerance: stops inside MetaImpactNugget 55 + victim geometry radius 8 = 63.0
		CHECK(w.health(hero40) == hp0);
		CHECK(knockedBack);
	}

	// 3. Neighbor case: approach from 300 onto hero with PathfindDiameter = 25 (as GondorBoromir_mod): closes inside 48.0 and deals 150 damage.
	{
		CombatWorld w(kLocal79Objects);
		w.combat().setAutoAcquireEnabled(false);

		Object *troll = w.unit("L79ClubTroll", 'A', 300.0f, 300.5f);
		Object *hero25 = w.unit("L79HeroDiameter25", 'B', 600.0f, 300.0f);
		const float hp0 = w.health(hero25);

		troll->getAIUpdateInterface()->aiAttackObject(hero25, CMD_FROM_PLAYER);

		int stopFrame = -1;
		float hitDist = 0.0f;
		for (int f = 0; f < 60; ++f)
		{
			w.logic->runLogicFrame();
			if (stopFrame < 0 && troll->getWeapons()->isWithinAttackRange(*hero25))
			{
				stopFrame = f;
			}
			if (w.health(hero25) < hp0)
			{
				hitDist = dist2D(*troll, *hero25);
				break;
			}
		}
		REQUIRE(stopFrame >= 0); // tolerance: reach frame depends on locomotor acceleration steps
		CHECK(hitDist > 0.0f);   // tolerance: positive distance between attacker and victim centers at hit frame
		CHECK(hitDist <= 48.0f); // tolerance: inside DamageNugget radius 40.0 + victim geometry radius 8.0 = 48.0 (RW 0xAD48F0)
		CHECK(w.health(hero25) == hp0 - 150.0f);
	}
}

TEST_CASE("LOCAL-79 ENNORATH scenario P-124: WildDuhrbur ordered from 300 onto GondorBoromir (PathfindDiameter 40) vs GondorBoromir_mod (PathfindDiameter 25) on map mp fall back 8p (RW 0x6EAF79, RW 0x6ED843, RW 0x6CC07C)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-79 Ennorath scenario P-124"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionWild");
	game.logic().combat().setAutoAcquireEnabled(false);
	const int p1 = game.index();
	const int p2 = game.game->players().findPlayerWithName("Player_2")->getPlayerIndex();
	std::string err;

	Pathfinder &pf = game.game->ai().pathfinder();

	// Case A: WildDuhrbur vs GondorBoromir (men/boromir.ini: GeometryMajorRadius = 8.0, PathfindDiameter = 40.0 -> footprint 4).
	{
		Object *buhrdur = game.game->createObject("WildDuhrbur", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(buhrdur != nullptr, err);
		Object *boromir40 = game.game->createObject("GondorBoromir", p2, Coord3D{ 2000.0f, 2300.0f, 0.0f }, 3.14159265f, &err);
		REQUIRE_MESSAGE(boromir40 != nullptr, err);

		game.logic().runLogicFrame();
		game.logic().runLogicFrame();

		const PathfindObject *pfBuhrdur = game.game->ai().findObjectByID(buhrdur->getID());
		const PathfindObject *pfBoromir40 = game.game->ai().findObjectByID(boromir40->getID());
		REQUIRE(pfBuhrdur != nullptr);
		REQUIRE(pfBoromir40 != nullptr);
		CHECK(pf.footprintSize(pfBuhrdur) == 4);
		CHECK(pf.footprintSize(pfBoromir40) == 4);

		const float hp0 = boromir40->getBodyModule()->getHealth();
		buhrdur->getAIUpdateInterface()->aiAttackObject(boromir40, CMD_FROM_PLAYER);

		float firstReachDist = -1.0f;
		for (int f = 0; f < 120; ++f)
		{
			game.logic().runLogicFrame();
			if (firstReachDist < 0.0f && buhrdur->getWeapons()->isWithinAttackRange(*boromir40))
			{
				firstReachDist = dist2D(*buhrdur, *boromir40);
			}
		}
		REQUIRE(firstReachDist > 0.0f); // tolerance: positive distance when melee reach first turns true
		CHECK(firstReachDist > 48.0f);  // tolerance: 4x4 footprint + 2 range cells + 4x4 footprint turns true outside 40 + 8 = 48.0 damage reach
		CHECK(boromir40->getBodyModule()->getHealth() == hp0);

		game.logic().destroyObject(buhrdur);
		game.logic().destroyObject(boromir40);
		game.logic().runLogicFrame();
	}

	// Case B: WildDuhrbur vs GondorBoromir_mod (gondor/gondorboromir.ini: GeometryMajorRadius = 8.0, PathfindDiameter = 25 -> footprint 2).
	{
		Object *buhrdur = game.game->createObject("WildDuhrbur", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(buhrdur != nullptr, err);
		Object *boromir25 = game.game->createObject("GondorBoromir_mod", p2, Coord3D{ 2000.0f, 2300.0f, 0.0f }, 3.14159265f, &err);
		REQUIRE_MESSAGE(boromir25 != nullptr, err);

		game.logic().runLogicFrame();
		game.logic().runLogicFrame();

		const PathfindObject *pfBuhrdur = game.game->ai().findObjectByID(buhrdur->getID());
		const PathfindObject *pfBoromir25 = game.game->ai().findObjectByID(boromir25->getID());
		REQUIRE(pfBuhrdur != nullptr);
		REQUIRE(pfBoromir25 != nullptr);
		CHECK(pf.footprintSize(pfBuhrdur) == 4);
		CHECK(pf.footprintSize(pfBoromir25) == 2);

		const float hp0 = boromir25->getBodyModule()->getHealth();
		buhrdur->getAIUpdateInterface()->aiAttackObject(boromir25, CMD_FROM_PLAYER);

		float firstReachDist = -1.0f;
		for (int f = 0; f < 120; ++f)
		{
			game.logic().runLogicFrame();
			if (firstReachDist < 0.0f && buhrdur->getWeapons()->isWithinAttackRange(*boromir25))
			{
				firstReachDist = dist2D(*buhrdur, *boromir25);
			}
			if (boromir25->getBodyModule()->getHealth() < hp0)
			{
				break;
			}
		}
		REQUIRE(firstReachDist > 0.0f);                // tolerance: positive distance when melee reach first turns true
		CHECK(firstReachDist <= 48.0f);                // tolerance: 4x4 footprint + 2 range cells + 2x2 footprint turns true inside 40 + 8 = 48.0 damage reach
		CHECK(boromir25->getBodyModule()->getHealth() < hp0); // tolerance: MollokClubSwing deals damage once inside the 48.0 damage overlap
	}
}

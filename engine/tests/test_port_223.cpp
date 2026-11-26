// OpenBFME. PORT-223 tests (fork issue 533, P-150 / S-325 / S-787):
//   * AIAttackMeleeEngageState (0xE2, RW 0x74F599 / 0x74B1D6 / 0x74B716): a mounted melee hero chasing a single fleeing infantryman paces at
//     max(0, dist - (srcR + vicR + 10.0f) + victimSpeed), sets RUNNING_DOWN_FROM_BEHIND when ourMaxSpeed > desiredSpeed, and fires m_fireState
//     (RW 0x740E39) on the run without stopping short for MELEE_AIM; against a faster fleeing target it keeps chasing without hitting; when the
//     target stops it drops RUNNING_DOWN_FROM_BEHIND and finishes through MELEE_AIM -> MELEE_FIRE;
//   * AIAttackPursueTargetState (0x64, RW 0x744401 / 0x746058 / 0x74ECD9 / 0x74AD8E): runs for a turreted unit pursuing a slower fleeing
//     PhysicsBehavior target and paces at 0.95 * victimSpeed in weapon range; hands over to Approach (0x65) when the target is faster;
//   * AIAttackApproachState (0x65, RW 0x749D46 / 0x749E09): predicts a fleeing victim 4.0 frames ahead (RW 0xBD88C0 = 4.0f) for range checks;
//   * Snapshot round-trip (AIAttackPursueState::xfer and AIAttackMeleeEngageState::xfer v2) preserves state hash and continuation.
// Synthetic data only: no retail or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/AI/AIAttackMelee.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"

#include <cmath>
#include <vector>

using namespace combattest;

namespace
{
const char kPort223Objects[] =
	"Locomotor P223HeroHorseLoco\n"
	"  Surfaces = GROUND RUBBLE\n"
	"  Speed = 110\n"
	"  TurnTime = 1500\n"
	"  Acceleration = 1500\n"
	"  SlowTurnRadius = 0.0\n"
	"  FastTurnRadius = 48.0\n"
	"  Braking = 2000\n"
	"  MinTurnSpeed = 10%\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = FOUR_WHEELS\n"
	"  CloseEnoughDist = 2.0\n"
	"End\n"
	"Weapon P223HeroSword\n"
	"  AttackRange = 20.0\n"
	"  MeleeWeapon = Yes\n"
	"  LeechRangeWeapon = Yes\n"
	"  DelayBetweenShots = 520\n"
	"  PreAttackDelay = 500\n"
	"  PreAttackType = PER_SHOT\n"
	"  FiringDuration = 500\n"
	"  DamageNugget\n"
	"    Damage = 150\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = HERO\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P223TurretBow\n"
	"  AttackRange = 120\n"
	"  WeaponSpeed = 200\n"
	"  DelayBetweenShots = 400\n"
	"  PreAttackDelay = 200\n"
	"  PreAttackType = PER_SHOT\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon P223ShortBow\n"
	"  AttackRange = 60\n"
	"  WeaponSpeed = 200\n"
	"  DelayBetweenShots = 400\n"
	"  PreAttackDelay = 200\n"
	"  PreAttackType = PER_SHOT\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object P223MountedHero\n"
	"  KindOf = HERO CAVALRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 240\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 25\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P223HeroSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P223HeroHorseLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 110\n"
	"  End\n"
	"End\n"
	"Object P223Runner\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CrushableLevel = 2\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object P223FastRunner\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CrushableLevel = 2\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 140\n"
	"  End\n"
	"End\n"
	"Object P223TurretSkirmisher\n"
	"  KindOf = CAVALRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P223TurretBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"    Turret\n"
	"      TurretTurnRate = 360\n"
	"      ControlledWeaponSlots = PRIMARY\n"
	"    End\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P223HeroHorseLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n"
	"Object P223RangedChaser\n"
	"  KindOf = CAVALRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P223ShortBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n";

struct Port223World : CombatWorld
{
	Port223World() : CombatWorld(kPort223Objects) { combat().setAutoAcquireEnabled(false); }

	AIUpdateInterface *ai(Object *o) { return o ? o->getAIUpdateInterface() : nullptr; }

	AIAttackState *attackState(Object *o)
	{
		AIUpdateInterface *a = ai(o);
		return a ? dynamic_cast<AIAttackState *>(a->stateMachine().currentState()) : nullptr;
	}

	unsigned innerState(Object *o)
	{
		AIAttackState *att = attackState(o);
		return att && att->attackMachine() ? att->attackMachine()->currentStateId() : 0u;
	}

	void runEast(Object *o)
	{
		o->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 950.0f, o->getPosition()->y, 0.0f }, CMD_FROM_AI);
		for (int f = 0; f < 20; ++f)
		{
			frames(1);
			AIUpdateInterface *a = ai(o);
			if (a->curLocomotor() && a->curLocomotor()->isFasterThanQuarterSpeed(a->locomotorHost()))
			{
				return;
			}
		}
		FAIL("runner never exceeded a quarter of its maximum speed");
	}
};
} // namespace

TEST_CASE("PORT-223 (1): a mounted melee hero ordered on a single fleeing target runs it down and strikes on the run in MELEE_ENGAGE (RW 0x74B1D6 / 0x740E39)")
{
	Port223World w;
	Object *hero = w.unit("P223MountedHero", 'A', 150, 300);
	Object *runner = w.unit("P223Runner", 'B', 260, 300);
	runner->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner);

	const ObjectID runnerId = runner->getID();
	REQUIRE(w.ai(hero)->aiAttackObject(runner, CMD_FROM_PLAYER));

	bool sawEngage = false;
	bool sawRunningDown = false;
	bool sawAimDuringRun = false;
	int firstHitFrame = -1;
	const float startHp = w.health(runner);
	float hpAfterFirstHit = startHp;

	for (int f = 0; f < 80 && w.byId(runnerId) != nullptr; ++f)
	{
		w.frames(1);
		if (w.innerState(hero) == (unsigned)MELEE_ENGAGE)
		{
			sawEngage = true;
		}
		if (hero->testStatus((unsigned)CombatNames::statuses().runningDownFromBehind))
		{
			sawRunningDown = true;
		}
		if (w.innerState(hero) == (unsigned)MELEE_AIM)
		{
			sawAimDuringRun = true;
		}
		Object *cur = w.byId(runnerId);
		const float hp = cur ? w.health(cur) : 0.0f;
		if (firstHitFrame < 0 && hp < startHp)
		{
			firstHitFrame = f;
			hpAfterFirstHit = hp;
			CHECK(w.innerState(hero) == (unsigned)MELEE_ENGAGE); // hit landed inline via m_fireState without stopping for MELEE_AIM
		}
	}

	CHECK(sawEngage);
	CHECK(sawRunningDown);
	CHECK_FALSE(sawAimDuringRun);
	// tolerance: hit landed within the 80-frame chase loop
	CHECK(firstHitFrame >= 0);
	// tolerance: hero catch-up and 500 ms PreAttackDelay land within 20 frames
	CHECK(firstHitFrame <= 20);
	// tolerance: inline m_fireState dealt positive damage
	CHECK(hpAfterFirstHit < startHp);
	// Continues hitting on the run until the fleeing soldier is dead
	CHECK(w.byId(runnerId) == nullptr);
	CHECK(w.combat().counters().kills == 1);
}

TEST_CASE("PORT-223 (2): a mounted melee hero ordered on a faster single fleeing target keeps chasing in MELEE_ENGAGE without stopping short to swing")
{
	Port223World w;
	Object *hero = w.unit("P223MountedHero", 'A', 200, 300);
	Object *fast = w.unit("P223FastRunner", 'B', 260, 300);
	fast->setOrientation(0.0f);
	w.frames(2);
	w.runEast(fast);

	const float startHp = w.health(fast);
	REQUIRE(w.ai(hero)->aiAttackObject(fast, CMD_FROM_PLAYER));

	bool sawAim = false;
	bool sawRunningDown = false;
	for (int f = 0; f < 25; ++f)
	{
		w.frames(1);
		if (w.innerState(hero) == (unsigned)MELEE_AIM)
		{
			sawAim = true;
		}
		if (hero->testStatus((unsigned)CombatNames::statuses().runningDownFromBehind))
		{
			sawRunningDown = true;
		}
	}

	CHECK(w.innerState(hero) == (unsigned)MELEE_ENGAGE);
	CHECK_FALSE(sawAim);
	CHECK_FALSE(sawRunningDown);
	CHECK(w.health(fast) == startHp);
}

TEST_CASE("PORT-223 (3): when the fleeing target stops, the hero drops RUNNING_DOWN_FROM_BEHIND and kills it via MELEE_AIM -> MELEE_FIRE")
{
	Port223World w;
	Object *hero = w.unit("P223MountedHero", 'A', 150, 300);
	Object *runner = w.unit("P223Runner", 'B', 300, 300);
	runner->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner);

	const ObjectID runnerId = runner->getID();
	REQUIRE(w.ai(hero)->aiAttackObject(runner, CMD_FROM_PLAYER));
	w.frames(8);
	REQUIRE(w.innerState(hero) == (unsigned)MELEE_ENGAGE);

	// Stop the target
	w.ai(runner)->aiIdle(CMD_FROM_AI);
	bool clearedRunningDown = false;
	bool sawAimOrFire = false;
	for (int f = 0; f < 50 && w.byId(runnerId) != nullptr; ++f)
	{
		w.frames(1);
		if (!hero->testStatus((unsigned)CombatNames::statuses().runningDownFromBehind))
		{
			clearedRunningDown = true;
		}
		const unsigned s = w.innerState(hero);
		if (s == (unsigned)MELEE_AIM || s == (unsigned)MELEE_FIRE)
		{
			sawAimOrFire = true;
		}
	}

	CHECK(clearedRunningDown);
	CHECK(sawAimOrFire);
	CHECK(w.byId(runnerId) == nullptr);
}

TEST_CASE("PORT-223 (4): ATTACK_PURSUE (0x64) runs for a turreted unit chasing a slower PhysicsBehavior target and paces at 0.95 * victimSpeed in range (RW 0x746058 / 0x74AD8E)")
{
	Port223World w;
	Object *skirmisher = w.unit("P223TurretSkirmisher", 'A', 120, 300);
	Object *runner = w.unit("P223Runner", 'B', 340, 300);
	runner->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner);

	REQUIRE(w.ai(skirmisher)->aiAttackObject(runner, CMD_FROM_PLAYER));
	CHECK(w.innerState(skirmisher) == (unsigned)ATTACK_PURSUE);

	bool sawPacedSpeed = false;
	for (int f = 0; f < 40; ++f)
	{
		w.frames(1);
		CHECK(w.innerState(skirmisher) == (unsigned)ATTACK_PURSUE);
		const float desired = w.ai(skirmisher)->mover().desiredSpeed();
		const float vSpeed = w.ai(runner)->curLocomotor()->speed();
		if (std::fabs(desired - vSpeed * 0.95f) < 0.05f)
		{
			sawPacedSpeed = true;
			break;
		}
	}
	CHECK(sawPacedSpeed);

	// Against a faster target (140 >= 100), canPursue is false once target accelerates past ourMaxSpeed (20/frame), and Pursue hands over to ATTACK_APPROACH
	Object *fast = w.unit("P223FastRunner", 'B', 500, 450);
	fast->setOrientation(0.0f);
	w.frames(2);
	w.runEast(fast);
	for (int f = 0; f < 10 && w.ai(fast)->curLocomotor()->speed() < 20.0f; ++f)
	{
		w.frames(1);
	}
	// tolerance: fast runner must accelerate past ourMaxSpeed (20/frame) so canPursue returns false
	REQUIRE(w.ai(fast)->curLocomotor()->speed() >= 20.0f);
	REQUIRE(w.ai(skirmisher)->aiAttackObject(fast, CMD_FROM_PLAYER));
	CHECK(w.innerState(skirmisher) == (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-223 (5): Approach (0x65) tests range at the 4-frame predicted position (RW 0xBD88C0 = 4.0f) against a fleeing target")
{
	Port223World w;
	// P223RangedChaser has AttackRange = 60, radii 8 + 8 = 16; Runner speed = 10/frame -> 4-frame lead = 40 units.
	// RW 0x74E7A8 (Approach::onEnter) tests unpredicted range; RW 0x749E09 (Approach::update) tests 4-frame predicted range.
	// Start 80 units behind (edge 64 > 60 -> enters Approach) and close inside current range (edge <= 60) while 4-frame predicted edge > 60.
	Object *chaser = w.unit("P223RangedChaser", 'A', 200, 300);
	Object *runner = w.unit("P223Runner", 'B', 280, 300);
	runner->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner);

	const Coord3D behind{ runner->getPosition()->x - 80.0f, 300.0f, 0.0f };
	chaser->setPosition(&behind);
	REQUIRE(w.ai(chaser)->aiAttackObject(runner, CMD_FROM_PLAYER));
	// Non-turreted: passes immediately through Pursue -> Approach (edge 64 > 60 at onEnter)
	REQUIRE(w.innerState(chaser) == (unsigned)ATTACK_APPROACH);
	// Advance until inside current AttackRange (60) while still outside 4-frame predicted range
	bool sawInsideCurrentButStillApproaching = false;
	for (int f = 0; f < 10; ++f)
	{
		w.frames(1);
		const float edge = runner->getPosition()->x - chaser->getPosition()->x - 16.0f;
		if (edge <= 60.0f && edge > 20.0f)
		{
			CHECK(w.innerState(chaser) == (unsigned)ATTACK_APPROACH);
			sawInsideCurrentButStillApproaching = true;
			break;
		}
	}
	CHECK(sawInsideCurrentButStillApproaching);

	// Keep running until chaser closes inside predicted range and transitions to Aim/Fire
	bool leftApproach = false;
	for (int f = 0; f < 25; ++f)
	{
		w.frames(1);
		if (w.innerState(chaser) != (unsigned)ATTACK_APPROACH)
		{
			leftApproach = true;
			break;
		}
	}
	CHECK(leftApproach);
}

TEST_CASE("PORT-223 (6): snapshot xfer round-trip preserves AIAttackPursueState (0x64) and AIAttackMeleeEngageState (0xE2) m_fireState")
{
	Port223World w;
	Object *hero = w.unit("P223MountedHero", 'A', 150, 300);
	Object *runner = w.unit("P223Runner", 'B', 220, 300);
	runner->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner);
	REQUIRE(w.ai(hero)->aiAttackObject(runner, CMD_FROM_PLAYER));

	bool reachedPreAttack = false;
	for (int f = 0; f < 25; ++f)
	{
		w.frames(1);
		if (hero->testStatus((unsigned)CombatNames::statuses().runningDownFromBehind) && w.innerState(hero) == (unsigned)MELEE_ENGAGE)
		{
			reachedPreAttack = true;
			break;
		}
	}
	REQUIRE(reachedPreAttack);

	AIAttackState *att = w.attackState(hero);
	REQUIRE(att != nullptr);
	StateHasher hBefore;
	att->crc(hBefore);
	const std::uint32_t crcBefore = hBefore.value();

	XferSave save;
	att->xfer(save);
	const std::vector<std::uint8_t> bytes = save.data();
	REQUIRE(!bytes.empty());

	XferLoad load(bytes);
	att->xfer(load);
	CHECK(load.loadBytesLeftInBlock() == 0);

	StateHasher hAfter;
	att->crc(hAfter);
	CHECK(hAfter.value() == crcBefore);

	// Also round-trip Pursue (0x64)
	Object *skirmisher = w.unit("P223TurretSkirmisher", 'A', 120, 450);
	Object *runner2 = w.unit("P223Runner", 'B', 300, 450);
	runner2->setOrientation(0.0f);
	w.frames(2);
	w.runEast(runner2);
	REQUIRE(w.ai(skirmisher)->aiAttackObject(runner2, CMD_FROM_PLAYER));
	REQUIRE(w.innerState(skirmisher) == (unsigned)ATTACK_PURSUE);
	w.frames(3);
	AIAttackState *att2 = w.attackState(skirmisher);
	REQUIRE(att2 != nullptr);
	StateHasher h2Before;
	att2->crc(h2Before);
	XferSave save2;
	att2->xfer(save2);
	XferLoad load2(save2.data());
	att2->xfer(load2);
	CHECK(load2.loadBytesLeftInBlock() == 0);
	StateHasher h2After;
	att2->crc(h2After);
	CHECK(h2After.value() == h2Before.value());
}

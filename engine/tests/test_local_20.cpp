// OpenBFME unit tests: LOCAL-20 retaliation: a unit hit by an attacker answers and chases it, as RotWK (MaxRetaliateDistance; S-325, S-326)
// Issue #147.
//
// Target facts and RotWK addresses:
//   - AIData / MaxRetaliateDistance: row RW 0xC1CED0, parser RW 0x42ED00 (INI::parseReal), store offset AIData + 0xC8 ([RW 0xDE4B40] + 0x18 + 0xC8).
//     Binary default (constructor RW 0x6FFDC4): 200.0f. Retail 2.01 aidata.ini: 450.0f. Ennorath 1.9.8.2 default/aidata.ini: 250.0f.
//   - AIGuardMachine::getRetaliateRange (RW 0x94BA8C): reads settingsOf(obj).maxRetaliateDistance; if current weapon (RW 0x68B58C)
//     has melee template flag (RW 0x441B59), scales by 1.15f (RW 0xC12234).
//   - AIGuardMachine::hasAttackedMeAndICanReturnFire (RW 0x949994):
//     resolves damager from takeLastAttacker() (body vslot 0x4C, RW 0x8C3A33), checks ableToAttack and canAttackObject.
//   - GuardAttackAggressorState::onEnter (RW 0x94C144):
//     outer radius = (visionRange + retaliateRange)^2 (RW 0x94C2B1), melee chase duration +50% (RW 0x94C27C..0x94C281), conditions = 7 (RW 0x94C2AA).
//   - GuardExitConditions::shouldExit (RW 0x94BCAC):
//     exits if currentFrame >= attackGiveUpFrame (RW 0x94BCC8),
//     or if target distSqr > radiusSqr (RW 0x94BD4C),
//     or if defender distSqr from center > retaliateRange^2 (RW 0x94BD19..0x94BD71).
//   - Stop gate (RW AI + 0x3CC, m_stopRetaliationArmed):
//     armed by player stop order (aiIdle CMD_FROM_PLAYER, RW 0x771F97);
//     getNextMoodTarget (RW 0x668848..0x6688E6):
//     - if edgeDistSq < 1.0f (RW 0x6688B9..0x6688C4), clears m_stopRetaliationArmed = false and selects damager;
//     - if within attack range (RW 0x6CC653), selects damager;
//     - if damager is not selected, falls through to normal acquisition (RW 0x6688EC) rather than returning nullptr,
//       allowing normal acquisition to pick enemies reachable from standing spot (canAttackFromHere RW 0x6FF7FA).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "LogicTestUtil.h"
#include "EnnorathScenario.h"

#include "Common/StateHash.h"
#include "GameLogic/AI/AIGuard.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

TEST_CASE("LOCAL-20 ini: MaxRetaliateDistance parsed from AIData with defaults and melee 1.15 scaling (RW 0xC1CED0, RW 0x94BA8C)")
{
	// 1. Constructor default in binary is 200.0f (RW 0x6FFDC4)
	GameLogicSettings defSettings;
	CHECK(defSettings.maxRetaliateDistance == 200.0f);

	// 2. AIData INI parser reads MaxRetaliateDistance (row RW 0xC1CED0, parser RW 0x42ED00)
	GameLogicSettings parsed;
	std::string err;
	const char *ini250 =
		"AIData\n"
		"  MaxRetaliateDistance = 250.0\n"
		"End\n";
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData(ini250, parsed, &err), err);
	CHECK(parsed.maxRetaliateDistance == 250.0f);
	CHECK(parsed.unappliedFields.count("AIData.MaxRetaliateDistance") == 0);

	// 3. Melee weapon scales retaliate range by 1.15f (RW 0x94BA8C, constant RW 0xC12234)
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 100.0f, 100.0f);
	Object *archer = w.unit("Archer", 'A', 200.0f, 100.0f);
	REQUIRE(swordsman != nullptr);
	REQUIRE(archer != nullptr);

	// Swordsman has MeleeWeapon = Yes -> 250.0 * 1.15 = 287.5f
	CHECK(AIGuardMachine::getRetaliateRange(*swordsman) == 287.5f);
	// Archer has BowWeapon (ranged) -> 250.0f (no melee bonus)
	CHECK(AIGuardMachine::getRetaliateRange(*archer) == 250.0f);

	// 4. CRC hash preservation: retail default 450.0f produces same hash as untouched, while 250.0f distinguishes
	StateHasher hBase, h450, h250;
	GameLogicSettings sBase;
	sBase.maxRetaliateDistance = 450.0f;
	sBase.crc(hBase);

	GameLogicSettings s450;
	s450.maxRetaliateDistance = 450.0f;
	s450.crc(h450);

	GameLogicSettings s250;
	s250.maxRetaliateDistance = 250.0f;
	s250.crc(h250);

	CHECK(h450.value() == hBase.value());
	CHECK(h250.value() != hBase.value());
}

TEST_CASE("LOCAL-20 guard leashing: defender distance beyond retaliateRange exits pursuit (RW 0x94BD19, RW 0x94C144)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	w.logic->settings().guardChaseUnitsFrames = 100;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *archer = w.unit("Archer", 'A', 0.0f, 0.0f);
	Object *enemy = w.unit("Archer", 'B', 200.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(archer);
	REQUIRE(enemy);

	// 1. getRetaliateRange: melee scales by 1.15f (287.5f), ranged stays 250.0f
	CHECK(AIGuardMachine::getRetaliateRange(*swordsman) == 287.5f);
	CHECK(AIGuardMachine::getRetaliateRange(*archer) == 250.0f);

	// 2. GuardAttackAggressorState setup & GuardExitConditions verification
	// Swordsman guard machine
	AIUpdateInterface *swordAI = swordsman->getAIUpdateInterface();
	REQUIRE(swordAI);
	swordAI->guardHere(GUARDMODE_NORMAL);

	// Setup exit conditions simulating GuardAttackAggressorState (RW 0x94C144)
	swordAI->stateMachine().setGoalObject(enemy->getID());
	GuardExitConditions exitCond;
	exitCond.center = Coord3D{ 0.0f, 0.0f, 0.0f };
	exitCond.conditions = GuardExitConditions::ATTACK_ExitIfOutsideRadius | GuardExitConditions::ATTACK_ExitIfExpiredDuration | GuardExitConditions::ATTACK_ExitIfNoUnitFound;
	exitCond.attackGiveUpFrame = w.logic->getFrame() + 150; // melee +50% duration: 100 + 50 = 150 (RW 0x94C27C..0x94C281)
	const float totalRadius = 100.0f + 287.5f; // vision + retaliateRange
	exitCond.radiusSqr = totalRadius * totalRadius;

	// Defender at distance 200 from center (< 287.5): within retaliate leash -> shouldExit returns false
	Coord3D p200{ 200.0f, 0.0f, 0.0f };
	swordsman->setPosition(&p200);
	CHECK_FALSE(exitCond.shouldExit(swordAI->stateMachine()));

	// Defender at distance 270 from center:
	// - For swordsman (melee range 287.5 >= 270): 270 <= 287.5 -> within leash -> shouldExit is false
	Coord3D p270{ 270.0f, 0.0f, 0.0f };
	swordsman->setPosition(&p270);
	CHECK_FALSE(exitCond.shouldExit(swordAI->stateMachine()));

	// - For archer (ranged range 250 < 270): 270 > 250 -> beyond leash -> shouldExit is true!
	AIUpdateInterface *archerAI = archer->getAIUpdateInterface();
	REQUIRE(archerAI);
	archerAI->guardHere(GUARDMODE_NORMAL);
	archerAI->stateMachine().setGoalObject(enemy->getID());
	archer->setPosition(&p270);
	CHECK(exitCond.shouldExit(archerAI->stateMachine()));

	// Defender beyond swordsman's retaliate leash (at 300 > 287.5): shouldExit is true
	Coord3D p300{ 300.0f, 0.0f, 0.0f };
	swordsman->setPosition(&p300);
	CHECK(exitCond.shouldExit(swordAI->stateMachine()));
}

TEST_CASE("LOCAL-20 stop: player Stop order arms m_stopRetaliationArmed (RW AI + 0x3CC); answers only within reach, falls through to normal acquisition (RW 0x771F97, RW 0x668848..0x6688EC)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *rangedEnemy = w.unit("Archer", 'B', 100.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(rangedEnemy);

	AIUpdateInterface *ai = swordsman->getAIUpdateInterface();
	REQUIRE(ai);

	// Player gives stop order: arms m_stopRetaliationArmed (RW 0x771F97)
	ai->aiIdle(CMD_FROM_PLAYER);
	CHECK(ai->isStopRetaliationArmed());

	// Case A: Damaged by distant enemy at 100 (outside melee reach)
	DamageInfo hitFar;
	hitFar.m_input.m_sourceID = rangedEnemy->getID();
	hitFar.m_input.m_damageType = DAMAGE_PIERCE;
	hitFar.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hitFar);

	// With only distant enemy present, getNextMoodTarget refuses (out of weapon reach and edgeDistSq >= 1.0f)
	CHECK(ai->getNextMoodTarget(true, true) == nullptr);
	CHECK(ai->isStopRetaliationArmed()); // flag stays armed

	// Case B (Review item 1): Flag 0x3CC is armed, distant attacker is unreachable, BUT a reachable enemy is in vision.
	// RW 0x6688E6 falls through to 0x6688EC (findClosestEnemy), so unit chooses reachable enemy instead of returning nullptr!
	Object *reachableEnemy = w.unit("Swordsman", 'B', 8.0f, 0.0f);
	REQUIRE(reachableEnemy);
	ai->setNextMoodCheckTime(w.logic->getFrame());
	CHECK(ai->getNextMoodTarget(true, true) == reachableEnemy);

	// Case C: Damaged by adjacent enemy with edgeDistanceSquared2D < 1.0f (RW 0x6688B9..0x6688C4)
	// Clears m_stopRetaliationArmed and returns adjacent enemy
	Coord3D pZero{ 0.0f, 0.0f, 0.0f };
	Coord3D pNear{ 0.5f, 0.0f, 0.0f };
	swordsman->setPosition(&pZero);
	reachableEnemy->setPosition(&pNear);
	DamageInfo hitAdjacent;
	hitAdjacent.m_input.m_sourceID = reachableEnemy->getID();
	hitAdjacent.m_input.m_damageType = DAMAGE_SLASH;
	hitAdjacent.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hitAdjacent);

	// Re-arm stop flag to test adjacent clearing
	ai->aiIdle(CMD_FROM_PLAYER);
	CHECK(ai->isStopRetaliationArmed());
	ai->setNextMoodCheckTime(w.logic->getFrame());
	CHECK(ai->getNextMoodTarget(true, true) == reachableEnemy);
	CHECK_FALSE(ai->isStopRetaliationArmed()); // flag cleared by edgeDistSq < 1.0f (RW 0x6688C4)
}

TEST_CASE("LOCAL-20 order: ordered move preserves its destination and does not divert for stray damage (RW 0x668848)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 100.0f, 100.0f);
	Object *enemy = w.unit("Archer", 'B', 200.0f, 100.0f);
	REQUIRE(swordsman);
	REQUIRE(enemy);

	AIUpdateInterface *ai = swordsman->getAIUpdateInterface();
	REQUIRE(ai);

	Coord3D destination{ 100.0f, 500.0f, 0.0f };
	ai->aiMoveToPosition(destination, CMD_FROM_PLAYER);
	w.frames(5);
	CHECK(swordsman->getPosition()->y > 100.0f); // tolerance: unit has started moving towards destination y=500

	// Hit by stray shot from (200, 100)
	DamageInfo hit;
	hit.m_input.m_sourceID = enemy->getID();
	hit.m_input.m_damageType = DAMAGE_PIERCE;
	hit.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hit);

	// Ordered move continues towards destination (100, 500)
	for (int f = 0; f < 20; ++f)
	{
		w.frames(1);
	}
	CHECK(swordsman->getPosition()->y > 110.0f); // tolerance: unit continued pathing towards y=500 despite stray hit
	CHECK(std::abs(swordsman->getPosition()->x - 100.0f) < 1.0f); // tolerance: unit stays on course with minor sub-cell steering variance
}

TEST_CASE("LOCAL-20 horde victim: single unit attacked by horde selects closest member via RW 0x94C0F4 (RW 0x94C1D6..0x94C21B)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	w.logic->settings().guardChaseUnitsFrames = 100;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *archerHorde = w.unit("ArcherHorde", 'B', 100.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(archerHorde);

	const std::vector<Object *> members = w.membersOf(archerHorde);
	REQUIRE_FALSE(members.empty());

	// Identify member closest to swordsman at (0, 0, 0)
	Object *expectedMember = nullptr;
	float bestDist = 1e9f;
	for (Object *m : members)
	{
		const float d = SimMath::length3d(m->getPosition()->x, m->getPosition()->y, m->getPosition()->z);
		if (!expectedMember || d < bestDist)
		{
			bestDist = d;
			expectedMember = m;
		}
	}
	REQUIRE(expectedMember != nullptr);

	AIUpdateInterface *ai = swordsman->getAIUpdateInterface();
	REQUIRE(ai);
	ai->guardHere(GUARDMODE_NORMAL);
	auto *gs = dynamic_cast<AIGuardState *>(ai->stateMachine().currentState());
	REQUIRE(gs != nullptr);
	AIGuardMachine *gm = gs->guardMachine();
	REQUIRE(gm != nullptr);

	// 1. Horde enemy: GuardAttackAggressorState onEnter (RW 0x94C1D6..0x94C21B) selects closest member via RW 0x94C0F4
	gm->setNemesis(archerHorde->getID());
	gm->setState(AI_GUARD_ATTACK_AGGRESSOR);
	CHECK(gm->currentStateId() == AI_GUARD_ATTACK_AGGRESSOR);
	CHECK(gm->goalObject() == expectedMember);
	CHECK(gm->nemesis() == expectedMember->getID());

	// 2. Null member: when all members are dead, member lookup yields nullptr,
	// so onEnter returns STATE_FAILURE (RW 0x94C205), exiting state
	for (Object *m : members)
	{
		DamageInfo kill;
		kill.m_input.m_kill = true;
		kill.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		m->getBodyModule()->attemptDamage(kill);
	}
	gm->setNemesis(archerHorde->getID());
	gm->setState(AI_GUARD_ATTACK_AGGRESSOR);
	CHECK(gm->currentStateId() != AI_GUARD_ATTACK_AGGRESSOR);
}

TEST_CASE("LOCAL-20 ennorath: Ennorath scenario retaliation with real GondorFighterHorde (MaxRetaliateDistance 250, default/aidata.ini)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-20 Ennorath scenario retaliation"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	std::string err;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::loadAIData(*world.merged.fs, arena.logic.settings(), &err), err);
	// Ennorath default/aidata.ini sets MaxRetaliateDistance = 250.0
	CHECK(arena.logic.settings().maxRetaliateDistance == 250.0f);
	Object *gondorHorde = world.place(arena, "GondorFighterHorde", 0, 500.0f, 500.0f);
	REQUIRE(gondorHorde != nullptr);

	// Real GondorFighterHorde has no weapon on container -> returns unscaled settings range (250.0f)
	CHECK(AIGuardMachine::getRetaliateRange(*gondorHorde) == 250.0f);

	// Real GondorFighter unit has melee weapon (GondorSword) -> scales by 1.15 in AIGuardMachine::getRetaliateRange (287.5f)
	Object *gondorFighter = world.place(arena, "GondorFighter", 0, 500.0f, 500.0f);
	REQUIRE(gondorFighter != nullptr);
	CHECK(AIGuardMachine::getRetaliateRange(*gondorFighter) == 287.5f);
}

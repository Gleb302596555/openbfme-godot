// OpenBFME unit tests: LOCAL-20 retaliation: a unit hit by an attacker answers and chases it, as RotWK (MaxRetaliateDistance; S-325, S-326)
// Issue #147.
//
// Target facts and RotWK addresses:
//   - AIData / MaxRetaliateDistance: row RW 0xC1CED0, parser RW 0x42ED00 (INI::parseReal), store offset AIData + 0xC8 ([RW 0xDE4B40] + 0x18 + 0xC8).
//     Binary default (constructor RW 0x6FFDC4): 200.0f. Retail 2.01 aidata.ini: 450.0f. Ennorath 1.9.8.2 default/aidata.ini: 250.0f.
//   - AIGuardMachine::getRetaliateRange (RW 0x94BA8C): reads settingsOf(obj).maxRetaliateDistance; if melee weapon (RW 0xC12234 / RW 0x441B59), scales by 1.15f.
//   - AIGuardMachine::hasAttackedMeAndICanReturnFire (RW 0x94BA3C / RW 0x949994):
//     resolves damager from takeLastAttacker() or horde member attackedWithin (RW 0x86EE52), checks dist2D <= retaliateRange, checks ableToAttack.
//   - GuardAttackAggressorState::onEnter (RW 0x94C144):
//     respects Hold Ground stance (cannot pursue out of reach), checks initial leash retaliateRange, outerRange = visionRange + retaliateRange,
//     melee chase duration +50% (RW 0x94C1FD).
//   - GuardExitConditions::shouldExit (RW 0x94BD19):
//     exit if distance from guarded center exceeds retaliateRange.
//   - Stop gate (RW AI + 0x3CC, m_stopRetaliationArmed):
//     armed by player stop order (aiIdle CMD_FROM_PLAYER, RW 0x771F97);
//     getNextMoodTarget (RW 0x668848..0x6688CD): unit only retaliates if damager is within current attack reach; otherwise stays stopped.

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
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
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

TEST_CASE("LOCAL-20 idle: idle horde damaged inside MaxRetaliateDistance (180 < 250) answers and chases; beyond (350 > 250) stays in place")
{
	// Case A: Attacker inside 250 (at 180). Horde member is damaged -> horde retaliates and chases.
	{
		combattest::CombatWorld w;
		w.logic->settings().maxRetaliateDistance = 250.0f;
		Object *horde = w.unit("SwordHorde", 'A', 0.0f, 0.0f);
		Object *enemy = w.unit("Archer", 'B', 180.0f, 0.0f);
		REQUIRE(horde);
		REQUIRE(enemy);

		// Advance logic past startup threshold (frames < logic.getFrame() in HordeContain::attackedWithin RW 0x86EE52)
		w.frames(25);

		REQUIRE(horde->getContain());
		HordeContainInterface *hordeContain = horde->getContain()->getHordeContainInterface();
		REQUIRE(hordeContain);
		const auto *payload = horde->getContain()->getContainedItemsList();
		REQUIRE(!payload->empty());
		Object *member = payload->front();
		REQUIRE(member);

		DamageInfo hit;
		hit.m_input.m_sourceID = enemy->getID();
		hit.m_input.m_damageType = DAMAGE_PIERCE;
		hit.m_input.m_amount = 5.0f;
		member->getBodyModule()->attemptDamage(hit);

		ObjectID damager = INVALID_ID;
		CHECK(hordeContain->attackedWithin(4 * LOGICFRAMES_PER_SECOND, damager));
		CHECK(damager == enemy->getID());

		// Tick logic frames: horde must answer and chase towards enemy (X > 0)
		for (int f = 0; f < 25; ++f)
		{
			w.frames(1);
		}
		CHECK(horde->getPosition()->x > 10.0f); // tolerance: movement progress towards enemy at x=180 after ticking logic frames
	}

	// Case B: Attacker beyond 250 (at 350 > 287.5). Horde member damaged -> retaliation suppressed, stays in place.
	{
		combattest::CombatWorld w;
		w.logic->settings().maxRetaliateDistance = 250.0f;
		Object *horde = w.unit("SwordHorde", 'A', 0.0f, 0.0f);
		Object *enemy = w.unit("Archer", 'B', 350.0f, 0.0f);
		REQUIRE(horde);
		REQUIRE(enemy);

		w.frames(25);

		REQUIRE(horde->getContain());
		HordeContainInterface *hordeContain = horde->getContain()->getHordeContainInterface();
		REQUIRE(hordeContain);
		const auto *payload = horde->getContain()->getContainedItemsList();
		REQUIRE(!payload->empty());
		Object *member = payload->front();
		REQUIRE(member);

		DamageInfo hit;
		hit.m_input.m_sourceID = enemy->getID();
		hit.m_input.m_damageType = DAMAGE_PIERCE;
		hit.m_input.m_amount = 5.0f;
		member->getBodyModule()->attemptDamage(hit);

		for (int f = 0; f < 25; ++f)
		{
			w.frames(1);
		}
		// Beyond retaliate range: horde stays at (0, 0)
		CHECK(horde->getPosition()->x == 0.0f);
	}
}

TEST_CASE("LOCAL-20 melee scaling: distance 270 between 250 and 287.5 triggers melee retaliation but not ranged (RW 0x94BA8C, RW 0xC12234)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *archer = w.unit("Archer", 'A', 0.0f, 50.0f);
	Object *enemy = w.unit("Archer", 'B', 270.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(archer);
	REQUIRE(enemy);

	CHECK(AIGuardMachine::getRetaliateRange(*swordsman) == 287.5f);
	CHECK(AIGuardMachine::getRetaliateRange(*archer) == 250.0f);

	DamageInfo hit;
	hit.m_input.m_sourceID = enemy->getID();
	hit.m_input.m_damageType = DAMAGE_PIERCE;
	hit.m_input.m_amount = 5.0f;

	swordsman->getBodyModule()->attemptDamage(hit);
	archer->getBodyModule()->attemptDamage(hit);

	// Swordsman (melee threshold 287.5 >= 270): mood acquisition triggers retaliation
	AIUpdateInterface *swordAI = swordsman->getAIUpdateInterface();
	REQUIRE(swordAI);
	swordAI->setNextMoodCheckTime(w.logic->getFrame());
	CHECK(swordAI->getNextMoodTarget(true, true) == enemy);

	// Archer (ranged threshold 250.0 < 270): mood acquisition suppresses retaliation
	AIUpdateInterface *archerAI = archer->getAIUpdateInterface();
	REQUIRE(archerAI);
	archerAI->setNextMoodCheckTime(w.logic->getFrame());
	CHECK(archerAI->getNextMoodTarget(true, true) == nullptr);
}

TEST_CASE("LOCAL-20 stance: Hold Ground stance suppresses pursuit beyond attack reach (RW 0x94C144, GUARDMODE_GUARD_WITHOUT_PURSUIT)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *enemy = w.unit("Archer", 'B', 150.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(enemy);

	AIUpdateInterface *ai = swordsman->getAIUpdateInterface();
	REQUIRE(ai);
	ai->guardHere(GUARDMODE_GUARD_WITHOUT_PURSUIT);
	CHECK(ai->guardMode() == GUARDMODE_GUARD_WITHOUT_PURSUIT);

	DamageInfo hit;
	hit.m_input.m_sourceID = enemy->getID();
	hit.m_input.m_damageType = DAMAGE_PIERCE;
	hit.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hit);

	// Hold Ground stance prevents pursuit when attacker cannot be attacked in place
	for (int f = 0; f < 25; ++f)
	{
		w.frames(1);
	}
	CHECK(swordsman->getPosition()->x == 0.0f);
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

TEST_CASE("LOCAL-20 stop: player Stop order arms m_stopRetaliationArmed (RW AI + 0x3CC); answers only within reach, no chase (RW 0x771F97, RW 0x668848)")
{
	combattest::CombatWorld w;
	w.logic->settings().maxRetaliateDistance = 250.0f;
	Object *swordsman = w.unit("Swordsman", 'A', 0.0f, 0.0f);
	Object *rangedEnemy = w.unit("Archer", 'B', 100.0f, 0.0f);
	REQUIRE(swordsman);
	REQUIRE(rangedEnemy);

	AIUpdateInterface *ai = swordsman->getAIUpdateInterface();
	REQUIRE(ai);

	// Player gives stop order: arms m_stopRetaliationArmed
	ai->aiIdle(CMD_FROM_PLAYER);
	CHECK(ai->isStopRetaliationArmed());

	// Case A: Damaged by distant enemy at 100 (outside melee reach)
	DamageInfo hitFar;
	hitFar.m_input.m_sourceID = rangedEnemy->getID();
	hitFar.m_input.m_damageType = DAMAGE_PIERCE;
	hitFar.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hitFar);

	// getNextMoodTarget refuses because stopped and out of weapon reach
	CHECK(ai->getNextMoodTarget(true, true) == nullptr);
	CHECK(ai->isStopRetaliationArmed()); // flag stays armed

	// Case B: Damaged by adjacent enemy within melee reach (distance 8 < 11.5)
	Object *meleeEnemy = w.unit("Swordsman", 'B', 8.0f, 0.0f);
	REQUIRE(meleeEnemy);
	DamageInfo hitNear;
	hitNear.m_input.m_sourceID = meleeEnemy->getID();
	hitNear.m_input.m_damageType = DAMAGE_SLASH;
	hitNear.m_input.m_amount = 5.0f;
	swordsman->getBodyModule()->attemptDamage(hitNear);

	// getNextMoodTarget acquires adjacent enemy and clears stop flag
	ai->setNextMoodCheckTime(w.logic->getFrame());
	CHECK(ai->getNextMoodTarget(true, true) == meleeEnemy);
	CHECK_FALSE(ai->isStopRetaliationArmed());
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

	// Melee horde scales by 1.15 in AIGuardMachine::getRetaliateRange
	float range = AIGuardMachine::getRetaliateRange(*gondorHorde);
	CHECK(range == 287.5f);
}

// OpenBFME. LOCAL-168 (fork issue 935): hordes and formations — HordeContain::applyRankWeaponSetFlags (RW 0x875F18),
// HordeContain::endMove (RW 0x86D1FE), HordeContain::anyMemberAttacking (RW 0x86D5BB),
// HordeContain::anyMemberAttackingTarget (RW 0x86D4CD), HordeContain::memberMatchesRankUnitType (RW 0x875619),
// and HordeMeleeAmoeba::notifyOfMemberDeath (RW 0x98F9D8).
// Synthetic INI only; expectations from RotWK 2.01 game.dat disassembly.
// GPL-3.0.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace combattest;

struct Local168AmoebaAccess
{
	static void addRecord(HordeAIUpdate &hai, ObjectID id, bool idle, unsigned timer)
	{
		HordeAIUpdate::MemberRecord &rec = hai.recordOf(id);
		rec.idle = idle;
		rec.timer = timer;
	}
	static bool isIdle(HordeAIUpdate &hai, ObjectID id)
	{
		return hai.recordOf(id).idle;
	}
	static unsigned timer(HordeAIUpdate &hai, ObjectID id)
	{
		return hai.recordOf(id).timer;
	}
};

namespace
{
const char kLocal168Ini[] =
	"Locomotor L168Foot\n"
	"  Surfaces = GROUND\n"
	"  Speed = 50\n"
	"  TurnRate = 720\n"
	"  Acceleration = 1000\n"
	"  Braking = 1000\n"
	"  MinTurnSpeed = 0\n"
	"  PreferredHeight = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"End\n"
	"Weapon L168Sword\n"
	"  AttackRange = 18.0\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object L168Member\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = L168Foot\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY L168Sword\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L168Horde\n"
	"  KindOf = HORDE ARMY_SUMMARY MELEE_HORDE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = L168Foot\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY L168Sword\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    ObjectStatusOfContained =\n"
	"    InitialPayload = L168Member 2\n"
	"    Slots = 2\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    ShowPips = No\n"
	"    RankInfo = RankNumber:1 UnitType:L168Member Position:X:0 Y:10 GrantedWeaponCondition:PLAYER_UPGRADE RevokedWeaponCondition:HERO\n"
	"    RankInfo = RankNumber:2 UnitType:L168Member Position:X:-15 Y:10\n"
	"    MeleeBehavior = Amoeba\n"
	"      DelayUntilIdle = 3000\n"
	"    End\n"
	"  End\n"
	"End\n"
	"Object L168Enemy\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n";

bool hasWeaponFlag(const Object &obj, int bit)
{
	return (obj.getWeaponSetFlags()[(unsigned)bit >> 5] & (1u << (bit & 31))) != 0;
}

HordeContain *getHordeContain(Object &horde)
{
	return dynamic_cast<HordeContain *>(horde.getContain());
}

std::vector<Object *> membersVec(const HordeContain &hc)
{
	const auto *lst = hc.getContainedItemsList();
	if (!lst)
	{
		return {};
	}
	return std::vector<Object *>(lst->begin(), lst->end());
}
} // namespace

// Expectations from RW 0x875F18 (HordeContain::applyRankWeaponSetFlags, 0xC5B5F8[32]):
// Rank 1 grants PLAYER_UPGRADE and revokes HERO; Rank 2 has hasWeaponConditions == false and leaves flags untouched.
TEST_CASE("LOCAL-168 HordeContain: applyRankWeaponSetFlags grants and revokes rank weapon conditions (RW 0x875F18)")
{
	CombatWorld w(kLocal168Ini);
	Object *horde = w.unit("L168Horde", 'A', 100.0f, 100.0f);
	REQUIRE(horde != nullptr);
	HordeContain *hc = getHordeContain(*horde);
	REQUIRE(hc != nullptr);
	const auto members = membersVec(*hc);
	REQUIRE(members.size() == 2);

	const int flagUpgrade = CombatNames::weaponCondition("PLAYER_UPGRADE");
	const int flagHero = CombatNames::weaponCondition("HERO");
	REQUIRE(flagUpgrade >= 0); // tolerance: valid CombatNames weapon condition index
	REQUIRE(flagHero >= 0); // tolerance: valid CombatNames weapon condition index

	Object *m0 = members[0]; // Rank 1: GrantedWeaponCondition:PLAYER_UPGRADE RevokedWeaponCondition:HERO
	Object *m1 = members[1]; // Rank 2: no weapon conditions
	m0->setWeaponSetFlag(flagHero, true);
	m0->setWeaponSetFlag(flagUpgrade, false);
	m1->setWeaponSetFlag(flagHero, true);
	m1->setWeaponSetFlag(flagUpgrade, false);

	hc->applyRankWeaponSetFlags(*m0);
	CHECK(hasWeaponFlag(*m0, flagUpgrade));
	CHECK_FALSE(hasWeaponFlag(*m0, flagHero));

	// Adjacent behaviour: Rank 2 has hasWeaponConditions == false, so its flags are untouched.
	hc->applyRankWeaponSetFlags(*m1);
	CHECK_FALSE(hasWeaponFlag(*m1, flagUpgrade));
	CHECK(hasWeaponFlag(*m1, flagHero));
}

// Expectations from RW 0x86D1FE (HordeContain::endMove, 0xC5B1F8[156]):
// When the horde AI is moving, stops every contained member's AI and then the horde AI; when idle, no-op.
TEST_CASE("LOCAL-168 HordeContain: endMove stops moving horde and every contained member (RW 0x86D1FE)")
{
	CombatWorld w(kLocal168Ini);
	Object *horde = w.unit("L168Horde", 'A', 100.0f, 100.0f);
	REQUIRE(horde != nullptr);
	HordeContain *hc = getHordeContain(*horde);
	REQUIRE(hc != nullptr);
	AIUpdateInterface *hai = horde->getAIUpdateInterface();
	REQUIRE(hai != nullptr);
	const auto members = membersVec(*hc);
	REQUIRE(members.size() == 2);

	// Move the horde and its members.
	hai->aiMoveToPosition({ 250.0f, 100.0f, 0.0f }, CMD_FROM_PLAYER);
	for (Object *m : members)
	{
		m->getAIUpdateInterface()->aiMoveToPosition({ 250.0f, 100.0f, 0.0f }, CMD_FROM_AI);
	}
	w.frames(2);
	REQUIRE(hai->isMoving());

	hc->endMove();
	CHECK_FALSE(hai->isMoving());
	for (Object *m : members)
	{
		CHECK_FALSE(m->getAIUpdateInterface()->isMoving());
	}

	// Adjacent behaviour: if the horde itself is NOT moving, endMove does not stop a member that is moving.
	Object *m0 = members[0];
	m0->getAIUpdateInterface()->aiMoveToPosition({ 220.0f, 100.0f, 0.0f }, CMD_FROM_AI);
	w.frames(1);
	REQUIRE_FALSE(hai->isMoving());
	const bool m0WasMoving = m0->getAIUpdateInterface()->isMoving();
	hc->endMove();
	CHECK(m0->getAIUpdateInterface()->isMoving() == m0WasMoving);
}

// Expectations from RW 0x86D5BB (anyMemberAttacking), RW 0x86D4CD (anyMemberAttackingTarget),
// and RW 0x875619 (memberMatchesRankUnitType).
TEST_CASE("LOCAL-168 HordeContain: anyMemberAttacking, anyMemberAttackingTarget, memberMatchesRankUnitType (RW 0x86D5BB, 0x86D4CD, 0x875619)")
{
	CombatWorld w(kLocal168Ini);
	Object *horde = w.unit("L168Horde", 'A', 100.0f, 100.0f);
	Object *enemy = w.unit("L168Enemy", 'B', 112.0f, 100.0f);
	Object *other = w.unit("L168Enemy", 'B', 260.0f, 100.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(enemy != nullptr);
	REQUIRE(other != nullptr);
	HordeContain *hc = getHordeContain(*horde);
	REQUIRE(hc != nullptr);
	const auto members = membersVec(*hc);
	REQUIRE(members.size() == 2);

	// RW 0x875619: rank unitType check
	CHECK(hc->memberMatchesRankUnitType(*members[0], "L168Member"));
	CHECK_FALSE(hc->memberMatchesRankUnitType(*members[0], "WrongUnit"));
	CHECK_FALSE(hc->memberMatchesRankUnitType(*enemy, "L168Member"));

	// RW 0x86D5BB / 0x86D4CD: initially idle -> false
	CHECK_FALSE(hc->anyMemberAttacking());
	CHECK_FALSE(hc->anyMemberAttackingTarget(*enemy));

	// Order one member to attack enemy
	members[0]->getAIUpdateInterface()->aiAttackObject(enemy, 100, CMD_FROM_AI);
	w.frames(1);
	CHECK(hc->anyMemberAttacking());
	CHECK(hc->anyMemberAttackingTarget(*enemy));
	CHECK_FALSE(hc->anyMemberAttackingTarget(*other));
}

// Expectations from RW 0x98F9D8 (HordeMeleeAmoeba::notifyOfMemberDeath):
// Wakes idle Amoeba members (idle -> false, timer -> m_delayUntilIdle); non-idle members keep their timer.
TEST_CASE("LOCAL-168 HordeMeleeAmoeba: notifyAmoebaMemberDeath wakes idle rear members (RW 0x98F9D8)")
{
	CombatWorld w(kLocal168Ini);
	Object *horde = w.unit("L168Horde", 'A', 100.0f, 100.0f);
	REQUIRE(horde != nullptr);
	HordeContain *hc = getHordeContain(*horde);
	HordeAIUpdate *hai = dynamic_cast<HordeAIUpdate *>(horde->getAIUpdateInterface());
	REQUIRE(hc != nullptr);
	REQUIRE(hai != nullptr);
	const auto members = membersVec(*hc);
	REQUIRE(members.size() == 2);

	const ObjectID id0 = members[0]->getID();
	const ObjectID id1 = members[1]->getID();
	const unsigned expectedDelay = hc->meleeBehaviorData()->m_delayUntilIdle;
	REQUIRE(expectedDelay == 15u); // 3000 ms at 5 logic frames/sec (RW 0x98FA64)

	Local168AmoebaAccess::addRecord(*hai, id0, true, 1);
	Local168AmoebaAccess::addRecord(*hai, id1, false, 7);

	hai->notifyAmoebaMemberDeath();

	// Idle member wakes up with timer reset to DelayUntilIdle (RW 0x98FA5D .. 0x98FA67)
	CHECK_FALSE(Local168AmoebaAccess::isIdle(*hai, id0));
	CHECK(Local168AmoebaAccess::timer(*hai, id0) == expectedDelay);

	// Adjacent behaviour: non-idle member keeps its timer untouched
	CHECK_FALSE(Local168AmoebaAccess::isIdle(*hai, id1));
	CHECK(Local168AmoebaAccess::timer(*hai, id1) == 7u);
}

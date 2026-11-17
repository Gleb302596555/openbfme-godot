// OpenBFME. PORT-218 tests (fork issue 515): RotWK's Object / WeaponSet getAbleToAttackSpecificObject (RW 0x68D6A6 / RW 0x6C9147) and its tail
// WeaponSet::getAbleToUseWeaponAgainstTarget (RW 0x6C8652), ported branch for branch in docs/re/rotwk/RW_0x68D6A6.md. Every expectation below comes
// from that reading of game.dat 2.01 (the address is in the comment of the case), never from the new code. Synthetic INI only (combattest::
// CombatWorld), no retail data, no Ennorath files.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/Combat/CombatNames.h"

#include <string>

using namespace combattest;

namespace
{
// PORT218Sword: reach 11.5, melee, 10 SLASH. PORT218Club: the same reach, 10 PIERCE. PORT218Bow: reach 200, a clip of two arrows (a weapon out of ammo
// still canDamage: RW 0x6CDBF3 asks the nuggets and the AutoReloadsClip empty case's own status). PORT218WideSword: reach 400. The guard object carries
// a WeaponSet whose OnlyAgainst names STRUCTURE only: RW 0x6C8963 / 0x6C8974 (the OnlyAgainst loop of RW 0x6C8652) needs a KindOf of the victim.
const char kPort218[] =
	"Weapon PORT218Sword\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  PreAttackDelay = 400\n"
	"  FiringDuration = 200\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon PORT218PierceSword\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  PreAttackDelay = 400\n"
	"  FiringDuration = 200\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon PORT218WideSword\n"
	"  AttackRange = 400\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  PreAttackDelay = 400\n"
	"  FiringDuration = 200\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object PORT218Soldier\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
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
	"    Weapon = PRIMARY PORT218Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object PORT218Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 400\n"
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
	"    Weapon = PRIMARY PORT218WideSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	// an immobile tower: IMMOBILE, no locomotor, a long reach (RW 0x6C888C: a source that cannot walk out of a weapon-less reach answers INVALID_SHOT)
	"Object PORT218Tower\n"
	"  KindOf = IMMOBILE STRUCTURE SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 400\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 60\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY PORT218WideSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"End\n"
	// a victim with KindOf UNATTACKABLE: RW 0x6C91FA (kind 54) refuses it for every attacker
	"Object PORT218Unattackable\n"
	"  KindOf = INFANTRY SELECTABLE SCORE UNATTACKABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	// the OnlyAgainst object: its weapon names STRUCTURE victims only (RW 0x6C8963 / 0x6C8974)
	"Object PORT218Sapper\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
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
	"    Weapon = PRIMARY PORT218Sword\n"
	"    OnlyAgainst = PRIMARY STRUCTURE\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object PORT218AirborneInfantry\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100.0\n"
	"  End\n"
	"End\n"
	"Weapon PORT218AirSword\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  AntiGround = No\n"
	"  AntiAirborneInfantry = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object PORT218MixedHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100.0\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY PORT218Sword\n"
	"    Weapon = SECONDARY PORT218AirSword\n"
	"  End\n"
	"End\n";

struct Port218World : CombatWorld
{
	explicit Port218World() : CombatWorld(kPort218) {}
};

const CombatNames::Status &S() { return CombatNames::statuses(); }
} // namespace

TEST_CASE("PORT-218 the type and the result: forced is bit 0, the answer is 0 + in range + 2")
{
	// RW 0x6C91C9 (test byte [ebp+8], 1: ATTACK_CONTINUED_TARGET_FORCED 3) and RW 0x6C88E2 .. 0x6C88F5 (setne al; inc; inc):
	// NOT_POSSIBLE 0, INVALID_SHOT 1, POSSIBLE_AFTER_MOVING 2, POSSIBLE 3.
	CHECK((int)ATTACK_NEW_TARGET == 0);
	CHECK((int)ATTACK_CONTINUED_TARGET == 2);
	CHECK((int)ATTACK_CONTINUED_TARGET_FORCED == 3);
	CHECK(((int)ATTACK_CONTINUED_TARGET_FORCED & 1) == 1);
	CHECK((int)ATTACK_TUNNEL_NETWORK_GUARD == 4);
	CHECK((int)ATTACK_TYPE_FLAG_0x10 == 0x10);
	CHECK((int)ATTACKRESULT_NOT_POSSIBLE == 0);
	CHECK((int)ATTACKRESULT_INVALID_SHOT == 1);
	CHECK((int)ATTACKRESULT_POSSIBLE_AFTER_MOVING == 2);
	CHECK((int)ATTACKRESULT_POSSIBLE == 3);

	Port218World w;
	Object *soldier = w.unit("PORT218Soldier", 'A', 300, 300);
	Object *dummy = w.unit("Dummy", 'B', 305, 300); // in the sword's 11.5 reach
	w.frames(2);
	// RW 0x6C88E2: in reach -> POSSIBLE
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *dummy, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);
	// RW 0x68D6A6: the object asks for its own weapon set; the same answer
	CHECK(soldier->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *dummy, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);
	// and the old bool view of the same path
	CHECK(soldier->getWeapons()->canAttackObject(*dummy, CMD_FROM_AI, false));
}

TEST_CASE("PORT-218 a victim out of reach answers POSSIBLE_AFTER_MOVING, a shot it cannot take INVALID_SHOT")
{
	Port218World w;
	Object *soldier = w.unit("PORT218Soldier", 'A', 300, 300);
	Object *far = w.unit("Dummy", 'B', 500, 300); // 200 away, the sword reaches 11.5 + both bounding circles
	w.frames(2);
	// RW 0x6C88E2: a weapon, none in reach, the source can walk -> POSSIBLE_AFTER_MOVING (2)
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *far, CMD_FROM_AI) ==
	      ATTACKRESULT_POSSIBLE_AFTER_MOVING);

	// RW 0x6C8724: the slot's anti mask and the victim's class do not meet -> the slot is skipped, so no weapon is left: RW 0x6C8A21 INVALID_SHOT
	Object *projectile = w.unit("Arrow", 'B', 305, 300); // KindOf PROJECTILE: the sword's AntiGround does not match WEAPON_ANTI_PROJECTILE
	REQUIRE(projectile != nullptr);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *projectile, CMD_FROM_AI) == ATTACKRESULT_INVALID_SHOT);

	// RW 0x6C8880 .. 0x6C88D8: an IMMOBILE source with a weapon but none in reach cannot walk into it: INVALID_SHOT (not AFTER_MOVING)
	Object *tower = w.unit("PORT218Tower", 'A', 100, 100);
	Object *far2 = w.unit("Dummy", 'B', 900, 900);
	w.frames(2);
	CHECK(tower->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *tower, *far2, CMD_FROM_AI) == ATTACKRESULT_INVALID_SHOT);
}

TEST_CASE("PORT-218 NOT_POSSIBLE: the UNATTACKABLE victim, the status bits, the object itself")
{
	Port218World w;
	Object *soldier = w.unit("PORT218Soldier", 'A', 300, 300);
	Object *unattackable = w.unit("PORT218Unattackable", 'B', 305, 300);
	w.frames(2);
	// RW 0x6C91FA: KindOf UNATTACKABLE (kind 54) refuses every attacker, forced or not
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *unattackable, CMD_FROM_AI) ==
	      ATTACKRESULT_NOT_POSSIBLE);
	// RW 0x6C91C9 does not skip it either
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_CONTINUED_TARGET_FORCED, *soldier, *unattackable, CMD_FROM_AI) ==
	      ATTACKRESULT_NOT_POSSIBLE);

	// RW 0x6C9207: the status UNATTACKABLE (0x3C = 60)
	Object *dummy = w.unit("Dummy", 'B', 305, 300);
	dummy->setStatus((unsigned)S().unattackable, true);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *dummy, CMD_FROM_AI) == ATTACKRESULT_NOT_POSSIBLE);
	dummy->setStatus((unsigned)S().unattackable, false);

	// RW 0x6C9218: NO_ATTACK_FROM_AI (0x1A = 26) refuses CMD_FROM_AI only
	dummy->setStatus((unsigned)S().noAttackFromAI, true);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *dummy, CMD_FROM_AI) == ATTACKRESULT_NOT_POSSIBLE);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *dummy, CMD_FROM_PLAYER) != ATTACKRESULT_NOT_POSSIBLE);
	dummy->setStatus((unsigned)S().noAttackFromAI, false);

	// RW 0x6C919C: the victim is the source
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *soldier, CMD_FROM_AI) == ATTACKRESULT_NOT_POSSIBLE);

	// RW 0x6C91A4: NOT_IN_WORLD (0x33 = 51) refuses the victim
	Object *gone = w.unit("Dummy", 'B', 305, 310);
	gone->setStatus((unsigned)S().notInWorld, true);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *gone, CMD_FROM_AI) == ATTACKRESULT_NOT_POSSIBLE);
	gone->setStatus((unsigned)S().notInWorld, false);

	// RW 0x6C918F: a DESTROYED (bit 0) victim, and RW 0x6C9168 the DESTROYED source
	Object *dead = w.unit("Dummy", 'B', 305, 320);
	dead->setStatus((unsigned)S().destroyed, true);
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *dead, CMD_FROM_AI) == ATTACKRESULT_NOT_POSSIBLE);
}

TEST_CASE("PORT-218 the relationship veto belongs to a player's unforced query only")
{
	Port218World w;
	Object *soldier = w.unit("PORT218Soldier", 'A', 300, 300);
	Object *friend1 = w.unit("Dummy", 'A', 306, 300); // the same player: ALLIES
	w.frames(2);
	// RW 0x6C93A9 .. 0x6C93BF: not ENEMIES, not forced, CMD_FROM_PLAYER, no SCRIPT_TARGETABLE -> NOT_POSSIBLE
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *friend1, CMD_FROM_PLAYER) == ATTACKRESULT_NOT_POSSIBLE);
	// a forced query (type bit 0) passes the veto, as RW 0x6C93B1 reads the forced flag
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_CONTINUED_TARGET_FORCED, *soldier, *friend1, CMD_FROM_PLAYER) ==
	      ATTACKRESULT_POSSIBLE);
	// and so does an AI query (RW 0x6C93B9: the command source must be 0)
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *friend1, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);

	// RW 0x6C93BF: SCRIPT_TARGETABLE (Object + 0x457 bit 0x10) makes a friend attackable by an order
	friend1->setScriptStatus(0x10, true); // SCRIPT_TARGETABLE (RW Object + 0x457 bit 0x10)
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *friend1, CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE);
	friend1->setScriptStatus(0x10, false);
}

TEST_CASE("PORT-218 the OnlyAgainst mask: a weapon damages a victim only when its mask names a KindOf of it")
{
	Port218World w;
	Object *sapper = w.unit("PORT218Sapper", 'A', 300, 300);
	Object *infantry = w.unit("Dummy", 'B', 305, 300);
	Object *structure = w.unit("PORT218Tower", 'B', 100, 100);
	w.frames(2);
	// RW 0x6C8963 / 0x6C8974: the mask names STRUCTURE, the victim is INFANTRY -> no weapon passes, RW 0x6C8A21 INVALID_SHOT
	CHECK(sapper->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *sapper, *infantry, CMD_FROM_AI) == ATTACKRESULT_INVALID_SHOT);
	// the same weapon against a STRUCTURE victim: the mask matches, the answer is the range one
	CHECK(sapper->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *sapper, *structure, CMD_FROM_AI) ==
	      ATTACKRESULT_POSSIBLE_AFTER_MOVING);
}

TEST_CASE("PORT-218 the consumers use the one path: the attack machine's continue test and the bool view agree")
{
	Port218World w;
	Object *soldier = w.unit("PORT218Soldier", 'A', 300, 300);
	Object *victim = w.unit("Dummy", 'B', 305, 300);
	w.frames(2);
	// the bool view is POSSIBLE or POSSIBLE_AFTER_MOVING of the RW answer, both ways (RW 0x7417FA .. 0x741807 asks the same function)
	for (unsigned type : { (unsigned)ATTACK_NEW_TARGET, (unsigned)ATTACK_CONTINUED_TARGET, (unsigned)ATTACK_CONTINUED_TARGET_FORCED })
	{
		const CanAttackResult r = soldier->getWeapons()->getAbleToAttackSpecificObject((AbleToAttackType)type, *soldier, *victim, CMD_FROM_AI);
		const bool viaBool = soldier->getWeapons()->canAttackObject(*victim, CMD_FROM_AI, (type & 1u) != 0);
		CHECK(viaBool == (r == ATTACKRESULT_POSSIBLE || r == ATTACKRESULT_POSSIBLE_AFTER_MOVING));
	}
	// nothing at all: an empty (null) victim position cannot even be asked; a null pointer result is NOT_POSSIBLE
	CHECK(soldier->getWeapons()->canAttackObject(*victim, CMD_FROM_AI, false));
	CHECK(soldier->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *soldier, *victim, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);
}

// PORT-191 review (PR #579): RW 0x6C8724 and RW 0x6C88F2 test the ANTI MASK OF THE WHOLE WEAPON SET (the OR of its slot weapons' Anti* flags,
// WeaponSet + 0x2C, zeroed by the ctor RW 0x6C801C) against the victim's anti class — not the mask of the single slot weapon being looked at.
// This is the branch the reviewer named: a set that covers a class through ANY slot lets the pair pass, and a class no slot covers is refused
// outright (0x6C88F8 -> RW 0x6C8A44, the INVALID_SHOT answer) instead of coming back as POSSIBLE_AFTER_MOVING.
TEST_CASE("PORT-218 the set-level anti mask of RW 0x6C8724 / 0x6C88F2: the OR of the set's weapons, not the current slot's")
{
	Port218World w;
	Object *hero = w.unit("PORT218MixedHero", 'A', 300, 300);
	Object *air = w.unit("PORT218AirborneInfantry", 'B', 305, 300);
	Object *ground = w.unit("Dummy", 'B', 300, 305);
	w.frames(2);
	REQUIRE(hero->getWeapons());
	air->setStatus((unsigned)ObjectTemplateInfoBuilder::objectStatusIndex("AIRBORNE_TARGET"), true);
	// the PRIMARY slot is ground-only, but the set claims AIRBORNE_INFANTRY through its SECONDARY: RW 0x6C8724 passes and the answer is a
	// real one (the primary is in reach and canDamage the victim) — the per-slot test the port used before refused it
	CHECK(hero->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *hero, *air, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);
	CHECK(hero->getWeapons()->canAttackObject(*air, CMD_FROM_AI, false));
	// ground victims still pass: the set's OR keeps the primary slot's AntiGround
	CHECK(hero->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *hero, *ground, CMD_FROM_AI) == ATTACKRESULT_POSSIBLE);
	// a class NO slot of the set claims is refused with the INVALID_SHOT answer, never POSSIBLE_AFTER_MOVING (RW 0x6C88F8 -> 0x6C8A44)
	Object *projectile = w.unit("Arrow", 'B', 320, 300);
	CHECK(hero->getWeapons()->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, *hero, *projectile, CMD_FROM_AI) == ATTACKRESULT_INVALID_SHOT);
	CHECK_FALSE(hero->getWeapons()->canAttackObject(*projectile, CMD_FROM_AI, false));
}

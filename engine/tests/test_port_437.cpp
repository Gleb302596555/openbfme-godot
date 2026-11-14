// OpenBFME. PORT-437 tests (issue 437, stop S-325): the generic attack machine's object conditions, as RotWK builds them.
//
// RW 0x7486FD (the generic attack machine's constructor) selects one of six StateConditionInfo tables and gives it to Aim 0x66 and Fire 0x67
// (RW 0x74879D / RW 0x7487F5). A mobile owner that is not forced gets RW 0xC29DD4, a forced one RW 0xC29E04 (RW 0x74872C / RW 0x748735):
//   RW 0xC29DD4 = { 0x7457DF -> 0x64, 0 }; { 0x745901 -> 0x64, 0 }; { 0x741798 -> 0x270F, 2 };
//   RW 0xC29E04 = { 0x7457DF -> 0x64, 0 }; { 0x741798 -> 0x270F, 3 }; { 0x745901 -> 0x64, 0 };
// The port has no separate Chase state, so both records go to the approach state 0x65, which walks the attack path (see makeGenericAttackMachine).
//
// The predicates read here, instruction by instruction:
//   * RW 0x7457DF (out of weapon reach): no goal -> true (RW 0x745809 .. 0x7458E5); a leech-range deadline still ahead of the logic clock
//     (RW 0x6CA32D on Weapon + 0x50 against TheGameLogic + 0x40, RW 0x7458B4) or a blocked attack view (RW 0x744AAA, RW 0x7458A2) -> true; else the
//     current weapon's reach (RW 0x6CC653, extra 0, min range checked; RW 0x7458DC): out of reach -> true;
//   * RW 0x745901 (crush the victim instead): template + 0x60D UseCrushAttack (RW 0x74591C), the goal in no AI container (RW 0x745928), no turret on the
//     current weapon (RW 0x745940), AIData AICrushesInfantry (RW 0x745945), a COMPUTER controlling player (RW 0x745968), the crush policy RW 0x69519A
//     (RW 0x745973) and DONT_AUTO_CRUSH_INFANTRY clear (RW 0x74597C).
// Synthetic data only: no retail or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/Team.h"
#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"

using namespace combattest;

namespace
{
// two archers and one infantry target, all on one side, plus the crush pair: a CAVALRY crusher and an INFANTRY victim
const char kPort437Objects[] =
	"Weapon P437Bow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 400\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon P437CrushBlade\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  CanFireWhileMoving = Yes\n"
	"  DelayBetweenShots = 400\n"
	"  DamageNugget\n"
	"    Damage = 5\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object P437Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
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
	"    Weapon = PRIMARY P437Bow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P437Footman\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CrushableLevel = 0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100000\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P437Rider\n"
	"  KindOf = CAVALRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  CrusherLevel = 2\n"
	"  CrushableLevel = 0\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P437CrushBlade\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

struct Port437World : CombatWorld
{
	Port437World() : CombatWorld(kPort437Objects) { combat().setAutoAcquireEnabled(false); }
	// the outer AI state, then the inner attack machine's state: 0x65 approach means the machine went back to the chase
	unsigned outer(Object *o) { return o->getAIUpdateInterface()->currentStateId(); }
	unsigned inner(Object *o)
	{
		AIUpdateInterface *ai = o->getAIUpdateInterface();
		AIState *s = ai->stateMachineOrNull() ? ai->stateMachineOrNull()->currentState() : nullptr;
		AIAttackState *att = dynamic_cast<AIAttackState *>(s);
		REQUIRE(att != nullptr);
		REQUIRE(att->attackMachine() != nullptr);
		return att->attackMachine()->currentStateId();
	}
	AIUpdateInterface *ai(Object *o) { return o->getAIUpdateInterface(); }
	void order(Object *attacker, Object *victim)
	{
		REQUIRE(ai(attacker)->aiAttackObject(victim, CMD_FROM_PLAYER));
		frames(2);
		REQUIRE(outer(attacker) == (unsigned)AI_ATTACK_OBJECT);
	}
};
} // namespace

TEST_CASE("PORT-437 out of reach: a victim that leaves the weapon's reach sends the generic machine back to the chase (RW 0x7457DF -> 0x64, table RW 0xC29DD4)")
{
	Port437World w;
	Object *archer = w.unit("P437Archer", 'A', 300, 300);
	Object *foe = w.unit("P437Footman", 'B', 340, 300);
	w.frames(2);
	w.order(archer, foe);
	// in reach at the order: the archer aims (or already fires), no chase
	CHECK(w.inner(archer) != (unsigned)ATTACK_APPROACH);
	// RW 0x7457DF: the goal object out of the current weapon's reach (RW 0x6CC653) -> Chase 0x64
	Coord3D far{ 1400.0f, 1400.0f, 0.0f };
	foe->setPosition(&far);
	w.frames(2);
	CHECK(w.outer(archer) == (unsigned)AI_ATTACK_OBJECT);
	CHECK(w.inner(archer) == (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-437 out of reach: a victim inside the reach keeps the aim or the fire state; the machine does not walk (RW 0x7457DF false)")
{
	Port437World w;
	Object *archer = w.unit("P437Archer", 'A', 300, 300);
	Object *foe = w.unit("P437Footman", 'B', 340, 300);
	w.frames(2);
	w.order(archer, foe);
	for (int i = 0; i < 6; ++i)
	{
		w.frames(1);
		REQUIRE(w.outer(archer) == (unsigned)AI_ATTACK_OBJECT);
		const unsigned s = w.inner(archer);
		CHECK((s == (unsigned)ATTACK_AIM || s == (unsigned)ATTACK_FIRE || s == (unsigned)ATTACK_WAIT));
	}
}

TEST_CASE("PORT-437 crush: with AICrushesInfantry on a crushing unit that can crush its infantry victim goes to the chase (RW 0x745901 -> 0x64)")
{
	Port437World w;
	REQUIRE(w.logic->settings().aiCrushesInfantry);
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	// the owner has to be the computer player: the crush record is gated by the controlling player's type == COMPUTER (RW 0x745968)
	REQUIRE(w.playerOf('B')->getPlayerType() == PLAYER_COMPUTER);
	w.order(rider, foe);
	CHECK(w.inner(rider) == (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-437 crush: AICrushesInfantry off (AIData + 0x8C, RW 0x745945) leaves the archer shooting its infantry victim")
{
	Port437World w;
	w.logic->settings().aiCrushesInfantry = false;
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	w.order(rider, foe);
	w.frames(4);
	CHECK(w.outer(rider) == (unsigned)AI_ATTACK_OBJECT);
	CHECK(w.inner(rider) != (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-437 crush: a HUMAN controlling player does not take the crush record even with AICrushesInfantry on (RW 0x745968)")
{
	Port437World w;
	REQUIRE(w.logic->settings().aiCrushesInfantry);
	w.playerOf('B')->setPlayerType(PLAYER_HUMAN, false);
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	w.order(rider, foe);
	w.frames(4);
	CHECK(w.inner(rider) != (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-437 crush: a victim in a horde (an AI container, RW 0x745928) is never crushed; the machine keeps shooting")
{
	Port437World w;
	REQUIRE(w.logic->settings().aiCrushesInfantry);
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	// RW 0x745928: the goal object's AI container + 0x27C must be null
	REQUIRE(foe->getContainedBy() == nullptr);
	w.order(rider, foe);
	CHECK(w.inner(rider) == (unsigned)ATTACK_APPROACH);
}

TEST_CASE("PORT-437 cannot possibly attack still wins over the crush record: an ordered attack on an UNATTACKABLE victim ends the machine (RW 0xC29E04 record 2 -> 0x270F)")
{
	Port437World w;
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	w.order(rider, foe);
	REQUIRE(w.inner(rider) == (unsigned)ATTACK_APPROACH);
	foe->setStatus((unsigned)CombatNames::statuses().unattackable, true);
	w.frames(3);
	// UNATTACKABLE (RW 0x7417E4 -> RW 0x68D6A6's answer) ends the machine: the outer attack state leaves for the chase first, so what the test can
	// see is that the machine did NOT take the crush record of the same table (record 1 runs before it)
	w.frames(3);
	CHECK(w.outer(rider) != (unsigned)AI_FORCE_ATTACK_OBJECT);
}

TEST_CASE("PORT-437 forced attack: the forced table RW 0xC29E04 keeps the crush record after the cannot-attack one, and its userData is 3")
{
	Port437World w;
	Object *rider = w.unit("P437Rider", 'B', 300, 300);
	Object *foe = w.unit("P437Footman", 'A', 340, 300);
	w.frames(2);
	REQUIRE(w.ai(rider)->aiForceAttackObject(foe, CMD_FROM_PLAYER));
	w.frames(2);
	REQUIRE(w.outer(rider) == (unsigned)AI_FORCE_ATTACK_OBJECT);
	AIState *s = w.ai(rider)->stateMachineOrNull()->currentState();
	AIAttackState *att = dynamic_cast<AIAttackState *>(s);
	REQUIRE(att != nullptr);
	REQUIRE(att->attackMachine() != nullptr);
	AIState *aim = att->attackMachine()->findState(ATTACK_AIM);
	REQUIRE(aim != nullptr);
	REQUIRE(aim->conditions().size() == 3); // RW 0xC29E04 has three records
	CHECK(aim->conditions()[0].toId == (unsigned)ATTACK_APPROACH); // 0x7457DF -> 0x64
	CHECK(aim->conditions()[1].toId == (unsigned)MACHINE_DONE_FAILURE); // 0x741798 -> 0x270F
	CHECK(aim->conditions()[1].userData == 3); // the forced table's userData
	CHECK(aim->conditions()[2].toId == (unsigned)ATTACK_APPROACH); // 0x745901 -> 0x64
}

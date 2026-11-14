// OpenBFME. GPL-3.0.
// PORT-165 synthetic regression: a MELEE_HORDE changes to a ranged position weapon.
// The position-attack machine is selected at RW 0x74CFF1..0x74D019.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/AI/AIAttackHordePosition.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/WeaponSetToggle.h"

using namespace combattest;
namespace {
const char kSynthetic[] =
	"Weapon P165HordeRangefinder\n"
	"  AttackRange = 12\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  LeechRangeWeapon = Yes\n"
	"  HordeAttackNugget\n"
	"  End\n"
	"End\n"
	"Weapon P165HordeBombardRangefinder\n"
	"  AttackRange = 250\n"
	"  MinimumAttackRange = 50\n"
	"  DelayBetweenShots = 1000\n"
	"  LeechRangeWeapon = Yes\n"
	"  AcceptableAimDelta = 45\n"
	"  NoVictimNeeded = Yes\n"
	"  BombardType = Yes\n"
	"  ScatterRadius = 40\n"
	"  HordeAttackNugget\n"
	"  End\n"
	"End\n"
	"Weapon P165Bow\n"
	"  AttackRange = 300\n"
	"  DelayBetweenShots = 0\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon P165BowBombard\n"
	"  AttackRange = 350\n"
	"  MinimumAttackRange = 25\n"
	"  DelayBetweenShots = 800\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  NoVictimNeeded = Yes\n"
	"  BombardType = Yes\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object P165Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 250\n"
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
	"    Weapon = PRIMARY P165Bow\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY P165Bow\n"
	"    Weapon = TERTIARY P165BowBombard\n"
	"    AutoChooseSources = PRIMARY FROM_PLAYER FROM_SCRIPT FROM_AI\n"
	"    AutoChooseSources = TERTIARY FROM_PLAYER FROM_SCRIPT\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P165ArcherHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
		"  VisionRange = 250\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P165HordeRangefinder\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY P165HordeRangefinder\n"
	"    Weapon = TERTIARY P165HordeBombardRangefinder\n"
	"    AutoChooseSources = TERTIARY FROM_PLAYER FROM_SCRIPT\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P165Archer Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20\n"
	"    RankInfo = RankNumber:2 UnitType:P165Archer Position:X:30 Y:0 Position:X:30 Y:20 Position:X:30 Y:-20\n"
	"    InitialPayload = P165Archer 6\n"
	"    RanksToReleaseWhenAttacking = 1 2\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct TestWorld : CombatWorld
{
    TestWorld() : CombatWorld(kSynthetic) { WeaponCommands::registerHandlers(dispatcher); }
};

}

TEST_CASE("PORT-165: position horde wait tracks active member states (RW 0x747999, 0x742CEA, 0x86D562)")
{
    TestWorld w;
    w.combat().setAutoAcquireEnabled(false);
    Object *horde = w.unit("P165ArcherHorde", 'A', 300, 300);
    REQUIRE(horde);
    w.frames(12);
    const auto members = w.membersOf(horde);
    REQUIRE(members.size() == 6);
    AIStateMachine hordeMachine(*horde->getAIUpdateInterface(), "P165WaitTest");
    AIHordePositionWaitState wait(hordeMachine);
    CHECK(wait.onEnter() == STATE_CONTINUE);
    CHECK(wait.update() == STATE_FAILURE); // at least one member is not active
    AIStateMachine memberMachine(*members.front()->getAIUpdateInterface(), "P165NonHordeWaitTest");
    AIHordePositionWaitState nonHordeWait(memberMachine);
    CHECK(nonHordeWait.onEnter() == STATE_FAILURE);
    const Coord3D spot{300.0f, 150.0f, w.logic->getGroundHeight(300.0f, 150.0f)};
    for (Object *member : members)
    {
        WeaponSetToggle::setObjectWeaponSetFlag(*member, WeaponSetToggle::WEAPONSET_TOGGLE_1, true);
        member->getWeapons()->setWeaponLock(2, LOCKED_TEMPORARILY);
        member->getAIUpdateInterface()->aiAttackPosition(spot, 0x7fffffff, CMD_FROM_AI);
    }
    CHECK(wait.update() == STATE_CONTINUE); // every live member's state reports active
    Object *target = w.unit("Dummy", 'B', 500, 500);
    REQUIRE(target);
    hordeMachine.setGoalObject(target->getID());
    for (Object *member : members) member->getAIUpdateInterface()->setCurrentVictim(target);
    CHECK(wait.update() == STATE_CONTINUE); // RW 0x86D438: all have the same victim
    members.front()->getAIUpdateInterface()->setCurrentVictim(nullptr);
    CHECK(wait.update() == STATE_FAILURE);
    hordeMachine.setGoalObject((ObjectID)INVALID_ID);
    members.front()->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
    CHECK(wait.update() == STATE_FAILURE); // one inactive member is sufficient
}

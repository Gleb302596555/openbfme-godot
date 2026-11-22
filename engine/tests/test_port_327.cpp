// OpenBFME. GPL-3.0. Synthetic INI only.
// RW 0x82C167: the fog gate only applies to a human owner's non-script actions;
// RW 0x82CBD5, 0x82C798 and 0x82C4E9 call it for entry, healing and repair.
// DONOR BFME2 0x0042B4F5 / 0x0042B246 (ActionManagerSpecialPowerChecks.cpp).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/Player.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"

using namespace combattest;
namespace
{
const char kObjects[] =
    "Object Port327Infantry\n KindOf = INFANTRY SELECTABLE\n TransportSlotCount = 1\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\nEnd\n"
    "Object Port327Vehicle\n KindOf = VEHICLE SELECTABLE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\nEnd\n"
    "Object Port327Depot\n KindOf = STRUCTURE IMMOBILE HEAL_PAD REPAIR_PAD\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 10\n GeometryHeight = 20\n"
    " Body = ActiveBody Body\n MaxHealth = 500\n End\n"
    " Behavior = TransportContain Contain\n Slots = 4\n PassengerFilter = ANY +INFANTRY\n AllowOwnPlayerInsideOverride = Yes\n End\nEnd\n";
struct Rig
{
    CombatWorld world{kObjects};
    ShroudManager shroud{*world.logic};
    Object *infantry;
    Object *vehicle;
    Object *depot;
    Rig()
    {
        shroud.init(40.0f, 1);
        shroud.setExtent(0, 0, 1000, 1000);
        shroud.attach();
        infantry = world.unit("Port327Infantry", 'A', 100, 100);
        vehicle = world.unit("Port327Vehicle", 'A', 100, 120);
        depot = world.unit("Port327Depot", 'A', 600, 600);
        shroud.update();
        DamageInfo damage;
        damage.m_input.m_damageType = DAMAGE_UNRESISTABLE;
        damage.m_input.m_amount = 30;
        infantry->attemptDamage(damage);
        vehicle->attemptDamage(damage);
        REQUIRE(shroud.getObjectStatus(*depot, world.playerOf('A')->getPlayerIndex()) == OBJECTSHROUD_SHROUDED);
    }
};
}

TEST_CASE("PORT-327 computer actions may enter, heal and repair at a shrouded depot")
{
    Rig r;
    r.world.playerOf('A')->setPlayerType(PLAYER_COMPUTER, false);
    CHECK(GarrisonRules::canEnterObject(*r.infantry, r.depot, CMD_FROM_AI, 0, false));
    CHECK(GarrisonRules::canGetHealedAt(*r.infantry, r.depot, CMD_FROM_AI));
    CHECK(GarrisonRules::canGetRepairedAt(*r.vehicle, r.depot, CMD_FROM_AI));
}

TEST_CASE("PORT-327 human fog gate still exempts script orders")
{
    Rig r;
    r.world.playerOf('A')->setPlayerType(PLAYER_HUMAN, false);
    CHECK_FALSE(GarrisonRules::canEnterObject(*r.infantry, r.depot, CMD_FROM_PLAYER, 0, false));
    CHECK_FALSE(GarrisonRules::canGetHealedAt(*r.infantry, r.depot, CMD_FROM_PLAYER));
    CHECK_FALSE(GarrisonRules::canGetRepairedAt(*r.vehicle, r.depot, CMD_FROM_PLAYER));
    CHECK(GarrisonRules::canEnterObject(*r.infantry, r.depot, CMD_FROM_SCRIPT, 0, false));
    CHECK(GarrisonRules::canGetHealedAt(*r.infantry, r.depot, CMD_FROM_SCRIPT));
    CHECK(GarrisonRules::canGetRepairedAt(*r.vehicle, r.depot, CMD_FROM_SCRIPT));
}

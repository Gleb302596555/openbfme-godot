// OpenBFME. GPL-3.0. Synthetic INI only.
// RW 0x82C167: the fog gate only applies to a human owner's non-script actions;
// RW 0x82CBD5, 0x82C798 and 0x82C4E9 call it for entry, healing and repair.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/PartitionManager.h"
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


// RW 0x82DFB7 (0x82E93C..0x82EA56): Rebuild scans alive allied
// STRUCTURE objects in 2D within the integer-truncated cursor radius, skips
// construction in [0, 99), and accepts only health strictly below maximum.
// DONOR BFME2 0x0041D60B (ActionManagerCanDoSpecialPowerAtLocation.cpp).
namespace
{
const char kRebuild[] =
    "SpecialPower Port327Rebuild\n Enum = SPECIAL_SPELL_BOOK_REBUILD\n Flags = WATER_OK\n ReloadTime = 0\n RadiusCursorRadius = 50.9\nEnd\n"
    "Object Port327Caster\n KindOf = INFANTRY SELECTABLE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = Port327Rebuild\n End\nEnd\n"
    "Object Port327Building\n KindOf = STRUCTURE IMMOBILE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 10\n GeometryHeight = 20\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\nEnd\n";
struct RebuildWorld : CombatWorld
{
    SpecialPowerStore powers;
    SpecialPowerStore *saved = TheSpecialPowerStore;
    Object *caster;
    RebuildWorld()
    {
        TheSpecialPowerStore = &powers;
        w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
        SpecialPowerModules::registerAll(w.modules);
        const std::string error = w.load(kRebuild);
        REQUIRE_MESSAGE(error.empty(), error);
        logic->partition().setRegion(0, 0, 1000, 1000);
        caster = unit("Port327Caster", 'A', 200, 200);
        frames(1);
    }
    ~RebuildWorld() { TheSpecialPowerStore = saved; }
    bool allowed(float x, float y)
    {
        const SpecialPowerTemplate *power = powers.findSpecialPowerTemplate("Port327Rebuild");
        REQUIRE(power);
        return ActionManager::canDoSpecialPowerAtLocation(*caster, Coord3D{x, y, 0}, power, 0);
    }
    void damage(Object *object)
    {
        DamageInfo info;
        info.m_input.m_damageType = DAMAGE_UNRESISTABLE;
        info.m_input.m_amount = 30;
        object->attemptDamage(info);
    }
};
}

TEST_CASE("PORT-327 Rebuild accepts a damaged allied finished structure")
{
    RebuildWorld w;
    Object *building = w.unit("Port327Building", 'A', 400, 400);
    w.frames(1);
    CHECK_FALSE(w.allowed(400, 400)); // healthy: no candidate
    w.damage(building);
    CHECK(w.allowed(400, 400)); // RW 0x82EA11..0x82EA33
    building->setConstructionPercent(98.0f);
    CHECK_FALSE(w.allowed(400, 400)); // RW 0x82E9F3..0x82EA0F
    building->setConstructionPercent(99.0f);
    CHECK(w.allowed(400, 400));
}

TEST_CASE("PORT-327 Rebuild excludes enemies and truncates its radius")
{
    RebuildWorld w;
    Object *enemy = w.unit("Port327Building", 'B', 400, 400);
    w.frames(1);
    w.damage(enemy);
    CHECK_FALSE(w.allowed(400, 400)); // RW 0x82E968..0x82E9BA: ALLIES
    Object *ally = w.unit("Port327Building", 'A', 400, 400);
    w.frames(1);
    w.damage(ally);
    CHECK(w.allowed(450, 400));       // radius truncates from 50.9 to 50
    CHECK_FALSE(w.allowed(450.5f, 400));
}

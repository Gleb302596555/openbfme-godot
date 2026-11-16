// Synthetic body damage-state tests; no game files. RW 0x8C283C and RW 0x8C3FA3 are the oracles.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
const char kData[] =
    "ModifierList Port243Damaged\n Category = BUFF\n Duration = 0\n Modifier = ARMOR 80%\nEnd\n"
    "ModifierList Port243Really\n Category = DEBUFF\n Duration = 0\n Modifier = ARMOR 60%\nEnd\n"
    "Object Port243Active\n KindOf = SELECTABLE INFANTRY\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n MaxHealthDamaged = 70\n MaxHealthReallyDamaged = 30\n"
    " UseDefaultDamageSettings = No\n EnteringDamagedTransitionTime = 1000\n EnteringReallyDamagedTransitionTime = 2000\n"
    " DamagedAttributeModifier = Port243Damaged\n ReallyDamagedAttributeModifier = Port243Really\n End\n"
    " Behavior = AttributeModifierPoolUpdate PoolTag\n End\nEnd\n"
    "Object Port243Respawn\n KindOf = SELECTABLE INFANTRY\n"
    " Body = RespawnBody BodyTag\n MaxHealth = 100\n MaxHealthDamaged = 70\n MaxHealthReallyDamaged = 30\n"
    " UseDefaultDamageSettings = No\n EnteringDamagedTransitionTime = 1000\n EnteringReallyDamagedTransitionTime = 2000\n"
    " DamagedAttributeModifier = Port243Damaged\n ReallyDamagedAttributeModifier = Port243Really\n End\n"
    " Behavior = AttributeModifierPoolUpdate PoolTag\n End\nEnd\n";

struct Port243World : CombatWorld
{
    AttributeModifierStore modifiers;
    AttributeModifierStore *saved = TheAttributeModifierStore;
    Port243World() : CombatWorld()
    {
        TheAttributeModifierStore = &modifiers;
        modifiers.registerBlock(w.fx.env.blocks);
        const std::string err = w.load(kData, INI_LOAD_OVERWRITE, "port243.ini");
        REQUIRE_MESSAGE(err.empty(), err);
    }
    ~Port243World() { TheAttributeModifierStore = saved; }
};
void hit(Object &o, float amount, int type)
{
    DamageInfo d;
    d.m_input.m_damageType = type;
    d.m_input.m_amount = amount;
    o.attemptDamage(d);
}
}

TEST_CASE("PORT-243: ActiveBody and RespawnBody switch modifiers on damage and healing")
{
    Port243World w;
    for (const char *name : {"Port243Active", "Port243Respawn"})
    {
        Object *o = w.unit(name, 'A', 250, 250);
        REQUIRE(o);
        auto *pool = dynamic_cast<AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate"));
        REQUIRE(pool);
        CHECK_FALSE(pool->hasList("Port243Damaged"));
        hit(*o, 40, DAMAGE_UNRESISTABLE); // RW 0x8C283C: state 1 adds damaged
        CHECK(pool->hasList("Port243Damaged"));
        CHECK_FALSE(pool->hasList("Port243Really"));
        hit(*o, 35, DAMAGE_UNRESISTABLE); // state 2 removes damaged then adds really damaged
        CHECK_FALSE(pool->hasList("Port243Damaged"));
        CHECK(pool->hasList("Port243Really"));
        hit(*o, 40, DAMAGE_HEALING); // state 1 again, then state 0
        CHECK(pool->hasList("Port243Damaged"));
        CHECK_FALSE(pool->hasList("Port243Really"));
        hit(*o, 20, DAMAGE_HEALING);
        CHECK_FALSE(pool->hasList("Port243Damaged"));
        CHECK_FALSE(pool->hasList("Port243Really"));
    }
}

TEST_CASE("PORT-243: only damaging state transitions set the temporary busy expiry")
{
    Port243World w;
    Object *o = w.unit("Port243Active", 'A', 300, 300);
    REQUIRE(o);
    hit(*o, 40, DAMAGE_UNRESISTABLE); // RW 0x8C43C3..0x8C441D: 1000 ms = 5 frames
    CHECK(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    o->checkDisabledStatus(w.logic->getFrame() + 4);
    CHECK(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    o->checkDisabledStatus(w.logic->getFrame() + 5);
    CHECK_FALSE(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    hit(*o, 1, DAMAGE_UNRESISTABLE); // same state must not restart it
    CHECK_FALSE(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    hit(*o, 35, DAMAGE_UNRESISTABLE); // really damaged: 2000 ms = 10 frames
    CHECK(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    o->checkDisabledStatus(w.logic->getFrame() + 9);
    CHECK(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    o->checkDisabledStatus(w.logic->getFrame() + 10);
    CHECK_FALSE(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
    hit(*o, 40, DAMAGE_HEALING); // RW 0x8C2FC1: healing does not start the timer
    CHECK_FALSE(o->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
}

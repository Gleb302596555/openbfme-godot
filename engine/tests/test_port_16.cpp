// PORT-16, issue #72: prerequisite tests for the body damage record used by SUPPLY_SOURCE_ATTACKED.
// All objects and damage records are synthetic; expected behavior comes from the cited RotWK instructions.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"

using namespace combattest;

namespace
{
const char kPort16Objects[] =
    "Object Port16Proxy\n"
    "  KindOf = STRUCTURE\n"
    "  Body = SymbioticStructuresBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n";

DamageInfo port16Hit(const Object *source, int type, bool noEffect)
{
    DamageInfo info;
    info.m_input.m_sourceID = source ? source->getID() : INVALID_ID;
    info.m_input.m_amount = 10.0f;
    info.m_input.m_damageType = type;
    info.m_output.m_noEffect = noEffect;
    return info;
}
} // namespace

TEST_CASE("PORT-16 AIData: ForceSkirmishAI has the retail default and parses a boolean")
{
    GameLogicSettings settings;
    CHECK_FALSE(settings.forceSkirmishAI); // RW 0x6FFD49
    std::string error;
    REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  ForceSkirmishAI = Yes\nEnd\n", settings, &error), error);
    CHECK(settings.forceSkirmishAI); // RW 0xC1CC80 -> 0x42E558
    REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  ForceSkirmishAI = No\nEnd\n", settings, &error), error);
    CHECK_FALSE(settings.forceSkirmishAI);
}

TEST_CASE("PORT-16 damage record: a recent record survives a hit without a source")
{
    CombatWorld w;
    Object *attacker = w.unit("Swordsman", 'A', 300, 300);
    Object *target = w.unit("Dummy", 'B', 400, 300);
    auto *body = dynamic_cast<ActiveBody *>(target->getBodyModule());
    REQUIRE(body != nullptr);
    DamageInfo first = port16Hit(attacker, DAMAGE_UNRESISTABLE, true);
    target->attemptDamage(first);
    const unsigned firstFrame = w.logic->getFrame();
    int type = -1;
    bool noEffect = false;
    REQUIRE(body->lastDamageRecordType(type));
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(type == DAMAGE_UNRESISTABLE);
    CHECK(noEffect);
    CHECK(body->lastDamageFrame() == firstFrame);

    // RW 0x8C42C9 .. 0x8C42CB: within the same frame, no incoming source retains the old record.
    DamageInfo second = port16Hit(nullptr, DAMAGE_SLASH, false);
    target->attemptDamage(second);
    REQUIRE(body->lastDamageRecordType(type));
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(type == DAMAGE_UNRESISTABLE);
    CHECK(noEffect);
    CHECK(body->lastDamager() == attacker->getID());
    CHECK(body->lastDamageFrame() == firstFrame);

    // RW 0x8C42A2 .. 0x8C42A9: two frames later, even a hit without a source replaces the record.
    w.frames(2);
    DamageInfo later = port16Hit(nullptr, DAMAGE_PIERCE, false);
    target->attemptDamage(later);
    REQUIRE(body->lastDamageRecordType(type));
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(type == DAMAGE_PIERCE);
    CHECK_FALSE(noEffect);
    CHECK(body->lastDamageFrame() == w.logic->getFrame());
}

TEST_CASE("PORT-16 damage record: symbiotic body forwards both record and frame")
{
    CombatWorld w(kPort16Objects);
    Object *attacker = w.unit("Swordsman", 'A', 200, 300);
    Object *target = w.unit("Dummy", 'B', 300, 300);
    Object *proxy = w.unit("Port16Proxy", 'B', 350, 300);
    auto *body = dynamic_cast<SymbioticStructuresBody *>(proxy->getBodyModule());
    REQUIRE(body != nullptr);
    int type = -1;
    bool noEffect = false;
    // RW 0x8C4FF5 / 0x8C511E: no symbiote gives null and -1.
    CHECK_FALSE(body->lastDamageRecordType(type));
    CHECK_FALSE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(body->lastDamageFrame() == 0xFFFFFFFFu);
    body->setSymbiote(target->getID());
    DamageInfo hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, true);
    target->attemptDamage(hit);
    REQUIRE(body->lastDamageRecordType(type));
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(type == DAMAGE_UNRESISTABLE);
    CHECK(noEffect);
    CHECK(body->lastDamageFrame() == w.logic->getFrame());
}

TEST_CASE("PORT-16 damage record: noEffect survives the native snapshot")
{
    CombatWorld w;
    Object *attacker = w.unit("Swordsman", 'A', 300, 300);
    Object *target = w.unit("Dummy", 'B', 400, 300);
    auto *body = dynamic_cast<ActiveBody *>(target->getBodyModule());
    REQUIRE(body != nullptr);
    DamageInfo first = port16Hit(attacker, DAMAGE_UNRESISTABLE, true);
    target->attemptDamage(first);
    XferSave saved;
    body->xfer(saved);

    DamageInfo second = port16Hit(attacker, DAMAGE_SLASH, false);
    target->attemptDamage(second); // INFANTRY replaces a recent record (RW 0x8C42DD)
    bool noEffect = true;
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK_FALSE(noEffect);
    XferLoad loaded(saved.data());
    body->xfer(loaded);
    CHECK(loaded.atEnd());
    REQUIRE(body->lastDamageRecordNoEffect(noEffect));
    CHECK(noEffect);
    int type = -1;
    REQUIRE(body->lastDamageRecordType(type));
    CHECK(type == DAMAGE_UNRESISTABLE);
    // Earlier native body snapshots omitted this bit: reject rather than reconstruct it as a guessed value.
    std::vector<std::uint8_t> old = saved.data();
    REQUIRE_FALSE(old.empty());
    old[0] = 2;
    XferLoad oldLoad(old);
    CHECK_THROWS_AS(body->xfer(oldLoad), XferException);
}

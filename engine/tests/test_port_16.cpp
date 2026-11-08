// PORT-16, issue #72: prerequisite tests for the body damage record used by SUPPLY_SOURCE_ATTACKED.
// All objects and damage records are synthetic; expected behavior comes from the cited RotWK instructions.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "Common/StateHash.h"
#include "GameLogic/AI/PlayerSupplyAttack.h"

using namespace combattest;

namespace
{
const char kPort16Objects[] =
    "Object Port16Proxy\n"
    "  KindOf = STRUCTURE DOZER\n"
    "  Body = SymbioticStructuresBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object Port16Worker\n"
    "  KindOf = DOZER\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object Port16Harvester\n"
    "  KindOf = HARVESTER\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object Port16Center\n"
    "  KindOf = SUPPLY_GATHERING_CENTER\n"
    "  Body = ActiveBody ModuleTag_Body\n"
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

TEST_CASE("PORT-16 AIData: supply safety radius has the retail default and uses the real parser")
{
    // RW 0x6FFC04 / 0x6FFC7D load the float at 0xBDD764 (250.0).
    GameLogicSettings settings;
    CHECK(settings.supplyCenterSafeRadius == 250.0f);
    std::string error;
    REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  SupplyCenterSafeRadius = 123.75\n  RepulsedDistance = 17.5\nEnd\n", settings, &error), error);
    CHECK(settings.supplyCenterSafeRadius == 123.75f); // row 0xC1CD80 -> parseReal RW 0x42ED00
    CHECK(settings.repulsedDistance == 17.5f); // adjacent AIData row remains independent
    CHECK(settings.unappliedFields.count("AIData.SupplyCenterSafeRadius") == 0);
    REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  ForceSkirmishAI = Yes\nEnd\n", settings, &error), error);
    CHECK(settings.supplyCenterSafeRadius == 123.75f); // omitted fields survive a later block
    CHECK(settings.forceSkirmishAI);
    GameLogicSettings invalid;
    CHECK_FALSE(GameLogicSettingsLoader::scanAIData("AIData\n  SupplyCenterSafeRadius = far\nEnd\n", invalid, &error));
    CHECK(error.find("SupplyCenterSafeRadius") != std::string::npos);
}

TEST_CASE("PORT-16 AIData: supply safety radius contributes to the game hash")
{
    CombatWorld w;
    const auto initial = w.logic->computeStateHash();
    w.logic->settings().supplyCenterSafeRadius = 123.75f;
    CHECK(w.logic->computeStateHash() != initial);
    w.logic->settings().supplyCenterSafeRadius = 250.0f;
    CHECK(w.logic->computeStateHash() == initial);
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

TEST_CASE("PORT-16 attacked helper: timer, team order, noEffect and snapshot")
{
    CombatWorld w(kPort16Objects);
    Object *attacker = w.unit("Swordsman", 'B', 300, 300);
    Object *first = w.unit("Port16Worker", 'A', 400, 300);
    Object *second = w.unit("Port16Worker", 'A', 450, 300);
    w.frames(10);
    DamageInfo hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, false);
    first->attemptDamage(hit);
    hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, false);
    second->attemptDamage(hit);
    Player *p = w.playerOf('A');
    p->setAttackedBy(w.playerOf('B')->getPlayerIndex(), 10);

    PlayerSupplyAttack state;
    CHECK_FALSE(state.check(*p, w.teams, 0));
    CHECK(state.nextFrame() == 10);
    CHECK_FALSE(state.check(*p, w.teams, 9));
    REQUIRE(state.check(*p, w.teams, 10));
    CHECK(state.nextFrame() == 20);
    CHECK(state.objectID() == second->getID()); // newest team member first
    StateHasher before;
    state.crc(before);
    XferSave saved;
    state.xfer(saved);
    // A throttled condition clears the previously selected ID, but not its timer.
    CHECK_FALSE(state.check(*p, w.teams, 11));
    CHECK(state.objectID() == INVALID_ID);
    CHECK(state.nextFrame() == 20);
    StateHasher changed;
    state.crc(changed);
    CHECK(changed.value() != before.value());
    XferLoad load(saved.data());
    state.xfer(load);
    CHECK(load.atEnd());
    CHECK(state.nextFrame() == 20);
    CHECK(state.objectID() == second->getID());
    StateHasher restored;
    state.crc(restored);
    CHECK(restored.value() == before.value());
    // Frame zero resets the timer, but unusually does not clear the selected ID.
    CHECK_FALSE(state.check(*p, w.teams, 0));
    CHECK(state.nextFrame() == 10);
    CHECK(state.objectID() == second->getID());
    // RW 0x8F744B: the other caller clears the timer. At the exact
    // ten-frame boundary the player's gate passes but a body hit is too old.
    state.clearTimer();
    CHECK_FALSE(state.check(*p, w.teams, 20));
    CHECK(state.nextFrame() == 30);
    CHECK(state.objectID() == INVALID_ID);
}

TEST_CASE("PORT-16 attacked helper: no-effect records and a null body do not count")
{
    CombatWorld w(kPort16Objects);
    Object *attacker = w.unit("Swordsman", 'B', 300, 300);
    Object *worker = w.unit("Port16Worker", 'A', 400, 300);
    w.unit("Port16Proxy", 'A', 450, 300); // Symbiotic body with no symbiote: null record
    w.frames(10);
    DamageInfo hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, true);
    worker->attemptDamage(hit);
    Player *p = w.playerOf('A');
    p->setAttackedBy(w.playerOf('B')->getPlayerIndex(), 10);
    PlayerSupplyAttack state;
    CHECK_FALSE(state.check(*p, w.teams, 10));
    CHECK(state.nextFrame() == 20);
    CHECK(state.objectID() == INVALID_ID);
    // The stale player attack prevents a scan and does not advance the timer.
    state.clearTimer();
    CHECK_FALSE(state.check(*p, w.teams, 21));
    CHECK(state.nextFrame() == 0);
}

TEST_CASE("PORT-16 attacked helper: every eligible kind and the unsigned never-hit boundary")
{
    CombatWorld w(kPort16Objects);
    Object *attacker = w.unit("Swordsman", 'B', 100, 100);
    Object *worker = w.unit("Port16Worker", 'A', 200, 100);
    Player *p = w.playerOf('A');
    // RW adds 10 to unsigned 0xffffffff: without the usual frame-zero call,
    // a never-hit ActiveBody passes the frame check at frame 1, not at 9.
    PlayerSupplyAttack state;
    REQUIRE(state.check(*p, w.teams, 1));
    CHECK(state.objectID() == worker->getID());
    state.clearTimer();
    CHECK_FALSE(state.check(*p, w.teams, 9));
    w.frames(10);
    Object *center = w.unit("Port16Center", 'A', 300, 100);
    Object *harvester = w.unit("Port16Harvester", 'A', 400, 100);
    Object *ordinary = w.unit("Dummy", 'A', 500, 100);
    DamageInfo hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, false);
    center->attemptDamage(hit);
    hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, false);
    harvester->attemptDamage(hit);
    hit = port16Hit(attacker, DAMAGE_UNRESISTABLE, false);
    ordinary->attemptDamage(hit);
    p->setAttackedBy(w.playerOf('B')->getPlayerIndex(), 10);
    state.clearTimer();
    REQUIRE(state.check(*p, w.teams, 10));
    CHECK(state.objectID() == harvester->getID()); // ordinary is newer but not an eligible kind
    // Suppress the harvester's record; the center is the next eligible member.
    hit = port16Hit(attacker, DAMAGE_SLASH, true);
    harvester->attemptDamage(hit);
    state.clearTimer();
    REQUIRE(state.check(*p, w.teams, 10));
    CHECK(state.objectID() == center->getID());
}

// Both editions: RW 0x8BCC50 / 0x8BCC82, BFME2 0x8B8B0F / 0x8B8B41.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientEvents.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/OathbreakersFadeAwayBehavior.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

namespace {
const char kObjects[] =
    "Object TestOathbreaker\n KindOf = INFANTRY SELECTABLE\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AITag\n End\n"
    " Behavior = OathbreakersFadeAwayBehavior FadeTag\n FadeOutTime = 3500\n End\nEnd\n";
const char kHorde[] =
    "Object TestOathHorde\n KindOf = HORDE SELECTABLE\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = HordeContain ContainTag\n Slots = 2\n End\nEnd\n"
    "Object TestOathMachine\n KindOf = MACHINE SELECTABLE\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = OathbreakersFadeAwayBehavior FadeTag\n FadeOutTime = 3500\n End\nEnd\n";
}

TEST_CASE("modgap2 oath: removing a MACHINE member does not release a formation slot")
{
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        const std::string objects = std::string(kObjects) + kHorde;
        combattest::CombatWorld w(objects.c_str());
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *horde = w.unit("TestOathHorde", 'A', 300, 300);
        Object *machine = w.unit("TestOathMachine", 'A', 310, 300);
        Object *keeper = w.unit("TestOathbreaker", 'A', 315, 300);
        REQUIRE(horde); REQUIRE(machine); REQUIRE(keeper);
        auto *contain = dynamic_cast<HordeContain *>(horde->getContain());
        REQUIRE(contain);
        REQUIRE(contain->addToContain(machine));
        REQUIRE(contain->addToContain(keeper));
        REQUIRE(contain->freeSlotIndices().empty()); // this synthetic horde has no rank slots
        const ObjectID machineId = machine->getID();
        machine->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
        w.frames(30);
        CHECK(w.logic->findObjectByID(machineId) == nullptr);
        CHECK(contain->freeSlotIndices().empty()); // RW 0x86C4BE, BFME2 0x8693AD
    }
}

TEST_CASE("modgap2 oath: a horde member keeps selection and reports partial removal")
{
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        const std::string objects = std::string(kObjects) + kHorde;
        combattest::CombatWorld w(objects.c_str());
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *horde = w.unit("TestOathHorde", 'A', 300, 300);
        Object *ghost = w.unit("TestOathbreaker", 'A', 310, 300);
        REQUIRE(horde); REQUIRE(ghost); REQUIRE(horde->getContain());
        REQUIRE(horde->getContain()->addToContain(ghost));
        const ObjectID id = ghost->getID();
        ghost->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
        w.frames(7);
        REQUIRE(w.logic->findObjectByID(id));
        CHECK(ghost->isScriptSelectable()); // HORDE_MEMBER skips the start action
        w.frames(30);
        CHECK(w.logic->findObjectByID(id) == nullptr);
        bool found = false;
        for (const auto &line : w.logic->report().stops) found = found || line == OathbreakersFadeAwayBehavior::hordeStop();
        CHECK(found);
    }
}

TEST_CASE("modgap2 oath: exact opacity reaches the client and is restored after load")
{
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kObjects);
        w.logic->setEdition(edition);
        ClientEventRecorder events(*w.logic);
        w.logic->setClientHooks(&events);
        struct Detach { GameLogic &logic; ~Detach() { logic.setClientHooks(nullptr); } } detach{ *w.logic };
        CAPTURE(gameEditionName(edition));
        Object *ghost = w.unit("TestOathbreaker", 'A', 300, 300);
        REQUIRE(ghost);
        auto *fade = dynamic_cast<OathbreakersFadeAwayBehavior *>(ghost->findModule("OathbreakersFadeAwayBehavior"));
        REQUIRE(fade);
        events.take();
        ghost->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
        w.frames(7);
        REQUIRE(fade->fadingStarted());
        auto recorded = events.take();
        bool gotOpacity = false;
        float lastOpacity = 1.0f;
        for (const auto &ev : recorded) if (ev.kind == ClientEvent::SET_OPACITY && ev.object == ghost->getID()) {
            gotOpacity = true;
            lastOpacity = ev.opacity;
        }
        REQUIRE(gotOpacity);
        CHECK(lastOpacity == fade->opacity());
        XferSave saved;
        fade->xfer(saved);
        w.frames(2);
        REQUIRE(fade->opacity() != lastOpacity);
        XferLoad load(saved.data());
        fade->xfer(load);
        CHECK(load.atEnd());
        CHECK(fade->opacity() == lastOpacity);
        events.take();
        events.loadedObject(*ghost);
        recorded = events.take();
        gotOpacity = false;
        for (const auto &ev : recorded) if (ev.kind == ClientEvent::SET_OPACITY && ev.object == ghost->getID()) {
            gotOpacity = true;
            CHECK(ev.opacity == lastOpacity);
        }
        CHECK(gotOpacity);
    }
}

TEST_CASE("modgap2 oath: fulfilled oath starts timed removal")
{
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kObjects);
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *ghost = w.unit("TestOathbreaker", 'A', 300, 300);
        REQUIRE(ghost);
        const ObjectID id = ghost->getID();
        w.frames(10);
        REQUIRE(w.logic->findObjectByID(id));
        CHECK(ghost->isScriptSelectable());
        ghost->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
        w.frames(7);
        REQUIRE(w.logic->findObjectByID(id));
        CHECK_FALSE(ghost->isScriptSelectable());
        w.frames(30);
        CHECK(w.logic->findObjectByID(id) == nullptr);
    }
}

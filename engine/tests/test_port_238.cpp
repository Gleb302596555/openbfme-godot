// OpenBFME. GPL-3.0.
// RW 0x8840FC -> 0x756CD7 -> 0x94EF1A: ring delivery processes the
// carrier's group order after idle, before the ring status is removed.
// Synthetic INI only; no game or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameLogic/AI/AIOrdersManager.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/Module/InvisibilityModules.h"

#include <memory>
#include <vector>

using namespace combattest;

namespace
{
const char *const kData =
    "Upgrade Upgrade_P238Ring\n Type = OBJECT\nEnd\n"
    "Object P238Carrier\n"
    " KindOf = INFANTRY HERO SELECTABLE\n"
    " TransportSlotCount = 1\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    "End\n"
    "Object P238Bearer\n"
    " KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " Behavior = CitadelSlaughterHordeContain RingEntry\n"
    "  PassengerFilter = NONE +P238Ring\n"
    "  ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n"
    "  ContainMax = 1\n"
    "  StatusForRingEntry = HOLDING_THE_RING\n"
    "  UpgradeForRingEntry = Upgrade_P238Ring\n"
    "  ObjectToDestroyForRingEntry = NONE +P238Ring\n"
    " End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    "End\n"
    "Object P238Ring\n"
    " KindOf = CRATE ONE_RING NO_COLLIDE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 2\n GeometryHeight = 2\n"
    "End\n";

struct Observation
{
    bool ready = false;
    int checks = 0;
    bool hadRing = false;
    std::vector<unsigned> activations;
};

class ProbeOrder final : public AIOrdersManager::GroupOrder
{
public:
    ProbeOrder(std::vector<ObjectID> objects, Object &carrier, unsigned label, Observation &observation)
        : GroupOrder(std::move(objects)), m_carrier(carrier), m_label(label), m_observation(observation) {}
    void activate(ObjectID) override { m_observation.activations.push_back(m_label); }
    bool complete(ObjectID) override
    {
        ++m_observation.checks;
        m_observation.hadRing = m_carrier.testStatus((unsigned)CombatNames::status("HOLDING_THE_RING"));
        return m_observation.ready;
    }
    void removed(ObjectID) override {}
    void preparePatrol(ObjectID) override {}
    std::unique_ptr<GroupOrder> clone() const override
    {
        return std::make_unique<ProbeOrder>(objects(), m_carrier, m_label, m_observation);
    }
private:
    Object &m_carrier;
    unsigned m_label;
    Observation &m_observation;
};

struct World : CombatWorld
{
    World()
    {
        const std::string error = w.load(kData, INI_LOAD_OVERWRITE, "port238.ini");
        REQUIRE_MESSAGE(error.empty(), error);
        logic->partition().setRegion(0, 0, 1000, 1000);
    }
};

void deliver(CitadelSlaughterHordeContain &contain, Object &carrier)
{
    carrier.setStatus((unsigned)CombatNames::status("HOLDING_THE_RING"), true);
    REQUIRE(contain.addToContain(&carrier));
}
} // namespace

TEST_CASE("PORT-238 delivery processes the completed carrier order before clearing the ring")
{
    World w;
    Object *bearer = w.unit("P238Bearer", 'A', 500, 500);
    Object *carrier = w.unit("P238Carrier", 'A', 500, 500);
    Object *other = w.unit("P238Carrier", 'A', 510, 500);
    Object *ring = w.unit("P238Ring", 'A', 500, 500);
    const ObjectID ringId = ring->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    Observation observation;
    AIOrdersManager &manager = w.ai->ordersManager();
    const auto first = manager.submit(0, std::make_unique<ProbeOrder>(std::vector<ObjectID>{carrier->getID(), other->getID()}, *carrier, 1, observation));
    const auto second = manager.submit(1, std::make_unique<ProbeOrder>(std::vector<ObjectID>{carrier->getID()}, *carrier, 2, observation));
    observation.ready = true;
    deliver(*contain, *carrier);
    CHECK(observation.checks == 1);
    CHECK(observation.hadRing);
    REQUIRE(manager.queue(carrier->getID()) != nullptr);
    CHECK(manager.queue(carrier->getID())->orders() == std::list<unsigned>{second});
    REQUIRE(manager.queue(other->getID()) != nullptr);
    CHECK(manager.queue(other->getID())->orders() == std::list<unsigned>{first});
    CHECK(observation.activations == std::vector<unsigned>{1, 1, 2});
    CHECK_FALSE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));
    CHECK(bearer->hasUpgrade("Upgrade_P238Ring"));
    // destroyObject marks immediately and the logic frame removes the ID later.
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
}

TEST_CASE("PORT-238 delivery preserves an incomplete carrier order")
{
    World w;
    Object *bearer = w.unit("P238Bearer", 'A', 500, 500);
    Object *carrier = w.unit("P238Carrier", 'A', 500, 500);
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    Observation observation;
    AIOrdersManager &manager = w.ai->ordersManager();
    const auto first = manager.submit(0, std::make_unique<ProbeOrder>(std::vector<ObjectID>{carrier->getID()}, *carrier, 1, observation));
    const auto second = manager.submit(1, std::make_unique<ProbeOrder>(std::vector<ObjectID>{carrier->getID()}, *carrier, 2, observation));
    deliver(*contain, *carrier);
    CHECK(observation.checks == 1);
    CHECK(observation.hadRing);
    REQUIRE(manager.queue(carrier->getID()) != nullptr);
    CHECK(manager.queue(carrier->getID())->orders() == std::list<unsigned>{first, second});
    CHECK(observation.activations == std::vector<unsigned>{1});
    CHECK(bearer->hasUpgrade("Upgrade_P238Ring"));
}


namespace
{
const char *const kButtonData =
    "CommandButton P238OneRing\n Command = ONE_RING\n InPalantir = Yes\n End\n"
    "CommandButton P238Stop\n Command = STOP\n InPalantir = Yes\n End\n"
    "CommandSet P238Commands\n 1 = P238OneRing\n 2 = P238Stop\n End\n"
    "Object P238Wearer\n"
    " KindOf = INFANTRY HERO SELECTABLE SCORE\n"
    " CommandSet = P238Commands\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " Behavior = StealthUpdate Stealth\n"
    "  StartsActive = Yes\n RingAnimTimeOn = 200\n RingAnimTimeOff = 200\n"
    "  OneRingDelayOn = 200\n OneRingDelayOff = 200\n RingDelayAfterRemoving = 1000\n"
    " End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    "End\n"
    "Object P238WithoutStealth\n"
    " KindOf = INFANTRY HERO SELECTABLE SCORE\n"
    " CommandSet = P238Commands\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
    "End\n";

struct ButtonWorld : World
{
    CommandStore commandStore;
    CommandStore *saved = TheCommandStore;
    MouseSettings mouse;
    MetaMap meta;
    PinholeView view{{0, -300, 400}, {100, 100, 0}, 1024, 768, 0.7f};
    std::unique_ptr<HudInput> input;
    std::unique_ptr<ControlBar> bar;

    ButtonWorld()
    {
        TheCommandStore = &commandStore;
        commandStore.setThingFactory(&w.things);
        w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
        w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
        const std::string error = w.load(kButtonData, INI_LOAD_OVERWRITE, "port238_button.ini");
        REQUIRE_MESSAGE(error.empty(), error);
        InvisibilityModules::registerHandlers(dispatcher);
        players.setLocalPlayer(playerOf('A'));
        input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, meta);
        input->context().commands = &commandStore;
        bar = std::make_unique<ControlBar>(input->context());
    }
    ~ButtonWorld()
    {
        bar.reset();
        input.reset();
        logic->reset();
        TheCommandStore = saved;
    }
    void select(Object &obj)
    {
        playerOf('A')->selection() = {obj.getID()};
        input->ui().mirrorSelection({obj.getID()});
        bar->update();
    }
    const ControlBarButton *button(const char *name)
    {
        for (const auto &b : bar->palantirButtons())
            if (b.button && b.button->m_name == name) return &b;
        return nullptr;
    }
};
} // namespace

TEST_CASE("PORT-238 One Ring button sends the ring toggle and enforces readiness")
{
    ButtonWorld w;
    Object *wearer = w.unit("P238Wearer", 'A', 150, 150);
    w.frames(3);
    auto *stealth = StealthUpdate::of(*wearer);
    REQUIRE(stealth != nullptr);
    w.select(*wearer);
    const auto *ring = w.button("P238OneRing");
    REQUIRE(ring != nullptr);
    // RW 0x942733 -> 0x94387B, CAN_STEALTH and frame >= ready frame.
    REQUIRE(ring->state == ButtonState::Enabled);
    std::vector<GameMessage> sent;
    w.input->setLogicMessageObserver([&](const GameMessage &m) { sent.push_back(m); });
    REQUIRE(w.bar->pressButton(ring->slot, true));
    CHECK(w.input->flush() == 1);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].getType() == MSG_ONE_RING); // RW 0x940435 -> 0x941A6B
    w.frames(1);
    REQUIRE(stealth->ringWorn());
    CHECK(wearer->testModelCondition(CombatNames::modelCondition("ONE_RING")));

    // RW 0x776B50 starts removal, and RW 0x777310 sets the cooldown after it finishes.
    for (int i = 0; i < 30 && wearer->getDisabledMask() != 0; ++i) w.frames(1);
    w.bar->update();
    ring = w.button("P238OneRing");
    REQUIRE(ring != nullptr);
    REQUIRE(ring->state == ButtonState::Enabled);
    REQUIRE(w.bar->pressButton(ring->slot, true));
    CHECK(w.input->flush() == 1);
    w.frames(1);
    for (int i = 0; i < 40 && stealth->ringWorn(); ++i) w.frames(1);
    REQUIRE_FALSE(stealth->ringWorn());
    w.bar->update();
    ring = w.button("P238OneRing");
    REQUIRE(ring != nullptr);
    CHECK(ring->state == ButtonState::Restricted);
    CHECK_FALSE(w.bar->pressButton(ring->slot, true));
    CHECK(w.input->flush() == 0);
    for (int i = 0; i < 50 && !stealth->ringReady(); ++i) w.frames(1);
    REQUIRE(stealth->ringReady());
    w.bar->update();
    ring = w.button("P238OneRing");
    REQUIRE(ring != nullptr);
    CHECK(ring->state == ButtonState::Enabled);
}

TEST_CASE("PORT-238 ring status gate does not restrict the adjacent stop command")
{
    ButtonWorld w;
    Object *wearer = w.unit("P238Wearer", 'A', 150, 150);
    w.frames(3);
    wearer->setStatus((unsigned)CombatNames::status("CAN_STEALTH"), false);
    w.select(*wearer);
    const auto *ring = w.button("P238OneRing");
    const auto *stop = w.button("P238Stop");
    REQUIRE(ring != nullptr);
    REQUIRE(stop != nullptr);
    CHECK(ring->state == ButtonState::Restricted); // RW 0x942733: status bit 0x12
    CHECK_FALSE(w.bar->pressButton(ring->slot, true));
    CHECK(stop->state == ButtonState::Enabled);
    std::vector<GameMessage> sent;
    w.input->setLogicMessageObserver([&](const GameMessage &m) { sent.push_back(m); });
    REQUIRE(w.bar->pressButton(stop->slot, true));
    CHECK(w.input->flush() == 1);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].getType() == MSG_DO_STOP);
}

TEST_CASE("PORT-238 ring button is available with CAN_STEALTH even without a stealth module")
{
    ButtonWorld w;
    Object *unit = w.unit("P238WithoutStealth", 'A', 150, 150);
    w.frames(3);
    REQUIRE(StealthUpdate::of(*unit) == nullptr);
    unit->setStatus((unsigned)CombatNames::status("CAN_STEALTH"), true);
    w.select(*unit);
    const auto *ring = w.button("P238OneRing");
    REQUIRE(ring != nullptr);
    CHECK(ring->state == ButtonState::Enabled); // RW 0x942733: null lookup -> 0x942FF1
    REQUIRE(w.bar->pressButton(ring->slot, true));
    CHECK(w.input->flush() == 1);
    w.frames(1);
    CHECK_FALSE(unit->testModelCondition(CombatNames::modelCondition("ONE_RING"))); // RW 0x7729A6 skips a null module
}

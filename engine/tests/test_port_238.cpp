// OpenBFME. GPL-3.0.
// RW 0x8840FC -> 0x756CD7 -> 0x94EF1A: ring delivery processes the
// carrier's group order after idle, before the ring status is removed.
// Synthetic INI only; no game or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AI/AIOrdersManager.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/PartitionManager.h"

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
    CHECK(w.byId(ringId) == nullptr);
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

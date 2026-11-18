// OpenBFME. GPL-3.0.
// RW 0x8840FC -> 0x756CD7 -> 0x94EF1A: ring delivery processes the
// carrier's group order after idle, before the ring status is removed.
// Synthetic INI only; no game or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameLogic/AI/AIOrdersManager.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpellEffectModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/SpellCommands.h"

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

TEST_CASE("PORT-238 adjacent stop still sends its message without CAN_STEALTH")
{
    ButtonWorld w;
    Object *wearer = w.unit("P238Wearer", 'A', 150, 150);
    w.frames(3);
    wearer->setStatus((unsigned)CombatNames::status("CAN_STEALTH"), false);
    w.select(*wearer);
    const auto *stop = w.button("P238Stop");
    REQUIRE(stop != nullptr);
    REQUIRE(stop->state == ButtonState::Enabled);
    std::vector<GameMessage> sent;
    w.input->setLogicMessageObserver([&](const GameMessage &m) { sent.push_back(m); });
    REQUIRE(w.bar->pressButton(stop->slot, true));
    CHECK(w.input->flush() == 1);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].getType() == MSG_DO_STOP);
}

TEST_CASE("PORT-238 ring upgrade create consumes one name and respects the exempt-status gate")
{
    World w;
    const char *data =
        "Upgrade Upgrade_P238First\n Type = OBJECT\n End\n"
        "Upgrade Upgrade_P238Second\n Type = OBJECT\n End\n"
        "Object P238DefaultGrant\n"
        " KindOf = INFANTRY SELECTABLE\n"
        " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
        " Behavior = GrantUpgradeCreate Grant\n"
        "  UpgradeToGrant = Upgrade_P238First Upgrade_P238Second\n"
        " End\n"
        "End\n"
        "Object P238ExemptGrant\n"
        " KindOf = INFANTRY SELECTABLE\n"
        " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
        " Behavior = GrantUpgradeCreate Grant\n"
        "  UpgradeToGrant = Upgrade_P238First Upgrade_P238Second\n"
        "  ExemptStatus = UNDER_CONSTRUCTION\n"
        " End\n"
        "End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_grant.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *defaultGrant = w.unit("P238DefaultGrant", 'A', 300, 300);
    Object *exemptGrant = w.unit("P238ExemptGrant", 'A', 350, 300);
    // RW 0xC70158: UpgradeToGrant uses 0x42EE5E (one unquoted token);
    // RW 0x8BD15E: creation requires the exempt UNDER_CONSTRUCTION bit.
    CHECK_FALSE(defaultGrant->hasUpgrade("Upgrade_P238First"));
    CHECK_FALSE(defaultGrant->hasUpgrade("Upgrade_P238Second"));
    CHECK(exemptGrant->hasUpgrade("Upgrade_P238First"));
    CHECK_FALSE(exemptGrant->hasUpgrade("Upgrade_P238Second"));
    // RW 0x8BD1D4: build completion grants the one configured name once.
    defaultGrant->friend_onBuildComplete();
    CHECK(defaultGrant->hasUpgrade("Upgrade_P238First"));
    CHECK_FALSE(defaultGrant->hasUpgrade("Upgrade_P238Second"));
}

namespace
{
struct HealWorld : World
{
    SpecialPowerStore powers;
    SpecialPowerStore *saved = TheSpecialPowerStore;
    HealWorld()
    {
        TheSpecialPowerStore = &powers;
        w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
        SpecialAbilityModules::registerAll(w.modules);
        SpecialPowerModules::registerAll(w.modules);
        const char *data =
            "SpecialPower SpecialAbilityP238Heal\n Enum = SPECIAL_GENERAL_TARGETLESS_THREE\n ReloadTime = 260000\n End\n"
            "SpecialPower SpecialAbilityP238Dummy\n Enum = SPECIAL_GENERAL_TARGETLESS\n ReloadTime = 1\n End\n"
            "Object P238HealHero\n"
            " KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK\n"
            " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
            " Body = ActiveBody Body\n MaxHealth = 100\n InitialHealth = 40\n End\n"
            " Behavior = AIUpdateInterface AI\n End\n"
            " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = SpecialAbilityP238Heal\n UpdateModuleStartsAttack = Yes\n End\n"
            " Behavior = ActivateModuleSpecialPower Mover\n"
            "  SpecialPowerTemplate = SpecialAbilityP238Heal\n UnpackTime = 1\n PreparationTime = 3\n PackTime = 2096\n IgnoreFacingCheck = Yes\n"
            "  TriggerSpecialPower = Heal OBJECTPOS\n End\n"
            " Behavior = PlayerHealSpecialPower Heal\n"
            "  SpecialPowerTemplate = SpecialAbilityP238Dummy\n HealAmount = 0.5\n HealAsPercent = Yes\n HealAffects = HERO\n HealRadius = 1\n End\n"
            "End\n"
            "Object P238HealNonHero\n"
            " KindOf = INFANTRY SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
            " Body = ActiveBody Body\n MaxHealth = 100\n InitialHealth = 40\n End\n"
            "End\n";
        const std::string error = w.load(data, INI_LOAD_OVERWRITE, "port238_heal.ini");
        REQUIRE_MESSAGE(error.empty(), error);
    }
    ~HealWorld()
    {
        logic->reset();
        TheSpecialPowerStore = saved;
    }
};
} // namespace

TEST_CASE("PORT-238 ring hero activated module heals a nearby friendly hero at object position")
{
    HealWorld w;
    Object *hero = w.unit("P238HealHero", 'A', 300, 300);
    Object *ally = w.unit("P238HealHero", 'A', 300, 300);
    Object *enemy = w.unit("P238HealHero", 'B', 300, 300);
    Object *nonhero = w.unit("P238HealNonHero", 'A', 300, 300);
    Object *farHero = w.unit("P238HealHero", 'A', 310, 300);
    auto *ability = dynamic_cast<ActivateModuleSpecialPower *>(hero->findModule("ActivateModuleSpecialPower"));
    auto *heal = dynamic_cast<PlayerHealSpecialPower *>(hero->findModule("PlayerHealSpecialPower"));
    REQUIRE(ability != nullptr);
    REQUIRE(heal != nullptr);
    REQUIRE(hero->getBodyModule()->getHealth() == 40.0f);
    // RW 0x8D222A invokes the Heal tag's special-power vslot 0x30 at OBJECTPOS;
    // RW 0x8CC574 -> 0x8CC4B4 / 0x8CC37B applies percent healing to eligible heroes.
    ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
    for (int i = 0; i < 40 && ability->abilitiesTriggered() == 0; ++i) w.frames(1);
    REQUIRE(ability->abilitiesTriggered() == 1);
    CHECK(ability->activations() == 1);
    CHECK(hero->getBodyModule()->getHealth() == 90.0f);
    CHECK(ally->getBodyModule()->getHealth() == 90.0f);
    CHECK(enemy->getBodyModule()->getHealth() == 40.0f);
    CHECK(nonhero->getBodyModule()->getHealth() == 40.0f);
    CHECK(farHero->getBodyModule()->getHealth() == 40.0f);
}

namespace
{
struct HealHudWorld : HealWorld
{
    CommandStore store;
    CommandStore *savedCommands = TheCommandStore;
    SpellCommands spellCommands;
    MouseSettings mouse;
    MetaMap meta;
    PinholeView view{{0, -300, 400}, {100, 100, 0}, 1024, 768, 0.7f};
    std::unique_ptr<HudInput> input;
    std::unique_ptr<ControlBar> bar;
    HealHudWorld()
    {
        TheCommandStore = &store;
        store.setThingFactory(&w.things);
        w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
        w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
        spellCommands.registerHandlers(dispatcher);
        const char *data =
            "Upgrade Upgrade_P238Rank\n Type = OBJECT\n End\n"
            "CommandButton P238HealButton\n Command = SPECIAL_POWER\n SpecialPower = SpecialAbilityP238Heal\n InPalantir = Yes\n End\n"
            "CommandSet P238HealCommands\n 1 = P238HealButton\n End\n"
            "Object P238HudHealHero\n"
            " KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK SCORE\n CommandSet = P238HealCommands\n"
            " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
            " Body = ActiveBody Body\n MaxHealth = 100\n InitialHealth = 40\n End\n"
            " Behavior = AIUpdateInterface AI\n End\n"
            " Behavior = UnpauseSpecialPowerUpgrade Unlock\n SpecialPowerTemplate = SpecialAbilityP238Heal\n TriggeredBy = Upgrade_P238Rank\n End\n"
            " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = SpecialAbilityP238Heal\n UpdateModuleStartsAttack = Yes\n StartsPaused = Yes\n End\n"
            " Behavior = ActivateModuleSpecialPower Mover\n"
            "  SpecialPowerTemplate = SpecialAbilityP238Heal\n UnpackTime = 1\n PreparationTime = 3\n PackTime = 2096\n IgnoreFacingCheck = Yes\n"
            "  TriggerSpecialPower = Heal OBJECTPOS\n End\n"
            " Behavior = PlayerHealSpecialPower Heal\n"
            "  SpecialPowerTemplate = SpecialAbilityP238Dummy\n HealAmount = 0.5\n HealAsPercent = Yes\n HealAffects = HERO\n HealRadius = 1\n End\n"
            "End\n";
        const std::string error = w.load(data, INI_LOAD_OVERWRITE, "port238_heal_hud.ini");
        REQUIRE_MESSAGE(error.empty(), error);
        players.setLocalPlayer(playerOf('A'));
        input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, meta);
        input->context().commands = &store;
        bar = std::make_unique<ControlBar>(input->context());
    }
    ~HealHudWorld()
    {
        bar.reset();
        input.reset();
        logic->reset();
        TheCommandStore = savedCommands;
    }
    const ControlBarButton *button()
    {
        for (const auto &b : bar->palantirButtons())
            if (b.button && b.button->m_name == "P238HealButton") return &b;
        return nullptr;
    }
};
}

TEST_CASE("PORT-238 ring heal HUD requires unlock and starts recharge after casting")
{
    HealHudWorld w;
    Object *hero = w.unit("P238HudHealHero", 'A', 300, 300);
    auto *ability = dynamic_cast<ActivateModuleSpecialPower *>(hero->findModule("ActivateModuleSpecialPower"));
    auto *power = dynamic_cast<SpecialPowerModule *>(hero->findModule("SpecialPowerModule"));
    REQUIRE(ability != nullptr);
    REQUIRE(power != nullptr);
    w.frames(3);
    w.playerOf('A')->selection() = {hero->getID()};
    w.input->ui().mirrorSelection({hero->getID()});
    w.bar->update();
    const auto *button = w.button();
    REQUIRE(button != nullptr);
    CHECK(button->state == ButtonState::Restricted);
    CHECK(power->pauseCount() == 1);
    // RW 0x8B9676: the upgrade unpauses the matching power and sets its ready frame.
    hero->giveUpgrade("Upgrade_P238Rank");
    w.frames(1);
    w.bar->update();
    button = w.button();
    REQUIRE(button != nullptr);
    REQUIRE(power->pauseCount() == 0);
    REQUIRE(button->state == ButtonState::Enabled);
    std::vector<GameMessage> sent;
    w.input->setLogicMessageObserver([&](const GameMessage &m) { sent.push_back(m); });
    REQUIRE(w.bar->pressButton(button->slot, true));
    CHECK(w.input->flush() == 1);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].getType() == MSG_DO_SPECIAL_POWER); // RW 0x94156B, targetless case
    for (int i = 0; i < 40 && ability->abilitiesTriggered() == 0; ++i) w.frames(1);
    REQUIRE(ability->abilitiesTriggered() == 1);
    CHECK(hero->getBodyModule()->getHealth() == 90.0f);
    CHECK_FALSE(power->isReady());
    CHECK(power->getReadyFrame() == w.logic->getFrame() + 1300u); // 260 seconds at 5 logic frames/second
    w.bar->update();
    button = w.button();
    REQUIRE(button != nullptr);
    CHECK(button->state == ButtonState::NotReady);
}

TEST_CASE("PORT-238 ring heal unlocks at the synthetic Sauron level five threshold")
{
    HealHudWorld w;
    ExperienceLevelSystem levels;
    ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
    struct RestoreLevels
    {
        HealHudWorld &world;
        ExperienceLevelSystem *saved;
        ~RestoreLevels() { world.logic->reset(); TheExperienceLevelSystem = saved; }
    } restore{w, savedLevels};
    TheExperienceLevelSystem = &levels;
    levels.registerBlocks(w.w.fx.env.blocks);
    // SauronRingLevel1..5, experiencelevels.inc:19783..19866. Other fields
    // (level-up modifiers, FX and decals) are deliberately omitted.
    const char *data =
        "ExperienceLevel P238Rank1\n TargetNames = P238HudHealHero\n RequiredExperience = 1\n Rank = 1\n End\n"
        "ExperienceLevel P238Rank2\n TargetNames = P238HudHealHero\n RequiredExperience = 550\n Rank = 2\n End\n"
        "ExperienceLevel P238Rank3\n TargetNames = P238HudHealHero\n RequiredExperience = 800\n Rank = 3\n End\n"
        "ExperienceLevel P238Rank4\n TargetNames = P238HudHealHero\n RequiredExperience = 1400\n Rank = 4\n End\n"
        "ExperienceLevel P238Rank5\n TargetNames = P238HudHealHero\n RequiredExperience = 2000\n Rank = 5\n Upgrades = Upgrade_P238Rank\n End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_levels.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *hero = w.unit("P238HudHealHero", 'A', 300, 300);
    auto *tracker = hero->getExperienceTracker();
    auto *power = dynamic_cast<SpecialPowerModule *>(hero->findModule("SpecialPowerModule"));
    REQUIRE(tracker != nullptr);
    REQUIRE(power != nullptr);
    // RW 0x79D8EF -> 0x79D141 checks thresholds; 0x79DA9D queues a grant
    // via 0x8215C2, whose upgrade is applied at 0x821368.
    tracker->setExperienceAndLevel(1999.0f, false);
    w.frames(3);
    CHECK(tracker->getRank() == 4);
    CHECK_FALSE(hero->hasUpgrade("Upgrade_P238Rank"));
    CHECK(power->pauseCount() == 1);
    tracker->setExperienceAndLevel(2000.0f, false);
    w.frames(3);
    CHECK(tracker->getRank() == 5);
    CHECK(hero->hasUpgrade("Upgrade_P238Rank"));
    CHECK(power->pauseCount() == 0);
    CHECK(power->isReady());
}

TEST_CASE("PORT-238 ring delivery grants both synthetic necromancer upgrades")
{
    World w;
    const char *data =
        "Upgrade Upgrade_P238Hero\n Type = OBJECT\n End\n"
        "Upgrade Upgrade_P238Fortress\n Type = OBJECT\n End\n"
        "Object P238Necromancer\n"
        " KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
        " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
        " Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = CitadelSlaughterHordeContain RingEntry\n"
        "  PassengerFilter = NONE +P238Ring\n ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n ContainMax = 1\n"
        "  StatusForRingEntry = HOLDING_THE_RING\n"
        "  UpgradeForRingEntry = Upgrade_P238Hero Upgrade_P238Fortress\n"
        "  ObjectToDestroyForRingEntry = NONE +P238Ring\n End\n"
        " Behavior = ModelConditionUpgrade RingModel\n"
        "  TriggeredBy = Upgrade_P238Hero Upgrade_P238Fortress\n RequiresAllTriggers = Yes\n"
        "  AddConditionFlags = ONE_RING\n End\n"
        "End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_necromancer.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *bearer = w.unit("P238Necromancer", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    const ObjectID ringId = ring->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    CHECK_FALSE(bearer->hasUpgrade("Upgrade_P238Hero"));
    CHECK_FALSE(bearer->hasUpgrade("Upgrade_P238Fortress"));
    CHECK_FALSE(bearer->testModelCondition(CombatNames::modelCondition("ONE_RING")));
    // RW 0x8840FC iterates the configured upgrade vector after the ring removal;
    // necromantreborn.ini:743..753 names two OBJECT upgrades and requires both.
    deliver(*contain, *carrier);
    CHECK(bearer->hasUpgrade("Upgrade_P238Hero"));
    CHECK(bearer->hasUpgrade("Upgrade_P238Fortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    w.frames(3);
    CHECK(bearer->testModelCondition(CombatNames::modelCondition("ONE_RING")));

    // Independent control: one of the two trigger upgrades is insufficient.
    Object *control = w.unit("P238Necromancer", 'A', 400, 400);
    control->giveUpgrade("Upgrade_P238Hero");
    w.frames(3);
    CHECK_FALSE(control->testModelCondition(CombatNames::modelCondition("ONE_RING")));
    control->giveUpgrade("Upgrade_P238Fortress");
    w.frames(3);
    CHECK(control->testModelCondition(CombatNames::modelCondition("ONE_RING")));
}

TEST_CASE("PORT-238 synthetic necromancer transforms after ring delivery")
{
    HealHudWorld w;
    const char *data =
        "Upgrade Upgrade_P238TransformHero\n Type = OBJECT\n End\n"
        "Upgrade Upgrade_P238TransformFortress\n Type = OBJECT\n End\n"
        "SpecialPower P238TransformPower\n Enum = SPECIAL_TOGGLE_MOUNTED\n ReloadTime = 1000\n End\n"
        "CommandButton P238TransformButton\n Command = SPECIAL_POWER\n SpecialPower = P238TransformPower\n"
        " Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY OK_FOR_MULTI_EXECUTE\n InPalantir = No\n End\n"
        "Object P238NecromancerTransform\n"
        " KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
        " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
        " Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = UnpauseSpecialPowerUpgrade EnableTransform\n SpecialPowerTemplate = P238TransformPower\n TriggeredBy = Upgrade_P238TransformHero\n End\n"
        " Behavior = SpecialPowerModule TransformPower\n SpecialPowerTemplate = P238TransformPower\n UpdateModuleStartsAttack = Yes\n StartsPaused = Yes\n End\n"
        " Behavior = ToggleMountedSpecialAbilityUpdate Transform\n SpecialPowerTemplate = P238TransformPower\n MountedTemplate = P238GreatTransform\n"
        "  UnpackTime = 2000\n PreparationTime = 0\n PackTime = 0\n OpacityTarget = 0\n AwardXPForTriggering = 0\n IgnoreFacingCheck = Yes\n End\n"
        " Behavior = DoCommandUpgrade AutoTransform\n TriggeredBy = Upgrade_P238TransformHero\n GetUpgradeCommandButtonName = P238TransformButton\n End\n"
        " Behavior = CitadelSlaughterHordeContain RingEntry\n PassengerFilter = NONE +P238Ring\n ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n ContainMax = 1\n"
        "  StatusForRingEntry = HOLDING_THE_RING\n UpgradeForRingEntry = Upgrade_P238TransformHero Upgrade_P238TransformFortress\n"
        "  ObjectToDestroyForRingEntry = NONE +P238Ring\n End\n"
        " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
        "End\n"
        "Object P238GreatTransform\n"
        " KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
        " Body = ActiveBody Body\n MaxHealth = 200\n End\n Behavior = AIUpdateInterface AI\n End\n"
        " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
        "End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_transform.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *bearer = w.unit("P238NecromancerTransform", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    const ObjectID originalId = bearer->getID();
    const ObjectID ringId = ring->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    // RW 0x8841C2 grants the upgrades; 0x8B8E2E presses the special-power
    // button; 0x8B1690 -> 0x8B140D builds its MountedTemplate.
    deliver(*contain, *carrier);
    CHECK(bearer->hasUpgrade("Upgrade_P238TransformHero"));
    CHECK(bearer->hasUpgrade("Upgrade_P238TransformFortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *replacement = nullptr;
    for (int i = 0; i < 50 && replacement == nullptr; ++i)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238GreatTransform") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getControllingPlayer() == w.playerOf('A'));
    w.frames(3);
    CHECK(w.byId(originalId) == nullptr);
}

TEST_CASE("PORT-238 synthetic fire form gates two pulses on HERO and ends")
{
    World w;
    AttributeModifierStore modifiers;
    AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
    struct RestoreModifiers
    {
        World &world;
        AttributeModifierStore *saved;
        ~RestoreModifiers() { world.logic->reset(); TheAttributeModifierStore = saved; }
    } restore{w, savedModifiers};
    TheAttributeModifierStore = &modifiers;
    modifiers.registerBlock(w.w.fx.env.blocks);
    SpecialAbilityModules::registerAll(w.w.modules);
    SpellEffectModules::registerAll(w.w.modules);
    w.combat().setAutoAcquireEnabled(false);
    // The timings, damage and radius mirror the literal Sauron declarations,
    // with synthetic names, armor and no HUD, real assets, FX or mod loading.
    const char *data =
        "ModifierList P238FlameBuff\n Category = SPELL\n Modifier = ARMOR 50%\n Modifier = SPEED 125%\n Duration = 60000\n End\n"
        "Weapon P238FlameInitial\n RadiusDamageAffects = ENEMIES\n AttackRange = 50\n LeechRangeWeapon = Yes\n"
        " DelayBetweenShots = 80000\n PreAttackDelay = 0\n PreAttackType = PER_SHOT\n FiringDuration = 0\n"
        " DamageNugget\n Damage = 300\n Radius = 150\n DamageType = FLAME\n DeathType = BURNED\n End\n End\n"
        "Weapon P238FlamePulse\n RadiusDamageAffects = ENEMIES\n AttackRange = 50\n LeechRangeWeapon = Yes\n"
        " DelayBetweenShots = 3000\n PreAttackDelay = 0\n PreAttackType = PER_SHOT\n FiringDuration = 0\n"
        " DamageNugget\n Damage = 5\n Radius = 50\n DamageType = FLAME\n DeathType = BURNED\n End\n End\n"
        "Object P238FireHero\n KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
        " Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = AttributeModifierPoolUpdate Pool\n End\n"
        " Behavior = FireWeaponUpdate Fire\n"
        "  FireWeaponNugget\n WeaponName = P238FlameInitial\n FireDelay = 0\n OneShot = No\n End\n"
        "  FireWeaponNugget\n WeaponName = P238FlamePulse\n FireDelay = 100\n OneShot = No\n End\n"
        "  HeroModeTrigger = Yes\n End\n"
        " Behavior = HeroModeSpecialAbilityUpdate Ignite\n UnpackTime = 1000\n PackTime = 500\n"
        "  StopUnitBeforeActivating = Yes\n IgnoreFacingCheck = Yes\n HeroAttributeModifier = P238FlameBuff\n HeroEffectDuration = 60000\n End\n End\n"
        "Object P238FireTarget\n KindOf = INFANTRY SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " ArmorSet\n Conditions = None\n Armor = NoArmor\n End\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_fire.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *hero = w.unit("P238FireHero", 'A', 300, 300);
    Object *nearEnemy = w.unit("P238FireTarget", 'B', 320, 300);
    Object *midEnemy = w.unit("P238FireTarget", 'B', 400, 300);
    Object *farEnemy = w.unit("P238FireTarget", 'B', 480, 300);
    Object *ally = w.unit("P238FireTarget", 'A', 320, 300);
    auto *fire = dynamic_cast<FireWeaponUpdate *>(hero->findModule("FireWeaponUpdate"));
    auto *ability = dynamic_cast<HeroModeSpecialAbilityUpdate *>(hero->findModule("HeroModeSpecialAbilityUpdate"));
    auto *pool = dynamic_cast<AttributeModifierPool *>(hero->findModule("AttributeModifierPoolUpdate"));
    REQUIRE(fire != nullptr);
    REQUIRE(ability != nullptr);
    REQUIRE(pool != nullptr);
    const int heroCondition = CombatNames::modelCondition("HERO");
    REQUIRE(heroCondition == 220); // RW model condition HERO = 0xDC
    w.frames(10);
    CHECK_FALSE(hero->testModelCondition(heroCondition));
    CHECK(fire->shots() == 0);
    CHECK(nearEnemy->getBodyModule()->getHealth() == 1000.0f);
    ability->initiateIntentToDoSpecialPower(nullptr, nullptr, nullptr, 0, 0);
    for (int i = 0; i < 40 && ability->abilitiesTriggered() == 0; ++i) w.frames(1);
    REQUIRE(ability->abilitiesTriggered() == 1);
    REQUIRE(hero->testModelCondition(heroCondition));
    REQUIRE(pool->hasList("P238FlameBuff"));
    for (int i = 0; i < 10 && fire->shots() < 2; ++i) w.frames(1);
    REQUIRE(fire->shots() == 2);
    CHECK(nearEnemy->getBodyModule()->getHealth() == 695.0f);
    CHECK(midEnemy->getBodyModule()->getHealth() == 700.0f);
    CHECK(farEnemy->getBodyModule()->getHealth() == 1000.0f);
    CHECK(ally->getBodyModule()->getHealth() == 1000.0f);
    w.frames(20);
    CHECK(fire->shots() == 3); // the 3-second nugget repeats, rather than OneShot
    for (int i = 0; i < 330 && hero->testModelCondition(heroCondition); ++i) w.frames(1);
    REQUIRE_FALSE(hero->testModelCondition(heroCondition));
    CHECK_FALSE(pool->hasList("P238FlameBuff"));
    const auto shotsAfterExpiry = fire->shots();
    w.frames(20);
    CHECK(fire->shots() == shotsAfterExpiry);
}

TEST_CASE("PORT-238 synthetic Will temporarily dominates eligible enemies in an area")
{
    HealWorld w;
    w.combat().setAutoAcquireEnabled(false);
    const char *data =
        "SpecialPower P238Will\n Enum = SPECIAL_DOMINATE_ENEMY\n ReloadTime = 280000\n End\n"
        "Object P238WillCaster\n KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = P238Will\n UpdateModuleStartsAttack = Yes\n End\n"
        " Behavior = DominateEnemySpecialPower Will\n SpecialPowerTemplate = P238Will\n StartAbilityRange = 275\n"
        "  UnpackTime = 2000\n PreparationTime = 1\n FreezeAfterTriggerDuration = 1000\n IgnoreFacingCheck = Yes\n"
        "  AttributeModifierAffects = ALL -DOZER -HERO -STRUCTURE -ARMY_OF_DEAD -MOVE_ONLY ENEMIES NEUTRAL\n"
        "  DominateRadius = 120\n PermanentlyConvert = No\n End\n End\n"
        "Object P238WillTroop\n KindOf = INFANTRY SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = TemporarilyDefectUpdate Defect\n DefectDuration = 30000\n End\n End\n"
        "Object P238WillHorde\n KindOf = HORDE MELEE_HORDE SELECTABLE\n"
        " Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 30\n GeometryHeight = 20\n"
        " Body = ImmortalBody Body\n MaxHealth = 1\n End\n Behavior = HordeAIUpdate AI\n End\n"
        " Behavior = TemporarilyDefectUpdate Defect\n DefectDuration = 30000\n End\n"
        " Behavior = HordeContain Contain\n"
        "  RankInfo = RankNumber:1 UnitType:P238WillTroop Position:X:10 Y:0 Position:X:10 Y:20 Position:X:10 Y:-20\n"
        "  InitialPayload = P238WillTroop 3\n End\n"
        " LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n End\n"
        "Object P238WillHero\n KindOf = INFANTRY HERO SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n Behavior = TemporarilyDefectUpdate Defect\n DefectDuration = 30000\n End\n End\n"
        "Object P238WillDozer\n KindOf = INFANTRY DOZER SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n Behavior = TemporarilyDefectUpdate Defect\n DefectDuration = 30000\n End\n End\n"
        "Object P238WillBuilding\n KindOf = STRUCTURE SELECTABLE\n Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n Behavior = TemporarilyDefectUpdate Defect\n DefectDuration = 30000\n End\n End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_will.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *caster = w.unit("P238WillCaster", 'A', 300, 300);
    Object *enemy = w.unit("P238WillTroop", 'B', 350, 300);
    Object *horde = w.unit("P238WillHorde", 'B', 420, 300);
    auto *contain = horde->getContain();
    REQUIRE(contain != nullptr);
    REQUIRE(contain->getContainCount() == 3);
    std::vector<Object *> members(contain->getContainedItemsList()->begin(), contain->getContainedItemsList()->end());
    Object *enemyHero = w.unit("P238WillHero", 'B', 360, 300);
    Object *enemyDozer = w.unit("P238WillDozer", 'B', 370, 300);
    Object *enemyBuilding = w.unit("P238WillBuilding", 'B', 380, 300);
    Object *farEnemy = w.unit("P238WillTroop", 'B', 500, 300);
    Object *ally = w.unit("P238WillTroop", 'A', 340, 300);
    Player *neutral = w.players.getNeutralPlayer();
    REQUIRE(neutral != nullptr);
    Object *neutralTroop = w.spawn("P238WillTroop", 355, 310, 0, neutral->getDefaultTeam());
    REQUIRE(caster->getRelationship(*neutralTroop) == NEUTRAL);
    Player *a = caster->getControllingPlayer();
    Player *b = enemy->getControllingPlayer();
    Team *originalTeam = enemy->getTeam();
    auto *will = dynamic_cast<DominateEnemySpecialPower *>(caster->findModule("DominateEnemySpecialPower"));
    REQUIRE(will != nullptr);
    const auto *dataForWill = dynamic_cast<const DominateEnemySpecialPowerModuleData *>(will->abilityData());
    REQUIRE(dataForWill != nullptr);
    REQUIRE(dataForWill->m_affects.get() != nullptr);
    CHECK(ObjectFilterMatch::allows(*w.logic, *dataForWill->m_affects, *neutralTroop, a));
    CHECK_FALSE(ObjectFilterMatch::allows(*w.logic, *dataForWill->m_affects, *enemyHero, a));
    const Coord3D target{350, 300, 0};
    will->initiateIntentToDoSpecialPower(will->abilityData()->m_specialPowerTemplate, nullptr, &target, 0, 0);
    for (int i = 0; i < 40 && will->abilitiesTriggered() == 0; ++i) w.frames(1);
    REQUIRE(will->abilitiesTriggered() == 1);
    CHECK(will->dominated() == 2); // infantry plus horde, not its HORDE_MEMBER objects
    CHECK(enemy->getControllingPlayer() == a);
    CHECK(enemy->getOriginalTeam() == originalTeam);
    CHECK(horde->getControllingPlayer() == a);
    for (Object *member : members) CHECK(member->getControllingPlayer() == a);
    CHECK(enemyHero->getControllingPlayer() == b);
    CHECK(enemyDozer->getControllingPlayer() == b);
    CHECK(enemyBuilding->getControllingPlayer() == b);
    CHECK(farEnemy->getControllingPlayer() == b);
    CHECK(ally->getControllingPlayer() == a);
    CHECK(neutralTroop->getControllingPlayer() == neutral); // area relationship gate precedes the filter's NEUTRAL
    auto *defect = TemporarilyDefectUpdate::of(*enemy);
    REQUIRE(defect != nullptr);
    CHECK(defect->dominator() == caster->getID());
    CHECK(defect->endFrame() == w.logic->getFrame() + 150u); // 30000 ms at 5 fps
    const int status = CombatNames::status("TEMPORARILY_DEFECTED");
    REQUIRE(status == 0x3E); // TEMPORARILY_DEFECTED
    CHECK(enemy->testStatus((unsigned)status));
    w.frames(100);
    CHECK(enemy->getControllingPlayer() == a);
    w.frames(65); // update polls every ten frames after the expiry
    CHECK(enemy->getControllingPlayer() == b);
    CHECK(enemy->getTeam() == originalTeam);
    CHECK(horde->getControllingPlayer() == b);
    for (Object *member : members) CHECK(member->getControllingPlayer() == b);
    CHECK_FALSE(enemy->testStatus((unsigned)status));
    CHECK(defect->endFrame() == 0);
    CHECK(defect->dominator() == INVALID_ID);
}

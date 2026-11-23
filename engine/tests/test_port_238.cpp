// OpenBFME. GPL-3.0.
// RW 0x8840FC -> 0x756CD7 -> 0x94EF1A: ring delivery processes the
// carrier's group order after idle, before the ring status is removed.
// Synthetic INI only; no game or mod files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "Common/INI.h"
#include "Common/NameKeyGenerator.h"
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
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Module/AttachUpdate.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/SpellEffectModules.h"
#include "GameLogic/Module/StoreObjectsSpecialPower.h"
#include "GameLogic/Module/TeleportToCasterSpecialPower.h"
#include "GameLogic/Module/UpgradeModuleClasses.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/SkirmishAI/AITacticalAI.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"

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

namespace
{
struct PhialWorld : HealWorld
{
    PhialWorld()
    {
        EmotionModules::registerAll(w.modules);
        const char *ini =
            "SpecialPower SpecialAbilityP238Phial\n Enum = SPECIAL_GENERAL_TARGETLESS\n ReloadTime = 30000\n End\n"
            "Object P238PhialBearer\n"
            " KindOf = INFANTRY HERO SELECTABLE\n"
            " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
            " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
            " Behavior = AIUpdateInterface AI\n End\n"
            " Behavior = StealthUpdate Stealth\n StartsActive = Yes\n RingAnimTimeOn = 200\n RingAnimTimeOff = 200\n"
            "  OneRingDelayOn = 200\n OneRingDelayOff = 200\n RingDelayAfterRemoving = 1000\n End\n"
            " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = SpecialAbilityP238Phial\n UpdateModuleStartsAttack = Yes\n End\n"
            " Behavior = ModelConditionSpecialAbilityUpdate Phial\n SpecialPowerTemplate = SpecialAbilityP238Phial\n"
            "  UnpackTime = 2000\n PreparationTime = 1\n PersistentPrepTime = 600\n PackTime = 2666\n"
            "  IgnoreFacingCheck = Yes\n GenerateTerror = Yes\n EmotionPulseRadius = 150\n DisableWhenWearingTheRing = Yes\n End\n"
            "End\n"
            "Object P238PhialTarget\n KindOf = INFANTRY SELECTABLE\n"
            " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
            " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
            " Behavior = EmotionTrackerUpdate Emotion\n End\n End\n";
        const std::string error = w.load(ini, INI_LOAD_OVERWRITE, "port238_phial.ini");
        REQUIRE_MESSAGE(error.empty(), error);
    }
};
} // namespace

TEST_CASE("PORT-238 Phial conditions reject the worn ring and terror targets nearby enemies")
{
    PhialWorld w;
    Object *bearer = w.unit("P238PhialBearer", 'A', 300, 300);
    Object *enemy = w.unit("P238PhialTarget", 'B', 350, 300);
    Object *ally = w.unit("P238PhialTarget", 'A', 350, 300);
    Object *farEnemy = w.unit("P238PhialTarget", 'B', 550, 300);
    REQUIRE(bearer != nullptr);
    REQUIRE(enemy != nullptr);
    REQUIRE(ally != nullptr);
    REQUIRE(farEnemy != nullptr);
    auto *ability = dynamic_cast<ModelConditionSpecialAbilityUpdate *>(bearer->findModule("ModelConditionSpecialAbilityUpdate"));
    auto *stealth = StealthUpdate::of(*bearer);
    REQUIRE(ability != nullptr);
    REQUIRE(stealth != nullptr);
    w.frames(3);
    CHECK(ability->allowsSpecialPowerUse()); // RW 0x8515E4
    stealth->toggleRing();
    REQUIRE(stealth->ringWorn());
    CHECK_FALSE(ability->allowsSpecialPowerUse());
    for (int i = 0; i < 30 && bearer->getDisabledMask() != 0; ++i) w.frames(1);
    stealth->toggleRing();
    for (int i = 0; i < 40 && stealth->ringWorn(); ++i) w.frames(1);
    REQUIRE_FALSE(stealth->ringWorn());
    CHECK(ability->allowsSpecialPowerUse());
    ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
    for (int i = 0; i < 80 && ability->abilitiesTriggered() == 0; ++i) w.frames(1);
    REQUIRE(ability->abilitiesTriggered() == 1);
    CHECK(ability->emotionRequests() == 1); // RW 0x894701: enemy in range only
    auto *enemyEmotion = EmotionTrackerUpdate::of(*enemy);
    auto *allyEmotion = EmotionTrackerUpdate::of(*ally);
    auto *farEmotion = EmotionTrackerUpdate::of(*farEnemy);
    REQUIRE(enemyEmotion != nullptr);
    REQUIRE(allyEmotion != nullptr);
    REQUIRE(farEmotion != nullptr);
    CHECK(enemyEmotion->requested(EMOTION_TERROR));
    CHECK(enemyEmotion->requestSource(EMOTION_TERROR) == bearer->getID());
    CHECK_FALSE(allyEmotion->requested(EMOTION_TERROR));
    CHECK_FALSE(farEmotion->requested(EMOTION_TERROR));
}

TEST_CASE("PORT-238 synthetic Boromir ring replacement synchronizes selected power timers")
{
    HealHudWorld w;
    // One OBJECT ring upgrade, DoCommandUpgrade and the two named timers model
    // gondorboromir.ini:1197..1225,1271..1282, without actual mod loading.
    const char *data =
        "Upgrade Upgrade_P238BoromirRing\n Type = OBJECT\n End\n"
        "SpecialPower P238BoromirTransform\n Enum = SPECIAL_TOGGLE_MOUNTED\n ReloadTime = 1000\n End\n"
        "SpecialPower P238BoromirHorn\n Enum = SPECIAL_GENERAL_TARGETLESS\n ReloadTime = 120000\n End\n"
        "SpecialPower P238BoromirShield\n Enum = SPECIAL_HERO_MODE\n ReloadTime = 120000\n End\n"
        "SpecialPower P238BoromirOther\n Enum = SPECIAL_GENERAL_TARGETLESS_TWO\n ReloadTime = 120000\n End\n"
        "CommandButton P238BoromirSwitch\n Command = SPECIAL_POWER\n SpecialPower = P238BoromirTransform\n"
        " Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY\n InPalantir = No\n End\n"
        "Object P238Boromir\n KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n"
        " Body = ActiveBody Body\n MaxHealth = 100\n End\n Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = UnpauseSpecialPowerUpgrade Enable\n SpecialPowerTemplate = P238BoromirTransform\n TriggeredBy = Upgrade_P238BoromirRing\n End\n"
        " Behavior = SpecialPowerModule TransformPower\n SpecialPowerTemplate = P238BoromirTransform\n UpdateModuleStartsAttack = Yes\n StartsPaused = Yes\n End\n"
        " Behavior = ToggleMountedSpecialAbilityUpdate Transform\n SpecialPowerTemplate = P238BoromirTransform\n MountedTemplate = P238BoromirRingForm\n"
        "  SynchronizeTimerOnSpecialPower = P238BoromirHorn P238BoromirShield\n UnpackTime = 2000\n PreparationTime = 0\n PackTime = 0\n IgnoreFacingCheck = Yes\n End\n"
        " Behavior = DoCommandUpgrade AutoTransform\n TriggeredBy = Upgrade_P238BoromirRing\n GetUpgradeCommandButtonName = P238BoromirSwitch\n End\n"
        " Behavior = CitadelSlaughterHordeContain RingEntry\n PassengerFilter = NONE +P238Ring\n ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n ContainMax = 1\n"
        "  StatusForRingEntry = HOLDING_THE_RING\n UpgradeForRingEntry = Upgrade_P238BoromirRing\n ObjectToDestroyForRingEntry = NONE +P238Ring\n End\n"
        " Behavior = SpecialPowerModule Horn\n SpecialPowerTemplate = P238BoromirHorn\n End\n"
        " Behavior = SpecialPowerModule Shield\n SpecialPowerTemplate = P238BoromirShield\n End\n"
        " Behavior = SpecialPowerModule Other\n SpecialPowerTemplate = P238BoromirOther\n End\n"
        " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n End\n"
        "Object P238BoromirRingForm\n KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 8\n GeometryHeight = 20\n Body = ActiveBody Body\n MaxHealth = 100\n End\n"
        " Behavior = AIUpdateInterface AI\n End\n"
        " Behavior = SpecialPowerModule Horn\n SpecialPowerTemplate = P238BoromirHorn\n End\n"
        " Behavior = SpecialPowerModule Shield\n SpecialPowerTemplate = P238BoromirShield\n End\n"
        " Behavior = SpecialPowerModule Other\n SpecialPowerTemplate = P238BoromirOther\n End\n"
        " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n End\n";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_boromir.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *bearer = w.unit("P238Boromir", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(bearer != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    auto findPower = [&](Object &obj, const char *name) {
        const SpecialPowerTemplate *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    auto *horn = findPower(*bearer, "P238BoromirHorn");
    auto *shield = findPower(*bearer, "P238BoromirShield");
    auto *other = findPower(*bearer, "P238BoromirOther");
    REQUIRE(horn != nullptr);
    REQUIRE(shield != nullptr);
    REQUIRE(other != nullptr);
    horn->setReadyFrame(300);
    shield->setReadyFrame(600);
    other->setReadyFrame(900);
    Team *team = bearer->getTeam();
    bearer->setName("P238NamedBearer");
    const ObjectID originalId = bearer->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier); // RW 0x8840FC -> DoCommandUpgrade 0x8B8E2E
    CHECK(bearer->hasUpgrade("Upgrade_P238BoromirRing"));
    Object *replacement = nullptr;
    for (int i = 0; i < 50 && !replacement; ++i)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238BoromirRingForm") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    CHECK(replacement->getName() == "P238NamedBearer");
    auto *newHorn = findPower(*replacement, "P238BoromirHorn");
    auto *newShield = findPower(*replacement, "P238BoromirShield");
    auto *newOther = findPower(*replacement, "P238BoromirOther");
    REQUIRE(newHorn != nullptr);
    REQUIRE(newShield != nullptr);
    REQUIRE(newOther != nullptr);
    // RW 0x8B12BF iterates listed power names and calls 0x897368 to copy timers.
    CHECK(newHorn->getReadyFrame() == 300);
    CHECK(newShield->getReadyFrame() == 600);
    CHECK(newOther->getReadyFrame() != 900); // not listed
    w.frames(3);
    CHECK(w.byId(originalId) == nullptr);
}

TEST_CASE("PORT-238 ring ownership bias changes a synthetic WOTR surrender score")
{
    NameKeyGenerator keys;
    keys.init();
    SkirmishAIStore store(keys);
    INIEnvironment env;
    store.registerBlocks(env.blocks);
    // Active scalar values in Ennorath default/skirmishaidata.ini:1332..1341.
    // Synthetic blocks only; ringHeld is supplied explicitly rather than
    // claiming an OBJECT upgrade is already held by a player.
    const std::string ini =
        "SkirmishAIData\n FarmingThreshold = -2\n ArmyQualityBias = 3\n ArmyQuantityBias = 2\n"
        " HeroQualityBias = 2\n MapControlBias = 3\n BaseStrengthBias = 2\n RingOwnershipBias = 2\n"
        " LogicFramesTillRetreatChecksStart = 1500\n LogicFrameBetweenRetreatChecks = 400\n LogicFramesTillAISelfDestructs = 60\n End\n";
    INI loader(env);
    loader.loadMemory("port238_ai.ini", std::vector<std::uint8_t>(ini.begin(), ini.end()), INI_LOAD_OVERWRITE);
    REQUIRE(store.dataBlockSeen());
    const SkirmishAIData &d = store.data();
    CHECK(d.farmingThreshold == -2);
    CHECK(d.ringOwnershipBias == 2);
    CHECK(d.logicFrameBetweenRetreatChecks == 400);
    CHECK(d.logicFramesTillAISelfDestructs == 60);
    ForfeitTerms own, enemy;
    own.armyQuality = 1.0f;
    enemy.armyQuality = 10.0f;
    const int withoutRing = AITacticalAI::forfeitScoreFrom(own, enemy, false, d);
    const int withRing = AITacticalAI::forfeitScoreFrom(own, enemy, true, d);
    CHECK(withoutRing == -3);
    CHECK(withRing == -1); // RW 0x9B7D41..0x9B7D4F: add RingOwnershipBias
    ForfeitState noRing, hasRing;
    noRing.counter = hasRing.counter = d.logicFramesTillAISelfDestructs;
    int nextNoRing = 0, nextHasRing = 0;
    for (unsigned frame : {0u, 400u, 800u})
    {
        CHECK(AITacticalAI::forfeitFrame(noRing, nextNoRing, frame, [&] { return withoutRing; }, d).checked);
        CHECK(AITacticalAI::forfeitFrame(hasRing, nextHasRing, frame, [&] { return withRing; }, d).checked);
    }
    CHECK(noRing.triggered); // RW 0x9B7D7C..0x9B7D9B: three scores below threshold
    CHECK(noRing.badChecks == 3);
    CHECK_FALSE(hasRing.triggered);
    CHECK(hasRing.badChecks == 0);
    CHECK(noRing.counter == 60);
    CHECK(hasRing.counter == 60);
}

TEST_CASE("PORT-238 Elrond-style Restoration refresh excludes ring form and summoned allies")
{
    HealWorld w;
    const char *ini = R"ini(
SpecialPower P238Restore
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End
SpecialPower P238Cooldown
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 60000
End
Object ImladrisElrond_RingHero
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = SpecialPowerTimerRefreshSpecialPower Refresh
    SpecialPowerTemplate = P238Restore
    AffectAllies = Yes
    AttributeModifierRange = 150
    AttributeModifierAffects = ALL -ElvenElrond -ImladrisElrond -ArnorAranarth -ElrondBeacon -ImladrisElrond_RingHero -SUMMONED ALLIES
  End
  Behavior = SpecialPowerModule Cooldown
    SpecialPowerTemplate = P238Cooldown
  End
End
Object P238RestoreTarget
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = SpecialPowerModule Cooldown
    SpecialPowerTemplate = P238Cooldown
  End
End
Object P238RestoreSummoned
  KindOf = HERO INFANTRY SELECTABLE SUMMONED
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = SpecialPowerModule Cooldown
    SpecialPowerTemplate = P238Cooldown
  End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_elrond_restoration.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    w.logic->partition().setRegion(0, 0, 1000, 1000);
    Object *caster = w.unit("ImladrisElrond_RingHero", 'A', 300, 300);
    Object *sameForm = w.unit("ImladrisElrond_RingHero", 'A', 340, 300);
    Object *ally = w.unit("P238RestoreTarget", 'A', 350, 300);
    Object *summoned = w.unit("P238RestoreSummoned", 'A', 355, 300);
    Object *enemy = w.unit("P238RestoreTarget", 'B', 360, 300);
    Object *far = w.unit("P238RestoreTarget", 'A', 500, 300);
    REQUIRE(caster != nullptr);
    REQUIRE(sameForm != nullptr);
    REQUIRE(ally != nullptr);
    REQUIRE(summoned != nullptr);
    REQUIRE(enemy != nullptr);
    REQUIRE(far != nullptr);
    const auto cooldown = [](Object *o) { return dynamic_cast<SpecialPowerModule *>(o->findModule("SpecialPowerModule")); };
    for (Object *o : {caster, sameForm, ally, summoned, enemy, far})
    {
        w.logic->partition().registerObject(*o);
        REQUIRE(cooldown(o) != nullptr);
        cooldown(o)->startPowerRecharge(1.0f);
        REQUIRE_FALSE(cooldown(o)->isReady());
    }
    w.frames(1); // publish pending partition registrations before the area scan
    auto *refresh = dynamic_cast<SpecialPowerTimerRefreshSpecialPower *>(caster->findModule("SpecialPowerTimerRefreshSpecialPower"));
    REQUIRE(refresh != nullptr);
    REQUIRE(refresh->spData()->m_attributeModifierAffects.get() != nullptr);
    CHECK(ObjectFilterMatch::allows(*w.logic, *refresh->spData()->m_attributeModifierAffects, *ally, caster->getControllingPlayer()));
    CHECK(w.logic->partition().iterateObjectsInRange(*caster->getPosition(), 150, FROM_CENTER_2D, {}, ITER_SORTED_NEAR_TO_FAR).size() == 5);
    // Direct cast: this does not test the real level-7 gate, button or animations.
    refresh->doSpecialPower(0);
    w.frames(1);
    CHECK(refresh->triggers() == 1);
    CHECK(cooldown(ally)->isReady());
    CHECK_FALSE(cooldown(caster)->isReady());
    CHECK_FALSE(cooldown(sameForm)->isReady());
    CHECK_FALSE(cooldown(summoned)->isReady());
    CHECK_FALSE(cooldown(enemy)->isReady());
    CHECK_FALSE(cooldown(far)->isReady());
    CHECK(refresh->refreshed() == 1);
}

TEST_CASE("PORT-238 Elrond-style teaching grants experience only to allied heroes near the target")
{
    HealWorld w;
    ExperienceLevelSystem levels;
    ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
    struct RestoreLevels
    {
        HealWorld &world;
        ExperienceLevelSystem *saved;
        ~RestoreLevels() { world.logic->reset(); TheExperienceLevelSystem = saved; }
    } restore{w, savedLevels};
    TheExperienceLevelSystem = &levels;
    levels.registerBlocks(w.w.fx.env.blocks);
    const char *ini = R"ini(
ExperienceLevel P238TeachingRank1
  TargetNames = P238Teacher P238TeachingHero P238TeachingInfantry GasthausPalando GaladrielTornado
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel P238TeachingRank2
  TargetNames = P238Teacher P238TeachingHero P238TeachingInfantry GasthausPalando GaladrielTornado
  RequiredExperience = 100
  Rank = 2
End
SpecialPower P238TeachingPower
  Enum = SPECIAL_KINGS_FAVOR
  ReloadTime = 200000
End
Object P238Teacher
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = SpecialPowerModule Power
    SpecialPowerTemplate = P238TeachingPower
    UpdateModuleStartsAttack = Yes
  End
  Behavior = LevelGrantSpecialPower Teaching
    SpecialPowerTemplate = P238TeachingPower
    Experience = 200
    RadiusEffect = 50
    AcceptanceFilter = NONE +HERO +GasthausPalando +GasthausAlatar +ManveEagle +ImladrisGwaihir -GaladrielTornado02 -GaladrielTornado ALLIES
  End
End
Object P238TeachingHero
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object P238TeachingInfantry
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object GasthausPalando
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object GaladrielTornado
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_elrond_teaching.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *caster = w.unit("P238Teacher", 'A', 300, 300);
    Object *ally = w.unit("P238TeachingHero", 'A', 320, 300);
    Object *infantry = w.unit("P238TeachingInfantry", 'A', 330, 300);
    Object *enemy = w.unit("P238TeachingHero", 'B', 340, 300);
    Object *far = w.unit("P238TeachingHero", 'A', 360, 300);
    Object *namedAllowed = w.unit("GasthausPalando", 'A', 345, 300);
    Object *namedExcluded = w.unit("GaladrielTornado", 'A', 348, 300);
    for (Object *o : {caster, ally, infantry, enemy, far, namedAllowed, namedExcluded})
    {
        REQUIRE(o != nullptr);
        REQUIRE(o->getExperienceTracker() != nullptr);
        o->getExperienceTracker()->setExperienceAndLevel(1.0f, false);
    }
    auto *grant = dynamic_cast<LevelGrantSpecialPower *>(caster->findModule("LevelGrantSpecialPower"));
    REQUIRE(grant != nullptr);
    // Invoke the effect at the chosen position; this deliberately bypasses UI,
    // approach and the real level-3 unpause. RW 0x8C6FA4 -> 0x8C6E82.
    grant->triggerAtForTest(*caster->getPosition());
    CHECK(ally->getExperienceTracker()->getExperience() == 201.0f);
    CHECK(ally->getExperienceTracker()->getRank() == 2);
    CHECK(caster->getExperienceTracker()->getExperience() == 1.0f);
    CHECK(infantry->getExperienceTracker()->getExperience() == 1.0f);
    CHECK(enemy->getExperienceTracker()->getExperience() == 1.0f);
    CHECK(far->getExperienceTracker()->getExperience() == 1.0f);
    CHECK(namedAllowed->getExperienceTracker()->getExperience() == 201.0f);
    CHECK(namedAllowed->getExperienceTracker()->getRank() == 2);
    CHECK(namedExcluded->getExperienceTracker()->getExperience() == 1.0f);
    CHECK(grant->granted() == 2);
}

TEST_CASE("PORT-238 Isildur-style ring pulse grants the eligible infantry an object upgrade")
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
    SpellEffectModules::registerAll(w.w.modules);
    w.combat().setAutoAcquireEnabled(false);
    // The single active upgrade nugget of IsildurRingauswirkungWeapon, with
    // synthetic names; the other four literal modifier lists have commented-out
    // Upgrade fields and are intentionally not represented here.
    const char *ini = R"ini(
Upgrade Upgrade_P238IsildurForged
  Type = OBJECT
End
ModifierList P238IsildurGrant
  Category = SPELL
  Duration = 0
  Upgrade = Upgrade_P238IsildurForged Delay:103
End
Weapon P238IsildurPulse
  RadiusDamageAffects = ALLIES NOT_SIMILAR
  LeechRangeWeapon = Yes
  AttackRange = 50
  MeleeWeapon = No
  DelayBetweenShots = 5000
  PreAttackDelay = 0
  PreAttackType = PER_SHOT
  FiringDuration = 0
  AttributeModifierNugget
    AttributeModifier = P238IsildurGrant
    Radius = 15000
    SpecialObjectFilter = ANY +INFANTRY +CAVALRY -ARCHER +HORDE -STRUCTURE -HERO -MACHINE -DOZER -SHIP ALLIES
  End
End
Object P238IsildurPulseHero
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = FireWeaponUpdate Fire
    FireWeaponNugget
      WeaponName = P238IsildurPulse
      Offset = X:0 Y:0 Z:0
      FireDelay = 0
      OneShot = No
    End
  End
End
Object P238IsildurInfantry
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238IsildurArcher
  KindOf = INFANTRY ARCHER SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238IsildurHeroTarget
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_isildur_pulse.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    const ModifierListTemplate *list = modifiers.find("P238IsildurGrant");
    REQUIRE(list != nullptr);
    REQUIRE(list->m_upgrade.get() != nullptr);
    REQUIRE(list->m_upgrade->upgrade != nullptr);
    CHECK(list->m_upgrade->delayFrames == 1); // RW 0x805154 uses colon separators; ceil(103 ms * 0.005)
    Object *infantry = w.unit("P238IsildurInfantry", 'A', 320, 300);
    Object *archer = w.unit("P238IsildurArcher", 'A', 325, 300);
    Object *otherHero = w.unit("P238IsildurHeroTarget", 'A', 330, 300);
    Object *enemy = w.unit("P238IsildurInfantry", 'B', 335, 300);
    for (Object *o : {infantry, archer, otherHero, enemy})
    {
        REQUIRE(o != nullptr);
        w.logic->partition().registerObject(*o);
        CHECK_FALSE(o->hasUpgrade("Upgrade_P238IsildurForged"));
    }
    w.frames(1); // publish targets before creating the pulse source
    Object *hero = w.unit("P238IsildurPulseHero", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    auto *fire = dynamic_cast<FireWeaponUpdate *>(hero->findModule("FireWeaponUpdate"));
    REQUIRE(fire != nullptr);
    for (int i = 0; i < 10 && fire->shots() == 0; ++i) w.frames(1);
    REQUIRE(fire->shots() == 1);
    CHECK_FALSE(infantry->hasUpgrade("Upgrade_P238IsildurForged")); // delayed, not granted at impact
    w.frames(1);
    CHECK(infantry->hasUpgrade("Upgrade_P238IsildurForged"));
    CHECK_FALSE(archer->hasUpgrade("Upgrade_P238IsildurForged"));
    CHECK_FALSE(otherHero->hasUpgrade("Upgrade_P238IsildurForged"));
    CHECK_FALSE(enemy->hasUpgrade("Upgrade_P238IsildurForged"));
    w.frames(30); // 5000 ms = 25 simulation frames, so the non-one-shot record repeats
    CHECK(fire->shots() == 2);
}

TEST_CASE("PORT-238 Isildur-style fear marker selects the allied equivalent form")
{
    World w;
    w.combat().setAutoAcquireEnabled(false);
    // IsildurBlazeObject uses a name filter for the base hero even though the
    // ring form is a distinct template; RW 0x89515C scans, 0x73D5C2 compares.
    const char *ini = R"ini(
Object P238IsildurBase
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object P238IsildurRingForm
  EquivalentTo = P238IsildurBase
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object P238OtherHero
  KindOf = HERO INFANTRY SELECTABLE
  Body = ActiveBody Body
    MaxHealth = 100
  End
End
Object P238FearMarker
  KindOf = NO_COLLIDE IMMOBILE UNATTACKABLE IGNORE_FOR_VICTORY IGNORE_FOR_EVA_SPEECH_POSITION MOVE_ONLY
  Body = ActiveBody Body
    MaxHealth = 999999
  End
  Behavior = AttachUpdate Attach
    ObjectFilter = ANY +P238IsildurBase ALLIES
    ScanRange = 35
    AlwaysTeleport = No
    AnchorToTopOfGeometry = Yes
  End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_isildur_marker.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *other = w.unit("P238OtherHero", 'A', 301, 300);
    Object *enemy = w.unit("P238IsildurBase", 'B', 302, 300);
    Object *ringForm = w.unit("P238IsildurRingForm", 'A', 320, 300);
    Object *marker = w.unit("P238FearMarker", 'A', 300, 300);
    REQUIRE(other != nullptr);
    REQUIRE(enemy != nullptr);
    REQUIRE(ringForm != nullptr);
    REQUIRE(marker != nullptr);
    auto *attach = dynamic_cast<AttachUpdate *>(marker->findModule("AttachUpdate"));
    REQUIRE(attach != nullptr);
    w.logic->partition().update();
    CHECK(attach->parentID() == 0);
    CHECK(attach->update() == UPDATE_SLEEP_NONE);
    CHECK(attach->parentID() == ringForm->getID());
    CHECK(marker->getPosition()->x == ringForm->getPosition()->x);
    CHECK_FALSE(marker->wasTeleportGatheredAt(w.logic->getFrame()));
}

TEST_CASE("PORT-238 Arvedui-style ring delivery chooses a form from the protection upgrade")
{
    HealHudWorld w;
    // arnor/new/arnorarveduinew.ini:1460..1548: the two OBJECT ring
    // upgrades arrive via contain; protection conflicts with the evil command,
    // and both protection and ring are required to enable the good command.
    const char *data = R"ini(
Upgrade Upgrade_P238ArveduiRing
  Type = OBJECT
End
Upgrade Upgrade_P238ArveduiFortress
  Type = OBJECT
End
Upgrade Upgrade_P238ArveduiProtected
  Type = OBJECT
End
Upgrade Upgrade_P238ArveduiPlayerRing
  Type = PLAYER
End
SpecialPower P238ArveduiEvilPower
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower P238ArveduiGoodPower
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
CommandButton P238ArveduiEvilButton
  Command = SPECIAL_POWER
  SpecialPower = P238ArveduiEvilPower
  InPalantir = No
End
CommandButton P238ArveduiGoodButton
  Command = SPECIAL_POWER
  SpecialPower = P238ArveduiGoodPower
  InPalantir = No
End
Object P238ArveduiBase
  KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = UnpauseSpecialPowerUpgrade EnableEvil
    SpecialPowerTemplate = P238ArveduiEvilPower
    TriggeredBy = Upgrade_P238ArveduiRing
  End
  Behavior = SpecialPowerModule EvilPower
    SpecialPowerTemplate = P238ArveduiEvilPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate EvilToggle
    SpecialPowerTemplate = P238ArveduiEvilPower
    MountedTemplate = P238ArveduiEvil
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade EvilCommand
    TriggeredBy = Upgrade_P238ArveduiRing
    ConflictsWith = Upgrade_P238ArveduiProtected
    GetUpgradeCommandButtonName = P238ArveduiEvilButton
  End
  Behavior = UnpauseSpecialPowerUpgrade EnableGood
    SpecialPowerTemplate = P238ArveduiGoodPower
    TriggeredBy = Upgrade_P238ArveduiRing Upgrade_P238ArveduiProtected
    RequiresAllTriggers = Yes
  End
  Behavior = SpecialPowerModule GoodPower
    SpecialPowerTemplate = P238ArveduiGoodPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate GoodToggle
    SpecialPowerTemplate = P238ArveduiGoodPower
    MountedTemplate = P238ArveduiGood
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade GoodCommand
    TriggeredBy = Upgrade_P238ArveduiRing Upgrade_P238ArveduiProtected
    RequiresAllTriggers = Yes
    GetUpgradeCommandButtonName = P238ArveduiGoodButton
  End
  Behavior = ObjectCreationUpgrade ProtectionFromPlayer
    TriggeredBy = Upgrade_P238ArveduiPlayerRing
    GrantUpgrade = Upgrade_P238ArveduiProtected
  End
  Behavior = DoCommandUpgrade GoodCommandFromPlayer
    TriggeredBy = Upgrade_P238ArveduiPlayerRing
    GetUpgradeCommandButtonName = P238ArveduiGoodButton
  End
  Behavior = CitadelSlaughterHordeContain RingEntry
    PassengerFilter = NONE +P238Ring
    ObjectStatusOfContained = UNSELECTABLE ENCLOSED
    ContainMax = 1
    StatusForRingEntry = HOLDING_THE_RING
    UpgradeForRingEntry = Upgrade_P238ArveduiRing Upgrade_P238ArveduiFortress
    ObjectToDestroyForRingEntry = NONE +P238Ring
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 55
  End
End
Object P238ArveduiEvil
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = UnpauseSpecialPowerUpgrade EnableGood
    SpecialPowerTemplate = P238ArveduiGoodPower
    TriggeredBy = Upgrade_P238ArveduiProtected
  End
  Behavior = SpecialPowerModule GoodPower
    SpecialPowerTemplate = P238ArveduiGoodPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate GoodToggle
    SpecialPowerTemplate = P238ArveduiGoodPower
    MountedTemplate = P238ArveduiGood
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade GoodCommand
    TriggeredBy = Upgrade_P238ArveduiProtected
    GetUpgradeCommandButtonName = P238ArveduiGoodButton
  End
  Behavior = DoCommandUpgrade GoodCommandFromPlayer
    TriggeredBy = Upgrade_P238ArveduiPlayerRing
    GetUpgradeCommandButtonName = P238ArveduiGoodButton
  End
  Behavior = ObjectCreationUpgrade ProtectionFromPlayer
    TriggeredBy = Upgrade_P238ArveduiPlayerRing
    GrantUpgrade = Upgrade_P238ArveduiProtected
  End
End
Object P238ArveduiGood
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AI
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_arvedui.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    bool protectedHero = false;
    bool usePlayerUpgrade = false;
    bool playerUpgradeAfterEvil = false;
    SUBCASE("unprotected") { protectedHero = false; }
    SUBCASE("object protection before delivery") { protectedHero = true; }
    SUBCASE("player protection before delivery") { protectedHero = true; usePlayerUpgrade = true; }
    SUBCASE("player protection after evil replacement") { playerUpgradeAfterEvil = true; }
    Object *bearer = w.unit("P238ArveduiBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(bearer != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID original = bearer->getID();
    const ObjectID ringId = ring->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(bearer->getContain());
    REQUIRE(contain != nullptr);
    if (usePlayerUpgrade)
    {
        // RW 0x8B8281 / 0x8B8464: the PLAYER flag queues a delayed
        // ObjectCreationUpgrade grant to the hero before ring delivery.
        const UpgradeTemplate *playerUpgrade = Player::resolveUpgrade("Upgrade_P238ArveduiPlayerRing", true);
        REQUIRE(playerUpgrade != nullptr);
        w.playerOf('A')->addUpgrade(playerUpgrade, Player::UPGRADE_STATUS_COMPLETE);
        w.frames(3);
        REQUIRE(bearer->hasUpgrade("Upgrade_P238ArveduiProtected"));
    }
    else if (protectedHero) bearer->giveUpgrade("Upgrade_P238ArveduiProtected");
    // RW 0x8840FC -> 0x8D26F3 / 0x8B8E2E -> 0x8B1690.
    deliver(*contain, *carrier);
    CHECK(bearer->hasUpgrade("Upgrade_P238ArveduiRing"));
    CHECK(bearer->hasUpgrade("Upgrade_P238ArveduiFortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *selected = nullptr;
    Object *other = nullptr;
    for (int i = 0; i < 50 && !selected; ++i)
    {
        w.frames(1);
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
        {
            if (!o->getTemplate()) continue;
            const std::string &name = o->getTemplate()->getName();
            if (name == (protectedHero ? "P238ArveduiGood" : "P238ArveduiEvil")) selected = o;
            if (name == (protectedHero ? "P238ArveduiEvil" : "P238ArveduiGood")) other = o;
        }
    }
    REQUIRE(selected != nullptr);
    CHECK(other == nullptr);
    CHECK(selected->getControllingPlayer() == w.playerOf('A'));
    w.frames(3);
    CHECK(w.byId(original) == nullptr);
    if (playerUpgradeAfterEvil)
    {
        const ObjectID evilId = selected->getID();
        const UpgradeTemplate *playerUpgrade = Player::resolveUpgrade("Upgrade_P238ArveduiPlayerRing", true);
        REQUIRE(playerUpgrade != nullptr);
        w.playerOf('A')->addUpgrade(playerUpgrade, Player::UPGRADE_STATUS_COMPLETE);
        w.frames(3);
        REQUIRE(w.byId(evilId) != nullptr);
        REQUIRE(w.byId(evilId)->hasUpgrade("Upgrade_P238ArveduiProtected"));
        Object *good = nullptr;
        for (int i = 0; i < 50 && !good; ++i)
        {
            w.frames(1);
            for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
                if (o->getTemplate() && o->getTemplate()->getName() == "P238ArveduiGood") good = o;
        }
        REQUIRE(good != nullptr);
        CHECK(good->getControllingPlayer() == w.playerOf('A'));
        w.frames(3);
        CHECK(w.byId(evilId) == nullptr);
    }
}

TEST_CASE("PORT-238 Smaug-style ring pulse marks only its owner's treasure troves")
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
    SpellEffectModules::registerAll(w.w.modules);
    w.combat().setAutoAcquireEnabled(false);
    w.playerOf('A')->setPlayerRelationship(w.playerOf('B'), ALLIES);
    w.playerOf('B')->setPlayerRelationship(w.playerOf('A'), ALLIES);
    // includes/weapon.inc:44468 and attributemodifier.inc:8054: RW 0x88F554
    // fires the pulse; 0x90EAF9 applies the filtered modifier, which sets USER_6.
    const char *ini = R"ini(
ModifierList P238SmaugTroveMarker
  Category = WEAPON
  Duration = 5000
  ModelCondition = USER_6
End
Weapon P238SmaugPulse
  RadiusDamageAffects = ALLIES NOT_SIMILAR
  LeechRangeWeapon = Yes
  AttackRange = 50
  MeleeWeapon = No
  DelayBetweenShots = 5000
  PreAttackDelay = 0
  PreAttackType = PER_SHOT
  FiringDuration = 0
  AttributeModifierNugget
    AttributeModifier = P238SmaugTroveMarker
    Radius = 15000
    SpecialObjectFilter = NONE +P238Trove +P238TrovePlayer SAME_PLAYER
  End
End
Object P238SmaugPulseHero
  KindOf = HERO MONSTER SELECTABLE
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = FireWeaponUpdate Fire
    FireWeaponNugget
      WeaponName = P238SmaugPulse
      Offset = X:0 Y:0 Z:0
      FireDelay = 0
      OneShot = No
    End
  End
End
Object P238Trove
  KindOf = STRUCTURE SELECTABLE
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238TrovePlayer
  KindOf = STRUCTURE SELECTABLE
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238OtherStructure
  KindOf = STRUCTURE SELECTABLE
  Body = ActiveBody Body
    MaxHealth = 100
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_smaug_pulse.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *own = w.unit("P238Trove", 'A', 320, 300);
    Object *ownVariant = w.unit("P238TrovePlayer", 'A', 325, 300);
    Object *ally = w.unit("P238Trove", 'B', 330, 300);
    Object *other = w.unit("P238OtherStructure", 'A', 335, 300);
    const int user6 = CombatNames::modelCondition("USER_6");
    REQUIRE(user6 == 458); // RW 0xD9FAD8: USER_6
    for (Object *o : {own, ownVariant, ally, other})
    {
        REQUIRE(o != nullptr);
        w.logic->partition().registerObject(*o);
        CHECK_FALSE(o->testModelCondition(user6));
    }
    w.frames(1);
    Object *smaug = w.unit("P238SmaugPulseHero", 'A', 300, 300);
    REQUIRE(smaug != nullptr);
    auto *fire = dynamic_cast<FireWeaponUpdate *>(smaug->findModule("FireWeaponUpdate"));
    REQUIRE(fire != nullptr);
    for (int i = 0; i < 10 && fire->shots() == 0; ++i) w.frames(1);
    REQUIRE(fire->shots() == 1);
    CHECK(own->testModelCondition(user6));
    CHECK(ownVariant->testModelCondition(user6));
    CHECK_FALSE(ally->testModelCondition(user6));
    CHECK_FALSE(other->testModelCondition(user6));
    w.frames(30);
    CHECK(fire->shots() == 2);
    CHECK(own->testModelCondition(user6));
    CHECK_FALSE(ally->testModelCondition(user6));
}

TEST_CASE("PORT-238 Smaug-style flight and ground replacement copy only their listed timers")
{
    HealHudWorld w;
    // wildsmaug_new.ini:1549 and :2374: fly lists five powers, ground lists
    // four. RW 0x8B140D -> 0x8B12BF -> 0x897368 copies listed timers.
    // The real ground form does not declare the fifth power. A second,
    // deliberately hypothetical layout with it on both sides checks list sensitivity.
    bool fromFly = false;
    bool groundHasDominate = false;
    SUBCASE("real-layout fly to ground") { fromFly = true; }
    SUBCASE("real-layout ground to fly") { fromFly = false; }
    SUBCASE("both-have-fifth fly to ground") { fromFly = true; groundHasDominate = true; }
    SUBCASE("both-have-fifth ground to fly") { fromFly = false; groundHasDominate = true; }
    std::string data = R"ini(
SpecialPower P238SmaugToggle
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower P238SmaugFireball
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower P238SmaugFlight
  Enum = SPECIAL_GENERAL_TARGETLESS_TWO
  ReloadTime = 120000
End
SpecialPower P238SmaugIncinerate
  Enum = SPECIAL_HERO_MODE
  ReloadTime = 120000
End
SpecialPower P238SmaugCurse
  Enum = SPECIAL_ATHELAS
  ReloadTime = 120000
End
SpecialPower P238SmaugDominate
  Enum = SPECIAL_DOMINATE_ENEMY
  ReloadTime = 120000
End
)ini";
    const std::vector<std::string> names = {"P238SmaugFireball", "P238SmaugFlight", "P238SmaugIncinerate", "P238SmaugCurse", "P238SmaugDominate"};
    for (int fly = 0; fly < 2; ++fly)
    {
        data += std::string("Object ") + (fly ? "P238SmaugFly" : "P238SmaugGround") + R"ini(
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule TogglePower
    SpecialPowerTemplate = P238SmaugToggle
    UpdateModuleStartsAttack = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate Toggle
    SpecialPowerTemplate = P238SmaugToggle
)ini";
        data += std::string("    MountedTemplate = ") + (fly ? "P238SmaugGround\n" : "P238SmaugFly\n");
        data += "    SynchronizeTimerOnSpecialPower = P238SmaugFireball P238SmaugFlight P238SmaugIncinerate P238SmaugCurse";
        if (fly) data += " P238SmaugDominate";
        data += fly ? R"ini(
    StartAbilityRange = 50
    UnpackTime = 0
    PreparationTime = 1
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
)ini" : R"ini(
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
)ini";
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (i == 4 && !fly && !groundHasDominate) continue;
            data += "  Behavior = SpecialPowerModule Timer" + std::to_string(i) + "\n    SpecialPowerTemplate = " + names[i] + "\n  End\n";
        }
        data += "End\n";
    }
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_smaug_toggle.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *source = w.unit(fromFly ? "P238SmaugFly" : "P238SmaugGround", 'A', 300, 300);
    REQUIRE(source != nullptr);
    source->setName("P238SmaugNamed");
    const ObjectID sourceId = source->getID();
    Team *team = source->getTeam();
    const auto power = [&](Object &obj, const std::string &name) {
        const SpecialPowerTemplate *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        auto *timer = power(*source, names[i]);
        if (i == 4 && !fromFly && !groundHasDominate)
        {
            CHECK(timer == nullptr);
            continue;
        }
        REQUIRE(timer != nullptr);
        timer->setReadyFrame(500 + (unsigned)i * 100);
    }
    auto *toggle = dynamic_cast<ToggleMountedSpecialAbilityUpdate *>(source->findModule("ToggleMountedSpecialAbilityUpdate"));
    REQUIRE(toggle != nullptr);
    const Coord3D landing{310.0f, 300.0f, 0.0f};
    toggle->initiateIntentToDoSpecialPower(toggle->abilityData()->m_specialPowerTemplate, nullptr, fromFly ? &landing : nullptr, 0, 0);
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == (fromFly ? "P238SmaugGround" : "P238SmaugFly")) replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    CHECK(replacement->getName() == "P238SmaugNamed");
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        auto *timer = power(*replacement, names[i]);
        if (i == 4 && fromFly && !groundHasDominate)
        {
            CHECK(timer == nullptr);
            continue;
        }
        REQUIRE(timer != nullptr);
        if (i < 4 || fromFly) CHECK(timer->getReadyFrame() == 500 + (unsigned)i * 100);
        else CHECK(timer->getReadyFrame() == w.logic->getFrame()); // unlisted timer starts ready
    }
    w.frames(3);
    CHECK(w.byId(sourceId) == nullptr);
}

TEST_CASE("PORT-238 Radagast-style eagle toggle resolves distinct crow power names")
{
    HealHudWorld w;
    // rohanradagast.ini:1695,3055 lists Ravens for take-off but Krhenruf
    // for landing. RW 0x8B12BF -> 0x69C146 / 0x68C26D -> 0x897368.
    bool fromEagle = false;
    SUBCASE("take-off lists Ravens") { fromEagle = false; }
    SUBCASE("landing lists Krhenruf") { fromEagle = true; }
    std::string data = R"ini(
SpecialPower P238RadaToggle
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower SpecialAbilityRadagastRavens
  Enum = SPECIAL_SPELL_BOOK_DRAGON_STRIKE
  ReloadTime = 120000
End
SpecialPower SpecialAbilityRadagastKrhenruf
  Enum = SPECIAL_SPELL_BOOK_DRAGON_STRIKE
  ReloadTime = 60000
End
SpecialPower SpecialAbilityRadagastDornengestrpp
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower SpecialAbilityRadagastFreunde
  Enum = SPECIAL_GENERAL_TARGETLESS_TWO
  ReloadTime = 120000
End
SpecialPower SpecialAbilityRadagastWordOfPoultry
  Enum = SPECIAL_HERO_MODE
  ReloadTime = 120000
End
SpecialPower SpecialAbilityNatursanktum
  Enum = SPECIAL_ATHELAS
  ReloadTime = 120000
End
)ini";
    const std::vector<std::string> names = {"SpecialAbilityRadagastDornengestrpp", "SpecialAbilityRadagastFreunde", "SpecialAbilityRadagastWordOfPoultry", "SpecialAbilityNatursanktum", "SpecialAbilityRadagastRavens"};
    for (int eagle = 0; eagle < 2; ++eagle)
    {
        data += std::string("Object ") + (eagle ? "P238RadaEagle" : "P238RadaGround") + R"ini(
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule TogglePower
    SpecialPowerTemplate = P238RadaToggle
    UpdateModuleStartsAttack = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate Toggle
    SpecialPowerTemplate = P238RadaToggle
)ini";
        data += std::string("    MountedTemplate = ") + (eagle ? "P238RadaGround\n" : "P238RadaEagle\n");
        data += "    SynchronizeTimerOnSpecialPower = SpecialAbilityRadagastDornengestrpp SpecialAbilityRadagastFreunde SpecialAbilityRadagastWordOfPoultry SpecialAbilityNatursanktum ";
        data += eagle ? "SpecialAbilityRadagastKrhenruf\n" : "SpecialAbilityRadagastRavens\n";
        data += eagle ? R"ini(
    StartAbilityRange = 50
    UnpackTime = 0
    PreparationTime = 1
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
)ini" : R"ini(
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
)ini";
        for (std::size_t i = 0; i < names.size(); ++i)
            data += "  Behavior = SpecialPowerModule Timer" + std::to_string(i) + "\n    SpecialPowerTemplate = " + names[i] + "\n    AvailableAtStart = Yes\n  End\n";
        data += "End\n";
    }
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_radagast_timers.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    const auto *ravens = w.powers.findSpecialPowerTemplate("SpecialAbilityRadagastRavens");
    const auto *krhenruf = w.powers.findSpecialPowerTemplate("SpecialAbilityRadagastKrhenruf");
    REQUIRE(ravens != nullptr);
    REQUIRE(krhenruf != nullptr);
    CHECK(ravens != krhenruf); // same Enum, distinct named templates
    Object *source = w.unit(fromEagle ? "P238RadaEagle" : "P238RadaGround", 'A', 300, 300);
    REQUIRE(source != nullptr);
    CHECK(SpecialPowerModules::findModule(*source, krhenruf) == nullptr);
    const auto power = [&](Object &obj, const std::string &name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        auto *timer = power(*source, names[i]);
        REQUIRE(timer != nullptr);
        timer->setReadyFrame(500 + (unsigned)i * 100);
    }
    auto *toggle = dynamic_cast<ToggleMountedSpecialAbilityUpdate *>(source->findModule("ToggleMountedSpecialAbilityUpdate"));
    REQUIRE(toggle != nullptr);
    const Coord3D landing{310.0f, 300.0f, 0.0f};
    toggle->initiateIntentToDoSpecialPower(toggle->abilityData()->m_specialPowerTemplate, nullptr, fromEagle ? &landing : nullptr, 0, 0);
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == (fromEagle ? "P238RadaGround" : "P238RadaEagle")) replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(SpecialPowerModules::findModule(*replacement, krhenruf) == nullptr);
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        auto *timer = power(*replacement, names[i]);
        REQUIRE(timer != nullptr);
        if (i < 4 || !fromEagle) CHECK(timer->getReadyFrame() == 500 + (unsigned)i * 100);
        else CHECK(timer->getReadyFrame() == w.logic->getFrame()); // synthetic AvailableAtStart
    }
}

TEST_CASE("PORT-238 Witch-king-style delivery creates a player marker and swaps the owner's fortress set")
{
    HealHudWorld w;
    // angwk.ini:1030-1057; projectile.ini:7580-7611; angmarfortress.ini:1093-1110.
    // RW 0x8840FC grants the ring; 0x8B8281 / 0x8B8464 create the marker;
    // 0x8BD17B / 0x6AEE22 grant a player upgrade; 0x8B7C68 changes the command set.
    const char *data = R"ini(
Upgrade Upgrade_P238WKRing
  Type = OBJECT
End
Upgrade Upgrade_P238WKFortress
  Type = OBJECT
End
Upgrade Upgrade_P238WKMarker
  Type = PLAYER
End
Upgrade Upgrade_P238WKHQ
  Type = PLAYER
End
Upgrade Upgrade_P238WKWrongParent
  Type = PLAYER
End
Upgrade Upgrade_P238WKChoiceD
  Type = PLAYER
End
Upgrade Upgrade_P238WKChoiceM
  Type = PLAYER
End
Upgrade Upgrade_P238WKChoiceR
  Type = PLAYER
End
SpecialPower P238WKTransform
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
CommandButton P238WKSwitch
  Command = SPECIAL_POWER
  SpecialPower = P238WKTransform
  InPalantir = No
End
CommandButton P238WKStop
  Command = STOP
  InPalantir = Yes
End
CommandSet P238WKFortBase
  1 = P238WKStop
End
CommandSet P238WKFortChoiceD
  1 = P238WKStop
End
CommandSet P238WKFortChoiceM
  1 = P238WKStop
End
CommandSet P238WKFortChoiceR
  1 = P238WKStop
End
CommandSet P238WKFortRing
  1 = P238WKStop
End
CommandSet P238WKFortRingChoiceD
  1 = P238WKStop
End
CommandSet P238WKFortRingChoiceM
  1 = P238WKStop
End
CommandSet P238WKFortRingChoiceR
  1 = P238WKStop
End
Object P238WKMarkerParent
  KindOf = INERT IMMOBILE UNATTACKABLE
  Body = ActiveBody Body
    MaxHealth = 1
  End
  Behavior = LifetimeUpdate Life
    MinLifetime = 0
    MaxLifetime = 0
  End
  Behavior = GrantUpgradeCreate Grant
    UpgradeToGrant = Upgrade_P238WKWrongParent
  End
  Behavior = SlowDeathBehavior Death
    DestructionDelay = 0
  End
End
ChildObject P238WKMarker P238WKMarkerParent
  Behavior = GrantUpgradeCreate Grant
    UpgradeToGrant = Upgrade_P238WKMarker
  End
End
Object P238WKFort
  KindOf = STRUCTURE SELECTABLE
  CommandSet = P238WKFortBase
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = CommandSetUpgrade ChoiceD
    TriggeredBy = Upgrade_P238WKChoiceD
    CommandSet = P238WKFortChoiceD
  End
  Behavior = CommandSetUpgrade ChoiceM
    TriggeredBy = Upgrade_P238WKChoiceM
    CommandSet = P238WKFortChoiceM
  End
  Behavior = CommandSetUpgrade ChoiceR
    TriggeredBy = Upgrade_P238WKChoiceR
    CommandSet = P238WKFortChoiceR
  End
  Behavior = CommandSetUpgrade Ring
    TriggeredBy = Upgrade_P238WKMarker
    CommandSet = P238WKFortRing
  End
  Behavior = CommandSetUpgrade RingChoiceD
    TriggeredBy = Upgrade_P238WKMarker Upgrade_P238WKChoiceD
    RequiresAllTriggers = Yes
    CommandSet = P238WKFortRingChoiceD
  End
  Behavior = CommandSetUpgrade RingChoiceM
    TriggeredBy = Upgrade_P238WKMarker Upgrade_P238WKChoiceM
    RequiresAllTriggers = Yes
    CommandSet = P238WKFortRingChoiceM
  End
  Behavior = CommandSetUpgrade RingChoiceR
    TriggeredBy = Upgrade_P238WKMarker Upgrade_P238WKChoiceR
    RequiresAllTriggers = Yes
    CommandSet = P238WKFortRingChoiceR
  End
End
Object P238WKBase
  KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule RingGainStarter
    SpecialPowerTemplate = P238WKTransform
    UpdateModuleStartsAttack = Yes
    StartsPaused = No
  End
  Behavior = ToggleMountedSpecialAbilityUpdate RingGain
    SpecialPowerTemplate = P238WKTransform
    MountedTemplate = P238WKRingForm
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade RingCommand
    TriggeredBy = Upgrade_P238WKRing
    GetUpgradeCommandButtonName = P238WKSwitch
  End
  Behavior = ObjectCreationUpgrade Marker
    TriggeredBy = Upgrade_P238WKRing
    Delay = 0
    ThingToSpawn = P238WKMarker
  End
  Behavior = CitadelSlaughterHordeContain RingEntry
    PassengerFilter = NONE +P238Ring
    ObjectStatusOfContained = UNSELECTABLE ENCLOSED
    ContainMax = 1
    StatusForRingEntry = HOLDING_THE_RING
    UpgradeForRingEntry = Upgrade_P238WKRing Upgrade_P238WKFortress
    ObjectToDestroyForRingEntry = NONE +P238Ring
  End
End
Object P238WKRingForm
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = GrantUpgradeCreate EnableHQ
    UpgradeToGrant = Upgrade_P238WKHQ
    GiveOnBuildComplete = Yes
  End
  Behavior = SpecialPowerModule RingGainStarter
    SpecialPowerTemplate = P238WKTransform
    UpdateModuleStartsAttack = Yes
    StartsPaused = No
  End
  Behavior = ToggleMountedSpecialAbilityUpdate RingGain
    SpecialPowerTemplate = P238WKTransform
    MountedTemplate = P238WKRingForm
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade RingCommand
    TriggeredBy = Upgrade_P238WKRing
    GetUpgradeCommandButtonName = P238WKSwitch
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_wk_marker.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    std::string choice;
    bool choiceLate = false;
    SUBCASE("no prior subfaction") { choice = ""; }
    SUBCASE("Dunedain-style choice before hand-over") { choice = "D"; }
    SUBCASE("Magic-style choice before hand-over") { choice = "M"; }
    SUBCASE("Rhudaur-style choice before hand-over") { choice = "R"; }
    SUBCASE("Dunedain-style choice after hand-over") { choice = "D"; choiceLate = true; }
    SUBCASE("Magic-style choice after hand-over") { choice = "M"; choiceLate = true; }
    SUBCASE("Rhudaur-style choice after hand-over") { choice = "R"; choiceLate = true; }
    Object *fort = w.unit("P238WKFort", 'A', 200, 200);
    Object *otherFort = w.unit("P238WKFort", 'B', 600, 600);
    Object *hero = w.unit("P238WKBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(fort != nullptr);
    REQUIRE(otherFort != nullptr);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    if (!choice.empty() && !choiceLate)
    {
        const auto *u = Player::resolveUpgrade("Upgrade_P238WKChoice" + choice, true);
        REQUIRE(u != nullptr);
        w.playerOf('A')->addUpgrade(u, Player::UPGRADE_STATUS_COMPLETE);
        w.frames(2);
    }
    CHECK(fort->getCommandSetName() == (choice.empty() || choiceLate ? "P238WKFortBase" : "P238WKFortChoice" + choice));
    CHECK(otherFort->getCommandSetName() == "P238WKFortBase");
    CHECK_FALSE(w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKMarker"));
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238WKRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238WKFortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    ObjectID markerId = INVALID_ID;
    for (int i = 0; i < 15 && !w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKMarker"); ++i)
    {
        w.frames(1);
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == "P238WKMarker") markerId = o->getID();
    }
    REQUIRE(w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKMarker"));
    REQUIRE(markerId != INVALID_ID);
    CHECK_FALSE(w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKWrongParent"));
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238WKMarker"));
    CHECK(fort->getCommandSetName() == (choice.empty() || choiceLate ? "P238WKFortRing" : "P238WKFortRingChoice" + choice));
    CHECK(otherFort->getCommandSetName() == "P238WKFortBase");
    if (choiceLate)
    {
        const auto *u = Player::resolveUpgrade("Upgrade_P238WKChoice" + choice, true);
        REQUIRE(u != nullptr);
        w.playerOf('A')->addUpgrade(u, Player::UPGRADE_STATUS_COMPLETE);
        w.frames(2);
        CHECK(fort->getCommandSetName() == "P238WKFortRingChoice" + choice);
    }
    Object *replacement = nullptr;
    for (int i = 0; i < 50 && !replacement; ++i)
    {
        w.frames(1);
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == "P238WKRingForm") replacement = o;
    }
    REQUIRE(replacement != nullptr);
    const ObjectID replacementId = replacement->getID();
    CHECK(replacement->getControllingPlayer() == w.playerOf('A'));
    CHECK_FALSE(replacement->hasUpgrade("Upgrade_P238WKRing"));
    CHECK(w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKHQ"));
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238WKHQ"));
    w.frames(15); // longer than the synthetic 2000 ms self-replacement unpack
    CHECK(w.byId(heroId) == nullptr);
    CHECK(w.byId(markerId) == nullptr);
    REQUIRE(w.byId(replacementId) != nullptr);
    CHECK(w.playerOf('A')->hasUpgradeComplete("Upgrade_P238WKMarker"));
    CHECK(fort->getCommandSetName() == (choice.empty() ? "P238WKFortRing" : "P238WKFortRingChoice" + choice));
    // An explicit OBJECT upgrade, not actual ring reacquisition, fires the
    // synthetic ring form's self-targeting command once (angwk.ini:2884-2905).
    w.byId(replacementId)->giveUpgrade("Upgrade_P238WKRing");
    Object *selfReplacement = nullptr;
    for (int i = 0; i < 50 && !selfReplacement; ++i)
    {
        w.frames(1);
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getID() != replacementId && o->getTemplate() && o->getTemplate()->getName() == "P238WKRingForm") selfReplacement = o;
    }
    REQUIRE(selfReplacement != nullptr);
    const ObjectID selfId = selfReplacement->getID();
    CHECK_FALSE(selfReplacement->hasUpgrade("Upgrade_P238WKRing"));
    w.frames(15);
    CHECK(w.byId(replacementId) == nullptr);
    CHECK(w.byId(selfId) != nullptr);
}

TEST_CASE("PORT-238 Galadriel-style ring choice combines player and object upgrades")
{
    HealHudWorld w;
    // lothloriengaladriel.ini:877-970: accept needs the PLAYER choice and
    // both OBJECT ring upgrades; reject needs only its PLAYER choice.
    // RW 0x8D26F3 / 0x8B8E2E -> 0x8B1690; command-set mux RW 0x8B7CB9.
    const char *data = R"ini(
Upgrade Upgrade_P238GalRing
  Type = OBJECT
End
Upgrade Upgrade_P238GalFortress
  Type = OBJECT
End
Upgrade Upgrade_P238GalAccept
  Type = PLAYER
End
Upgrade Upgrade_P238GalReject
  Type = PLAYER
End
Upgrade Upgrade_P238GalPreviously
  Type = PLAYER
End
SpecialPower P238GalAcceptPower
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower P238GalRejectPower
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
CommandButton P238GalAcceptButton
  Command = SPECIAL_POWER
  SpecialPower = P238GalAcceptPower
  InPalantir = No
End
CommandButton P238GalRejectButton
  Command = SPECIAL_POWER
  SpecialPower = P238GalRejectPower
  InPalantir = No
End
CommandButton P238GalStop
  Command = STOP
  InPalantir = Yes
End
CommandSet P238GalBaseSet
  1 = P238GalStop
End
CommandSet P238GalDecisionSet
  1 = P238GalStop
End
CommandSet P238GalPreviouslySet
  1 = P238GalStop
End
Object P238GalBase
  KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
  CommandSet = P238GalBaseSet
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule AcceptStarter
    SpecialPowerTemplate = P238GalAcceptPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = No
  End
  Behavior = ToggleMountedSpecialAbilityUpdate AcceptToggle
    SpecialPowerTemplate = P238GalAcceptPower
    MountedTemplate = P238GalAcceptForm
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade AcceptCommand
    TriggeredBy = Upgrade_P238GalAccept Upgrade_P238GalRing Upgrade_P238GalFortress
    RequiresAllTriggers = Yes
    GetUpgradeCommandButtonName = P238GalAcceptButton
  End
  Behavior = SpecialPowerModule RejectStarter
    SpecialPowerTemplate = P238GalRejectPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = No
  End
  Behavior = ToggleMountedSpecialAbilityUpdate RejectToggle
    SpecialPowerTemplate = P238GalRejectPower
    MountedTemplate = P238GalRejectForm
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade RejectCommand
    TriggeredBy = Upgrade_P238GalReject
    GetUpgradeCommandButtonName = P238GalRejectButton
  End
  Behavior = CommandSetUpgrade PreviousChoice
    TriggeredBy = Upgrade_P238GalPreviously
    CommandSet = P238GalPreviouslySet
  End
  Behavior = CitadelSlaughterHordeContain RingEntry
    PassengerFilter = NONE +P238Ring
    ObjectStatusOfContained = UNSELECTABLE ENCLOSED
    ContainMax = 1
    StatusForRingEntry = HOLDING_THE_RING
    UpgradeForRingEntry = Upgrade_P238GalRing Upgrade_P238GalFortress
    ObjectToDestroyForRingEntry = NONE +P238Ring
  End
  Behavior = CommandSetUpgrade Decision
    TriggeredBy = Upgrade_P238GalRing Upgrade_P238GalFortress
    ConflictsWith = Upgrade_P238GalPreviously
    CommandSet = P238GalDecisionSet
  End
End
Object P238GalAcceptForm
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
End
Object P238GalRejectForm
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_galadriel_choice.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    std::string selected;
    std::string partial;
    bool selectBefore = false;
    bool deliverRing = true;
    bool previousBefore = false;
    bool previousAfter = false;
    SUBCASE("delivery leaves a choice") {}
    SUBCASE("accept after delivery") { selected = "Accept"; }
    SUBCASE("accept before delivery") { selected = "Accept"; selectBefore = true; }
    SUBCASE("accept with only hero ring upgrade") { selected = "Accept"; selectBefore = true; partial = "Upgrade_P238GalRing"; }
    SUBCASE("accept with only fortress ring upgrade") { selected = "Accept"; selectBefore = true; partial = "Upgrade_P238GalFortress"; }
    SUBCASE("reject after delivery") { selected = "Reject"; }
    SUBCASE("reject without delivery") { selected = "Reject"; selectBefore = true; deliverRing = false; }
    SUBCASE("previous choice before delivery") { previousBefore = true; }
    SUBCASE("previous choice after delivery") { previousAfter = true; }
    Object *hero = w.unit("P238GalBase", 'A', 300, 300);
    Object *other = w.unit("P238GalBase", 'B', 600, 600);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(other != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID otherId = other->getID();
    const ObjectID ringId = ring->getID();
    auto givePlayer = [&](const std::string &name) {
        const auto *upgrade = Player::resolveUpgrade(name, true);
        REQUIRE(upgrade != nullptr);
        w.playerOf('A')->addUpgrade(upgrade, Player::UPGRADE_STATUS_COMPLETE);
    };
    if (previousBefore) givePlayer("Upgrade_P238GalPreviously");
    if (selectBefore) givePlayer("Upgrade_P238Gal" + selected);
    if (!partial.empty())
    {
        hero->giveUpgrade(partial);
        w.frames(20); // one listed OBJECT upgrade and PLAYER choice are not all three
        REQUIRE(w.byId(heroId) != nullptr);
        CHECK(w.byId(heroId)->getTemplate()->getName() == "P238GalBase");
    }
    if (deliverRing)
    {
        auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(w.byId(heroId)->getContain());
        REQUIRE(contain != nullptr);
        deliver(*contain, *carrier);
        CHECK(w.byId(heroId)->hasUpgrade("Upgrade_P238GalRing"));
        CHECK(w.byId(heroId)->hasUpgrade("Upgrade_P238GalFortress"));
        REQUIRE(w.byId(ringId) != nullptr);
        CHECK(w.byId(ringId)->isDestroyed());
        CHECK(w.byId(heroId)->getCommandSetName() == (previousBefore ? "P238GalPreviouslySet" : "P238GalDecisionSet"));
    }
    if (previousAfter)
    {
        givePlayer("Upgrade_P238GalPreviously");
        CHECK(w.byId(heroId)->getCommandSetName() == "P238GalPreviouslySet");
    }
    if (!selected.empty() && !selectBefore) givePlayer("Upgrade_P238Gal" + selected);
    Object *replacement = nullptr;
    for (int i = 0; i < 50; ++i)
    {
        w.frames(1);
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == "P238Gal" + selected + "Form") replacement = o;
        if (replacement) break;
    }
    if (selected.empty())
    {
        CHECK(replacement == nullptr);
        CHECK(w.byId(heroId) != nullptr);
        REQUIRE(w.byId(heroId) != nullptr);
        CHECK(w.byId(heroId)->getCommandSetName() == (previousBefore || previousAfter ? "P238GalPreviouslySet" : "P238GalDecisionSet"));
    }
    else
    {
        REQUIRE(replacement != nullptr);
        CHECK(replacement->getControllingPlayer() == w.playerOf('A'));
        w.frames(3);
        CHECK(w.byId(heroId) == nullptr);
        if (!deliverRing) CHECK(w.byId(ringId) != nullptr);
    }
    REQUIRE(w.byId(otherId) != nullptr);
    CHECK(w.byId(otherId)->getCommandSetName() == "P238GalBaseSet");
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238GalAccept"));
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238GalReject"));
}

TEST_CASE("PORT-238 Galadriel-style Telperion weapon sweeps damage modifiers and paralysis")
{
    HealWorld w;
    AttributeModifierStore modifiers;
    AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
    struct RestoreModifiers
    {
        HealWorld &world;
        AttributeModifierStore *saved;
        ~RestoreModifiers() { world.logic->reset(); TheAttributeModifierStore = saved; }
    } restore{w, savedModifiers};
    TheAttributeModifierStore = &modifiers;
    modifiers.registerBlock(w.w.fx.env.blocks);
    w.combat().setAutoAcquireEnabled(false);
    // includes/weapon.inc:35206-35256: the damage nugget is 2200 FLAME at
    // radius 170 with DamageScalar 0% ALL -HERO. RW 0x90E28C fills damage;
    // Names and troll filter are synthetic; no actual assets or mod are loaded.
    const char *data = R"ini(
ModifierList P238GalStone
  Category = WEAPON
  Modifier = ARMOR 9999%
  Modifier = SPEED 0%
  Duration = 20000
  ModelCondition = STONED
End
ModifierList P238GalHeroDebuff
  Category = DEBUFF
  Modifier = SPEED 30%
  Duration = 13000
  ReplaceInCategoryIfLongest = Yes
End
ModifierList P238GalDebuff
  Category = DEBUFF
  Modifier = RESIST_KNOCKBACK 100%
  Modifier = CRUSHABLE_LEVEL 5
  Duration = 12000
  ReplaceInCategoryIfLongest = Yes
End
SpecialPower P238GalTelperion
  Enum = SPECIAL_WORD_OF_POWER
  ReloadTime = 200000
End
Weapon P238GalTelperionWeapon
  AttackRange = 80
  RadiusDamageAffects = ENEMIES NEUTRALS NOT_SIMILAR
  DelayBetweenShots = 5000
  PreAttackDelay = 2600
  PreAttackType = PER_ATTACK
  FiringDuration = 1400
  AttributeModifierNugget
    AttributeModifier = P238GalStone
    Radius = 170
    SpecialObjectFilter = ANY +P238GalTelperionTroll
  End
  AttributeModifierNugget
    AttributeModifier = P238GalHeroDebuff
    Radius = 170
    SpecialObjectFilter = NONE +HERO
  End
  AttributeModifierNugget
    AttributeModifier = P238GalDebuff
    Radius = 170
  End
  ParalyzeNugget
    Radius = 170
    Duration = 12000
    SpecialObjectFilter = ALL -HERO -STRUCTURE -MordorTribute_forHarad -MordorTribute_forHaradSmall
    FreezeAnimation = No
    AffectHordeMembers = Yes
  End
  DamageNugget
    Damage = 2200
    Radius = 170
    DelayTime = 0
    DamageType = FLAME
    DeathType = BURNED
    DamageScalar = 0% ALL -HERO
  End
End
Object P238GalTelperionCaster
  KindOf = HERO INFANTRY SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  Body = ActiveBody Body
    MaxHealth = 10000
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule Starter
    SpecialPowerTemplate = P238GalTelperion
    UpdateModuleStartsAttack = Yes
  End
  Behavior = WeaponFireSpecialAbilityUpdate Cast
    SpecialPowerTemplate = P238GalTelperion
    SpecialWeapon = P238GalTelperionWeapon
    WhichSpecialWeapon = 1
    SkipContinue = Yes
    UnpackTime = 1700
    PackTime = 1
    FreezeAfterTriggerDuration = 2500
    StartAbilityRange = 80
    IgnoreFacingCheck = Yes
  End
End
Object P238GalTelperionHero
  KindOf = HERO INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  ArmorSet
    Conditions = None
    Armor = NoArmor
  End
  Body = ActiveBody Body
    MaxHealth = 10000
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238GalTelperionInfantry
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  ArmorSet
    Conditions = None
    Armor = NoArmor
  End
  Body = ActiveBody Body
    MaxHealth = 10000
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
Object P238GalTelperionTroll
  KindOf = MONSTER SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryHeight = 10
  ArmorSet
    Conditions = None
    Armor = NoArmor
  End
  Body = ActiveBody Body
    MaxHealth = 10000
  End
  Behavior = AttributeModifierPoolUpdate Pool
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_galadriel_telperion.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *caster = w.unit("P238GalTelperionCaster", 'A', 300, 300);
    Object *enemyHero = w.unit("P238GalTelperionHero", 'B', 320, 300);
    Object *enemyInfantry = w.unit("P238GalTelperionInfantry", 'B', 330, 300);
    Object *troll = w.unit("P238GalTelperionTroll", 'B', 350, 300);
    Object *allyHero = w.unit("P238GalTelperionHero", 'A', 340, 300);
    Object *farHero = w.unit("P238GalTelperionHero", 'B', 500, 300);
    for (Object *o : {caster, enemyHero, enemyInfantry, troll, allyHero, farHero})
    {
        REQUIRE(o != nullptr);
        w.logic->partition().registerObject(*o);
        CHECK(o->getBodyModule()->getHealth() == 10000.0f);
    }
    w.frames(1);
    auto *ability = dynamic_cast<WeaponFireSpecialAbilityUpdate *>(caster->findModule("WeaponFireSpecialAbilityUpdate"));
    REQUIRE(ability != nullptr);
    ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
    for (int i = 0; i < 100 && ability->shotsFired() == 0; ++i) w.frames(1);
    REQUIRE(ability->shotsFired() == 1);
    CHECK(enemyHero->getBodyModule()->getHealth() == 7800.0f);
    CHECK(enemyInfantry->getBodyModule()->getHealth() == 10000.0f);
    CHECK(troll->getBodyModule()->getHealth() == 10000.0f);
    CHECK(allyHero->getBodyModule()->getHealth() == 10000.0f);
    CHECK(farHero->getBodyModule()->getHealth() == 10000.0f);
    auto pool = [](Object *o) { return dynamic_cast<AttributeModifierPool *>(o->findModule("AttributeModifierPoolUpdate")); };
    for (Object *o : {enemyHero, enemyInfantry, troll, allyHero, farHero}) REQUIRE(pool(o) != nullptr);
    CHECK(pool(enemyHero)->hasList("P238GalHeroDebuff"));
    CHECK_FALSE(pool(enemyHero)->hasList("P238GalDebuff")); // longer category wins
    CHECK(pool(enemyInfantry)->hasList("P238GalDebuff"));
    CHECK(pool(troll)->hasList("P238GalStone"));
    CHECK(pool(troll)->hasList("P238GalDebuff"));
    CHECK_FALSE(pool(allyHero)->hasList("P238GalHeroDebuff"));
    CHECK_FALSE(pool(farHero)->hasList("P238GalHeroDebuff"));
    CHECK_FALSE(enemyHero->isDisabledByType(DISABLED_USER_PARALYZED));
    CHECK(enemyInfantry->isDisabledByType(DISABLED_USER_PARALYZED));
    CHECK(troll->isDisabledByType(DISABLED_USER_PARALYZED));
    CHECK_FALSE(allyHero->isDisabledByType(DISABLED_USER_PARALYZED));
    CHECK_FALSE(farHero->isDisabledByType(DISABLED_USER_PARALYZED));
}

TEST_CASE("PORT-238 Theoden-style two upgrades gate replacement and two present timers survive")
{
    HealHudWorld w;
    // theoden.ini:1306-1384,2558-2575: six timer names are listed but
    // this file directly declares only KingsFavor and GloriousCharge modules.
    // RW 0x8840FC -> 0x8B8E2E -> 0x8B1690; timer loop RW 0x8B12BF.
    const char *data = R"ini(
Upgrade Upgrade_P238TheoRing
  Type = OBJECT
End
Upgrade Upgrade_P238TheoFortress
  Type = OBJECT
End
SpecialPower P238TheoTransform
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower SpecialAbilityTheodenGloriousCharge
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower SpecialAbilityKingsFavor
  Enum = SPECIAL_GENERAL_TARGETLESS_TWO
  ReloadTime = 120000
End
SpecialPower SpecialAbilityHornOfGondor02
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower SpecialAbilityAragornBladeMaster
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower SpecialAbilityBoromirErnennung
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower SpecialAbilityBoromirOsgiliath
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 120000
End
SpecialPower P238TheoUnlisted
  Enum = SPECIAL_GENERAL_TARGETLESS_THREE
  ReloadTime = 120000
End
CommandButton P238TheoSwitch
  Command = SPECIAL_POWER
  SpecialPower = P238TheoTransform
  InPalantir = No
End
CommandButton P238TheoStop
  Command = STOP
  InPalantir = Yes
End
CommandSet P238TheoBaseSet
  1 = P238TheoStop
End
CommandSet P238TheoRingSet
  1 = P238TheoStop
End
Object P238Theo
  KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
  CommandSet = P238TheoBaseSet
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = UnpauseSpecialPowerUpgrade Enable
    SpecialPowerTemplate = P238TheoTransform
    TriggeredBy = Upgrade_P238TheoRing
  End
  Behavior = SpecialPowerModule Starter
    SpecialPowerTemplate = P238TheoTransform
    StartsPaused = Yes
    UpdateModuleStartsAttack = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate Transform
    SpecialPowerTemplate = P238TheoTransform
    MountedTemplate = P238TheoRingForm
    SynchronizeTimerOnSpecialPower = SpecialAbilityTheodenGloriousCharge SpecialAbilityKingsFavor SpecialAbilityHornOfGondor02 SpecialAbilityAragornBladeMaster SpecialAbilityBoromirErnennung SpecialAbilityBoromirOsgiliath
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade AutoTransform
    TriggeredBy = Upgrade_P238TheoRing Upgrade_P238TheoFortress
    RequiresAllTriggers = Yes
    GetUpgradeCommandButtonName = P238TheoSwitch
  End
  Behavior = CommandSetUpgrade RingSet
    TriggeredBy = Upgrade_P238TheoRing Upgrade_P238TheoFortress
    RequiresAllTriggers = Yes
    CommandSet = P238TheoRingSet
  End
  Behavior = CitadelSlaughterHordeContain RingEntry
    PassengerFilter = NONE +P238Ring
    ObjectStatusOfContained = UNSELECTABLE ENCLOSED
    ContainMax = 1
    StatusForRingEntry = HOLDING_THE_RING
    UpgradeForRingEntry = Upgrade_P238TheoRing Upgrade_P238TheoFortress
    ObjectToDestroyForRingEntry = NONE +P238Ring
  End
  Behavior = SpecialPowerModule Charge
    SpecialPowerTemplate = SpecialAbilityTheodenGloriousCharge
  End
  Behavior = SpecialPowerModule Favor
    SpecialPowerTemplate = SpecialAbilityKingsFavor
  End
  Behavior = SpecialPowerModule Other
    SpecialPowerTemplate = P238TheoUnlisted
  End
End
Object P238TheoRingForm
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule Charge
    SpecialPowerTemplate = SpecialAbilityTheodenGloriousCharge
  End
  Behavior = SpecialPowerModule Favor
    SpecialPowerTemplate = SpecialAbilityKingsFavor
  End
  Behavior = SpecialPowerModule Other
    SpecialPowerTemplate = P238TheoUnlisted
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_theoden.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    std::string partial;
    SUBCASE("only ring upgrade") { partial = "Upgrade_P238TheoRing"; }
    SUBCASE("only fortress upgrade") { partial = "Upgrade_P238TheoFortress"; }
    SUBCASE("deliver both upgrades") {}
    Object *hero = w.unit("P238Theo", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    Team *team = hero->getTeam();
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    auto *charge = power(*hero, "SpecialAbilityTheodenGloriousCharge");
    auto *favor = power(*hero, "SpecialAbilityKingsFavor");
    auto *other = power(*hero, "P238TheoUnlisted");
    REQUIRE(charge != nullptr);
    REQUIRE(favor != nullptr);
    REQUIRE(other != nullptr);
    for (const char *name : {"SpecialAbilityHornOfGondor02", "SpecialAbilityAragornBladeMaster", "SpecialAbilityBoromirErnennung", "SpecialAbilityBoromirOsgiliath"})
    {
        REQUIRE(w.powers.findSpecialPowerTemplate(name) != nullptr);
        CHECK(power(*hero, name) == nullptr);
    }
    charge->setReadyFrame(300);
    favor->setReadyFrame(600);
    other->setReadyFrame(900);
    if (!partial.empty())
    {
        hero->giveUpgrade(partial);
        w.frames(30);
        REQUIRE(w.byId(heroId) != nullptr);
        CHECK(w.byId(heroId)->getTemplate()->getName() == "P238Theo");
        CHECK(w.byId(heroId)->getCommandSetName() == "P238TheoBaseSet");
        CHECK(w.byId(ringId) != nullptr);
        return;
    }
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238TheoRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238TheoFortress"));
    CHECK(hero->getCommandSetName() == "P238TheoRingSet");
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238TheoRingForm") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    for (const char *name : {"SpecialAbilityHornOfGondor02", "SpecialAbilityAragornBladeMaster", "SpecialAbilityBoromirErnennung", "SpecialAbilityBoromirOsgiliath"})
        CHECK(power(*replacement, name) == nullptr);
    charge = power(*replacement, "SpecialAbilityTheodenGloriousCharge");
    favor = power(*replacement, "SpecialAbilityKingsFavor");
    other = power(*replacement, "P238TheoUnlisted");
    REQUIRE(charge != nullptr);
    REQUIRE(favor != nullptr);
    REQUIRE(other != nullptr);
    CHECK(charge->getReadyFrame() == 300);
    CHECK(favor->getReadyFrame() == 600);
    CHECK(other->getReadyFrame() != 900);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
}

TEST_CASE("PORT-238 Saruman-style player flag selects ring replacement")
{
    HealHudWorld w;
    // saruman.ini:1110-1168,1279-1301,4150-4175: the conflicting flag
    // is PLAYER, not Arvedui's OBJECT protection. RW upgrade mux 0x8D26F3,
    // DoCommandUpgrade 0x8B8E2E and toggle replacement 0x8B1690.
    const char *data = R"ini(
Upgrade Upgrade_P238SarRing
  Type = OBJECT
End
Upgrade Upgrade_P238SarFortress
  Type = OBJECT
End
Upgrade Upgrade_P238SarChoice
  Type = PLAYER
End
SpecialPower P238SarCursedPower
  Enum = SPECIAL_TOGGLE_MOUNTED
  ReloadTime = 1000
End
SpecialPower P238SarBlessedPower
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End
CommandButton P238SarCursedButton
  Command = SPECIAL_POWER
  SpecialPower = P238SarCursedPower
  Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY OK_FOR_MULTI_EXECUTE
  InPalantir = No
End
CommandButton P238SarBlessedButton
  Command = SPECIAL_POWER
  SpecialPower = P238SarBlessedPower
  Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY OK_FOR_MULTI_EXECUTE
  InPalantir = Yes
End
Object P238SarBase
  KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = UnpauseSpecialPowerUpgrade EnableCursed
    SpecialPowerTemplate = P238SarCursedPower
    TriggeredBy = Upgrade_P238SarRing
  End
  Behavior = SpecialPowerModule CursedStarter
    SpecialPowerTemplate = P238SarCursedPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate CursedToggle
    SpecialPowerTemplate = P238SarCursedPower
    MountedTemplate = P238SarCursed
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade CursedCommand
    TriggeredBy = Upgrade_P238SarRing
    ConflictsWith = Upgrade_P238SarChoice
    GetUpgradeCommandButtonName = P238SarCursedButton
  End
  Behavior = UnpauseSpecialPowerUpgrade EnableBlessed
    SpecialPowerTemplate = P238SarBlessedPower
    TriggeredBy = Upgrade_P238SarRing Upgrade_P238SarChoice
    RequiresAllTriggers = Yes
  End
  Behavior = SpecialPowerModule BlessedStarter
    SpecialPowerTemplate = P238SarBlessedPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate BlessedToggle
    SpecialPowerTemplate = P238SarBlessedPower
    MountedTemplate = P238SarBlessed
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade BlessedCommand
    TriggeredBy = Upgrade_P238SarRing Upgrade_P238SarChoice
    RequiresAllTriggers = Yes
    GetUpgradeCommandButtonName = P238SarBlessedButton
  End
  Behavior = CitadelSlaughterHordeContain RingEntry
    PassengerFilter = NONE +P238Ring
    ObjectStatusOfContained = UNSELECTABLE ENCLOSED
    ContainMax = 1
    StatusForRingEntry = HOLDING_THE_RING
    UpgradeForRingEntry = Upgrade_P238SarRing Upgrade_P238SarFortress
    ObjectToDestroyForRingEntry = NONE +P238Ring
  End
End
Object P238SarCursed
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
  Behavior = SpecialPowerModule BlessedStarter
    SpecialPowerTemplate = P238SarBlessedPower
    UpdateModuleStartsAttack = Yes
    StartsPaused = Yes
  End
  Behavior = ToggleMountedSpecialAbilityUpdate BlessedToggle
    SpecialPowerTemplate = P238SarBlessedPower
    MountedTemplate = P238SarBlessed
    UnpackTime = 2000
    PreparationTime = 0
    PackTime = 0
    IgnoreFacingCheck = Yes
  End
  Behavior = DoCommandUpgrade BlessedCommand
    TriggeredBy = Upgrade_P238SarChoice
    GetUpgradeCommandButtonName = P238SarBlessedButton
  End
End
Object P238SarBlessed
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  Body = ActiveBody Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface AI
  End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_saruman.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    bool choiceBefore = false;
    bool choiceAfter = false;
    SUBCASE("unprotected delivery") {}
    SUBCASE("player flag before delivery") { choiceBefore = true; }
    SUBCASE("player flag after cursed replacement") { choiceAfter = true; }
    Object *hero = w.unit("P238SarBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    auto giveChoice = [&]() {
        const auto *upgrade = Player::resolveUpgrade("Upgrade_P238SarChoice", true);
        REQUIRE(upgrade != nullptr);
        w.playerOf('A')->addUpgrade(upgrade, Player::UPGRADE_STATUS_COMPLETE);
    };
    auto find = [&](const char *name) -> Object * {
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == name) return o;
        return nullptr;
    };
    if (choiceBefore) { giveChoice(); w.frames(3); CHECK(w.byId(heroId) != nullptr); }
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238SarRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238SarFortress"));
    auto *cursedCommand = dynamic_cast<DoCommandUpgrade *>(hero->findModuleByTag(w.w.keys.nameToKey("CursedCommand")));
    auto *blessedCommand = dynamic_cast<DoCommandUpgrade *>(hero->findModuleByTag(w.w.keys.nameToKey("BlessedCommand")));
    REQUIRE(cursedCommand != nullptr);
    REQUIRE(blessedCommand != nullptr);
    CHECK(cursedCommand->isAlreadyUpgraded() == !choiceBefore);
    CHECK(blessedCommand->isAlreadyUpgraded() == choiceBefore);
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    const char *expected = choiceBefore ? "P238SarBlessed" : "P238SarCursed";
    for (int i = 0; i < 50 && !find(expected); ++i) w.frames(1);
    Object *selected = find(expected);
    REQUIRE(selected != nullptr);
    CHECK(selected->getControllingPlayer() == w.playerOf('A'));
    CHECK(find(choiceBefore ? "P238SarCursed" : "P238SarBlessed") == nullptr);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
    if (choiceAfter)
    {
        const ObjectID cursedId = selected->getID();
        const auto *blessedTemplate = w.powers.findSpecialPowerTemplate("P238SarBlessedPower");
        auto *starter = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*selected, blessedTemplate));
        auto *toggle = dynamic_cast<ToggleMountedSpecialAbilityUpdate *>(selected->findModule("ToggleMountedSpecialAbilityUpdate"));
        REQUIRE(starter != nullptr);
        REQUIRE(toggle != nullptr);
        CHECK(starter->pauseCount() == 1);
        giveChoice(); // RW 0x696FD2 supplies 0x40000; 0x8980A6 skips the pause gate
        CHECK(starter->pauseCount() == 1);
        CHECK(toggle->isActive());
        for (int i = 0; i < 50 && !find("P238SarBlessed"); ++i) w.frames(1);
        Object *blessed = find("P238SarBlessed");
        REQUIRE(blessed != nullptr);
        CHECK(blessed->getControllingPlayer() == w.playerOf('A'));
        w.frames(3);
        CHECK(w.byId(cursedId) == nullptr);
    }
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238SarChoice"));
}

// Synthetic Gollum-style death mux; no real weapons, waypoints, Lua or mod loading.
TEST_CASE("PORT-238 Gollum-style death types select one creation list")
{
    struct RingDeathWorld : CombatWorld
    {
        ObjectCreationListStore lists{ w.keys };
        ObjectCreationListStore *saved = TheObjectCreationListStore;
        RingDeathWorld() : CombatWorld(R"ini(
Object P238Thief
 KindOf = INFANTRY SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = CreateObjectDie OrdinaryDeath
  DeathTypes = ALL -EXTRA_2 -EXTRA_3
  CreationList = OCL_P238Thief
 End
 Behavior = CreateObjectDie StolenDeath
  DeathTypes = NONE +EXTRA_2
  CreationList = OCL_P238Bearer
 End
End
Object P238NewThief
 KindOf = INFANTRY SELECTABLE
End
Object P238NewBearer
 KindOf = INFANTRY SELECTABLE
End
)ini")
        {
            TheObjectCreationListStore = &lists;
            w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
            const std::string err = w.load(R"ini(
ObjectCreationList OCL_P238Thief
 CreateObject
  ObjectNames = P238NewThief
  Count = 1
 End
End
ObjectCreationList OCL_P238Bearer
 CreateObject
  ObjectNames = P238NewBearer
  Count = 1
 End
End
)ini", INI_LOAD_OVERWRITE, "p238-gollum-ocl.ini");
            REQUIRE_MESSAGE(err.empty(), err);
        }
        ~RingDeathWorld() { TheObjectCreationListStore = saved; }
        int count(const char *name)
        {
            int n = 0;
            for (ObjectID id = 1; id < 64; ++id)
            {
                Object *o = byId(id);
                if (o && o->getTemplate()->getName() == name) ++n;
            }
            return n;
        }
    } w;

    Object *normal = w.unit("P238Thief", 'A', 50, 50);
    REQUIRE(normal);
    normal->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
    CHECK(normal->isEffectivelyDead());
    CHECK(w.count("P238NewThief") == 1);
    CHECK(w.count("P238NewBearer") == 0);

    Object *stolen = w.unit("P238Thief", 'A', 100, 50);
    REQUIRE(stolen);
    stolen->kill(DAMAGE_UNRESISTABLE, DEATH_EXTRA_2);
    CHECK(stolen->isEffectivelyDead());
    CHECK(w.count("P238NewThief") == 1);
    CHECK(w.count("P238NewBearer") == 1);

    Object *fortress = w.unit("P238Thief", 'A', 150, 50);
    REQUIRE(fortress);
    fortress->kill(DAMAGE_UNRESISTABLE, DEATH_EXTRA_3);
    CHECK(fortress->isEffectivelyDead());
    CHECK(w.count("P238NewThief") == 1);
    CHECK(w.count("P238NewBearer") == 1);
}

TEST_CASE("PORT-238 Gandalf-style white choice before or after ring delivery")
{
    HealHudWorld w;
    // gondorgandalf.ini:1415-1499, 2979-3003 and
    // gondorgandalfverfhrte.ini:1328-1350; RW 0x8D26F3, 0x8B8E2E, 0x8B1690.
    const char *data = R"ini(
Upgrade Upgrade_P238GanRing
 Type = OBJECT
End
Upgrade Upgrade_P238GanFortress
 Type = OBJECT
End
Upgrade Upgrade_P238GanWhite
 Type = PLAYER
End
SpecialPower P238GanWhitePower
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 1000
End
SpecialPower P238GanEvilPower
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 1000
End
SpecialPower P238GanGoodPower
 Enum = SPECIAL_GENERAL_TARGETLESS_TWO
 ReloadTime = 1000
End
CommandButton P238GanWhiteButton
 Command = SPECIAL_POWER
 SpecialPower = P238GanWhitePower
 InPalantir = No
End
CommandButton P238GanEvilButton
 Command = SPECIAL_POWER
 SpecialPower = P238GanEvilPower
 InPalantir = No
End
CommandButton P238GanGoodButton
 Command = SPECIAL_POWER
 SpecialPower = P238GanGoodPower
 InPalantir = No
End
Object P238GanBase
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule WhiteStarter
  SpecialPowerTemplate = P238GanWhitePower
  StartsPaused = No
  UpdateModuleStartsAttack = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate WhiteToggle
  SpecialPowerTemplate = P238GanWhitePower
  MountedTemplate = P238GanWhite
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade WhiteCommand
  TriggeredBy = Upgrade_P238GanWhite
  GetUpgradeCommandButtonName = P238GanWhiteButton
 End
 Behavior = SpecialPowerModule EvilStarter
  SpecialPowerTemplate = P238GanEvilPower
  StartsPaused = No
  UpdateModuleStartsAttack = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate EvilToggle
  SpecialPowerTemplate = P238GanEvilPower
  MountedTemplate = P238GanEvil
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade EvilCommand
  TriggeredBy = Upgrade_P238GanRing Upgrade_P238GanFortress
  ConflictsWith = Upgrade_P238GanWhite
  GetUpgradeCommandButtonName = P238GanEvilButton
 End
 Behavior = CitadelSlaughterHordeContain RingEntry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238GanRing Upgrade_P238GanFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
End
Object P238GanWhite
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule GoodStarter
  SpecialPowerTemplate = P238GanGoodPower
  StartsPaused = No
  UpdateModuleStartsAttack = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate GoodToggle
  SpecialPowerTemplate = P238GanGoodPower
  MountedTemplate = P238GanGood
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade GoodCommand
  TriggeredBy = Upgrade_P238GanRing Upgrade_P238GanFortress
  GetUpgradeCommandButtonName = P238GanGoodButton
 End
 Behavior = CitadelSlaughterHordeContain RingEntry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238GanRing Upgrade_P238GanFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
End
Object P238GanEvil
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule WhiteStarter
  SpecialPowerTemplate = P238GanWhitePower
  StartsPaused = No
  UpdateModuleStartsAttack = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate WhiteToggle
  SpecialPowerTemplate = P238GanWhitePower
  MountedTemplate = P238GanGood
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade WhiteCommand
  TriggeredBy = Upgrade_P238GanWhite
  GetUpgradeCommandButtonName = P238GanWhiteButton
 End
End
Object P238GanGood
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_gandalf.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    bool whiteFirst = false;
    bool simultaneous = false;
    SUBCASE("ring before white") {}
    SUBCASE("white before ring") { whiteFirst = true; }
    SUBCASE("ring during white transition") { whiteFirst = true; simultaneous = true; }
    Object *hero = w.unit("P238GanBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID originalId = hero->getID();
    const ObjectID ringId = ring->getID();
    auto find = [&](const char *name) -> Object * {
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == name) return o;
        return nullptr;
    };
    auto giveWhite = [&]() {
        const auto *upgrade = Player::resolveUpgrade("Upgrade_P238GanWhite", true);
        REQUIRE(upgrade != nullptr);
        w.playerOf('A')->addUpgrade(upgrade, Player::UPGRADE_STATUS_COMPLETE);
    };
    auto waitFor = [&](const char *name) -> Object * {
        for (int i = 0; i < 50 && !find(name); ++i) w.frames(1);
        return find(name);
    };
    if (whiteFirst)
    {
        giveWhite();
        if (!simultaneous)
        {
            hero = waitFor("P238GanWhite");
            REQUIRE(hero != nullptr);
            w.frames(3);
            CHECK(w.byId(originalId) == nullptr);
        }
    }
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238GanRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238GanFortress"));
    if (simultaneous)
    {
        auto *evilCommand = dynamic_cast<DoCommandUpgrade *>(hero->findModuleByTag(w.w.keys.nameToKey("EvilCommand")));
        REQUIRE(evilCommand != nullptr);
        CHECK_FALSE(evilCommand->isAlreadyUpgraded());
    }
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    if (simultaneous)
    {
        // Characterizes the port only: do not infer RW or a real-game race.
        Object *white = waitFor("P238GanWhite");
        REQUIRE(white != nullptr);
        const ObjectID whiteId = white->getID();
        w.frames(50);
        white = w.byId(whiteId);
        REQUIRE(white != nullptr);
        CHECK_FALSE(white->hasUpgrade("Upgrade_P238GanRing"));
        CHECK_FALSE(white->hasUpgrade("Upgrade_P238GanFortress"));
        CHECK(find("P238GanGood") == nullptr);
        CHECK(find("P238GanEvil") == nullptr);
        CHECK(white->getControllingPlayer() == w.playerOf('A'));
        CHECK(w.byId(originalId) == nullptr);
        return;
    }
    Object *selected = waitFor(whiteFirst ? "P238GanGood" : "P238GanEvil");
    REQUIRE(selected != nullptr);
    CHECK(selected->getControllingPlayer() == w.playerOf('A'));
    w.frames(3);
    if (!whiteFirst)
    {
        const ObjectID evilId = selected->getID();
        CHECK(w.byId(originalId) == nullptr);
        giveWhite();
        Object *good = waitFor("P238GanGood");
        REQUIRE(good != nullptr);
        CHECK(good->getControllingPlayer() == w.playerOf('A'));
        w.frames(3);
        CHECK(w.byId(evilId) == nullptr);
    }
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238GanWhite"));
}

TEST_CASE("PORT-238 Alatar-style ring teleport stores enemy and ally then releases them")
{
    // evilalatar.ini:2330-2371; specialpower.inc:6609-6628.
    // Direct worker intents only: not the chained HUD or forbidden-position gate.
    struct RingTeleportWorld : CombatWorld
    {
        SpecialPowerStore powers;
        SpecialPowerStore *saved = TheSpecialPowerStore;
        RingTeleportWorld()
        {
            logic->partition().setRegion(0, 0, 1000, 1000);
            TheSpecialPowerStore = &powers;
            w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
            const std::string err = w.load(R"ini(
SpecialPower P238AlatarStore
 Enum = SPECIAL_STORE_LIST_2
 ReloadTime = 220000
 ObjectFilter = ANY +INFANTRY +CAVALRY +HERO +MONSTER +MACHINE -UNATTACKABLE
End
SpecialPower P238AlatarRelease
 Enum = SPECIAL_TELEPORT_LIST_TO_POSITION
 ReloadTime = 320000
End
Object P238AlatarCaster
 KindOf = INFANTRY HERO SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule StorePower
  SpecialPowerTemplate = P238AlatarStore
  UpdateModuleStartsAttack = Yes
 End
 Behavior = SpecialPowerModule ReleasePower
  SpecialPowerTemplate = P238AlatarRelease
  UpdateModuleStartsAttack = Yes
 End
 Behavior = StoreObjectsSpecialPower Store
  SpecialPowerTemplate = P238AlatarStore
  Radius = 130
  StartAbilityRange = 1000000
  UnpackTime = 0
  PreparationTime = 1
  ChainedButton = P238AlatarReleaseButton
  IgnoreFacingCheck = Yes
 End
 Behavior = TeleportToCasterSpecialPower Release
  SpecialPowerTemplate = P238AlatarRelease
  Radius = 130
  MinDestinationRadius = 10
  MaxDestinationRadius = 100
  StartAbilityRange = 1000000
  UnpackTime = 2000
  PreparationTime = 1
  IgnoreFacingCheck = Yes
 End
End
Object P238AlatarUnit
 KindOf = INFANTRY SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = AIUpdateInterface AI
 End
End
Object P238AlatarProtected
 KindOf = INFANTRY SELECTABLE UNATTACKABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
End
Object P238AlatarBuilding
 KindOf = STRUCTURE SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
End
)ini", INI_LOAD_OVERWRITE, "port238_alatar.ini");
            REQUIRE_MESSAGE(err.empty(), err);
        }
        ~RingTeleportWorld() { TheSpecialPowerStore = saved; }
    } w;
    Object *caster = w.unit("P238AlatarCaster", 'A', 300, 300);
    Object *ally = w.unit("P238AlatarUnit", 'A', 320, 300);
    Object *enemy = w.unit("P238AlatarUnit", 'B', 340, 300);
    Object *far = w.unit("P238AlatarUnit", 'A', 500, 300);
    Object *protectedUnit = w.unit("P238AlatarProtected", 'A', 325, 310);
    Object *building = w.unit("P238AlatarBuilding", 'B', 330, 310);
    REQUIRE(caster != nullptr);
    REQUIRE(ally != nullptr);
    REQUIRE(enemy != nullptr);
    REQUIRE(far != nullptr);
    REQUIRE(protectedUnit != nullptr);
    REQUIRE(building != nullptr);
    auto *store = dynamic_cast<StoreObjectsSpecialPower *>(caster->findModule("StoreObjectsSpecialPower"));
    auto *release = dynamic_cast<TeleportToCasterSpecialPower *>(caster->findModule("TeleportToCasterSpecialPower"));
    REQUIRE(store != nullptr);
    REQUIRE(release != nullptr);
    const Coord3D source{300, 300, 0};
    const Coord3D destination{700, 700, 0};
    const Coord3D farBefore = *far->getPosition();
    const Coord3D protectedBefore = *protectedUnit->getPosition();
    const Coord3D buildingBefore = *building->getPosition();
    store->initiateIntentToDoSpecialPower(store->abilityData()->m_specialPowerTemplate, nullptr, &source, 0, 0);
    w.frames(20);
    REQUIRE(store->storedObjects().size() == 2);
    CHECK(store->storedObjects()[0] == ally->getID());
    CHECK(store->storedObjects()[1] == enemy->getID());
    release->initiateIntentToDoSpecialPower(release->abilityData()->m_specialPowerTemplate, nullptr, &destination, 0, 0);
    w.frames(80);
    CHECK(store->storedObjects().empty());
    auto distSquared = [](const Coord3D &a, const Coord3D &b) {
        const float x = a.x - b.x, y = a.y - b.y;
        return x * x + y * y;
    };
    CHECK(distSquared(*ally->getPosition(), destination) <= 10000.0f); // tolerance: RW findPositionAround chooses a point within max radius, or falls back to the destination
    CHECK(distSquared(*enemy->getPosition(), destination) <= 10000.0f); // tolerance: RW findPositionAround chooses a point within max radius, or falls back to the destination
    CHECK(distSquared(*caster->getPosition(), source) == 0.0f);
    CHECK(distSquared(*far->getPosition(), farBefore) == 0.0f);
    CHECK(distSquared(*protectedUnit->getPosition(), protectedBefore) == 0.0f);
    CHECK(distSquared(*building->getPosition(), buildingBefore) == 0.0f);
    CHECK(ally->getControllingPlayer() == w.playerOf('A'));
    CHECK(enemy->getControllingPlayer() == w.playerOf('B'));
}

TEST_CASE("PORT-238 Alatar-style single ring delivery copies present named timers")
{
    HealHudWorld w;
    // evilalatar.ini:970-1007,1010-1052,2279-2318. The six names are
    // literal, but this fixture supplies two common timers and one source-only.
    // RW 0x8840FC -> 0x8B8E2E -> 0x8B1690; timer loop 0x8B12BF.
    const char *data = R"ini(
Upgrade Upgrade_P238AlatarRing
 Type = OBJECT
End
SpecialPower P238AlatarTransform
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 1000
End
SpecialPower SpecialAbilityTobenderSturm
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 220000
End
SpecialPower SpecialAbilityLuftstorming
 Enum = SPECIAL_GENERAL_TARGETLESS_TWO
 ReloadTime = 220000
End
SpecialPower SpecialAbilityAlatarEyeOfStorm
 Enum = SPECIAL_GENERAL_TARGETLESS_THREE
 ReloadTime = 220000
End
SpecialPower SpecialAbilityAlatarTeleportAlly
 Enum = SPECIAL_STORE_LIST_2
 ReloadTime = 220000
End
SpecialPower SpecialAbilityAlatarTeleportStoredObjectsNew
 Enum = SPECIAL_TELEPORT_LIST_TO_POSITION
 ReloadTime = 320000
End
SpecialPower SpecialAbilityAlatarSummonOliphants
 Enum = SPECIAL_SPELL_BOOK_ARROW_VOLLEY_GOOD
 ReloadTime = 220000
End
SpecialPower P238AlatarUnlisted
 Enum = SPECIAL_SCREECH
 ReloadTime = 220000
End
CommandButton P238AlatarTransformButton
 Command = SPECIAL_POWER
 SpecialPower = P238AlatarTransform
 InPalantir = No
End
Object P238AlatarBase
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule TransformStarter
  SpecialPowerTemplate = P238AlatarTransform
  UpdateModuleStartsAttack = Yes
  StartsPaused = No
 End
 Behavior = ToggleMountedSpecialAbilityUpdate Transform
  SpecialPowerTemplate = P238AlatarTransform
  MountedTemplate = P238AlatarRingForm
  SynchronizeTimerOnSpecialPower = SpecialAbilityTobenderSturm SpecialAbilityLuftstorming SpecialAbilityAlatarEyeOfStorm SpecialAbilityAlatarTeleportAlly SpecialAbilityAlatarTeleportStoredObjectsNew SpecialAbilityAlatarSummonOliphants
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade Automatic
  TriggeredBy = Upgrade_P238AlatarRing
  GetUpgradeCommandButtonName = P238AlatarTransformButton
 End
 Behavior = CitadelSlaughterHordeContain Entry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238AlatarRing
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
 Behavior = SpecialPowerModule Storm
  SpecialPowerTemplate = SpecialAbilityTobenderSturm
 End
 Behavior = SpecialPowerModule Air
  SpecialPowerTemplate = SpecialAbilityLuftstorming
 End
 Behavior = SpecialPowerModule AllyStore
  SpecialPowerTemplate = SpecialAbilityAlatarTeleportAlly
 End
 Behavior = SpecialPowerModule Other
  SpecialPowerTemplate = P238AlatarUnlisted
 End
End
Object P238AlatarRingForm
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Storm
  SpecialPowerTemplate = SpecialAbilityTobenderSturm
 End
 Behavior = SpecialPowerModule Air
  SpecialPowerTemplate = SpecialAbilityLuftstorming
 End
 Behavior = SpecialPowerModule Other
  SpecialPowerTemplate = P238AlatarUnlisted
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_alatar_delivery.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *hero = w.unit("P238AlatarBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    auto *storm = power(*hero, "SpecialAbilityTobenderSturm");
    auto *air = power(*hero, "SpecialAbilityLuftstorming");
    auto *allyStore = power(*hero, "SpecialAbilityAlatarTeleportAlly");
    auto *other = power(*hero, "P238AlatarUnlisted");
    REQUIRE(storm != nullptr);
    REQUIRE(air != nullptr);
    REQUIRE(allyStore != nullptr);
    REQUIRE(other != nullptr);
    storm->setReadyFrame(300);
    air->setReadyFrame(600);
    allyStore->setReadyFrame(700);
    other->setReadyFrame(900);
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238AlatarRing"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238AlatarRingForm") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getControllingPlayer() == w.playerOf('A'));
    storm = power(*replacement, "SpecialAbilityTobenderSturm");
    air = power(*replacement, "SpecialAbilityLuftstorming");
    other = power(*replacement, "P238AlatarUnlisted");
    REQUIRE(storm != nullptr);
    REQUIRE(air != nullptr);
    REQUIRE(other != nullptr);
    CHECK(storm->getReadyFrame() == 300);
    CHECK(air->getReadyFrame() == 600);
    CHECK(other->getReadyFrame() == 11); // replacement creation frame; the unlisted timer is not copied
    CHECK(power(*replacement, "SpecialAbilityAlatarTeleportAlly") == nullptr);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
}

TEST_CASE("PORT-238 fortress-style death drop requires both object upgrades")
{
    // FortressRingFunc.inc: CreateObjectDie UpgradeRequired and
    // ModelConditionUpgrade RequiresAllTriggers; RW 0x888F04..0x888F83.
    struct FortressWorld : World
    {
        ObjectCreationListStore lists{ w.keys };
        ObjectCreationListStore *saved = TheObjectCreationListStore;
        FortressWorld()
        {
            TheObjectCreationListStore = &lists;
            w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
            const std::string err = w.load(R"ini(
Upgrade Upgrade_P238FortHero
 Type = OBJECT
End
Upgrade Upgrade_P238FortOwner
 Type = OBJECT
End
Object P238Fortress
 KindOf = STRUCTURE SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = CreateObjectDie Drop
  CreationList = OCL_P238FortRing
  UpgradeRequired = Upgrade_P238FortHero Upgrade_P238FortOwner
 End
 Behavior = ModelConditionUpgrade Model
  TriggeredBy = Upgrade_P238FortHero Upgrade_P238FortOwner
  RequiresAllTriggers = Yes
  AddConditionFlags = ONE_RING
 End
End
ObjectCreationList OCL_P238FortRing
 CreateObject
  ObjectNames = P238Ring
  Count = 1
 End
End
)ini", INI_LOAD_OVERWRITE, "port238_fortress.ini");
            REQUIRE_MESSAGE(err.empty(), err);
        }
        ~FortressWorld() { TheObjectCreationListStore = saved; }
        int ringCount()
        {
            int count = 0;
            for (ObjectID id = 1; id < 64; ++id)
            {
                Object *o = byId(id);
                if (o && o->getTemplate()->getName() == "P238Ring") ++count;
            }
            return count;
        }
    } w;
    bool heroUpgrade = false, fortressUpgrade = false;
    SUBCASE("neither") {}
    SUBCASE("hero only") { heroUpgrade = true; }
    SUBCASE("fortress only") { fortressUpgrade = true; }
    SUBCASE("both") { heroUpgrade = true; fortressUpgrade = true; }
    Object *fort = w.unit("P238Fortress", 'A', 300, 300);
    REQUIRE(fort != nullptr);
    if (heroUpgrade) fort->giveUpgrade("Upgrade_P238FortHero");
    if (fortressUpgrade) fort->giveUpgrade("Upgrade_P238FortOwner");
    w.frames(3);
    CHECK(fort->testModelCondition(CombatNames::modelCondition("ONE_RING")) == (heroUpgrade && fortressUpgrade));
    CHECK(w.ringCount() == 0);
    fort->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
    CHECK(fort->isEffectivelyDead());
    CHECK(w.ringCount() == (heroUpgrade && fortressUpgrade ? 1 : 0));
}

TEST_CASE("PORT-238 legacy Witch-king-style foot and mounted ring delivery chooses foot form")
{
    HealHudWorld w;
    // angmarwitchking.ini:1269-1318, 2941-2965.  Both declarations target
    // the foot ring form and list the same seven timers. Synthetic objects do
    // not exercise ChildObject overrides, mount admission, or the real INI.
    // RW Citadel 0x8840FC -> DoCommand 0x8B8E2E -> Toggle 0x8B1690;
    // named timer copy 0x8B12BF.
    const char *common = R"ini(
Upgrade Upgrade_P238LegacyRing
 Type = OBJECT
End
Upgrade Upgrade_P238LegacyFortress
 Type = OBJECT
End
SpecialPower P238LegacyTransform
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 1000
End
SpecialPower SpecialAbilityScreech
 Enum = SPECIAL_SCREECH
 ReloadTime = 120000
End
SpecialPower SpecialAbilityWitchkingMorgulBlade
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityCurseEnemy
 Enum = SPECIAL_GENERAL_TARGETLESS_TWO
 ReloadTime = 120000
End
SpecialPower SpecialAbilityVerdorbeneMagieAngmars
 Enum = SPECIAL_GENERAL_TARGETLESS_THREE
 ReloadTime = 120000
End
SpecialPower SpecialAbilityWordOfPower
 Enum = SPECIAL_SPELL_BOOK_ARROW_VOLLEY_GOOD
 ReloadTime = 120000
End
SpecialPower SpecialAbilityGrausamWitchking
 Enum = SPECIAL_STORE_LIST_2
 ReloadTime = 120000
End
SpecialPower SpecialAbilityAngmarWitchkingSchattenklinge
 Enum = SPECIAL_TELEPORT_LIST_TO_POSITION
 ReloadTime = 120000
End
SpecialPower P238LegacyOther
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 120000
End
CommandButton P238LegacySwitch
 Command = SPECIAL_POWER
 SpecialPower = P238LegacyTransform
 InPalantir = No
End
Object P238LegacyRingFoot
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Screech
  SpecialPowerTemplate = SpecialAbilityScreech
 End
 Behavior = SpecialPowerModule Curse
  SpecialPowerTemplate = SpecialAbilityCurseEnemy
 End
 Behavior = SpecialPowerModule Other
  SpecialPowerTemplate = P238LegacyOther
 End
End
Object P238LegacyRingMounted
 KindOf = CAVALRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
End
)ini";
    std::string error = w.w.load(common, INI_LOAD_OVERWRITE, "port238_legacy_wk_common.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    const char *source = R"ini(
Object P238LegacySource
 KindOf = P238LegacyKind HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Starter
  SpecialPowerTemplate = P238LegacyTransform
  UpdateModuleStartsAttack = Yes
  StartsPaused = No
 End
 Behavior = ToggleMountedSpecialAbilityUpdate Transform
  SpecialPowerTemplate = P238LegacyTransform
  MountedTemplate = P238LegacyRingFoot
  SynchronizeTimerOnSpecialPower = SpecialAbilityScreech SpecialAbilityWitchkingMorgulBlade SpecialAbilityCurseEnemy SpecialAbilityVerdorbeneMagieAngmars SpecialAbilityWordOfPower SpecialAbilityGrausamWitchking SpecialAbilityAngmarWitchkingSchattenklinge
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade AutoTransform
  TriggeredBy = Upgrade_P238LegacyRing
  GetUpgradeCommandButtonName = P238LegacySwitch
 End
 Behavior = CitadelSlaughterHordeContain RingEntry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238LegacyRing Upgrade_P238LegacyFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
 Behavior = SpecialPowerModule Screech
  SpecialPowerTemplate = SpecialAbilityScreech
 End
 Behavior = SpecialPowerModule Curse
  SpecialPowerTemplate = SpecialAbilityCurseEnemy
 End
 Behavior = SpecialPowerModule Other
  SpecialPowerTemplate = P238LegacyOther
 End
End
)ini";
    auto loadSource = [&](const char *name, const char *kind) {
        std::string data(source);
        auto replace = [&](const char *key, const char *value) {
            const auto pos = data.find(key);
            REQUIRE(pos != std::string::npos);
            data.replace(pos, std::string(key).size(), value);
        };
        replace("P238LegacySource", name);
        replace("P238LegacyKind", kind);
        const std::string loadError = w.w.load(data, INI_LOAD_OVERWRITE, "port238_legacy_wk_source.ini");
        REQUIRE_MESSAGE(loadError.empty(), loadError);
    };
    loadSource("P238LegacyFoot", "INFANTRY");
    loadSource("P238LegacyMounted", "CAVALRY");
    const char *form = nullptr;
    const char *mode = nullptr;
    // The fortress-only control must not trigger; ring-only must trigger.
    SUBCASE("foot")
    {
        form = "P238LegacyFoot";
        SUBCASE("deliver both") { mode = "delivery"; }
        SUBCASE("only fortress") { mode = "fortress"; }
        SUBCASE("only ring") { mode = "ring"; }
    }
    SUBCASE("mounted")
    {
        form = "P238LegacyMounted";
        SUBCASE("deliver both") { mode = "delivery"; }
        SUBCASE("only fortress") { mode = "fortress"; }
        SUBCASE("only ring") { mode = "ring"; }
    }
    REQUIRE(form != nullptr);
    REQUIRE(mode != nullptr);
    Object *hero = w.unit(form, 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    Team *team = hero->getTeam();
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    auto *screech = power(*hero, "SpecialAbilityScreech");
    auto *curse = power(*hero, "SpecialAbilityCurseEnemy");
    auto *other = power(*hero, "P238LegacyOther");
    REQUIRE(screech != nullptr);
    REQUIRE(curse != nullptr);
    REQUIRE(other != nullptr);
    screech->setReadyFrame(300);
    curse->setReadyFrame(600);
    other->setReadyFrame(900);
    if (std::string(mode) == "delivery")
    {
        auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
        REQUIRE(contain != nullptr);
        deliver(*contain, *carrier);
        CHECK(hero->hasUpgrade("Upgrade_P238LegacyRing"));
        CHECK(hero->hasUpgrade("Upgrade_P238LegacyFortress"));
        REQUIRE(w.byId(ringId) != nullptr);
        CHECK(w.byId(ringId)->isDestroyed());
    }
    else
    {
        hero->giveUpgrade(std::string(mode) == "ring" ? "Upgrade_P238LegacyRing" : "Upgrade_P238LegacyFortress");
        CHECK_FALSE(w.byId(ringId)->isDestroyed());
    }
    Object *replacement = nullptr;
    bool mountedReplacement = false;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
        {
            if (!obj->getTemplate()) continue;
            if (obj->getTemplate()->getName() == "P238LegacyRingFoot") replacement = obj;
            if (obj->getTemplate()->getName() == "P238LegacyRingMounted") mountedReplacement = true;
        }
    }
    CHECK_FALSE(mountedReplacement);
    if (std::string(mode) == "fortress")
    {
        CHECK(replacement == nullptr);
        REQUIRE(w.byId(heroId) != nullptr);
        CHECK(w.byId(heroId)->getTemplate()->getName() == form);
        return;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    screech = power(*replacement, "SpecialAbilityScreech");
    curse = power(*replacement, "SpecialAbilityCurseEnemy");
    other = power(*replacement, "P238LegacyOther");
    REQUIRE(screech != nullptr);
    REQUIRE(curse != nullptr);
    REQUIRE(other != nullptr);
    CHECK(screech->getReadyFrame() == 300);
    CHECK(curse->getReadyFrame() == 600);
    CHECK(other->getReadyFrame() != 900);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
}

TEST_CASE("PORT-238 classic ring-hero style faction trigger removes owner's object ring upgrades")
{
    HealHudWorld w;
    // elven/galadriel.ini:467-483 and mordor/sauron.ini:519-538 (LF lines).
    // Both ring upgrades are OBJECT in Ennorath upgrade.ini:181,198; faction
    // triggers are PLAYER. RW factory 0x65058C -> ctor 0x8BC0C2 -> secondary
    // vtable 0xC6FA38 slot +0x28 -> 0x8BC1B3; its OBJECT branch 0x8BC22A
    // calls Object::removeUpgrade, then Player::removeUpgrade(u, false) when
    // RemoveFromAllPlayerObjects is set. This uses direct synthetic creation,
    // not an actual build queue or the real ChildObject/load graph.
    const char *data = R"ini(
Upgrade Upgrade_P238ClassicRing
 Type = OBJECT
End
Upgrade Upgrade_P238ClassicFortressRing
 Type = OBJECT
End
Upgrade Upgrade_P238ClassicMen
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicElf
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicDwarf
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicWild
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicIsengard
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicMordor
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicGoblin
 Type = PLAYER
End
Upgrade Upgrade_P238ClassicAngmar
 Type = PLAYER
End
Object P238ClassicHolder
 KindOf = STRUCTURE SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 500
 End
End
Object P238ClassicGaladriel
 KindOf = INFANTRY HERO SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = RemoveUpgradeUpgrade RemoveRing
  TriggeredBy = Upgrade_P238ClassicMen Upgrade_P238ClassicElf Upgrade_P238ClassicDwarf
  UpgradeToRemove = Upgrade_P238ClassicRing Upgrade_P238ClassicFortressRing
  RemoveFromAllPlayerObjects = Yes
  SuppressEvaEventForRemoval = Yes
 End
End
Object P238ClassicSauron
 KindOf = INFANTRY HERO SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = RemoveUpgradeUpgrade RemoveRing
  TriggeredBy = Upgrade_P238ClassicWild Upgrade_P238ClassicIsengard Upgrade_P238ClassicMordor Upgrade_P238ClassicGoblin Upgrade_P238ClassicAngmar
  UpgradeToRemove = Upgrade_P238ClassicRing Upgrade_P238ClassicFortressRing
  RemoveFromAllPlayerObjects = Yes
  SuppressEvaEventForRemoval = Yes
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_classic_heroes.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    const char *heroName = nullptr;
    const char *factionName = nullptr;
    bool factionBeforeCreation = false;
    SUBCASE("Galadriel")
    {
        heroName = "P238ClassicGaladriel"; factionName = "Upgrade_P238ClassicElf";
        SUBCASE("faction already present") { factionBeforeCreation = true; }
        SUBCASE("faction granted later") { factionBeforeCreation = false; }
    }
    SUBCASE("Sauron")
    {
        heroName = "P238ClassicSauron"; factionName = "Upgrade_P238ClassicMordor";
        SUBCASE("faction already present") { factionBeforeCreation = true; }
        SUBCASE("faction granted later") { factionBeforeCreation = false; }
    }
    REQUIRE(heroName != nullptr);
    REQUIRE(factionName != nullptr);
    Object *holder = w.unit("P238ClassicHolder", 'A', 300, 300);
    Object *otherHolder = w.unit("P238ClassicHolder", 'A', 320, 300);
    Object *enemy = w.unit("P238ClassicHolder", 'B', 600, 600);
    REQUIRE(holder != nullptr);
    REQUIRE(otherHolder != nullptr);
    REQUIRE(enemy != nullptr);
    for (Object *obj : {holder, otherHolder, enemy})
    {
        obj->giveUpgrade("Upgrade_P238ClassicRing");
        obj->giveUpgrade("Upgrade_P238ClassicFortressRing");
    }
    w.frames(2); // let pending holders enter the world's owner scan
    const UpgradeTemplate *faction = TheUpgradeCenter->findUpgrade(factionName);
    REQUIRE(faction != nullptr);
    if (factionBeforeCreation)
        w.playerOf('A')->addUpgrade(faction, Player::UPGRADE_STATUS_COMPLETE);
    CHECK(holder->hasUpgrade("Upgrade_P238ClassicRing"));
    CHECK(holder->hasUpgrade("Upgrade_P238ClassicFortressRing"));
    CHECK(otherHolder->hasUpgrade("Upgrade_P238ClassicRing"));
    CHECK(otherHolder->hasUpgrade("Upgrade_P238ClassicFortressRing"));
    Object *hero = w.unit(heroName, 'A', 350, 300);
    REQUIRE(hero != nullptr);
    w.frames(2);
    if (!factionBeforeCreation)
    {
        CHECK(holder->hasUpgrade("Upgrade_P238ClassicRing"));
        CHECK(otherHolder->hasUpgrade("Upgrade_P238ClassicFortressRing"));
        w.playerOf('A')->addUpgrade(faction, Player::UPGRADE_STATUS_COMPLETE);
    }
    w.frames(2);
    CHECK_FALSE(holder->hasUpgrade("Upgrade_P238ClassicRing"));
    CHECK_FALSE(holder->hasUpgrade("Upgrade_P238ClassicFortressRing"));
    CHECK_FALSE(otherHolder->hasUpgrade("Upgrade_P238ClassicRing"));
    CHECK_FALSE(otherHolder->hasUpgrade("Upgrade_P238ClassicFortressRing"));
    CHECK(enemy->hasUpgrade("Upgrade_P238ClassicRing"));
    CHECK(enemy->hasUpgrade("Upgrade_P238ClassicFortressRing"));
    CHECK(w.playerOf('A')->hasUpgradeComplete(factionName));
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete(factionName));
}

TEST_CASE("PORT-238 Durin-style ungated death drop differs from a summoned fixture")
{
    // dwarvendurin.ini:888-899: direct CreateObjectDie with no UpgradeRequired;
    // the separately declared summoned form has no literal ring-drop module.
    // No real spawn route, lifetime, FX or actual template loading is tested.
    struct DurinWorld : World
    {
        ObjectCreationListStore lists{ w.keys };
        ObjectCreationListStore *saved = TheObjectCreationListStore;
        DurinWorld()
        {
            TheObjectCreationListStore = &lists;
            w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
            const std::string error = w.load(R"ini(
Upgrade Upgrade_P238DurinRing
 Type = OBJECT
End
Upgrade Upgrade_P238DurinFortress
 Type = OBJECT
End
Object P238Durin
 KindOf = INFANTRY HERO SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = CreateObjectDie Drop
  CreationList = OCL_P238DurinRing
 End
End
Object P238DurinSummoned
 KindOf = INFANTRY HERO SELECTABLE SUMMONED
 Body = ActiveBody Body
  MaxHealth = 500
 End
End
ObjectCreationList OCL_P238DurinRing
 CreateObject
  ObjectNames = P238Ring
  Count = 1
 End
End
)ini", INI_LOAD_OVERWRITE, "port238_durin.ini");
            REQUIRE_MESSAGE(error.empty(), error);
        }
        ~DurinWorld() { TheObjectCreationListStore = saved; }
        int ringCount()
        {
            int count = 0;
            for (ObjectID id = 1; id < 64; ++id)
            {
                Object *o = byId(id);
                if (o && o->getTemplate()->getName() == "P238Ring") ++count;
            }
            return count;
        }
    } w;
    bool hasUpgrades = false;
    SUBCASE("no upgrades") {}
    SUBCASE("both upgrades") { hasUpgrades = true; }
    Object *durin = w.unit("P238Durin", 'A', 300, 300);
    Object *summoned = w.unit("P238DurinSummoned", 'A', 350, 300);
    REQUIRE(durin != nullptr);
    REQUIRE(summoned != nullptr);
    if (hasUpgrades)
    {
        for (Object *o : {durin, summoned})
        {
            o->giveUpgrade("Upgrade_P238DurinRing");
            o->giveUpgrade("Upgrade_P238DurinFortress");
        }
    }
    CHECK(w.ringCount() == 0);
    summoned->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
    CHECK(summoned->isEffectivelyDead());
    CHECK(w.ringCount() == 0);
    durin->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
    CHECK(durin->isEffectivelyDead());
    CHECK(w.ringCount() == 1);
}

TEST_CASE("PORT-238 Durin-style pulse marks own and allied mausoleums, not enemy or other structures")
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
    SpellEffectModules::registerAll(w.w.modules);
    w.combat().setAutoAcquireEnabled(false);
    bool allied = false;
    SUBCASE("allied owner") { allied = true; }
    SUBCASE("enemy owner") { allied = false; }
    if (allied)
    {
        w.playerOf('A')->setPlayerRelationship(w.playerOf('B'), ALLIES);
        w.playerOf('B')->setPlayerRelationship(w.playerOf('A'), ALLIES);
    }
    // dwarvendurin.ini:894-901 -> weapon.inc:44485-44502 ->
    // attributemodifier.inc:8054-8058; RW 0x88F554 -> 0x90EAF9.
    const char *ini = R"ini(
ModifierList P238DurinMarker
 Category = WEAPON
 Duration = 5000
 ModelCondition = USER_6
End
Weapon P238DurinPulse
 RadiusDamageAffects = ALLIES NOT_SIMILAR
 LeechRangeWeapon = Yes
 AttackRange = 50
 MeleeWeapon = No
 DelayBetweenShots = 5000
 PreAttackDelay = 0
 PreAttackType = PER_SHOT
 FiringDuration = 0
 AttributeModifierNugget
  AttributeModifier = P238DurinMarker
  Radius = 19000
  SpecialObjectFilter = NONE +P238Mausoleum ALLIES
 End
End
Object P238DurinPulseHero
 KindOf = HERO INFANTRY SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = FireWeaponUpdate Fire
  FireWeaponNugget
   WeaponName = P238DurinPulse
   Offset = X:0 Y:0 Z:0
   FireDelay = 0
   OneShot = No
  End
 End
End
Object P238Mausoleum
 KindOf = STRUCTURE SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = AttributeModifierPoolUpdate Pool
 End
End
Object P238DurinOtherStructure
 KindOf = STRUCTURE SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 100
 End
 Behavior = AttributeModifierPoolUpdate Pool
 End
End
)ini";
    const std::string error = w.w.load(ini, INI_LOAD_OVERWRITE, "port238_durin_pulse.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *own = w.unit("P238Mausoleum", 'A', 320, 300);
    Object *otherPlayer = w.unit("P238Mausoleum", 'B', 330, 300);
    Object *otherKind = w.unit("P238DurinOtherStructure", 'A', 340, 300);
    const int user6 = CombatNames::modelCondition("USER_6");
    REQUIRE(user6 == 458);
    for (Object *obj : {own, otherPlayer, otherKind})
    {
        REQUIRE(obj != nullptr);
        w.logic->partition().registerObject(*obj);
        CHECK_FALSE(obj->testModelCondition(user6));
    }
    w.frames(1);
    Object *durin = w.unit("P238DurinPulseHero", 'A', 300, 300);
    REQUIRE(durin != nullptr);
    auto *fire = dynamic_cast<FireWeaponUpdate *>(durin->findModule("FireWeaponUpdate"));
    REQUIRE(fire != nullptr);
    for (int i = 0; i < 10 && fire->shots() == 0; ++i) w.frames(1);
    REQUIRE(fire->shots() == 1);
    CHECK(own->testModelCondition(user6));
    CHECK(otherPlayer->testModelCondition(user6) == allied);
    CHECK_FALSE(otherKind->testModelCondition(user6));
    w.frames(30);
    CHECK(fire->shots() == 2);
    CHECK(own->testModelCondition(user6));
    CHECK(otherPlayer->testModelCondition(user6) == allied);
    CHECK_FALSE(otherKind->testModelCondition(user6));
}

TEST_CASE("PORT-238 old Gondor Arvedui-style delivery replaces the base without inventing missing destination powers")
{
    HealHudWorld w;
    // arnor/arnorarvedui.ini:707-735,759-774,948-1750. The four names are
    // synchronized by the base but no matching module is literally declared
    // in the separate ring object. Synthetic fixtures do not resolve includes.
    // RW 0x8840FC -> 0x8B8E2E -> 0x8B1690; timer loop 0x8B12BF.
    const char *data = R"ini(
Upgrade Upgrade_P238OldArveduiRing
 Type = OBJECT
End
Upgrade Upgrade_P238OldArveduiFortress
 Type = OBJECT
End
SpecialPower P238OldArveduiTransform
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 1000
End
SpecialPower SpecialAbilitySchtzedesKnigsreichs
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityHaltetStand
 Enum = SPECIAL_GENERAL_TARGETLESS_TWO
 ReloadTime = 120000
End
SpecialPower SpecialAbilityAragornBladeMaster
 Enum = SPECIAL_GENERAL_TARGETLESS_THREE
 ReloadTime = 120000
End
SpecialPower SpellBookPalantirNorden
 Enum = SPECIAL_SCREECH
 ReloadTime = 120000
End
CommandButton P238OldArveduiSwitch
 Command = SPECIAL_POWER
 SpecialPower = P238OldArveduiTransform
 InPalantir = Yes
End
Object P238OldArveduiBase
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = UnpauseSpecialPowerUpgrade Enable
  SpecialPowerTemplate = P238OldArveduiTransform
  TriggeredBy = Upgrade_P238OldArveduiRing
  RequiresAllTriggers = Yes
 End
 Behavior = SpecialPowerModule Starter
  SpecialPowerTemplate = P238OldArveduiTransform
  UpdateModuleStartsAttack = Yes
  StartsPaused = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate Transform
  SpecialPowerTemplate = P238OldArveduiTransform
  MountedTemplate = P238OldArveduiRingForm
  SynchronizeTimerOnSpecialPower = SpecialAbilitySchtzedesKnigsreichs SpecialAbilityHaltetStand SpecialAbilityAragornBladeMaster SpellBookPalantirNorden
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade Automatic
  TriggeredBy = Upgrade_P238OldArveduiRing
  GetUpgradeCommandButtonName = P238OldArveduiSwitch
 End
 Behavior = CitadelSlaughterHordeContain Entry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238OldArveduiRing Upgrade_P238OldArveduiFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
 Behavior = SpecialPowerModule Wealth
  SpecialPowerTemplate = SpecialAbilitySchtzedesKnigsreichs
 End
 Behavior = SpecialPowerModule Hold
  SpecialPowerTemplate = SpecialAbilityHaltetStand
 End
 Behavior = SpecialPowerModule Blade
  SpecialPowerTemplate = SpecialAbilityAragornBladeMaster
 End
End
Object P238OldArveduiRingForm
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_old_arvedui.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *hero = w.unit("P238OldArveduiBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    Team *team = hero->getTeam();
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    for (const char *name : {"SpecialAbilitySchtzedesKnigsreichs", "SpecialAbilityHaltetStand", "SpecialAbilityAragornBladeMaster"})
    {
        auto *sourcePower = power(*hero, name);
        REQUIRE(sourcePower != nullptr);
        sourcePower->setReadyFrame(600);
    }
    CHECK(power(*hero, "SpellBookPalantirNorden") == nullptr);
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238OldArveduiRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238OldArveduiFortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238OldArveduiRingForm") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    for (const char *name : {"SpecialAbilitySchtzedesKnigsreichs", "SpecialAbilityHaltetStand", "SpecialAbilityAragornBladeMaster", "SpellBookPalantirNorden"})
        CHECK(power(*replacement, name) == nullptr);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
}

TEST_CASE("PORT-238 Necromancer and Gorthaur-style ring delivery copies only listed present timers")
{
    HealHudWorld w;
    // mordorsauron.ini:956-979, 1020-1058; 2231-2254, 2295-2334.
    // Distinct timer lists, both destination MordorSauron_mod; synthetic
    // objects and only two common abilities, not actual inherited modules.
    // RW 0x8840FC -> 0x8B8E2E -> 0x8B1690; timer loop 0x8B12BF.
    bool gorthaur = false;
    SUBCASE("Necromancer") { gorthaur = false; }
    SUBCASE("Gorthaur") { gorthaur = true; }
    std::string data = R"ini(
Upgrade Upgrade_P238OldSauronRing
 Type = OBJECT
End
Upgrade Upgrade_P238OldSauronFortress
 Type = OBJECT
End
SpecialPower P238OldSauronTransform
 Enum = SPECIAL_TOGGLE_MOUNTED
 ReloadTime = 1000
End
)ini";
    for (const char *name : {"SpecialAbilityDunklerSchleierNekromant", "SpecialAbilityDominateEnemySaruman",
         "SpecialAbilityMachtderVerdammten", "SpecialAbilityAragornBladeMaster", "SpecialAbilityWaffenstillstandAnnatar",
         "SpecialAbilityMouthOfSauronDissent", "SpecialAbilityArmeeausHirngespinsten", "SpecialAbilityTolinGaurhoth",
         "SpecialAbilityEwigeKnechtschaft", "SpecialAbilityVerschlingendeFinsternis", "SpecialAbilityGorthaurFlammenschlag"})
    {
        data += std::string("SpecialPower ") + name + "\n Enum = SPECIAL_GENERAL_TARGETLESS\n ReloadTime = 120000\nEnd\n";
    }
    data += R"ini(
CommandButton P238OldSauronSwitch
 Command = SPECIAL_POWER
 SpecialPower = P238OldSauronTransform
 InPalantir = No
End
Object P238OldSauronBase
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = UnpauseSpecialPowerUpgrade Enable
  SpecialPowerTemplate = P238OldSauronTransform
  TriggeredBy = Upgrade_P238OldSauronRing
 End
 Behavior = SpecialPowerModule Starter
  SpecialPowerTemplate = P238OldSauronTransform
  UpdateModuleStartsAttack = Yes
  StartsPaused = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate Transform
  SpecialPowerTemplate = P238OldSauronTransform
  MountedTemplate = P238OldSauronRingForm
  SynchronizeTimerOnSpecialPower = )ini";
    data += gorthaur
        ? "SpecialAbilityEwigeKnechtschaft SpecialAbilityVerschlingendeFinsternis SpecialAbilityGorthaurFlammenschlag SpecialAbilityDominateEnemySaruman SpecialAbilityMachtderVerdammten SpecialAbilityAragornBladeMaster SpecialAbilityWaffenstillstandAnnatar SpecialAbilityMouthOfSauronDissent SpecialAbilityArmeeausHirngespinsten SpecialAbilityTolinGaurhoth\n"
        : "SpecialAbilityDunklerSchleierNekromant SpecialAbilityDominateEnemySaruman SpecialAbilityMachtderVerdammten SpecialAbilityAragornBladeMaster SpecialAbilityWaffenstillstandAnnatar SpecialAbilityMouthOfSauronDissent SpecialAbilityArmeeausHirngespinsten SpecialAbilityTolinGaurhoth\n";
    data += R"ini(
  UnpackTime = 2000
  PreparationTime = 0
  PackTime = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade Automatic
  TriggeredBy = Upgrade_P238OldSauronRing
  GetUpgradeCommandButtonName = P238OldSauronSwitch
 End
 Behavior = CitadelSlaughterHordeContain Entry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238OldSauronRing Upgrade_P238OldSauronFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
 Behavior = SpecialPowerModule Might
  SpecialPowerTemplate = SpecialAbilityMachtderVerdammten
 End
 Behavior = SpecialPowerModule Eternal
  SpecialPowerTemplate = SpecialAbilityEwigeKnechtschaft
 End
End
Object P238OldSauronRingForm
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Might
  SpecialPowerTemplate = SpecialAbilityMachtderVerdammten
 End
 Behavior = SpecialPowerModule Eternal
  SpecialPowerTemplate = SpecialAbilityEwigeKnechtschaft
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_old_sauron.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *hero = w.unit("P238OldSauronBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    Team *team = hero->getTeam();
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    auto *might = power(*hero, "SpecialAbilityMachtderVerdammten");
    auto *eternal = power(*hero, "SpecialAbilityEwigeKnechtschaft");
    REQUIRE(might != nullptr);
    REQUIRE(eternal != nullptr);
    might->setReadyFrame(300);
    eternal->setReadyFrame(600);
    auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(hero->getContain());
    REQUIRE(contain != nullptr);
    deliver(*contain, *carrier);
    CHECK(hero->hasUpgrade("Upgrade_P238OldSauronRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238OldSauronFortress"));
    REQUIRE(w.byId(ringId) != nullptr);
    CHECK(w.byId(ringId)->isDestroyed());
    Object *replacement = nullptr;
    for (int frame = 0; frame < 50 && !replacement; ++frame)
    {
        w.frames(1);
        for (Object *obj = w.logic->getFirstObject(); obj; obj = obj->getNextObject())
            if (obj->getTemplate() && obj->getTemplate()->getName() == "P238OldSauronRingForm") replacement = obj;
    }
    REQUIRE(replacement != nullptr);
    CHECK(replacement->getTeam() == team);
    might = power(*replacement, "SpecialAbilityMachtderVerdammten");
    eternal = power(*replacement, "SpecialAbilityEwigeKnechtschaft");
    REQUIRE(might != nullptr);
    REQUIRE(eternal != nullptr);
    CHECK(might->getReadyFrame() == 300);
    if (gorthaur) CHECK(eternal->getReadyFrame() == 600);
    else CHECK(eternal->getReadyFrame() != 600);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
}

TEST_CASE("PORT-238 Sauron-mod style Athelas reactivates the first single-burst area healer")
{
    HealWorld w;
    HeroModules::registerAll(w.w.modules);
    // mordorsauron.ini:6682-6714,7091-7098; includes/specialpower.inc:4821.
    // RW 0x85562D initializes the waiting flag from ButtonTriggered; the
    // SPECIAL_ATHELAS branch at 0x854841 finds an AutoHealBehavior by name
    // and calls 0x855431. Direct synthetic initiation, no HUD/mod load/FX.
    const char *data = R"ini(
SpecialPower P238SauronOldHeal
 Enum = SPECIAL_ATHELAS
 ReloadTime = 260000
End
Object P238SauronOldHealCaster
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 10000
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Starter
  SpecialPowerTemplate = P238SauronOldHeal
  UpdateModuleStartsAttack = Yes
  StartsPaused = No
 End
 Behavior = SpecialAbilityUpdate Ability
  SpecialPowerTemplate = P238SauronOldHeal
  StartAbilityRange = 1.6
  UnpackTime = 1
  PreparationTime = 3
  PackTime = 2096
  AwardXPForTriggering = 0
  IgnoreFacingCheck = Yes
 End
 Behavior = AutoHealBehavior Medicine
  StartsActive = Yes
  ButtonTriggered = No
  HealingAmount = 5000
  Radius = 10
  HealOnlyOthers = No
  SingleBurst = Yes
  KindOf = HERO
 End
 Behavior = AutoHealBehavior Passive
  StartsActive = Yes
  HealingAmount = 1
  HealingDelay = 1000
  StartHealingDelay = 100000
  HealOnlyIfNotInCombat = Yes
 End
End
Object P238SauronOldHealHero
 KindOf = INFANTRY HERO SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 10000
 End
End
Object P238SauronOldHealInfantry
 KindOf = INFANTRY SELECTABLE
 Body = ActiveBody Body
  MaxHealth = 10000
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_old_sauron_heal.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    Object *ally = w.unit("P238SauronOldHealHero", 'A', 305, 300);
    Object *enemy = w.unit("P238SauronOldHealHero", 'B', 305, 300);
    Object *infantry = w.unit("P238SauronOldHealInfantry", 'A', 305, 300);
    Object *far = w.unit("P238SauronOldHealHero", 'A', 325, 300);
    for (Object *obj : {ally, enemy, infantry, far})
    {
        REQUIRE(obj != nullptr);
        w.logic->partition().registerObject(*obj);
    }
    DamageInfo damage;
    damage.m_input.m_amount = 6000.0f;
    damage.m_input.m_damageType = DAMAGE_UNRESISTABLE;
    ally->attemptDamage(damage);
    REQUIRE(ally->getBodyModule()->getHealth() == 4000.0f);
    w.frames(1); // publish the nearby wounded ally before creating the healer
    Object *caster = w.unit("P238SauronOldHealCaster", 'A', 300, 300);
    REQUIRE(caster != nullptr);
    w.logic->partition().registerObject(*caster);
    w.frames(5); // StartsActive / SingleBurst fires without any button use
    auto *heal = dynamic_cast<AutoHealBehavior *>(caster->findModule("AutoHealBehavior"));
    auto *ability = dynamic_cast<SpecialAbilityUpdate *>(caster->findModule("SpecialAbilityUpdate"));
    REQUIRE(heal != nullptr);
    REQUIRE(ability != nullptr);
    REQUIRE(heal->heals() == 1);
    REQUIRE(ally->getBodyModule()->getHealth() == 9000.0f);
    damage.m_input.m_amount = 5000.0f;
    ally->attemptDamage(damage);
    REQUIRE(ally->getBodyModule()->getHealth() == 4000.0f);
    damage.m_input.m_amount = 6000.0f;
    for (Object *obj : {enemy, infantry, far, caster})
    {
        obj->attemptDamage(damage);
        REQUIRE(obj->getBodyModule()->getHealth() == 4000.0f);
    }
    w.frames(3);
    CHECK(caster->getBodyModule()->getHealth() == 4000.0f);
    CHECK(ally->getBodyModule()->getHealth() == 4000.0f);
    ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
    for (int i = 0; i < 30 && heal->heals() < 3; ++i) w.frames(1);
    REQUIRE(ability->abilitiesTriggered() == 1);
    CHECK(heal->heals() == 3);
    CHECK(caster->getBodyModule()->getHealth() == 9000.0f);
    CHECK(ally->getBodyModule()->getHealth() == 9000.0f);
    CHECK(enemy->getBodyModule()->getHealth() == 4000.0f);
    CHECK(infantry->getBodyModule()->getHealth() == 4000.0f);
    CHECK(far->getBodyModule()->getHealth() == 4000.0f);
}

TEST_CASE("PORT-238 legacy Gandalf-style PLAYER white and OBJECT ring jointly unlock the good form")
{
    HealHudWorld w;
    HeroModules::registerAll(w.w.modules);
    // men/gandalf.ini:1511-1567,1586-1600, upgrade.ini:181,198,1062.
    // RW Object upgrade update 0x6936FE combines player and object masks;
    // mux 0x8D26F3 checks all triggers, unpause 0x8B9676 and auto-command
    // 0x8B8E2E lead to replacement 0x8B1690. No real UI or inheritance.
    const char *data = R"ini(
Upgrade Upgrade_P238LegacyGanRing
 Type = OBJECT
End
Upgrade Upgrade_P238LegacyGanFortress
 Type = OBJECT
End
Upgrade Upgrade_P238LegacyGanWhite
 Type = PLAYER
End
SpecialPower P238LegacyGanEvil
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 1000
End
SpecialPower P238LegacyGanGood
 Enum = SPECIAL_GENERAL_TARGETLESS_TWO
 ReloadTime = 1000
End
SpecialPower SpecialAbilityIstariLight
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityWordOfPower
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityWizardBlast
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityLightningSword
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
SpecialPower SpecialAbilityWordOfPowerGandalf
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 120000
End
CommandButton P238LegacyGanGoodButton
 Command = SPECIAL_POWER
 SpecialPower = P238LegacyGanGood
 InPalantir = Yes
End
Object P238LegacyGanBase
 KindOf = INFANTRY HERO SELECTABLE COMMANDCENTER CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = UnpauseSpecialPowerUpgrade EvilEnable
  SpecialPowerTemplate = P238LegacyGanEvil
  TriggeredBy = Upgrade_P238LegacyGanRing
 End
 Behavior = SpecialPowerModule EvilPower
  SpecialPowerTemplate = P238LegacyGanEvil
  UpdateModuleStartsAttack = Yes
  StartsPaused = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate EvilToggle
  SpecialPowerTemplate = P238LegacyGanEvil
  MountedTemplate = P238LegacyGanEvilForm
  UnpackTime = 2000
  IgnoreFacingCheck = Yes
 End
 Behavior = UnpauseSpecialPowerUpgrade GoodEnable
  SpecialPowerTemplate = P238LegacyGanGood
  TriggeredBy = Upgrade_P238LegacyGanWhite Upgrade_P238LegacyGanRing
  RequiresAllTriggers = Yes
 End
 Behavior = SpecialPowerModule GoodPower
  SpecialPowerTemplate = P238LegacyGanGood
  UpdateModuleStartsAttack = Yes
  StartsPaused = Yes
 End
 Behavior = ToggleMountedSpecialAbilityUpdate GoodToggle
  SpecialPowerTemplate = P238LegacyGanGood
  MountedTemplate = P238LegacyGanGoodForm
  SynchronizeTimerOnSpecialPower = SpecialAbilityIstariLight SpecialAbilityWordOfPower SpecialAbilityWizardBlast SpecialAbilityLightningSword
  UnpackTime = 2000
  IgnoreFacingCheck = Yes
 End
 Behavior = DoCommandUpgrade GoodCommand
  TriggeredBy = Upgrade_P238LegacyGanWhite Upgrade_P238LegacyGanRing
  RequiresAllTriggers = Yes
  GetUpgradeCommandButtonName = P238LegacyGanGoodButton
 End
 Behavior = SpecialPowerModule Istari
  SpecialPowerTemplate = SpecialAbilityIstariLight
  StartsPaused = No
 End
 Behavior = SpecialPowerModule Blast
  SpecialPowerTemplate = SpecialAbilityWizardBlast
  StartsPaused = No
 End
 Behavior = SpecialPowerModule Lightning
  SpecialPowerTemplate = SpecialAbilityLightningSword
  StartsPaused = No
 End
 Behavior = SpecialPowerModule WordGandalf
  SpecialPowerTemplate = SpecialAbilityWordOfPowerGandalf
  StartsPaused = No
 End
 Behavior = CitadelSlaughterHordeContain RingEntry
  PassengerFilter = NONE +P238Ring
  ObjectStatusOfContained = UNSELECTABLE ENCLOSED
  ContainMax = 1
  StatusForRingEntry = HOLDING_THE_RING
  UpgradeForRingEntry = Upgrade_P238LegacyGanRing Upgrade_P238LegacyGanFortress
  ObjectToDestroyForRingEntry = NONE +P238Ring
 End
End
Object P238LegacyGanGoodForm
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
 Behavior = SpecialPowerModule Istari
  SpecialPowerTemplate = SpecialAbilityIstariLight
  StartsPaused = No
 End
 Behavior = SpecialPowerModule Blast
  SpecialPowerTemplate = SpecialAbilityWizardBlast
  StartsPaused = No
 End
 Behavior = SpecialPowerModule Lightning
  SpecialPowerTemplate = SpecialAbilityLightningSword
  StartsPaused = No
 End
 Behavior = SpecialPowerModule WordGandalf
  SpecialPowerTemplate = SpecialAbilityWordOfPowerGandalf
  StartsPaused = No
 End
End
Object P238LegacyGanEvilForm
 KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
 Body = ActiveBody Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface AI
 End
End
)ini";
    const std::string error = w.w.load(data, INI_LOAD_OVERWRITE, "port238_legacy_gandalf.ini");
    REQUIRE_MESSAGE(error.empty(), error);
    bool whiteFirst = false;
    SUBCASE("ring first") {}
    SUBCASE("white first") { whiteFirst = true; }
    Object *hero = w.unit("P238LegacyGanBase", 'A', 300, 300);
    Object *carrier = w.unit("P238Carrier", 'A', 300, 300);
    Object *ring = w.unit("P238Ring", 'A', 300, 300);
    Object *other = w.unit("P238LegacyGanBase", 'B', 600, 600);
    REQUIRE(hero != nullptr);
    REQUIRE(carrier != nullptr);
    REQUIRE(ring != nullptr);
    REQUIRE(other != nullptr);
    auto count = [&](const char *name) {
        int result = 0;
        for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
            if (o->getTemplate() && o->getTemplate()->getName() == name) ++result;
        return result;
    };
    const ObjectID heroId = hero->getID();
    const ObjectID ringId = ring->getID();
    auto *goodPower = dynamic_cast<SpecialPowerModule *>(hero->findModuleByTag(w.w.keys.nameToKey("GoodPower")));
    auto *evilPower = dynamic_cast<SpecialPowerModule *>(hero->findModuleByTag(w.w.keys.nameToKey("EvilPower")));
    auto *goodEnable = dynamic_cast<UnpauseSpecialPowerUpgrade *>(hero->findModuleByTag(w.w.keys.nameToKey("GoodEnable")));
    auto *goodCommand = dynamic_cast<DoCommandUpgrade *>(hero->findModuleByTag(w.w.keys.nameToKey("GoodCommand")));
    auto *entry = dynamic_cast<CitadelSlaughterHordeContain *>(hero->findModule("CitadelSlaughterHordeContain"));
    REQUIRE(goodPower != nullptr);
    REQUIRE(evilPower != nullptr);
    REQUIRE(goodEnable != nullptr);
    REQUIRE(goodCommand != nullptr);
    REQUIRE(entry != nullptr);
    CHECK(goodPower->pauseCount() == 1);
    CHECK(evilPower->pauseCount() == 1);
    auto power = [&](Object &obj, const char *name) {
        const auto *t = w.powers.findSpecialPowerTemplate(name);
        return t ? dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(obj, t)) : nullptr;
    };
    const char *timerNames[] = {"SpecialAbilityIstariLight", "SpecialAbilityWizardBlast",
                                "SpecialAbilityLightningSword", "SpecialAbilityWordOfPowerGandalf"};
    const unsigned readyFrames[] = {400, 500, 600, 700};
    for (int i = 0; i < 4; ++i)
    {
        auto *module = power(*hero, timerNames[i]);
        REQUIRE(module != nullptr);
        module->setReadyFrame(readyFrames[i]);
    }
    const auto *white = Player::resolveUpgrade("Upgrade_P238LegacyGanWhite", true);
    REQUIRE(white != nullptr);
    auto giveWhite = [&]() { w.playerOf('A')->addUpgrade(white, Player::UPGRADE_STATUS_COMPLETE); };
    auto giveRing = [&]() {
        deliver(*entry, *carrier);
        REQUIRE(w.byId(ringId) != nullptr);
        CHECK(w.byId(ringId)->isDestroyed());
    };
    if (whiteFirst) giveWhite(); else giveRing();
    w.frames(5);
    REQUIRE(w.byId(heroId) != nullptr);
    CHECK_FALSE(goodEnable->isAlreadyUpgraded());
    CHECK_FALSE(goodCommand->isAlreadyUpgraded());
    CHECK(goodPower->pauseCount() == 1);
    CHECK(evilPower->pauseCount() == (whiteFirst ? 1 : 0));
    CHECK(count("P238LegacyGanGoodForm") == 0);
    CHECK(count("P238LegacyGanEvilForm") == 0);
    if (whiteFirst) giveRing(); else giveWhite();
    CHECK(hero->hasUpgrade("Upgrade_P238LegacyGanRing"));
    CHECK(hero->hasUpgrade("Upgrade_P238LegacyGanFortress"));
    CHECK(goodEnable->isAlreadyUpgraded());
    CHECK(goodCommand->isAlreadyUpgraded());
    CHECK(goodPower->pauseCount() == 0);
    for (int i = 0; i < 50 && count("P238LegacyGanGoodForm") == 0; ++i) w.frames(1);
    REQUIRE(count("P238LegacyGanGoodForm") == 1);
    Object *good = nullptr;
    for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
        if (o->getTemplate() && o->getTemplate()->getName() == "P238LegacyGanGoodForm") good = o;
    REQUIRE(good != nullptr);
    CHECK(good->getControllingPlayer() == w.playerOf('A'));
    for (int i = 0; i < 4; ++i)
    {
        auto *module = power(*good, timerNames[i]);
        REQUIRE(module != nullptr);
        if (i < 3) CHECK(module->getReadyFrame() == readyFrames[i]);
        else CHECK(module->getReadyFrame() != readyFrames[i]); // the list names WordOfPower, not WordOfPowerGandalf
    }
    CHECK(count("P238LegacyGanEvilForm") == 0);
    w.frames(3);
    CHECK(w.byId(heroId) == nullptr);
    CHECK_FALSE(w.playerOf('B')->hasUpgradeComplete("Upgrade_P238LegacyGanWhite"));
    CHECK(w.byId(other->getID()) == other);
}

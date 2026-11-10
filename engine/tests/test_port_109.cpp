// OpenBFME. GPL-3.0. Owner's rw.py proofs in issue #269; no game files.
#include "doctest.h"
#include "SyntheticHudTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameClient/CursorFile.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Module/GiveUpgradeUpdate.h"

namespace {
struct Rig : synthetichud::Rig
{
    SpecialPowerStore powers;
    SpecialPowerStore *previous = TheSpecialPowerStore;
    Rig()
    {
        TheSpecialPowerStore = &powers;
        w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
        const std::string error = w.load(
            "SpecialPower SpecialAbilityGiveUpgrade\n Enum = SPECIAL_GIVE_UPGRADE\n ReloadTime = 0\nEnd\n"
            "Upgrade Port109Weapon\n Type = OBJECT\n Cursor = WeaponUpgrade\nEnd\n"
            "Upgrade Port109Empty\n Type = OBJECT\nEnd\n"
            "Upgrade Port109Armor\n Type = OBJECT\n Cursor = ArmorUpgrade\nEnd\n"
            "Object Port109Carrier\n KindOf = SELECTABLE INFANTRY\n"
            " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
            " Behavior = AIUpdateInterface AI\n End\n"
            " Behavior = SpecialPowerModule Power\n SpecialPowerTemplate = SpecialAbilityGiveUpgrade\n End\n"
            " Behavior = GiveUpgradeUpdate Give\n SpecialPowerTemplate = SpecialAbilityGiveUpgrade\n DeliverUpgrade = No\n End\nEnd\n"
            "Object Port109Recipient\n KindOf = SELECTABLE INFANTRY\n"
            " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
            " Behavior = AIUpdateInterface AI\n End\n"
            " Behavior = LocomotorSetUpgrade Upgrade\n TriggeredBy = Port109Weapon Port109Empty Port109Armor\n RequiresAllTriggers = No\n End\nEnd\n"
            "ChildObject Port109Immobile Port109Recipient\n KindOf = SELECTABLE INFANTRY IMMOBILE\nEnd\n"
            "Object Port109NoAI\n KindOf = SELECTABLE INFANTRY\nEnd\n");
        REQUIRE_MESSAGE(error.empty(), error);
    }
    ~Rig() { TheSpecialPowerStore = previous; }
    const UpgradeTemplate *upgrade(const char *name)
    {
        const auto *u = TheUpgradeCenter->findUpgrade(name);
        REQUIRE(u != nullptr);
        return u;
    }
    Object *carrier(const char *name = "Port109Weapon")
    {
        Object *o = place("Port109Carrier", 100, 100);
        REQUIRE(o != nullptr);
        if (name) o->giveUpgrade(upgrade(name));
        return o;
    }
    int hint(Object *target, EvaluateType type = EvaluateType::DoHint)
    {
        return input->commandTranslator().evaluateContextCommand(target, target->getPosition(), type);
    }
};
}

TEST_CASE("PORT-109 WeaponUpgrade from first selected upgrade uses existing hint path")
{
    // RW 0x81F2D2..0x81F332 -> RW 0x5EE9CA -> RW 0x6A4081.
    Rig r;
    Object *source = r.carrier();
    Object *target = r.place("Port109Recipient", 200, 200);
    r.input->ui().mirrorSelection({source->getID()});
    REQUIRE(ActionManager::canGiveUpgrade(source, target));
    const size_t pending = r.input->stream().pending();
    CHECK(r.hint(target) == 0x7D6);
    CHECK(r.input->ui().cursor() == "WeaponUpgrade");
    CHECK(MouseCursorIndex(r.input->ui().cursor()) == 42);
    CHECK(r.input->stream().pending() == pending); // direct hint adaptation: no duplicate order
}

TEST_CASE("PORT-109 ANY success still resolves cursor from first source without upgrade to 1")
{
    // RW 0x69D572 (ANY), RW 0x81F337 (push 1); first is not passing source.
    Rig r;
    Object *first = r.carrier(nullptr);
    Object *second = r.carrier();
    Object *target = r.place("Port109Recipient", 200, 200);
    r.input->ui().mirrorSelection({first->getID(), second->getID()});
    CHECK_FALSE(ActionManager::canGiveUpgrade(first, target));
    REQUIRE(ActionManager::canGiveUpgrade(second, target));
    CHECK(r.hint(target) == 0x7D6);
    CHECK(r.input->ui().cursor() == "Normal");
    CHECK(MouseCursorIndex(r.input->ui().cursor()) == 1);
}

TEST_CASE("PORT-109 empty Cursor passes minus one through hint without Target fallback")
{
    // RW 0x5EE9CE -> 0x5EE9D7, unchanged at RW 0x6A4081.
    Rig r;
    Object *source = r.carrier("Port109Empty");
    Object *target = r.place("Port109Recipient", 200, 200);
    r.input->ui().mirrorSelection({source->getID()});
    REQUIRE(ActionManager::canGiveUpgrade(source, target));
    CHECK(r.hint(target, EvaluateType::EvaluateOnly) == 0x7D6);
    CHECK(r.input->ui().cursor().empty()); // existing client representation of no index
    CHECK(MouseCursorIndex(r.input->ui().cursor()) == -1);
    CHECK(MouseCursorIndex("UnknownCursor") == -1);
    CHECK(MouseCursorIndex("wEaPoNuPgRaDe") == 42);
}

TEST_CASE("PORT-109 selecting and scrolling suppress upgrade hints and commands do not duplicate hints")
{
    // RW 0x69BD53 guards; RW 0x81F2C0 no button -> end, not a move order.
    Rig r;
    Object *source = r.carrier();
    Object *target = r.place("Port109Recipient", 200, 200);
    r.input->ui().mirrorSelection({source->getID()});
    r.input->ui().setCursor("Arrow");
    r.input->ui().setSelecting(true);
    CHECK(r.hint(target) == 0x7D6);
    CHECK(r.input->ui().cursor() == "Arrow");
    r.input->ui().setSelecting(false);
    r.input->ui().setScrolling(true);
    r.input->ui().setCursor("Scroll");
    CHECK(r.hint(target) == 0x7D6);
    CHECK(r.input->ui().cursor() == "Scroll");
    r.input->ui().setScrolling(false);
    const size_t pending = r.input->stream().pending();
    CHECK(r.hint(target, EvaluateType::DoCommand) == 0);
    CHECK(r.input->stream().pending() == pending);
}

TEST_CASE("PORT-109 shared target predicate preserves ally AI and immobile rejection and contained stop")
{
    // RW 0x82D783..0x82D7AB, 0x89FE80 / 0x89FE86.
    Rig r;
    Object *source = r.carrier();
    Object *target = r.place("Port109Recipient", 200, 200);
    Object *enemy = r.place("Port109Recipient", 300, 300, "Bob");
    Object *immobile = r.place("Port109Immobile", 400, 400);
    Object *noAI = r.place("Port109NoAI", 450, 450);
    CHECK_FALSE(ActionManager::canGiveUpgrade(nullptr, target));
    CHECK_FALSE(ActionManager::canGiveUpgrade(source, nullptr));
    CHECK_FALSE(ActionManager::canGiveUpgrade(source, enemy));
    CHECK_FALSE(ActionManager::canGiveUpgrade(source, immobile));
    CHECK_FALSE(ActionManager::canGiveUpgrade(source, noAI));
    REQUIRE(ActionManager::canGiveUpgrade(source, target));
    target->friend_setContainedBy(source);
    CHECK_FALSE(ActionManager::canGiveUpgrade(source, target));
    bool stopped = false;
    for (const auto &line : r.logic->report().stops)
        if (line.find("PORT-109 contained upgrade target") != std::string::npos) stopped = true;
    CHECK(stopped);
    target->friend_setContainedBy(nullptr);
    CHECK(ActionManager::canGiveUpgrade(source, target));
}

TEST_CASE("PORT-109 mask lookup follows template head order not lowest bit")
{
    // RW 0x66F248 inserts at head; RW 0x66F468 returns first matching node.
    Rig r;
    Object *source = r.carrier();
    source->giveUpgrade(r.upgrade("Port109Armor"));
    CHECK(TheUpgradeCenter->findUpgrade(source->getUpgradeMask()) == r.upgrade("Port109Armor"));
    Object *target = r.place("Port109Recipient", 200, 200);
    r.input->ui().mirrorSelection({source->getID()});
    REQUIRE(ActionManager::canGiveUpgrade(source, target));
    CHECK(r.hint(target) == 0x7D6);
    CHECK(r.input->ui().cursor() == "ArmorUpgrade");
}

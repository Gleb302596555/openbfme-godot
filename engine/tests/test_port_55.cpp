// OpenBFME. GPL-3.0.
// PORT-55: synthetic INI only. Expected amounts/branches are from RW
// 0x883B4A, 0x68DDF1 (BountyValue, not the paid build price), 0x883BB2..
// 0x883CB7 (two ceils, multiplayer, container PRODUCTION and slaughter cost),
// 0x8838C3 (CanAlwaysEnter), 0x883A29 / 0x8840FC (ring predicate/callback).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AI/GarrisonCommands.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/GarrisonContain.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/ScriptEngine/LuaScriptEvents.h"

using namespace combattest;
namespace
{
std::string building(const char *name, const char *module, const char *extra = "", const char *percent = "25%")
{
    return std::string("Object ") + name + "\n"
        " KindOf = STRUCTURE IMMOBILE SCORE\n"
        " Geometry = CYLINDER\n GeometryMajorRadius = 20\n GeometryHeight = 30\n"
        " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
        " Behavior = " + module + " Contain\n"
        " PassengerFilter = ANY +INFANTRY +HORDE\n"
        " ObjectStatusOfContained = UNSELECTABLE ENCLOSED\n"
        " ContainMax = 20\n AllowOwnPlayerInsideOverride = Yes\n"
        " AllowEnemiesInside = No\n AllowAlliesInside = No\n AllowNeutralInside = No\n"
        " EntryOffset = X:-30 Y:0 Z:0\n EntryPosition = X:-20 Y:0 Z:0\n ExitOffset = X:-40 Y:0 Z:0\n"
        " CashBackPercent = " + percent + "\n" + extra + " End\nEnd\n";
}
const std::string kData =
    "Object P55Troop\n KindOf = INFANTRY SELECTABLE SCORE\n"
    " BuildCost = 701\n BountyValue = 17\n TransportSlotCount = 1\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\nEnd\n"
    "Object P55Token\n KindOf = NO_COLLIDE\nEnd\n"
    "Object P55Horde\n KindOf = HORDE INFANTRY SELECTABLE\n TransportSlotCount = 1\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = HordeAIUpdate AI\n End\n"
    " Behavior = HordeContain Horde\n"
    " RankInfo = RankNumber:1 UnitType:P55Troop Position:X:0 Y:0 Position:X:0 Y:15\n"
    " InitialPayload = P55Troop 2\n End\n"
    " LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\nEnd\n" +
    building("P55Citadel", "CitadelSlaughterHordeContain",
        " StatusForRingEntry = HOLDING_THE_RING\n"
        " ObjectToDestroyForRingEntry = NONE +P55Token\n"
        " UpgradeForRingEntry = Upgrade_Basic Upgrade_Barracks2\n") +
    building("P55Slaughter", "SlaughterHordeContain") +
    building("P55Always", "SlaughterHordeContain",
        " ContainMax = 0\n PassengerFilter = NONE\n CanAlwaysEnter = ANY +P55Troop\n"
        " StatusRequiredForCanAlwaysEnter = HOLDING_THE_RING\n") +
    building("P55Zero", "SlaughterHordeContain", " CanAlwaysEnter = ANY +INFANTRY\n", "0%") +
    building("P55Negative", "SlaughterHordeContain", "", "-25%");

SlaughterHordeContain &contain(Object *o)
{
    auto *c = dynamic_cast<SlaughterHordeContain *>(o->getContain());
    REQUIRE(c != nullptr);
    return *c;
}
}

TEST_CASE("PORT-55 ordinary citadel entry slaughters once and pays ceil of BountyValue times CashBackPercent")
{
    CombatWorld w(kData.c_str());
    Object *citadel = w.unit("P55Citadel", 'A', 400, 400);
    Object *troop = w.unit("P55Troop", 'A', 350, 400);
    const ObjectID id = troop->getID();
    troop->setBuildCostPaid(999.0f); // deliberately neither BountyValue nor BuildCost
    const int before = w.cash('A');
    int events = 0;
    w.logic->setScriptEventProc([&](int slot, Object &self, const LuaEventArgs &args) {
        if (slot != LUAEVENT_OnSlaughtered) return;
        ++events;
        CHECK(self.getID() == id);
        CHECK(args.arg[0].kind == LuaEventArg::OBJECT);
        CHECK(args.arg[0].objectId == (int)citadel->getID());
    });
    auto &c = contain(citadel);
    CHECK(c.isValidContainerFor(*troop, true, false));
    REQUIRE(c.addToContain(troop));
    CHECK(troop->isDestroyed()); // RW destroy, not just parked inside the citadel
    CHECK(troop->isEffectivelyDead());
    CHECK(troop->isDrawableHidden());
    CHECK(c.getContainCount() == 0);
    CHECK(w.cash('A') - before == 5); // RW: ceil(17 * .25) = 5, not 176 or 250
    CHECK(events == 1);
    CHECK(w.logic->economy().incomeLog().size() == 1);
    CHECK(w.logic->economy().incomeLog().back().amount == 5);
    w.frames(3);
    CHECK(w.byId(id) == nullptr);
    CHECK(w.cash('A') - before == 5);
    CHECK(events == 1);
}

TEST_CASE("PORT-55 SlaughterHordeContain uses the same payout and a negative percentage suppresses money not slaughter")
{
    CombatWorld w(kData.c_str());
    for (const char *name : {"P55Slaughter", "P55Negative"})
    {
        CAPTURE(name);
        Object *house = w.unit(name, 'A', 400, 400);
        Object *troop = w.unit("P55Troop", 'A', 350, 400);
        const ObjectID id = troop->getID();
        const int before = w.cash('A');
        REQUIRE(contain(house).addToContain(troop));
        CHECK(w.cash('A') - before == (std::string(name) == "P55Slaughter" ? 5 : 0));
        w.frames(1);
        CHECK(w.byId(id) == nullptr);
    }
}

TEST_CASE("PORT-55 CanAlwaysEnter requires the container status and bypasses filters capacity and owner restriction")
{
    CombatWorld w(kData.c_str());
    Object *house = w.unit("P55Always", 'A', 400, 400);
    Object *troop = w.unit("P55Troop", 'B', 350, 400);
    auto &c = contain(house);
    const unsigned ring = (unsigned)CombatNames::status("HOLDING_THE_RING");
    CHECK_FALSE(c.isValidContainerFor(*troop, true, true));
    troop->setStatus(ring, true);
    CHECK_FALSE(c.isValidContainerFor(*troop, true, true)); // status on entrant is insufficient
    troop->setStatus(ring, false);
    house->setStatus(ring, true);
    REQUIRE(c.isValidContainerFor(*troop, true, true));
    const int a = w.cash('A'), b = w.cash('B');
    REQUIRE(c.addToContain(troop));
    CHECK(w.cash('B') - b == 5); // RW 0x883B6B / 0x883CB7 pays the entrant's player
    CHECK(w.cash('A') == a);
    int maximum = -1, occupants = -1, heroes = -1;
    CHECK_FALSE(c.getContainerPipCounts(maximum, occupants, heroes));
    CHECK(maximum == 0);
    CHECK(occupants == 0);
    CHECK(heroes == 0);
}

TEST_CASE("PORT-55 actual enter command reaches ordinary citadel slaughter without an AI or command special case")
{
    CombatWorld w(kData.c_str());
    Object *house = w.unit("P55Citadel", 'A', 400, 400);
    Object *troop = w.unit("P55Troop", 'A', 330, 400);
    const ObjectID id = troop->getID();
    const int before = w.cash('A');
    w.playerOf('A')->selection() = {id};
    GameMessage message(MSG_ENTER, w.playerOf('A')->getPlayerIndex());
    message.appendObjectIDArgument(id);
    message.appendObjectIDArgument(house->getID());
    w.send(message);
    w.frames(100); // enough travel frames for the synthetic 70-unit walk
    CHECK(w.byId(id) == nullptr);
    CHECK(w.cash('A') - before == 5);
    CHECK(w.logic->economy().incomeLog().size() == 1);
}

TEST_CASE("PORT-55 ring callback survives shared base entry and a pending exit whole-game save4 round trip")
{
    CombatWorld w(kData.c_str());
    Object *house = w.unit("P55Citadel", 'A', 400, 400);
    Object *carrier = w.unit("P55Troop", 'A', 350, 400);
    Object *token = w.unit("P55Token", 'A', 350, 400);
    const ObjectID tokenID = token->getID(), carrierID = carrier->getID(), houseID = house->getID();
    carrier->setStatus((unsigned)CombatNames::status("HOLDING_THE_RING"), true);
    const int before = w.cash('A');
    auto *c = dynamic_cast<CitadelSlaughterHordeContain *>(&contain(house));
    REQUIRE(c != nullptr);
    REQUIRE(c->addToContain(carrier));
    CHECK_FALSE(carrier->isEffectivelyDead());
    CHECK_FALSE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));
    CHECK(token->isDestroyed());
    CHECK(c->pendingRingExit() == carrierID);
    CHECK(w.cash('A') == before);
    CHECK(w.logic->economy().incomeLog().empty());
    CHECK(w.playerOf('A')->hasUpgradeComplete("Upgrade_Basic"));
    CHECK(house->hasUpgrade("Upgrade_Barracks2"));
    std::string error;
    std::vector<std::uint8_t> saved;
    REQUIRE_MESSAGE(w.logic->saveSnapshot(saved, &error), error);
    w.frames(60);
    REQUIRE_MESSAGE(w.logic->loadSnapshot(saved, &error), error);
    carrier = w.byId(carrierID);
    REQUIRE(carrier != nullptr);
    c = dynamic_cast<CitadelSlaughterHordeContain *>(w.byId(houseID)->getContain());
    REQUIRE(c != nullptr);
    CHECK(c->pendingRingExit() == carrierID);
    w.frames(60);
    CHECK(w.byId(tokenID) == nullptr);
    CHECK(w.byId(carrierID) != nullptr);
    CHECK(carrier->getContainedBy() == nullptr);
    CHECK(c->pendingRingExit() == INVALID_ID);
    CHECK(w.cash('A') == before);
}

TEST_CASE("PORT-55 a slaughtered horde pays each arriving member and destroys its parent only at the last entry")
{
    CombatWorld w(kData.c_str());
    Object *house = w.unit("P55Citadel", 'A', 400, 400);
    Object *parent = w.unit("P55Horde", 'A', 300, 400);
    auto *h = dynamic_cast<HordeContain *>(parent->getContain());
    REQUIRE(h != nullptr);
    const auto members = *h->getContainedItemsList();
    REQUIRE(members.size() == 2);
    const int before = w.cash('A');
    h->enterContainer(house, CMD_FROM_PLAYER);
    REQUIRE(parent->getContainedBy() == house);
    CHECK(w.cash('A') == before); // no parent payment
    REQUIRE(contain(house).addToContain(members.front()));
    CHECK(w.cash('A') - before == 5);
    CHECK_FALSE(parent->isDestroyed());
    REQUIRE(contain(house).addToContain(members.back()));
    CHECK(parent->isDestroyed());
    CHECK(w.cash('A') - before == 10);
}

TEST_CASE("PORT-55 payout applies multiplayer and container production before the second ceil")
{
    CombatWorld w(kData.c_str());
    w.logic->economy().context().gameMode = EconomyContext::MODE_SKIRMISH;
    for (float &mult : w.logic->economy().settings().multiPlayMoneyMult) mult = 1.5f;
    w.logic->economy().setAttributeModifierProvider([](const Object &o, int type, float &factor) {
        if (o.getTemplate()->getName() == "P55Citadel" && type == Economy::ATTRIBUTE_MODIFIER_PRODUCTION)
        { factor = 0.5f; return true; }
        return false;
    });
    auto filter = std::make_shared<ObjectFilter>();
    filter->rule = ObjectFilter::RULE_ANY;
    filter->includeNames = {"P55Citadel"};
    w.playerOf('A')->addCostModifier(filter, {0.5f}, INVALID_ID, true);
    Object *house = w.unit("P55Citadel", 'A', 400, 400);
    Object *troop = w.unit("P55Troop", 'A', 350, 400);
    const int before = w.cash('A');
    REQUIRE(contain(house).addToContain(troop));
    CHECK(w.cash('A') - before == 2); // ceil(ceil(17*.25) * 1.5 * .5 * .5) = 2
}

TEST_CASE("PORT-55 zero-percent capture keeps transfer accounting through save4 and credits exactly the next different owner")
{
    CombatWorld w(kData.c_str());
    w.logic->settings().objectsThatScore = std::make_shared<ObjectFilter>(ObjectFilter::all(KindOfMaskType{}));
    Object *house = w.unit("P55Zero", 'A', 400, 400);
    Object *parent = w.unit("P55Horde", 'A', 300, 400);
    const ObjectID houseID = house->getID();
    auto *h = dynamic_cast<HordeContain *>(parent->getContain());
    REQUIRE(h != nullptr);
    const auto members = *h->getContainedItemsList();
    REQUIRE(members.size() == 2);
    const int cash = w.cash('A');
    h->enterContainer(house, CMD_FROM_PLAYER);
    for (Object *member : members) REQUIRE(contain(house).addToContain(member));
    w.frames(1);
    CHECK(w.cash('A') == cash);
    std::string error;
    std::vector<std::uint8_t> snapshot;
    REQUIRE_MESSAGE(w.logic->saveSnapshot(snapshot, &error), error);
    const std::uint32_t hash = w.hash();
    REQUIRE_MESSAGE(w.logic->loadSnapshot(snapshot, &error), error);
    CHECK(w.hash() == hash);
    house = w.byId(houseID);
    REQUIRE(house != nullptr);
    ScoreKeeper &oldScore = w.playerOf('A')->getScoreKeeper();
    ScoreKeeper &newScore = w.playerOf('B')->getScoreKeeper();
    // RW 0x79F60E has no enabled gate; RW 0x79F430 only tests counting.
    oldScore.setEnabled(false);
    newScore.setEnabled(false);
    newScore.setCounting(true);
    const int lostBefore = oldScore.unitsLost();
    const int destroyedBefore = newScore.unitsDestroyed(w.playerOf('A')->getPlayerIndex());
    Object *next = w.unit("P55Troop", 'B', 350, 400);
    REQUIRE(contain(house).addToContain(next));
    CHECK(house->getControllingPlayer() == w.playerOf('B'));
    CHECK(oldScore.unitsLost() - lostBefore == 2);
    CHECK(newScore.unitsDestroyed(w.playerOf('A')->getPlayerIndex()) - destroyedBefore == 2);
    Object *again = w.unit("P55Troop", 'B', 350, 400);
    REQUIRE(contain(house).addToContain(again));
    CHECK(oldScore.unitsLost() - lostBefore == 2); // the stored count was cleared
    CHECK(newScore.unitsDestroyed(w.playerOf('A')->getPlayerIndex()) - destroyedBefore == 2);
}

TEST_CASE("PORT-55 enter command sends a whole horde through the shared member callback")
{
    CombatWorld w(kData.c_str());
    Object *house = w.unit("P55Citadel", 'A', 400, 400);
    Object *parent = w.unit("P55Horde", 'A', 300, 400);
    const ObjectID parentID = parent->getID();
    const int before = w.cash('A');
    w.playerOf('A')->selection() = {parentID};
    GameMessage enter(MSG_ENTER, w.playerOf('A')->getPlayerIndex());
    enter.appendObjectIDArgument(parentID);
    enter.appendObjectIDArgument(house->getID());
    w.send(enter);
    w.frames(150); // bounded pathfinding wait, not an asserted retail travel time
    CHECK(w.byId(parentID) == nullptr);
    CHECK(w.cash('A') - before == 10);
    CHECK(w.logic->economy().incomeLog().size() == 2);
}

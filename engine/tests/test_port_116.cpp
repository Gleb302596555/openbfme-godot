// OpenBFME. GPL-3.0. PORT-116: synthetic INI; no retail or mod assets.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameClient/MessageStream/CommandXlat.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/AI/GarrisonCommands.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

using namespace combattest;
namespace
{
const char kObjects[] =
    "Object P116Soldier\n KindOf = INFANTRY SELECTABLE\n BountyValue = 12\n TransportSlotCount = 1\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AI\n End\n"
    " LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\nEnd\n"
    "Object P116Hero\n KindOf = INFANTRY HERO SELECTABLE\n TransportSlotCount = 1\n"
    " Body = ActiveBody Body\n MaxHealth = 100\n End\nEnd\n"
    "Object P116Horde\n KindOf = HORDE INFANTRY SELECTABLE\n TransportSlotCount = 1\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = HordeAIUpdate AI\n End\n Behavior = HordeContain Horde\n"
    " RankInfo = RankNumber:1 UnitType:P116Soldier Position:X:0 Y:0 Position:X:0 Y:15\n"
    " InitialPayload = P116Soldier 2\n End\n"
    " LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\nEnd\n"
    "Object P116Citadel\n KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 20\n GeometryHeight = 30\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = CitadelSlaughterHordeContain Contain\n"
    " PassengerFilter = ANY +INFANTRY +CAVALRY -HERO -SUMMONED\n"
    " ContainMax = 20\n AllowOwnPlayerInsideOverride = Yes\n"
    " AllowEnemiesInside = No\n AllowAlliesInside = No\n AllowNeutralInside = No\n"
    " EntryOffset = X:-30 Y:0 Z:0\n EntryPosition = X:-20 Y:0 Z:0\n"
    " CashBackPercent = 50%\n End\nEnd\n"
    "Object P116Slaughter\n KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = SlaughterHordeContain Contain\n PassengerFilter = ANY +INFANTRY\n"
    " ContainMax = 20\n AllowOwnPlayerInsideOverride = Yes\n End\nEnd\n"
    "Object P116Tower\n KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = HordeGarrisonContain Contain\n PassengerFilter = ANY +INFANTRY\n"
    " ContainMax = 20\n AllowOwnPlayerInsideOverride = Yes\n AllowAlliesInside = Yes\n End\nEnd\n"
    "Object P116Fortress\n KindOf = STRUCTURE IMMOBILE SELECTABLE COMMANDCENTER\n"
    " Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
    " Behavior = CitadelSlaughterHordeContain Contain\n PassengerFilter = ANY +INFANTRY\n"
    " StatusForRingEntry = HOLDING_THE_RING\n ContainMax = 20\n"
    " AllowOwnPlayerInsideOverride = Yes\n End\nEnd\n";

struct Rig
{
    CombatWorld world{kObjects};
    PinholeView view;
    InGameUI ui;
    MessageStream stream;
    HudContext ctx{*world.logic, world.ai.get(), view, ui, stream, nullptr, {}};
    CommandTranslator translator{ctx};
    Rig() { world.players.setLocalPlayer(world.playerOf('A')); GarrisonCommands::registerHandlers(world.dispatcher); }
    void select(Object *o) { ui.selectObject(o->getID()); world.playerOf('A')->selection() = {o->getID()}; }
};
}

TEST_CASE("PORT-116 citadel accepts same-player horde and context enter emits one command")
{
    Rig r;
    Object *citadel = r.world.unit("P116Citadel", 'A', 400, 400);
    Object *horde = r.world.unit("P116Horde", 'A', 300, 400);
    Object *enemy = r.world.unit("P116Horde", 'B', 300, 300);
    const ObjectID id = horde->getID();
    // RW 0x8660E2, 0x8661A1..0x8661BA, 0x87D16F: filter, owner and capacity.
    CHECK(GarrisonRules::canEnterObject(*horde, citadel, CMD_FROM_PLAYER, 0, false));
    CHECK_FALSE(GarrisonRules::canEnterObject(*enemy, citadel, CMD_FROM_PLAYER, 0, false));
    // RW 0x8661A1..0x8661C0: the same-player override does not admit an ally.
    r.world.playerOf('B')->setPlayerRelationship(r.world.playerOf('A'), ALLIES);
    CHECK_FALSE(GarrisonRules::canEnterObject(*enemy, citadel, CMD_FROM_PLAYER, 0, false));
    r.world.playerOf('B')->setPlayerRelationship(r.world.playerOf('A'), NEUTRAL);
    CHECK_FALSE(GarrisonRules::canEnterObject(*enemy, citadel, CMD_FROM_PLAYER, 0, false));
    r.world.playerOf('B')->setPlayerRelationship(r.world.playerOf('A'), ENEMIES);
    Object *hero = r.world.unit("P116Hero", 'A', 250, 400);
    CHECK_FALSE(GarrisonRules::canEnterObject(*hero, citadel, CMD_FROM_PLAYER, 0, false)); // RW 0x8660E2: passenger filter
    r.select(horde);
    CHECK(r.translator.evaluateContextCommand(citadel, citadel->getPosition(), EvaluateType::DoHint) == MSG_ENTER);
    CHECK(r.ui.cursor() == "SendToDeath"); // RW 0x82CFB5 -> 0xCB -> 0x6A4055 -> 0x35
    CHECK(r.stream.pending() == 0);
    CHECK(r.translator.evaluateContextCommand(citadel, citadel->getPosition(), EvaluateType::DoCommand) == MSG_ENTER);
    auto messages = r.stream.propagate();
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].type() == MSG_ENTER);
    const int before = r.world.cash('A');
    r.world.send(toGameMessage(messages[0], r.world.playerOf('A')->getPlayerIndex()));
    r.world.frames(150);
    CHECK(r.world.byId(id) == nullptr);
    CHECK(r.world.cash('A') - before == 12);
}

TEST_CASE("PORT-116 slaughter building uses the sell cursor")
{
    Rig r;
    Object *slaughter = r.world.unit("P116Slaughter", 'A', 400, 400);
    Object *soldier = r.world.unit("P116Soldier", 'A', 300, 400);
    r.select(soldier);
    CHECK(r.translator.evaluateContextCommand(slaughter, slaughter->getPosition(), EvaluateType::DoHint) == MSG_ENTER);
    CHECK(r.ui.cursor() == "SendToDeath"); // RW 0x82CFB5 -> 0xCB -> 0x6A4055
    CHECK(r.stream.pending() == 0);
}

TEST_CASE("PORT-116 ordinary garrison keeps its friendly enter cursor")
{
    Rig r;
    Object *tower = r.world.unit("P116Tower", 'A', 400, 400);
    Object *soldier = r.world.unit("P116Soldier", 'A', 300, 400);
    r.select(soldier);
    CHECK(r.translator.evaluateContextCommand(tower, tower->getPosition(), EvaluateType::DoHint) == MSG_ENTER);
    CHECK(r.ui.cursor() == "EnterFriendly"); // RW 0x82CFC1 -> 0xB9 -> 0x6A4031
    CHECK(r.stream.pending() == 0);
}

TEST_CASE("PORT-116 command center ring carrier retains ring cursor")
{
    Rig r;
    Object *fortress = r.world.unit("P116Fortress", 'A', 400, 400);
    Object *soldier = r.world.unit("P116Soldier", 'A', 300, 400);
    Object *carrier = r.world.unit("P116Soldier", 'A', 300, 420);
    carrier->setStatus((unsigned)CombatNames::status("HOLDING_THE_RING"), true);
    // RW 0x82CF81..0x82CFAF checks all selected objects, not only the entrant.
    r.select(carrier);
    r.select(soldier);
    CHECK(r.translator.evaluateContextCommand(fortress, fortress->getPosition(), EvaluateType::DoHint) == MSG_ENTER);
    CHECK(r.ui.cursor() == "DeliverRing"); // RW 0x82CFCD -> 0xCC -> 0x6A4059
    CHECK(r.stream.pending() == 0);
}

TEST_CASE("PORT-116 contested occupied garrison uses aggressive cursor")
{
    Rig r;
    Object *tower = r.world.unit("P116Tower", 'B', 400, 400);
    Object *occupant = r.world.unit("P116Soldier", 'B', 380, 400);
    Object *attacker = r.world.unit("P116Soldier", 'A', 300, 400);
    REQUIRE(tower->getContain()->addToContain(occupant));
    attacker->setStatus((unsigned)CombatNames::status("CAN_ENTER_ANYTHING"), true);
    r.select(attacker);
    CHECK(r.translator.evaluateContextCommand(tower, tower->getPosition(), EvaluateType::DoHint) == MSG_ENTER);
    CHECK(r.ui.cursor() == "EnterAggressive"); // RW 0x82CF2A..0x82CF77 -> 0xBA -> 0x6A4035
    CHECK(r.stream.pending() == 0);
}

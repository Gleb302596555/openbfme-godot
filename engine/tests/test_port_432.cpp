// PORT-432: isolated, synthetic spell-book cast investigation; no game archives.
// RW 0x930DE5 / 0x940435, 0x77A5D7 / 0x77B886, 0x8CC594 / 0x8CC4B4 and 0x8C8CE4.
#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "CombatTestUtil.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/SpellBookUI.h"
#include "GameLogic/GlobalWeatherSystem.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/SpellStores.h"

#include <string>
#include <vector>

namespace
{
// Ennorath's Rebuild is enum 0x81 and an ally-object button, not the similarly
// named position button. Timings are shortened for repeated deterministic casts;
// the module types, heal parameters and Cloud Break spacing follow the mod. The
// MP command-center filter is simplified to the kinds relevant to these objects.
const char kData[] = R"(
Science TestRebuildScience
End
Science TestCloudScience
End
SpecialPower TestRebuild
 Enum = SPECIAL_AT_VISIBLE_OBJECT
 ReloadTime = 1000
 RequiredSciences = TestRebuildScience
 ObjectFilter = ANY +STRUCTURE
End
SpecialPower TestCloudBreak
 Enum = SPECIAL_SPELL_BOOK_CLOUD_BREAK
 ReloadTime = 1000
 RequiredSciences = TestCloudScience
End
CommandButton TestRebuildButton
 Command = SPELL_BOOK
 SpecialPower = TestRebuild
 Options = NEED_TARGET_ALLY_OBJECT
End
CommandButton TestCloudButton
 Command = SPELL_BOOK
 SpecialPower = TestCloudBreak
End
CommandSet TestSpellSet
 1 = TestRebuildButton
 2 = TestCloudButton
End
Object TestSpellBook
 KindOf = SPELL_BOOK
 CommandSet = TestSpellSet
 Behavior = PlayerHealSpecialPower HealTag
  SpecialPowerTemplate = TestRebuild
  AvailableAtStart = No
  RequirementsFilterMPSkirmish = ANY +COMMANDCENTER -HERO
  HealAmount = 4000
  HealAsPercent = No
  HealRadius = 200
  HealAffects = STRUCTURE
 End
 Behavior = CloudBreakSpecialPower CloudTag
  SpecialPowerTemplate = TestCloudBreak
  AvailableAtStart = No
  RequirementsFilterMPSkirmish = ANY +COMMANDCENTER -HERO
  SunbeamObject = TestSunbeam
  ObjectSpacing = 300
 End
End
Object TestBuilding
 KindOf = STRUCTURE SELECTABLE
 Geometry = BOX
 GeometryMajorRadius = 10
 GeometryMinorRadius = 10
 GeometryHeight = 20
 Body = ActiveBody BodyTag
  MaxHealth = 5000
 End
End
Object TestCenter
 KindOf = STRUCTURE COMMANDCENTER IMMOBILE
 Body = ActiveBody BodyTag
  MaxHealth = 5000
 End
End
Object TestSunbeam
 KindOf = IMMOBILE
End
)";

struct SpellRig
{
 combattest::CombatWorld world{"", 100};
 SpellStores stores{world.w.keys};
 CommandStore commandStore;
 CommandStore *oldCommands = TheCommandStore;
 SpellCommands spells;
 Player *player = nullptr;
 Object *book = nullptr;
 Object *own = nullptr;
 Object *enemy = nullptr;
 InGameSpellBookModel bar;

 explicit SpellRig(bool grant = true, bool center = true)
 {
  stores.install();
  SpellStores::registerBlocks(world.w.fx.env.blocks);
  TheCommandStore = &commandStore;
  commandStore.setThingFactory(&world.w.things);
  world.w.fx.env.blocks.registerBlock("CommandButton", CommandStore::parseCommandButtonDefinitionGlobal);
  world.w.fx.env.blocks.registerBlock("CommandSet", CommandStore::parseCommandSetDefinitionGlobal);
  SpecialPowerModules::registerAll(world.w.modules);
  const std::string error = world.w.load(kData, INI_LOAD_OVERWRITE, "port432.ini");
  REQUIRE_MESSAGE(error.empty(), error);
  spells.registerHandlers(world.dispatcher);
  player = world.playerOf('A');
  REQUIRE(player);
  world.players.setLocalPlayer(player);
  player->initScience(SpellGameMode{true, false});
  world.logic->partition().setRegion(0, 0, 1000, 1000);
  book = world.unit("TestSpellBook", 'A', 100, 100);
  own = world.unit("TestBuilding", 'A', 500, 500);
  enemy = world.unit("TestBuilding", 'B', 550, 500);
  if (center)
  {
   Object *citadel = world.unit("TestCenter", 'A', 200, 200);
   REQUIRE(citadel);
   citadel->friend_onBuildComplete();
  }
  REQUIRE(book); REQUIRE(own); REQUIRE(enemy);
  own->friend_onBuildComplete();
  enemy->friend_onBuildComplete();
  player->science().setSpellBookId(book->getID());
  // RW 0x6AE268 / 0x6AE29F: acquire sciences after the book exists;
  // skirmish sets the matching power ready rather than waiting its reload.
  SpecialPowerModules::installScienceHooks(*world.logic);
  if (grant)
  {
   REQUIRE(player->science().grantScience(stores.sciences().getScienceFromInternalName("TestRebuildScience")));
   REQUIRE(player->science().grantScience(stores.sciences().getScienceFromInternalName("TestCloudScience")));
  }
  world.logic->partition().update();
 }
 ~SpellRig()
 {
  world.logic->reset();
  TheCommandStore = oldCommands;
  stores.uninstall();
 }
 SpecialPowerModule *module(const char *name)
 {
  const auto *power = stores.specialPowers().findSpecialPowerTemplate(name);
  REQUIRE(power);
  auto *m = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*book, power));
  REQUIRE(m);
  return m;
 }
 void send(const std::vector<GameMessage> &messages)
 {
  for (const auto &m : messages) world.commands.append(m);
  world.logic->runLogicFrame();
 }
 void ready(SpecialPowerModule &m)
 {
  for (int frame = 0; frame < 100 && !m.isReady(); ++frame) world.logic->runLogicFrame();
  REQUIRE(m.isReady());
 }
};
}

TEST_CASE("PORT-432 synthetic repeated object Rebuild and targetless Cloud Break through messages")
{
 SpellRig r;
 auto *heal = r.module("TestRebuild");
 auto *cloud = dynamic_cast<CloudBreakSpecialPower *>(r.module("TestCloudBreak"));
 REQUIRE(cloud);
 auto *body = dynamic_cast<ActiveBody *>(r.own->getBodyModule());
 REQUIRE(body);
 for (int n = 0; n < 3; ++n)
 {
  r.ready(*heal);
  body->internalChangeHealth(-1000);
  const float before = body->getHealth();
  REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
  std::vector<GameMessage> messages;
  REQUIRE(r.bar.press(0, r.player->getPlayerIndex(), messages));
  REQUIRE(r.bar.targeting());
  CHECK(messages.empty());
  CHECK(r.bar.objectValid(*r.world.logic, *r.own));
  CHECK_FALSE(r.bar.objectValid(*r.world.logic, *r.enemy));
  REQUIRE(r.bar.clickObject(*r.own, r.player->getPlayerIndex(), messages));
  REQUIRE(messages.size() == 1u);
  CHECK(messages[0].getType() == MSG_DO_SPECIAL_POWER_AT_OBJECT);
  const auto casts = r.spells.casts();
  r.send(messages);
  CHECK(r.spells.casts() == casts + 1);
  CHECK(body->getHealth() == before + 1000.0f); // RW 0x8CC417: heal is capped at max health
  CHECK_FALSE(heal->isReady());
  r.ready(*cloud);
  r.world.logic->weather().setBurnDecay(10);
  REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
  messages.clear();
  REQUIRE(r.bar.press(1, r.player->getPlayerIndex(), messages));
  CHECK_FALSE(r.bar.targeting());
  REQUIRE(messages.size() == 1u);
  const auto beamCount = cloud->sunbeamsMade();
  r.send(messages);
  CHECK(r.spells.casts() == casts + 2);
  CHECK_FALSE(cloud->isReady());
  CHECK(r.world.logic->weather().burnDecay() == 0u);
  CHECK(cloud->sunbeamsMade() == beamCount + 4); // RW 0x8C8B57: 300,600 on each axis of a 1000 by 1000 map
 }
 CHECK(r.spells.malformed() == 0u);
 CHECK(r.spells.foreignSources() == 0u);
}

TEST_CASE("PORT-432 synthetic cooling spell is refused without consuming another cast")
{
 SpellRig r;
 auto *cloud = r.module("TestCloudBreak");
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 std::vector<GameMessage> messages;
 REQUIRE(r.bar.press(1, r.player->getPlayerIndex(), messages));
 REQUIRE(messages.size() == 1u);
 r.send(messages);
 CHECK_FALSE(cloud->isReady());
 const auto casts = r.spells.casts();
 r.send(messages);
 CHECK(r.spells.casts() == casts);
 CHECK(r.spells.refused() == 1u);
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 messages.clear();
 CHECK_FALSE(r.bar.press(1, r.player->getPlayerIndex(), messages));
 CHECK(messages.empty());
}

TEST_CASE("PORT-432 targetless message routes cast the same ready spell book power")
{
 // RW 0x77A4CC and 0x77B8D6 both invoke groupDoSpecialPower 0x76F5DB;
 // use separate identical worlds to compare the explicit-source and book routes.
 bool bookRoute = false;
 SUBCASE("explicit source") { bookRoute = false; }
 SUBCASE("issuer spell book") { bookRoute = true; }
 SpellRig r;
 auto *cloud = dynamic_cast<CloudBreakSpecialPower *>(r.module("TestCloudBreak"));
 REQUIRE(cloud);
 REQUIRE(cloud->isReady());
 const auto *power = r.stores.specialPowers().findSpecialPowerTemplate("TestCloudBreak");
 REQUIRE(power);
 GameMessage message(bookRoute ? MSG_DO_SPELLBOOK_SPECIAL_POWER : MSG_DO_SPECIAL_POWER, r.player->getPlayerIndex());
 message.appendIntegerArgument((int)power->getID());
 message.appendIntegerArgument(0);
 message.appendObjectIDArgument(bookRoute ? INVALID_ID : r.book->getID());
 r.world.logic->weather().setBurnDecay(10);
 const auto beams = cloud->sunbeamsMade();
 r.send({message});
 CHECK(r.spells.casts() == 1u);
 CHECK(r.spells.refused() == 0u);
 CHECK(r.spells.malformed() == 0u);
 CHECK_FALSE(cloud->isReady());
 CHECK(r.world.logic->weather().burnDecay() == 0u);
 CHECK(cloud->sunbeamsMade() == beams + 4);
}

TEST_CASE("PORT-432 science gating: unowned powers do not send messages")
{
 SpellRig r(false);
 auto *heal = r.module("TestRebuild");
 auto *cloud = r.module("TestCloudBreak");
 CHECK_FALSE(heal->isReady());
 CHECK_FALSE(cloud->isReady());
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 std::vector<GameMessage> messages;
 CHECK_FALSE(r.bar.press(0, r.player->getPlayerIndex(), messages));
 CHECK_FALSE(r.bar.press(1, r.player->getPlayerIndex(), messages));
 CHECK(messages.empty());
 CHECK_FALSE(r.bar.targeting());
 REQUIRE(r.player->science().grantScience(r.stores.sciences().getScienceFromInternalName("TestRebuildScience")));
 CHECK(heal->isReady());
 CHECK_FALSE(cloud->isReady());
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 CHECK(r.bar.press(0, r.player->getPlayerIndex(), messages));
 CHECK(r.bar.targeting());
 CHECK(messages.empty());
}

TEST_CASE("PORT-432 skirmish requirement: only an owned command center enables the powers")
{
 // The mod's filter is ANY +COMMANDCENTER with additional exclusions; this
 // fixture uses the same relevant kind predicate. RW 0x8969E5 -> 0x6ABD0B.
 SpellRig r(true, false);
 auto *heal = r.module("TestRebuild");
 auto *cloud = r.module("TestCloudBreak");
 REQUIRE(r.player->science().mode().skirmishOrMultiplayer);
 CHECK_FALSE(heal->requirementsMet());
 CHECK_FALSE(cloud->requirementsMet());
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 std::vector<GameMessage> messages;
 CHECK_FALSE(r.bar.press(0, r.player->getPlayerIndex(), messages));
 CHECK_FALSE(r.bar.press(1, r.player->getPlayerIndex(), messages));
 Object *otherCenter = r.world.unit("TestCenter", 'B', 200, 200);
 REQUIRE(otherCenter);
 otherCenter->friend_onBuildComplete();
 CHECK_FALSE(heal->requirementsMet());
 Object *ownCenter = r.world.unit("TestCenter", 'A', 300, 300);
 REQUIRE(ownCenter);
 // The port approximates RW's construction model conditions with statuses (S-253).
 ownCenter->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
 CHECK_FALSE(heal->requirementsMet());
 CHECK_FALSE(cloud->requirementsMet());
 ownCenter->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);
 ownCenter->friend_onBuildComplete();
 CHECK(heal->requirementsMet());
 CHECK(cloud->requirementsMet());
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 CHECK(r.bar.press(1, r.player->getPlayerIndex(), messages));
 REQUIRE(messages.size() == 1u);
 r.send(messages);
 CHECK(r.spells.casts() == 1u);
 CHECK_FALSE(cloud->isReady());
}

TEST_CASE("PORT-432 queued cast rechecks the skirmish requirement")
{
 // RW 0x76F6DA -> 0x82D68E rechecks readiness and module requirements on
 // dispatch. Construction statuses approximate RW's model conditions (S-253).
 SpellRig r(true, false);
 Object *center = r.world.unit("TestCenter", 'A', 300, 300);
 REQUIRE(center);
 center->friend_onBuildComplete();
 auto *cloud = r.module("TestCloudBreak");
 REQUIRE(cloud->requirementsMet());
 REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
 std::vector<GameMessage> messages;
 REQUIRE(r.bar.press(1, r.player->getPlayerIndex(), messages));
 REQUIRE(messages.size() == 1u);
 center->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
 REQUIRE_FALSE(cloud->requirementsMet());
 r.send(messages);
 CHECK(r.spells.casts() == 0u);
 CHECK(r.spells.refused() == 1u);
 CHECK(cloud->isReady());
 center->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);
 REQUIRE(cloud->requirementsMet());
 r.send(messages);
 CHECK(r.spells.casts() == 1u);
 CHECK_FALSE(cloud->isReady());
}

TEST_CASE("PORT-432 synthetic Apt button: a stable click releases with batched or split events")
{
 using namespace apttest;
 // RW 0xB0FB30..0xB0FB4A drains all queued events in order; RW
 // 0xB0F256..0xB0F2E8 applies the down/up button transitions. This is an
 // input control, not the actual Ennorath movie or a proof of its failure.
 TestMovie movie;
 const auto shape = movie.addShape(0, 0, 10, 10, 1);
 movie.addCharacter(0);
 const auto shapeId = movie.addCharacter(shape);
 const auto pressCode = program(movie, [&](Asm &a) { fscmd(a, "press"); });
 const auto releaseCode = program(movie, [&](Asm &a) { fscmd(a, "release"); });
 const auto button = movie.addButton({ { 1, shapeId }, { 2, shapeId }, { 4, shapeId }, { 8, shapeId } },
                                     { { 0x04, 0, pressCode }, { 0x08, 0, releaseCode } });
 const auto buttonId = movie.addCharacter(button);
 TestMovie::Place place = placeChar(buttonId, 1, "button");
 place.flags |= APT_PLACE_HASMATRIX;
 place.translation[0] = 100;
 place.translation[1] = 100;
 movie.setRootFrames({ { movie.addPlaceItem(place) } });
 PlayerFx fx;
 REQUIRE(fx.load(0, "PORT432", movie));
 fx.step();
 fx.apt->input().postMouseMove(105, 105);
 fx.step();
 fx.commands();
 bool split = false;
 SUBCASE("down and up in one movie frame") { split = false; }
 SUBCASE("down and up in different movie frames") { split = true; }
 for (int trial = 0; trial < 20; ++trial)
 {
  fx.apt->input().postMouseButton(true);
  if (split) fx.step();
  fx.apt->input().postMouseButton(false);
  fx.step();
  CHECK(fx.commandsText() == "press release");
  CHECK_FALSE(fx.apt->input().mouseDown());
 }
 CHECK(fx.errorsText().empty());
}

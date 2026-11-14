// PORT-432: isolated, synthetic spell-book cast investigation; no game archives.
// RW 0x930DE5 / 0x940435, 0x77A5D7 / 0x77B886, 0x8CC594 / 0x8CC4B4 and 0x8C8CE4.
#include "doctest.h"
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
// the module types, heal parameters and Cloud Break spacing follow the mod.
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
  AvailableAtStart = Yes
  HealAmount = 4000
  HealAsPercent = No
  HealRadius = 200
  HealAffects = STRUCTURE
 End
 Behavior = CloudBreakSpecialPower CloudTag
  SpecialPowerTemplate = TestCloudBreak
  AvailableAtStart = Yes
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

 SpellRig()
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
  REQUIRE(player->science().grantScience(stores.sciences().getScienceFromInternalName("TestRebuildScience")));
  REQUIRE(player->science().grantScience(stores.sciences().getScienceFromInternalName("TestCloudScience")));
  world.logic->partition().setRegion(0, 0, 1000, 1000);
  book = world.unit("TestSpellBook", 'A', 100, 100);
  own = world.unit("TestBuilding", 'A', 500, 500);
  enemy = world.unit("TestBuilding", 'B', 550, 500);
  REQUIRE(book); REQUIRE(own); REQUIRE(enemy);
  own->friend_onBuildComplete();
  enemy->friend_onBuildComplete();
  player->science().setSpellBookId(book->getID());
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

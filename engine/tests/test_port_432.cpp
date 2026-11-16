// PORT-432: isolated, synthetic spell-book cast investigation; no game archives.
// RW 0x930DE5 / 0x940435, 0x77A5D7 / 0x77B886, 0x8CC594 / 0x8CC4B4 and 0x8C8CE4.
#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "CombatTestUtil.h"
#include "MapTestUtil.h"
#include "GameClient/InGameHud.h"
#include "GameClient/LiveGame.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/SpellBookUI.h"
#include "GameLogic/GlobalWeatherSystem.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/SpellStores.h"

#include <memory>
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

TEST_CASE("PORT-179 synthetic repeated object Rebuild and targetless Cloud Break through messages")
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

TEST_CASE("PORT-179 synthetic cooling spell is refused without consuming another cast")
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

TEST_CASE("PORT-179 targetless message routes cast the same ready spell book power")
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

TEST_CASE("PORT-179 science gating: unowned powers do not send messages")
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

TEST_CASE("PORT-179 skirmish requirement: only an owned command center enables the powers")
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

TEST_CASE("PORT-179 queued cast rechecks the skirmish requirement")
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

TEST_CASE("PORT-179 synthetic movie callback reaches the spell model and message dispatch")
{
 // RW 0x930DE5 routes the movie's slot callback to the corresponding command.
 // This invented movie connects the port's components; it is not Ennorath's movie
 // and does not exercise InGameHud's deferred callback or drawable picking.
 using namespace apttest;
 bool split = false;
 SUBCASE("batched mouse events") { split = false; }
 SUBCASE("split mouse events") { split = true; }
 SpellRig r;
 MemorySource source;
 RecordingShellServices services;
 ShellEnvironment environment;
 AptScreenFactoryTable factories;
 TestMovie level0;
 level0.setRootFrames({ {} });
 source.add("AptLevel0", level0);
 TestMovie movie;
 const auto shape = movie.addShape(0, 0, 10, 10, 1);
 movie.addCharacter(0);
 const auto shapeId = movie.addCharacter(shape);
 const auto script = program(movie, [](Asm &a) {
  a.defineFunction("SetButtonState", { "slot", "state" }, [](Asm &) {});
  a.defineFunction("FlashButton", { "slot" }, [](Asm &) {});
 });
 const auto sprite = movie.addSprite({ { movie.addActionItem(script) } });
 const auto spriteId = movie.addCharacter(sprite);
 const auto rebuildAction = program(movie, [](Asm &a) { a.getURL("FSCommand:OnAptInGameSpellBookButtonPressed", "0"); });
 const auto cloudAction = program(movie, [](Asm &a) { a.getURL("FSCommand:OnAptInGameSpellBookButtonPressed", "1"); });
 const auto rebuildButton = movie.addButton({ {1, shapeId}, {2, shapeId}, {4, shapeId}, {8, shapeId} }, { {0x08, 0, rebuildAction} });
 const auto cloudButton = movie.addButton({ {1, shapeId}, {2, shapeId}, {4, shapeId}, {8, shapeId} }, { {0x08, 0, cloudAction} });
 const auto rebuildId = movie.addCharacter(rebuildButton);
 const auto cloudId = movie.addCharacter(cloudButton);
 TestMovie::Place rebuild = placeChar(rebuildId, 2, "Rebuild");
 TestMovie::Place cloud = placeChar(cloudId, 3, "Cloud");
 rebuild.flags |= APT_PLACE_HASMATRIX;
 cloud.flags |= APT_PLACE_HASMATRIX;
 rebuild.translation[0] = 100; rebuild.translation[1] = 100;
 cloud.translation[0] = 120; cloud.translation[1] = 100;
 movie.setRootFrames({ { movie.addPlaceItem(placeChar(spriteId, 1, "SpellBookUI")), movie.addPlaceItem(rebuild), movie.addPlaceItem(cloud) } });
 source.add("Palantir", movie);
 std::vector<GameMessage> messages;
 int callbacks = 0;
 bool pressed = false;
 auto windows = std::make_unique<WindowManager>(source, services);
 auto shell = std::make_unique<Shell>(*windows, factories, services, environment);
 windows->init();
 auto palantir = std::make_unique<AptPalantir>(*windows, *shell);
 palantir->setSpellPressHandler([&](int index) {
  ++callbacks;
  pressed = r.bar.press(index, r.player->getPlayerIndex(), messages);
 });
 windows->update(33);
 windows->update(33);
 REQUIRE(windows->invokeCallback("OnAptInGameSpellBookLoaded", "SpellBookUI"));
 REQUIRE(windows->invokeCallback("OnAptInGameSpellBookShown", ""));
 AptPalantir::SpellSlot rebuildSlot;
 rebuildSlot.buttonIndex = 0;
 rebuildSlot.state = AptPalantir::SPELL_UP;
 AptPalantir::SpellSlot cloudSlot;
 cloudSlot.buttonIndex = 1;
 cloudSlot.state = AptPalantir::SPELL_UP;
 palantir->syncSpellBook({rebuildSlot, cloudSlot});
 REQUIRE(palantir->callErrors().empty());
 auto *heal = r.module("TestRebuild");
 auto *cloudPower = r.module("TestCloudBreak");
 auto *body = dynamic_cast<ActiveBody *>(r.own->getBodyModule());
 REQUIRE(body);
 for (int trial = 0; trial < 5; ++trial)
 {
  for (int slot = 0; slot < 2; ++slot)
  {
   r.ready(slot == 0 ? *heal : *cloudPower);
   if (slot == 0) body->internalChangeHealth(-1000);
   REQUIRE(r.bar.refresh(r.commandStore, *r.world.logic, *r.player));
   messages.clear();
   pressed = false;
   const int before = callbacks;
   windows->postMouseMove(slot == 0 ? 105 : 125, 105);
   windows->update(33);
   windows->postMouseButton(true);
   if (split) windows->update(33);
   windows->postMouseButton(false);
   windows->update(33);
   CHECK(callbacks == before + 1);
   REQUIRE(pressed);
   if (slot == 0)
   {
    REQUIRE(r.bar.targeting());
    REQUIRE(r.bar.objectValid(*r.world.logic, *r.own));
    REQUIRE(r.bar.clickObject(*r.own, r.player->getPlayerIndex(), messages));
   }
   REQUIRE(messages.size() == 1u);
   const auto beforeCasts = r.spells.casts();
   r.send(messages);
   CHECK(r.spells.casts() == beforeCasts + 1);
   CHECK_FALSE((slot == 0 ? heal : cloudPower)->isReady());
   if (slot == 0) CHECK(body->getHealth() == body->getMaxHealth());
  }
 }
 CHECK(r.spells.malformed() == 0u);
}

TEST_CASE("PORT-179 synthetic Apt button: a stable click releases with batched or split events")
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

namespace
{
using namespace apttest;
using namespace maptest;
const char kGameData[] = R"(
GameData
 ForceModelsToFollowTimeOfDay = No
 ForceModelsToFollowWeather = No
 UnitDamagedThreshold = 0.5
 UnitReallyDamagedThreshold = 0.25
 DefaultStartingCash = 100
 TerrainResourceCellSize = 10
 GoodCommandPoints = 10 100
 EvilCommandPoints = 10 100
 GoodCommandPointsAI = 10 100
 EvilCommandPointsAI = 10 100
 GoodCommandPointsBonus = 0
 EvilCommandPointsBonus = 0
 GoodCommandPointLimit = 100
 EvilCommandPointLimit = 100
 PowerLimit = 100
 ResourceBonusMultiplier = 1
 ResourceMultiplierLimit = 1
 MultiPlayMoneyMult = MP1:1.0
 GoodCommandPointsMP2 = 10 100
 GoodCommandPointsMP3 = 10 100
 GoodCommandPointsMP4 = 10 100
 GoodCommandPointsMP5 = 10 100
 GoodCommandPointsMP6 = 10 100
 GoodCommandPointsMP7 = 10 100
 GoodCommandPointsMP8 = 10 100
 EvilCommandPointsMP2 = 10 100
 EvilCommandPointsMP3 = 10 100
 EvilCommandPointsMP4 = 10 100
 EvilCommandPointsMP5 = 10 100
 EvilCommandPointsMP6 = 10 100
 EvilCommandPointsMP7 = 10 100
 EvilCommandPointsMP8 = 10 100
 PartitionCellSize = 10
 UnlookPersistDuration = 1000
 ShroudColor = R:0 G:0 B:0
 FogAlpha = 100
 ShroudAlpha = 200
 StealthFriendlyOpacity = 50%
 MaxPathfindCellsPerFrame = 100
 MaxCellsAdjustDestination = 100
 MaxCellsAdjustHordeMeleeDestination = 100
 MaxCellsPatchPath = 100
 MaxCellsFindPathLimit = 100
 MaxCellsFindAttackPath = 100
 MaxCellsAdjustToMeleeDestination = 100
 MaxCellsFindMeleeEngagementLocation = 100
 MaxCellsAdjustToPossibleDestination = 100
 MaxCellsToExamineTowardsGoal = 100
 MovementPenaltyDamageState = DAMAGED
End
)";

const char kAIData[] = R"(
AIData
 EnableRepulsors = No
 RepulsedDistance = 1
 WadeWaterDepth = 1
 DeepWaterDepth = 2
 HordesWaitForHordes = No
 MeleeApproachTolerance = 1
 MeleeApproachDist = 1
 CastleSiegeStandBackDistance = 1
 AttackUsesLineOfSight = No
End
)";

const char kMultiplayer[] = R"(
MultiplayerSettings
 InitialCreditsVeryLow = 100
 InitialCreditsLow = 100
 InitialCreditsMedium = 100
 InitialCreditsHigh = 100
 InitialCreditsVeryHigh = 100
 UseShroud = No
End
)";


std::string hudMap()
{
 Toc t; W h;
 h.i32(16).i32(16).i32(0).i32(1).i32(16).i32(16).i32(256);
 for (int i = 0; i < 256; ++i) h.u16(0);
 D side(t); side.s("playerName", "Player_1").b("playerIsHuman", true);
 W sl; sl.u8(1).i32(1).raw(side.bytes()).i32(0);
 D team(t); team.s("teamName", "teamPlayer_1").s("teamOwner", "Player_1").b("teamIsSingleton", true);
 Bytes chunks = cat(chunk(t, "HeightMapData", 5, h.b), chunk(t, "WorldInfo", 1, D(t).i("weather", 0).bytes()));
 chunks = cat(chunks, chunk(t, "SidesList", 6, sl.b));
 chunks = cat(chunks, chunk(t, "Teams", 1, W().i32(1).raw(team.bytes()).b));
 Bytes map = file(t, chunks);
 return std::string(map.begin(), map.end());
}

// This in-memory movie exercises the actual HUD; it does not claim to be the
// Ennorath movie. The map and mandatory settings above are synthetic too.
struct HudSpellRig
{
 initest::Fixture files;
 RetailObjectWorld world;
 ArchiveW3DFileSource source;
 WW3DAssetManager assets;
 MapObjectOptions mapOptions;
 LiveGame game;
 PinholeView view;
 MetaMap meta;
 MemorySource movies;
 RecordingShellServices services;
 ShellEnvironment environment;
 AptScreenFactoryTable factories;
 WindowManager windows;
 Shell shell;
 std::unique_ptr<InGameHud> hud;
 std::string error;
 Player *player = nullptr;
 Object *book = nullptr;

 HudSpellRig() : world(files.fsys), source(files.fsys), assets(source), game(world, files.fsys, assets, mapOptions),
  windows(movies, services), shell(windows, factories, services, environment)
 {
  const std::string all = kData;
  const auto power = all.find("SpecialPower TestRebuild");
  const auto button = all.find("CommandButton TestRebuildButton");
  const auto object = all.find("Object TestSpellBook");
  REQUIRE(power != std::string::npos); REQUIRE(button != std::string::npos); REQUIRE(object != std::string::npos);
  files.mount({ { "Data\\INI\\Default\\SoundEffects.ini", "AudioEvent DefaultSoundEffect\n Volume = 100\n Type = ui everyone\n Type = +DEFAULT\nEnd\n" },
   { "Data\\INI\\Default\\SubsystemLegendExpansion1.ini", "LoadSubsystem TheScienceStore\n InitFile = Data\\INI\\TestScience.ini\nEnd\nLoadSubsystem TheSpecialPowerStore\n InitFile = Data\\INI\\TestPowers.ini\nEnd\nLoadSubsystem TheThingFactory\n InitFile = Data\\INI\\TestObjects.ini\nEnd\n" },
   { "Data\\INI\\TestScience.ini", all.substr(0, power) }, { "Data\\INI\\TestPowers.ini", all.substr(power, button-power) },
   { "Data\\INI\\CommandButton.ini", all.substr(button, object-button) }, { "Data\\INI\\TestObjects.ini", all.substr(object) },
   { "Maps\\synthetic\\synthetic.map", hudMap() }, { "Data\\INI\\GameData.ini", kGameData },
   { "Data\\INI\\Default\\AIData.ini", kAIData }, { "Data\\INI\\Multiplayer.ini", kMultiplayer },
   { "Data\\INI\\InGameUI.ini", "InGameUI\n SelectNearestBuilderCycleTimeOut = 1000\nEnd\n" },
   { "Data\\Scripts\\Scripts.lua", "-- no scripts\n" }, { "Data\\Scripts\\ScriptEvents.xml", "<SageLuaScriptSection>\n</SageLuaScriptSection>\n" } });
  REQUIRE_MESSAGE(world.load(&error), error);
  LiveGame::Options options; options.mapName = "synthetic"; options.mapScripts = 0;
  REQUIRE_MESSAGE(game.load(options, &error), error);
  auto context = world.enterContext();
  player = game.players().getLocalPlayer();
  REQUIRE(player);
  book = game.createObject("TestSpellBook", player->getPlayerIndex(), Coord3D{50,50,0}, 0, &error);
  REQUIRE_MESSAGE(book, error);
  player->science().setSpellBookId(book->getID());
  REQUIRE(player->science().grantScience(TheScienceStore->getScienceFromInternalName("TestRebuildScience")));
  REQUIRE(player->science().grantScience(TheScienceStore->getScienceFromInternalName("TestCloudScience")));
  TestMovie level0; level0.setRootFrames({{}}); movies.add("AptLevel0", level0);
  TestMovie movie;
  const auto shape = movie.addShape(0, 0, 10, 10, 1); movie.addCharacter(0);
  const auto shapeId = movie.addCharacter(shape);
  const auto script = program(movie, [](Asm &a) {
   a.defineFunction("SetButtonState", {"slot", "state"}, [](Asm &) {});
   a.defineFunction("FlashButton", {"slot"}, [](Asm &) {});
  });
  const auto spriteId = movie.addCharacter(movie.addSprite({{movie.addActionItem(script)}}));
  const auto rebuild = program(movie, [](Asm &a) { a.getURL("FSCommand:OnAptInGameSpellBookButtonPressed", "0"); });
  const auto cloud = program(movie, [](Asm &a) { a.getURL("FSCommand:OnAptInGameSpellBookButtonPressed", "1"); });
  const auto rebuildId = movie.addCharacter(movie.addButton({{1,shapeId},{2,shapeId},{4,shapeId},{8,shapeId}}, {{0x08,0,rebuild}}));
  const auto cloudId = movie.addCharacter(movie.addButton({{1,shapeId},{2,shapeId},{4,shapeId},{8,shapeId}}, {{0x08,0,cloud}}));
  auto r = placeChar(rebuildId, 2, "Rebuild"); r.flags |= APT_PLACE_HASMATRIX; r.translation[0] = 100; r.translation[1] = 100;
  auto c = placeChar(cloudId, 3, "Cloud"); c.flags |= APT_PLACE_HASMATRIX; c.translation[0] = 120; c.translation[1] = 100;
  const auto init = program(movie, [](Asm &a) { fscmd(a, "AptPalantir::OnInitialized"); });
  movie.setRootFrames({{ movie.addActionItem(init), movie.addPlaceItem(placeChar(spriteId, 1, "SpellBookUI")), movie.addPlaceItem(r), movie.addPlaceItem(c) }});
  movies.add("Palantir", movie);
  factories.registerFactory("Palantir.apt", [](AptScreenContext &c) { return std::make_unique<AptPalantir>(c.windows, c.shell); });
  windows.init(); shell.push("Palantir.apt", true); windows.setMinimumFrameTime(false); windows.update(33);
  hud = std::make_unique<InGameHud>(InGameHud::Config{game, world, files.fsys, services, view, MouseSettings{}, meta, nullptr, &windows, &shell});
  REQUIRE_MESSAGE(hud->boot(&error), error);
  hud->setWindowSize(1024, 768);
  REQUIRE(windows.invokeCallback("OnAptInGameSpellBookLoaded", "SpellBookUI"));
  REQUIRE(windows.invokeCallback("OnAptInGameSpellBookShown", ""));
  hud->update(0.033);
 }
};
} // namespace

TEST_CASE("PORT-179 synthetic HUD movie click reaches targeting and targetless dispatch")
{
 bool split = false;
 SUBCASE("batched press and release") { split = false; }
 SUBCASE("separate press and release") { split = true; }
 HudSpellRig r;
 auto context = r.hud->enterContext();
 const auto *cloudTemplate = TheSpecialPowerStore->findSpecialPowerTemplate("TestCloudBreak");
 REQUIRE(cloudTemplate);
 auto *cloud = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*r.book, cloudTemplate));
 REQUIRE(cloud);
 auto click = [&](int x, int t) {
  REQUIRE(r.hud->isOverGui(x, 105));
  r.hud->mouseMove(x, 105); r.windows.update(33); r.hud->update(0.033);
  r.hud->mouseButton(HudInput::Button::Left, true, x, 105, 0, t);
  if (split) { r.windows.update(33); r.hud->update(0.033); }
  r.hud->mouseButton(HudInput::Button::Left, false, x, 105, 0, t+1);
  r.windows.update(33); r.hud->update(0.033);
 };
 for (int n = 0; n < 3; ++n)
 {
  for (int i = 0; i < 100 && !cloud->isReady(); ++i) r.game.stepFrame();
  REQUIRE(cloud->isReady());
  r.hud->update(0.033);
  REQUIRE(r.hud->palantir()->spellSlotState(1) == AptPalantir::SPELL_UP);
  const auto casts = r.game.spellCommands().casts();
  click(125, 100 + n*10);
  CHECK_FALSE(r.hud->spellBar().targeting());
  r.game.stepFrame();
  CHECK(r.game.spellCommands().casts() == casts + 1);
  CHECK_FALSE(cloud->isReady());
 }
 REQUIRE(r.hud->palantir()->spellSlotState(0) == AptPalantir::SPELL_UP);
 click(105, 200);
 CHECK(r.hud->spellBar().targeting());
}

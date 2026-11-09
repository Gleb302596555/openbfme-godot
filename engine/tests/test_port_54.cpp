// PORT-54: synthetic CostModifierUpgrade and Palantir movie, no game files.
#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "EconTestUtil.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/HudContext.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/PalantirCommandUI.h"
#include "GameLogic/Module/UpgradeModules.h"
#include "GameLogic/Object/Object.h"

using namespace apttest;
namespace
{
const char objects[] = R"(
Object SyntheticForge
 KindOf = SELECTABLE STRUCTURE
 Behavior = CostModifierUpgrade Tag_Discount
  StartsActive = Yes
  UpgradeDiscount = Yes
  LabelForPalantirString = TEST:Discount
  Percentage = -10%
  Percentage = -20%
 End
End
Object SyntheticHearth
 KindOf = SELECTABLE STRUCTURE
 Behavior = CostModifierUpgrade Tag_Cost
  StartsActive = Yes
  ObjectFilter = ANY +INFANTRY
  LabelForPalantirString = TEST:Cost
  Percentage = -4%
  Percentage = -8%
 End
End
Object SyntheticOtherFilter
 KindOf = SELECTABLE STRUCTURE
 Behavior = CostModifierUpgrade Tag_Other
  StartsActive = Yes
  ObjectFilter = ANY +CAVALRY
  Percentage = -40%
 End
End
Object SyntheticUnspecified
 KindOf = SELECTABLE STRUCTURE
 Behavior = CostModifierUpgrade Tag_NoLabel
  UpgradeDiscount = Yes
 End
End
Object SyntheticPlain
 KindOf = SELECTABLE STRUCTURE
End
Object SyntheticUnit
 KindOf = SELECTABLE INFANTRY
 Behavior = CostModifierUpgrade Tag_Unit
  StartsActive = Yes
  UpgradeDiscount = Yes
  LabelForPalantirString = TEST:Discount
  Percentage = -10%
 End
End
)";

struct Fixture
{
 econtest::Fx game{objects};
 PinholeView view;
 InGameUI ui;
 MessageStream stream;
 HudContext context{*game.logic, nullptr, view, ui, stream, nullptr, {}, {}};
 ControlBar bar{context};
 MemorySource movies;
 RecordingShellServices services;
 ShellEnvironment environment;
 AptScreenFactoryTable factories;
 WindowManager windows{movies, services};
 Shell shell{windows, factories, services, environment};
 int shows = 0, hides = 0;
 Fixture()
 {
  game.players.setLocalPlayer(game.player("Alice"));
  ui.setLogic(game.logic.get());
  ui.setMaxSelectCount(50);
  TestMovie level0;
  level0.setRootFrames({{} });
  movies.add("AptLevel0", level0);
  TestMovie pal;
  auto actions = program(pal, [](Asm &a) {
   for (const char *name : {"SetPalantirFrameState", "SetPlayerButtonsState", "EnablePlayerMagicButton", "SetPlayerFaction", "ShowCommandInterface", "HideCommandInterface", "HideRankInterface"})
    a.defineFunction(name, {}, [](Asm &) {});
   a.defineFunction("ShowCostModifierUpgradeInterface", {}, [](Asm &f) { fscmd(f, "Test:ShowCost"); });
   a.defineFunction("HideCostModifierUpgradeInterface", {}, [](Asm &f) { fscmd(f, "Test:HideCost"); });
   fscmd(a, "AptPalantir::OnInitialized");
  });
  pal.setRootFrames({{pal.addActionItem(actions)}});
  movies.add("Palantir", pal);
  factories.registerFactory("Palantir.apt", [](AptScreenContext &c) { return std::make_unique<AptPalantir>(c.windows, c.shell); });
  windows.init();
  windows.registerCommand("Test:ShowCost", [this](const std::string &) { ++shows; });
  windows.registerCommand("Test:HideCost", [this](const std::string &) { ++hides; });
  shell.push("Palantir.apt", true);
  windows.update(33);
  shell.update();
 }
 AptPalantir *palantir() { return dynamic_cast<AptPalantir *>(shell.top()); }
 PalantirCommandUI::CostModifierInfo info(Object *o) { return PalantirCommandUI::costModifierInfo(context, o->getID()); }
 void select(Object *o)
 {
  ui.deselectAll(false);
  ui.selectObject(o->getID());
  const auto cost = PalantirCommandUI::costModifierInfo(context, ui.firstSelected());
  AptPalantir::LocalState state;
  state.context = true;
  state.contextObject = ui.firstSelected();
  state.costVisible = cost.visible;
  state.costUpgradeDiscount = cost.upgradeDiscount;
  state.costAmount = cost.amount;
  // Synthetic GameText values; this is the same data passed by InGameHud::update.
  if (cost.label == "TEST:Discount") state.costLabelFormat = "Discount %d%%";
  if (cost.label == "TEST:Cost") state.costLabelFormat = "Cost %d%%";
  if (cost.label == ">UNSPECIFIED<") state.costLabelFormat = "MISSING: '>UNSPECIFIED<'";
  palantir()->setLocalState(state);
  palantir()->sync(bar);
 }
};
} // namespace

TEST_CASE("PORT-54 selected upgrade discount is formatted into the Palantir section")
{
 Fixture f;
 REQUIRE(f.palantir());
 REQUIRE(f.palantir()->initialized());
 Object *plain = f.game.makeAt("SyntheticPlain", "Alice", 1, 1);
 f.select(plain);
 CHECK(f.windows.aptText("APT:CostModifierUpgrade") == nullptr); // RW 0x92FC25: no module
 CHECK(f.shows == 0);
 Object *forge = f.game.makeAt("SyntheticForge", "Alice", 2, 2);
 f.select(forge);
 REQUIRE(f.windows.aptText("APT:CostModifierUpgrade"));
 CHECK(*f.windows.aptText("APT:CostModifierUpgrade") == "Discount -10%"); // RW 0x92F735, 0x92FCCD
 CHECK(f.shows == 1);
 f.game.makeAt("SyntheticForge", "Alice", 3, 3);
 f.select(forge);
 CHECK(*f.windows.aptText("APT:CostModifierUpgrade") == "Discount -20%"); // RW 0x6B2D20, 0x92F73A
 CHECK(f.shows == 1);
 f.select(plain);
 CHECK(f.hides == 1); // RW 0x92F9B2 on context change
}

TEST_CASE("PORT-54 object filters, missing labels and the structure gate follow RW")
{
 Fixture f;
 Object *hearth = f.game.makeAt("SyntheticHearth", "Alice", 1, 1);
 f.game.makeAt("SyntheticOtherFilter", "Alice", 2, 2);
 f.game.makeAt("SyntheticHearth", "Alice", 3, 3);
 const auto cost = f.info(hearth);
 CHECK(cost.visible);
 CHECK_FALSE(cost.upgradeDiscount);
 CHECK(cost.amount == 8); // RW 0x6AD9A9–0x6AD9D0: equal filter IDs only, then index 1
 f.select(hearth);
 REQUIRE(f.windows.aptText("APT:CostModifierUpgrade"));
 CHECK(*f.windows.aptText("APT:CostModifierUpgrade") == "Cost 8%");
 Object *noLabel = f.game.makeAt("SyntheticUnspecified", "Alice", 4, 4);
 const auto unspecified = f.info(noLabel);
 CHECK(unspecified.visible); // RW 0x92FBAB tests module presence, not whether it executed
 CHECK(unspecified.label == ">UNSPECIFIED<"); // RW 0x8B9E0D
 f.select(noLabel);
 CHECK(*f.windows.aptText("APT:CostModifierUpgrade") == "MISSING: '>UNSPECIFIED<'");
 Object *unit = f.game.makeAt("SyntheticUnit", "Alice", 5, 5);
 CHECK_FALSE(f.info(unit).visible); // RW 0x92FBC0: not STRUCTURE
 f.select(unit);
 CHECK(f.hides == 2);
}

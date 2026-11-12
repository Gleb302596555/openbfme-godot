// PORT-137: synthetic data; no game files. RW 0x942490 / 0x942FC6,
// 0x9412F8 / 0x77B51F / 0x76FB3F / 0x68C8DE / 0x8993FC.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameLogic/Module/AutoPickUpUpdate.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/WeaponSetToggle.h"
#include <vector>

namespace
{
const char objects[] = R"(
Object TestCarrier
 KindOf = SELECTABLE INFANTRY
 CommandSet = TestSet
 Body = ActiveBody BodyTag
  MaxHealth = 100
 End
 Behavior = TransportContain TransportTag
  Slots = 1
  PassengerFilter = ANY +INFANTRY
  AllowAlliesInside = Yes
 End
 Behavior = AutoPickUpUpdate PickupA
  RunFromButton = Yes
  RunFromButtonNumber = 2
 End
 Behavior = AutoPickUpUpdate PickupB
  RunFromButton = Yes
  RunFromButtonNumber = 3
 End
End
Object TestPlain
 KindOf = SELECTABLE INFANTRY
 CommandSet = TestSet
 Body = ActiveBody BodyTag
  MaxHealth = 100
 End
End
)";
const char buttons[] = R"(
CommandButton TestFind
 Command = WAKE_AUTO_PICKUP
 Options = OPTION_TWO
 DisableOnModelCondition = WEAPONSET_TOGGLE_1
 InPalantir = Yes
End
CommandButton TestToggle
 Command = TOGGLE_WEAPONSET
 InPalantir = Yes
End
CommandSet TestSet
 1 = TestFind
 2 = TestToggle
End
)";
struct Rig
{
 combattest::CombatWorld world{objects, 40};
 CommandStore store;
 CommandStore *old = TheCommandStore;
 MouseSettings mouse;
 MetaMap meta;
 PinholeView view{{0,-300,400},{100,100,0},1024,768,0.7f};
 HudInput input{*world.logic,world.ai.get(),view,world.commands,mouse,meta};
 ControlBar bar{input.context()};
 std::vector<GameMessage> sent;
 Rig()
 {
  TheCommandStore = &store;
  store.setThingFactory(&world.w.things);
  world.w.fx.env.blocks.registerBlock("CommandButton", CommandStore::parseCommandButtonDefinitionGlobal);
  world.w.fx.env.blocks.registerBlock("CommandSet", CommandStore::parseCommandSetDefinitionGlobal);
  const std::string err = world.w.load(buttons, INI_LOAD_OVERWRITE, "port137.ini");
  REQUIRE_MESSAGE(err.empty(), err);
  input.context().commands = &store;
  world.players.setLocalPlayer(world.players.findPlayerWithName("Alice"));
  input.setLogicMessageObserver([this](const GameMessage &m) { sent.push_back(m); });
 }
 ~Rig() { TheCommandStore = old; }
 Object *make(const char *name, float x) { return world.unit(name, 'A', x, 100); }
 void select(Object *o, const std::vector<Object *> &group)
 {
  input.ui().deselectAll(false);
  input.ui().selectObject(o->getID());
  auto &ids = world.players.findPlayerWithName("Alice")->selection();
  ids.clear();
  for (Object *member : group) ids.push_back(member->getID());
  bar.update();
 }
 const ControlBarButton *button(int slot)
 {
  for (const auto &b : bar.palantirButtons()) if (b.slot == slot) return &b;
  return nullptr;
 }
 AutoPickUpUpdate *pickup(Object *o, int n)
 {
  for (const auto &m : o->modules())
   if (auto *p = dynamic_cast<AutoPickUpUpdate *>(m.get())) if (p->getRunFromButtonNumber() == n) return p;
  return nullptr;
 }
};
}

TEST_CASE("PORT-137 availability follows weapon condition and contain")
{
 Rig r;
 Object *carrier = r.make("TestCarrier", 100);
 REQUIRE(carrier);
 r.select(carrier, {carrier});
 REQUIRE(r.button(0));
 CHECK(r.button(0)->state == ButtonState::Enabled); // RW 0x942FC6: empty non-horde contain
 CHECK(r.button(1)->state == ButtonState::Enabled); // neighbour: same RW contain rule
 carrier->setWeaponSetFlag(WeaponSetToggle::WEAPONSET_TOGGLE_1, true);
 r.bar.update();
 REQUIRE(r.button(0));
 CHECK(r.button(0)->state == ButtonState::Restricted); // RW 0x942490, rock model condition
 carrier->setWeaponSetFlag(WeaponSetToggle::WEAPONSET_TOGGLE_1, false);
 r.bar.update();
 REQUIRE(r.button(0));
 CHECK(r.button(0)->state == ButtonState::Enabled); // RW 0x691106 clears model condition
 Object *rider = r.make("TestPlain", 120);
 REQUIRE(carrier->getContain());
 REQUIRE(carrier->getContain()->addToContain(rider));
 r.bar.update();
 REQUIRE(r.button(0));
 CHECK(r.button(0)->state == ButtonState::Restricted); // RW 0x942FEB
 CHECK(r.button(1)->state == ButtonState::Restricted);
 Object *plain = r.make("TestPlain", 140);
 r.select(plain, {plain});
 REQUIRE(r.button(0));
 CHECK(r.button(0)->state == ButtonState::Enabled); // RW 0x942FD0: no contain
}

TEST_CASE("PORT-137 press sends one numbered wake to selected matching modules")
{
 Rig r;
 Object *a = r.make("TestCarrier", 100);
 Object *b = r.make("TestCarrier", 150);
 Object *outside = r.make("TestCarrier", 200);
 REQUIRE(a); REQUIRE(b); REQUIRE(outside);
 r.select(a, {a,b});
 auto *a2 = r.pickup(a,2); auto *a3 = r.pickup(a,3);
 auto *b2 = r.pickup(b,2); auto *outside2 = r.pickup(outside,2);
 REQUIRE(a2); REQUIRE(a3); REQUIRE(b2); REQUIRE(outside2);
 CHECK_FALSE(a2->buttonWake()); CHECK_FALSE(b2->buttonWake());
 REQUIRE(r.bar.pressButton(0,true));
 r.input.flush();
 REQUIRE(r.sent.size() == 1); // RW 0x9412F8: exactly one message per press
 CHECK(r.sent[0].getType() == MSG_WAKE_AUTO_PICKUP);
 REQUIRE(r.sent[0].getArgumentCount() == 1);
 CHECK(r.sent[0].getArgument(0)->integer == 2); // RW 0x941318
 r.world.dispatcher.dispatch(r.sent[0]); // RW 0x77B51F / 0x76FB3F
 CHECK(a2->buttonWake()); CHECK(a2->buttonWoken());
 CHECK(b2->buttonWake()); CHECK(b2->buttonWoken());
 CHECK_FALSE(a3->buttonWake()); CHECK_FALSE(outside2->buttonWake()); // RW 0x68C900
}

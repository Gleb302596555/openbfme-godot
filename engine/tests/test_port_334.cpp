// PORT-334: retail RotWK 2.01 update branches, synthetic INI only.
// Expectations: RW 0x89958F (AutoThrowObject), RW 0x89EC43 -> RW 0x89EB99
// (OCL countdown is gated by UNDER_CONSTRUCTION); native snapshots stay D3/S-4190.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameClient/BeaconClientUpdate.h"
#include "GameLogic/Module/AutoPickUpUpdate.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/AISpecialPowerUpdate.h"
#include "GameLogic/Module/EntEnragedUpdate.h"
#include "GameLogic/Module/AttachUpdate.h"
#include "GameLogic/Module/GateModules.h"
#include "GameLogic/Module/ProductionExitModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"

using namespace combattest;
namespace
{
const char kObjects[] = R"ini(
Weapon Port334Throw
 AttackRange = 300
 DelayBetweenShots = 0
 DamageNugget
  Damage = 20
  Radius = 0
  DamageType = MAGIC
  DeathType = NORMAL
 End
End
Object Port334Holder
 KindOf = INFANTRY SELECTABLE
 TransportSlotCount = 1
 VisionRange = 200
 Geometry = CYLINDER
 GeometryMajorRadius = 1
 GeometryHeight = 10
 Body = ActiveBody BodyTag
  MaxHealth = 1000
 End
 WeaponSet
  Conditions = None
  Weapon = PRIMARY Port334Throw
 End
 Behavior = TransportContain ContainTag
  Slots = 10
  AllowAlliesInside = Yes
  PassengerFilter = ANY +INFANTRY
 End
End
Object Port334Victim
 KindOf = INFANTRY SELECTABLE
 TransportSlotCount = 1
 ArmorSet
  Conditions = None
  Armor = NoArmor
 End
 Geometry = CYLINDER
 GeometryMajorRadius = 1
 GeometryHeight = 10
 Body = ActiveBody BodyTag
  MaxHealth = 1000
 End
End
)ini";
}

TEST_CASE("PORT-334 AutoThrow: a contained thrower with one passenger fires at closest visible enemy")
{
 for (int scenario = 0; scenario < 7; ++scenario)
 {
  CAPTURE(scenario);
  CombatWorld w;
  REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
  w.combat().setAutoAcquireEnabled(false);
  w.logic->partition().setRegion(0, 0, 1000, 1000); // RW 0x62FCCD: a map-backed scan, not the zero-extent world
  Object *carrier = w.unit("Port334Holder", 'A', 200, 200);
  Object *thrower = w.unit("Port334Holder", 'A', 200, 200);
  Object *passenger = w.unit("Port334Victim", 'A', 200, 200);
  Object *near = w.unit("Port334Victim", 'B', 240, 200);
  Object *far = w.unit("Port334Victim", 'B', 280, 200);
  REQUIRE(carrier); REQUIRE(thrower); REQUIRE(passenger); REQUIRE(near); REQUIRE(far);
  w.frames(2); // publish placed objects to the partition and finish the weapon's initial reload
  auto *outer = dynamic_cast<TransportContain *>(carrier->getContain());
  auto *inner = dynamic_cast<TransportContain *>(thrower->getContain());
  REQUIRE(outer); REQUIRE(inner);
  if (scenario != 1) REQUIRE(outer->addToContain(thrower));
  if (scenario != 2) REQUIRE(inner->addToContain(passenger));
  if (scenario == 5)
  {
   Object *second = w.unit("Port334Victim", 'A', 200, 200);
   REQUIRE(second);
   REQUIRE(inner->addToContain(second));
  }
  const ObjectID nearId = near->getID();
  if (scenario == 6) near->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
  AutoPickUpUpdateModuleData data;
  data.m_autoThrowObject = scenario != 3;
  data.m_scanDelayTime = 0;
  data.m_scanDistance = 1; // RW uses VisionRange, not this pickup radius
  if (scenario == 4)
  {
   Coord3D out{500, 200, 0}; near->setPosition(&out); far->setPosition(&out);
  }
  AutoPickUpUpdate update(thrower, &data);
  const float before = w.byId(nearId) ? w.health(w.byId(nearId)) : 0.0f;
  CHECK(update.update() == UPDATE_SLEEP_NONE);
  // RW 0x89960F / 0x89962C / 0x8996DE: contained, count == 1, vision range.
  const float after = w.byId(nearId) ? w.health(w.byId(nearId)) : 0.0f;
  CHECK(after == before - (scenario == 0 ? 20.0f : 0.0f));
  CHECK(w.health(far) == (scenario == 6 ? 980.0f : 1000.0f));
 }
}

TEST_CASE("PORT-334 OCL: construction blocks initial scheduling and due creation without consuming RNG")
{
 CombatWorld w;
 REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Victim", 'A', 200, 200);
 REQUIRE(obj);
 OCLUpdateModuleData data;
 data.m_minDelay = data.m_maxDelay = 1;
 data.m_amount = 1;
 OCLUpdate update(obj, &data);
 obj->setStatus(2, true); // RW 0x89EBAC: UNDER_CONSTRUCTION
 const auto before = w.logic->random().seedArray();
 CHECK(update.update() == UPDATE_SLEEP_NONE);
 CHECK(w.logic->random().seedArray() == before);
 CHECK(update.nextFrame() == 0); // RW 0x89EC59 skips schedule entirely while building
 CHECK(update.created() == 0);
 CHECK(update.getCountdownPercent() == 0.0f);
 obj->setStatus(2, false);
 CHECK(update.update() == UPDATE_SLEEP_NONE); // neighbour: first completed update only schedules
 CHECK(update.getCountdownPercent() == 0.0f);
 w.frames(2);
 obj->setStatus(2, true);
 const auto due = w.logic->random().seedArray();
 CHECK(update.update() == UPDATE_SLEEP_NONE);
 CHECK(w.logic->random().seedArray() == due);
 CHECK(update.nextFrame() == 1);
 CHECK(update.created() == 0);
 obj->setStatus(2, false);
 CHECK(update.update() == UPDATE_SLEEP_FOREVER); // RW 0x89ECC5: Amount reached only after construction
 CHECK(update.created() == 1);
 CHECK(update.getCountdownPercent() == 0.0f); // resumed: schedule advanced to now + 1
}

// RW 0x8CD9D2 constructor + RW 0x8CDCE9 clientUpdate: strict deadline from creation.
TEST_CASE("PORT-334 Beacon: delayed first presentation does not postpone the first pulse")
{
 BeaconClientUpdateModuleData data;
 data.m_framesBetweenRadarPulses = 30;
 BeaconClientUpdate update(nullptr, &data);
 update.setCreationFrame(100);
 CHECK_FALSE(update.pulseDue(130));
 CHECK(update.pulseDue(131));
 CHECK_FALSE(update.pulseDue(161));
 CHECK(update.pulseDue(162));
 update.hideBeacon();
 CHECK_FALSE(update.pulseDue(200));
}

// RW 0x8B73B7: init/inactive/no-skirmish-AI and defeat gates precede decision RNG.
TEST_CASE("PORT-334 AISpecialPower: inactive sleeps forever without consuming decision RNG")
{
 CombatWorld w;
 REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Victim", 'A', 200, 200);
 REQUIRE(obj);
 AISpecialPowerUpdateModuleData data;
 data.m_commandButtonName = "Port334MissingButton";
 data.m_specialPowerAIType = 0;
 AISpecialPowerUpdate update(obj, &data);
 const auto before = w.logic->random().seedArray();
 CHECK(update.update() == UPDATE_SLEEP_FOREVER);
 CHECK(update.initialized());
 CHECK_FALSE(update.active());
 CHECK(update.decision() == nullptr);
 CHECK(w.logic->random().seedArray() == before);
}

// RW 0x8B6866: forced rage ignores cooldown, suppression overrides force.
TEST_CASE("PORT-334 EntEnraged: suppression overrides forced rage and clears its status")
{
 CombatWorld w;
 REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Victim", 'A', 200, 200);
 REQUIRE(obj);
 EntEnragedUpdateModuleData data;
 data.m_enragedTime = 100;
 EntEnragedUpdate update(obj, &data);
 update.setForced(true, false);
 CHECK(update.update() == UPDATE_SLEEP_NONE);
 CHECK(obj->testStatus(0x36));
 update.setSuppressed(true);
 CHECK(update.update() == UPDATE_SLEEP_NONE);
 CHECK_FALSE(obj->testStatus(0x36));
}

// RW 0x8953BB: no parent -> next frame, not permanent sleep.
TEST_CASE("PORT-334 Attach: no matching parent keeps scanning without moving")
{
 CombatWorld w;
 const std::string objects = std::string(kObjects) +
  "ChildObject Port334Attachment Port334Victim\n"
  " Behavior = AttachUpdate AttachTag\n ScanRange = 1\n ObjectFilter = NONE\n End\nEnd\n";
 REQUIRE(w.w.load(objects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Attachment", 'A', 200, 200);
 REQUIRE(obj);
 // Attach's ctor wakes its scheduler slot; it must be created as an owned module,
 // not as a detached stack module on an already registered Object.
 auto *update = dynamic_cast<AttachUpdate *>(obj->findModule("AttachUpdate"));
 REQUIRE(update);
 CHECK(update->update() == UPDATE_SLEEP_NONE);
 CHECK(obj->getPosition()->x == 200);
 CHECK(obj->getPosition()->y == 200);
}

// RW 0x8B4D34: a human's gate without a skirmish AI sleeps forever.
TEST_CASE("PORT-334 AIGate: no AI and no gate interface sleeps forever")
{
 CombatWorld w;
 REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Victim", 'A', 200, 200);
 REQUIRE(obj);
 AIGateUpdateModuleData data;
 AIGateUpdate update(obj, &data);
 CHECK(update.update() == UPDATE_SLEEP_FOREVER);
}

// RW 0x88B211: SupplyCenterProductionExitUpdate's actual update is only sleep forever.
TEST_CASE("PORT-334 SupplyCenter: production exit has no polling update")
{
 CombatWorld w;
 REQUIRE(w.w.load(kObjects, INI_LOAD_OVERWRITE, "port334.ini").empty());
 Object *obj = w.unit("Port334Victim", 'A', 200, 200);
 REQUIRE(obj);
 DefaultProductionExitUpdateModuleData data;
 SupplyCenterProductionExitUpdate update(obj, &data);
 CHECK(update.update() == UPDATE_SLEEP_FOREVER);
}

// OpenBFME unit tests. PORT-5: synthetic, no game installation. GPL-3.0.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Module/GiantBirdAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/XferSave.h"
#include "Common/XferLoad.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SlaveWatcherBehavior.h"

// The weapon-delivery slot, called directly to exercise the RW 0x910EA1 path without claiming that a particular game setup can fire it.
namespace SlaveAttack { void deliver(GameLogic &logic, ObjectID ownerId, bool atVictim, const Coord3D &pos); }

namespace
{
const char kBirdAndTarget[] = R"ini(
Object Port5Bird
 KindOf = MONSTER SELECTABLE CAN_ATTACK GIANT_BIRD
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
 End
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 30
 End
End
Object Port5Target
 KindOf = IMMOBILE
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 100
 End
End
)ini";
constexpr const char *kMissingTarget = "[PORT-5] missing GiantBird ability target: command cancelled (INFERENCE, RW 0x76DAF4; owner decision #57)";
}

TEST_CASE("PORT-5: deleted deferred GiantBird target cancels once and is counted in the logic report")
{
 combattest::CombatWorld w;
 const std::string error = w.w.load(kBirdAndTarget, INI_LOAD_OVERWRITE, "port5-bird.ini");
 REQUIRE_MESSAGE(error.empty(), error);
 Object *bird = w.unit("Port5Bird", 'A', 200.0f, 200.0f);
 Object *victim = w.unit("Port5Target", 'B', 300.0f, 200.0f);
 REQUIRE(bird);
 REQUIRE(victim);
 auto *ai = dynamic_cast<GiantBirdAIUpdate *>(bird->getAIUpdateInterface());
 REQUIRE(ai);
 w.frames(1);
 // RW 0x75307D / 0x76CE37: direct synthetic gate; reachability in gameplay is not proven.
 ai->setAttacking(true);
 REQUIRE(ai->aiAbilityMoveToObject(victim, CMD_FROM_AI));
 const unsigned before = ai->currentStateId();
 const ObjectID victimID = victim->getID();
 w.logic->destroyObject(victim);
 w.logic->processDestroyList(); // RW 0x62902E removes the target ID before the reissued command
 REQUIRE(w.logic->findObjectByID(victimID) == nullptr);
 CHECK(w.logic->report().stopHits.count(kMissingTarget) == 0);
 ai->setAttacking(false);
 ai->update();
 CHECK(ai->currentStateId() == before);
 const auto report = w.logic->report();
 REQUIRE(report.stopHits.count(kMissingTarget) == 1);
 CHECK(report.stopHits.at(kMissingTarget) == 1);
 ai->update();
 CHECK(w.logic->report().stopHits.at(kMissingTarget) == 1);
}

namespace
{
const char kChargeFixture[] = R"ini(
SpecialPower Port5Charge
 Enum = SPECIAL_CHARGE_ATTACK
 ReloadTime = 0
End
Locomotor Port5Loco
 Surfaces = GROUND
 TurnTime = 500
 Acceleration = 510
 Braking = 510
 MinSpeed = 0
 ZAxisBehavior = NO_Z_MOTIVE_FORCE
 Appearance = TWO_LEGS
 ChargeSpeed = 200%
 ChargeAvailable = No
End
Object Port5Charger
 KindOf = MONSTER SELECTABLE CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 LocomotorSet
  Locomotor = Port5Loco
  Condition = SET_NORMAL
  Speed = 30
 End
 Behavior = SpecialAbilityUpdate ModuleTag_Ability
  SpecialPowerTemplate = Port5Charge
  ChargeAttackSpeedBoost = Yes
  IgnoreFacingCheck = Yes
  PackTime = 2000
 End
End
Object Port5NoBoost
 KindOf = MONSTER SELECTABLE CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 LocomotorSet
  Locomotor = Port5Loco
  Condition = SET_NORMAL
  Speed = 30
 End
 Behavior = SpecialAbilityUpdate ModuleTag_Ability
  SpecialPowerTemplate = Port5Charge
  ChargeAttackSpeedBoost = No
  IgnoreFacingCheck = Yes
  PackTime = 2000
 End
End
Object Port5Ordinary
 KindOf = MONSTER SELECTABLE CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 LocomotorSet
  Locomotor = Port5Loco
  Condition = SET_NORMAL
  Speed = 30
 End
End
)ini";
}

TEST_CASE("PORT-5: charge ability trigger, actual locomotor host and ability snapshot")
{
 combattest::CombatWorld w;
 SpecialPowerStore powers;
 SpecialPowerStore *old = TheSpecialPowerStore;
 struct Restore { SpecialPowerStore *p; ~Restore() { TheSpecialPowerStore = p; } } restore{old};
 TheSpecialPowerStore = &powers;
 w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
 const std::string error = w.w.load(kChargeFixture, INI_LOAD_OVERWRITE, "port5-charge.ini");
 REQUIRE_MESSAGE(error.empty(), error);
 Object *charger = w.unit("Port5Charger", 'A', 200.0f, 200.0f);
 Object *ordinary = w.unit("Port5Ordinary", 'A', 400.0f, 200.0f);
 Object *noBoost = w.unit("Port5NoBoost", 'A', 600.0f, 200.0f);
 REQUIRE(charger);
 REQUIRE(ordinary);
 REQUIRE(noBoost);
 w.frames(1);
 auto *ai = charger->getAIUpdateInterface();
 auto *other = ordinary->getAIUpdateInterface();
 REQUIRE(ai);
 REQUIRE(other);
 REQUIRE(ai->locomotor());
 REQUIRE(other->locomotor());
 SpecialAbilityUpdate *ability = nullptr;
 for (const auto &module : charger->modules())
  if (auto *a = dynamic_cast<SpecialAbilityUpdate *>(module.get())) ability = a;
 REQUIRE(ability);
 CHECK(charger->hasChargeAttackAbility());
 CHECK_FALSE(ordinary->hasChargeAttackAbility());
 CHECK(noBoost->hasChargeAttackAbility());
 SpecialAbilityUpdate *noBoostAbility = nullptr;
 for (const auto &module : noBoost->modules())
  if (auto *a = dynamic_cast<SpecialAbilityUpdate *>(module.get())) noBoostAbility = a;
 REQUIRE(noBoostAbility);
 noBoostAbility->initiateIntentToDoSpecialPower(noBoostAbility->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
 noBoostAbility->update();
 CHECK(noBoostAbility->abilitiesTriggered() == 1);
 CHECK_FALSE(noBoost->testModelCondition(0x84));
 REQUIRE(noBoost->getAIUpdateInterface()->locomotor());
 CHECK(noBoost->getAIUpdateInterface()->locomotor()->getMaxSpeedForCondition(noBoost->getAIUpdateInterface()->locomotorHost()) == doctest::Approx(6.0)); // tolerance: 0.2 seconds per frame is not exact in float32
 const float normal = ai->locomotor()->getMaxSpeedForCondition(ai->locomotorHost());
 CHECK(normal == doctest::Approx(6.0)); // tolerance: 0.2 seconds per frame is not exact in float32
 ordinary->setModelConditionState(0x84, true);
 CHECK(other->locomotor()->getMaxSpeedForCondition(other->locomotorHost()) == doctest::Approx(normal)); // tolerance: float32 speed conversion
 ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, nullptr, nullptr, 0, 0);
 ability->update();
 CHECK(ability->abilitiesTriggered() == 1);
 CHECK(charger->testModelCondition(0x84));
 CHECK(ai->locomotor()->getMaxSpeedForCondition(ai->locomotorHost()) == doctest::Approx(12.0)); // tolerance: float32 speed conversion
 XferSave saved;
 ability->xfer(saved);
 StateHasher before;
 ability->crc(before);
 ability->finishAbility(false, true);
 CHECK_FALSE(charger->testModelCondition(0x84));
 CHECK(ai->locomotor()->getMaxSpeedForCondition(ai->locomotorHost()) == doctest::Approx(normal)); // tolerance: float32 speed conversion
 XferLoad load(saved.data());
 ability->xfer(load);
 CHECK(load.atEnd());
 StateHasher after;
 ability->crc(after);
 CHECK(after.value() == before.value());
 // The model condition belongs to Object, whose snapshot is separate from this module's snapshot.
 charger->setModelConditionState(0x84, true);
 CHECK(ai->locomotor()->getMaxSpeedForCondition(ai->locomotorHost()) == doctest::Approx(12.0)); // tolerance: float32 speed conversion
 ability->finishAbility(false, true);
 CHECK_FALSE(charger->testModelCondition(0x84));

 // Exercise real base AI command 0x48 and restore its approach state (the path hook is rebound on load).
 REQUIRE(ai->aiAbilityApproachObject(ordinary, CMD_FROM_AI));
 CHECK(ai->aiStateType() == AI_ABILITY_APPROACH);
 CHECK(ai->stateMachine().goalObjectID() == ordinary->getID());
 XferSave aiSaved;
 ai->xferAIState(aiSaved);
 StateHasher aiBefore;
 ai->crc(aiBefore);
 ai->aiIdle(CMD_FROM_AI);
 XferLoad aiLoad(aiSaved.data());
 ai->xferAIState(aiLoad);
 CHECK(aiLoad.atEnd());
 CHECK(ai->aiStateType() == AI_ABILITY_APPROACH);
 CHECK(ai->stateMachine().goalObjectID() == ordinary->getID());
 StateHasher aiAfter;
 ai->crc(aiAfter);
 CHECK(aiAfter.value() == aiBefore.value());
 w.frames(2);
}

TEST_CASE("PORT-5: synthetic slave attack during preparation defers a charge target that is later deleted")
{
 combattest::CombatWorld w;
 SpecialPowerStore powers;
 SpecialPowerStore *old = TheSpecialPowerStore;
 struct Restore { SpecialPowerStore *p; ~Restore() { TheSpecialPowerStore = p; } } restore{old};
 TheSpecialPowerStore = &powers;
 w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
 const char fixture[] = R"ini(
SpecialPower Port5FlightCharge
 Enum = SPECIAL_CHARGE_ATTACK
 ReloadTime = 0
End
Locomotor Port5FlightLoco
 Surfaces = AIR
 Appearance = GIANT_BIRD
 TurnTime = 1000
 Acceleration = 1000
 Braking = 1000
 PreferredHeight = 60
 ZAxisBehavior = SURFACE_RELATIVE_HEIGHT
End
Object Port5FlightBird
 KindOf = MONSTER SELECTABLE CAN_ATTACK GIANT_BIRD
 Geometry = CYLINDER
 GeometryMajorRadius = 10
 GeometryHeight = 30
 WeaponSet
  Conditions = None
  Weapon = PRIMARY BowWeapon
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = GiantBirdAIUpdate ModuleTag_AI
 End
 LocomotorSet
  Locomotor = Port5FlightLoco
  Condition = SET_NORMAL
  Speed = 120
 End
 Behavior = SpecialAbilityUpdate ModuleTag_Ability
  SpecialPowerTemplate = Port5FlightCharge
  ChargeAttackSpeedBoost = Yes
  IgnoreFacingCheck = Yes
  StartAbilityRange = 500
  AbilityAbortRange = 1000
  PackTime = 2000
  PreparationTime = 2200
 End
End
Object Port5FlightMaster
 KindOf = SELECTABLE
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 1000
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 Behavior = SlaveWatcherBehavior ModuleTag_Watcher
  LetSlaveLive = Yes
 End
End
)ini";
 const std::string error = w.w.load(fixture, INI_LOAD_OVERWRITE, "port5-flight.ini");
 REQUIRE_MESSAGE(error.empty(), error);
 Object *birdObj = w.unit("Port5FlightBird", 'A', 200, 200);
 Object *attackVictim = w.unit("Swordsman", 'B', 300, 200);
 Object *abilityTarget = w.unit("Swordsman", 'B', 400, 300);
 Object *master = w.unit("Port5FlightMaster", 'A', 100, 100);
 REQUIRE(birdObj);
 REQUIRE(attackVictim);
 REQUIRE(abilityTarget);
 REQUIRE(master);
 auto *bird = dynamic_cast<GiantBirdAIUpdate *>(birdObj->getAIUpdateInterface());
 auto *watcher = SlaveWatcherBehavior::of(*master);
 REQUIRE(bird);
 REQUIRE(watcher);
 watcher->setSlave(birdObj->getID());
 SpecialAbilityUpdate *ability = nullptr;
 for (const auto &module : birdObj->modules())
  if (auto *a = dynamic_cast<SpecialAbilityUpdate *>(module.get())) ability = a;
 REQUIRE(ability);
 w.combat().setAutoAcquireEnabled(false);
 w.frames(1);
 ability->initiateIntentToDoSpecialPower(ability->abilityData()->m_specialPowerTemplate, abilityTarget, nullptr, 0, 0);
 w.frames(1);
 REQUIRE(ability->prepFrames() != 0); // tolerance: require the ability to be in its nonzero preparation window
 CHECK_FALSE(bird->isStateActive());
 master->getAIUpdateInterface()->setCurrentVictim(attackVictim);
 SlaveAttack::deliver(*w.logic, master->getID(), true, Coord3D{}); // RW 0x910EA1: no forced m_attacking
 CHECK(bird->swoopsStarted() == 1);
 for (int frame = 0; frame < 100 && ability->abilitiesTriggered() == 0; ++frame) w.frames(1);
 REQUIRE(ability->abilitiesTriggered() == 1);
 REQUIRE(bird->currentStateId() == GiantBirdAIUpdate::STATE_ATTACK);
 const ObjectID targetId = abilityTarget->getID();
 w.logic->destroyObject(abilityTarget);
 w.logic->processDestroyList();
 REQUIRE(w.logic->findObjectByID(targetId) == nullptr);
 for (int frame = 0; frame < 200 && w.logic->report().stopHits.count(kMissingTarget) == 0; ++frame) w.frames(1);
 const auto report = w.logic->report();
 REQUIRE(report.stopHits.count(kMissingTarget) == 1);
 CHECK(report.stopHits.at(kMissingTarget) == 1);
 // This is an OpenBFME synthetic path, not proof of retail or Ennorath reachability.
}

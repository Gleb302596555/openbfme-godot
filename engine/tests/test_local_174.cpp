// OpenBFME. GPL-3.0.
// LOCAL-174 (fork issue 955):
//   - Port of SimpleObjectIterator::sort (RW 0x952EE1) cases 3 & 4
//     (ITER_SORTED_CHEAP_TO_EXPENSIVE RW 0x950624, ITER_SORTED_EXPENSIVE_TO_CHEAP RW 0x95064B,
//     comparing unsigned 16-bit ThingTemplate + 0x5EA BuildCost).
//   - Binary-map audit of 447 uncited RotWK 2.01 object-module functions (0x8AAD6F..0xBC9F9D).

#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Module.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

TEST_CASE("LOCAL-174 SimpleObjectIterator::sort cheap/expensive modes (RW 0x952EE1)")
{
	// Expectations from RW 0x952EE1 (SimpleObjectIterator::sort), RW 0x950624 (ascending BuildCost +0x5EA),
	// and RW 0x95064B (descending BuildCost +0x5EA).
	logictest::LogicWorld lw;
	const std::string err = lw.w.load(R"(
Object CheapUnit
  BuildCost = 100
  Geometry = CYLINDER
  GeometryMajorRadius = 5.0
  GeometryHeight = 10.0
End
Object MidUnit
  BuildCost = 400
  Geometry = CYLINDER
  GeometryMajorRadius = 5.0
  GeometryHeight = 10.0
End
Object ExpensiveUnit
  BuildCost = 1200
  Geometry = CYLINDER
  GeometryMajorRadius = 5.0
  GeometryHeight = 10.0
End
)", INI_LOAD_OVERWRITE, "local174.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	lw.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);

	Object *mid = lw.make("MidUnit");
	Object *expensive = lw.make("ExpensiveUnit");
	Object *cheap = lw.make("CheapUnit");
	REQUIRE(mid != nullptr);
	REQUIRE(expensive != nullptr);
	REQUIRE(cheap != nullptr);

	const Coord3D pMid{ 110.0f, 100.0f, 0.0f };
	const Coord3D pExpensive{ 120.0f, 100.0f, 0.0f };
	const Coord3D pCheap{ 130.0f, 100.0f, 0.0f };
	mid->setPosition(&pMid);
	expensive->setPosition(&pExpensive);
	cheap->setPosition(&pCheap);

	const Coord3D origin{ 100.0f, 100.0f, 0.0f };

	// Case 3: ITER_SORTED_CHEAP_TO_EXPENSIVE (RW 0x952EE1 -> 0x952DF7 -> 0x950624)
	const PartitionHits cheapFirst = lw.logic->partition().iterateObjectsInRange(
		origin, 100.0f, FROM_CENTER_2D, PartitionFilterList{}, ITER_SORTED_CHEAP_TO_EXPENSIVE);
	REQUIRE(cheapFirst.size() == 3);
	CHECK(cheapFirst[0].object == cheap);
	CHECK(cheapFirst[1].object == mid);
	CHECK(cheapFirst[2].object == expensive);

	// Case 4: ITER_SORTED_EXPENSIVE_TO_CHEAP (RW 0x952EE1 -> 0x952E6C -> 0x95064B)
	const PartitionHits expensiveFirst = lw.logic->partition().iterateObjectsInRange(
		origin, 100.0f, FROM_CENTER_2D, PartitionFilterList{}, ITER_SORTED_EXPENSIVE_TO_CHEAP);
	REQUIRE(expensiveFirst.size() == 3);
	CHECK(expensiveFirst[0].object == expensive);
	CHECK(expensiveFirst[1].object == mid);
	CHECK(expensiveFirst[2].object == cheap);

	// Neighbor check: distance sorting (cases 1 & 2) stays ordered by distSqr, independent of BuildCost.
	const PartitionHits nearFirst = lw.logic->partition().iterateObjectsInRange(
		origin, 100.0f, FROM_CENTER_2D, PartitionFilterList{}, ITER_SORTED_NEAR_TO_FAR);
	REQUIRE(nearFirst.size() == 3);
	CHECK(nearFirst[0].object == mid);
	CHECK(nearFirst[1].object == expensive);
	CHECK(nearFirst[2].object == cheap);

	const PartitionHits farFirst = lw.logic->partition().iterateObjectsInRange(
		origin, 100.0f, FROM_CENTER_2D, PartitionFilterList{}, ITER_SORTED_FAR_TO_NEAR);
	REQUIRE(farFirst.size() == 3);
	CHECK(farFirst[0].object == cheap);
	CHECK(farFirst[1].object == expensive);
	CHECK(farFirst[2].object == mid);
}

TEST_CASE("LOCAL-174 cited RotWK modules are registered by ModuleFactory (RW 0x8B75D1..0x8E36FC)")
{
	// Expectations from RW 0x8B75D1..0x8E36FC: every Ennorath-used module cited by Local 174 is registered.
	logictest::LogicWorld lw;
	const char *const citedBehaviorModules[] = {
		"ArmorUpgrade",
		"AttributeModifierUpgrade",
		"BuildableHeroListUpgrade",
		"CastleUpgrade",
		"CommandPointsUpgrade",
		"CommandSetUpgrade",
		"CostModifierUpgrade",
		"DelayedLuaEventUpdate",
		"DetachableRiderUpdate",
		"EmotionTrackerUpdate",
		"GeometryUpgrade",
		"GrantUpgradeCreate",
		"InheritUpgradeCreate",
		"InvisibilitySpecialPower",
		"LevelUpUpgrade",
		"LocomotorSetUpgrade",
		"ModelConditionUpgrade",
		"ObjectCreationUpgrade",
		"PartTheHeavensUpdate",
		"RainOfFireUpdate",
		"RemoveUpgradeUpgrade",
		"ReplaceObjectUpdate",
		"RespawnUpdate",
		"SalvageCrateCollide",
		"SpawnUnitBehavior",
		"SpecialDisguiseUpdate",
		"SpellRechargeModifierUpgrade",
		"StatusBitsUpgrade",
		"SubObjectsUpgrade",
		"SupplyWarehouseCreate",
		"ToggleDeploySpecialAbilityUpdate",
		"ToggleMountedSpecialAbilityUpdate",
		"VeterancyCrateCollide",
		"WeaponSetUpgrade",
	};
	for (const char *name : citedBehaviorModules)
	{
		CAPTURE(name);
		CHECK(lw.w.modules.findModuleTemplate(name, MODULETYPE_BEHAVIOR) != nullptr);
	}
}

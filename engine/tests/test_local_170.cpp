// OpenBFME. GPL-3.0.
// LOCAL-170 (fork issue 937):
//   - Binary-map audit of 300 uncited RotWK 2.01 object-module and engine functions.
//   - Verifies ported counterparts cited by LOCAL-170:
//     * W3DModuleFactory::init (RW 0x464AC1) registers cited module classes.
//     * PartitionManager / ShroudManager thunks (RW 0xA39040, RW 0xA39070, RW 0xB4D8F0, RW 0xB4D960).

#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Module.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/System/ShroudManager.h"

TEST_CASE("LOCAL-170 ModuleFactory: cited RotWK modules are registered by init (RW 0x464AC1)")
{
	// Expectations from RW 0x464AC1 (W3DModuleFactory::init -> 0x6579C9) and the 29 cited buildFieldParse tables.
	logictest::LogicWorld lw;

	const char *const citedBehaviorModules[] = {
		"StealthUpdate",
		"PhysicsBehavior",
		"LifetimeUpdate",
		"StrafeAreaUpdate",
		"ThreatFinderUpdate",
		"BloodthirstyUpdate",
		"SpecialAbilityUpdate",
		"WallHubBehavior",
		"GettingBuiltBehavior",
		"BuildingBehavior",
		"HitReactionBehavior",
		"ClickReactionBehavior",
		"AutoAbilityBehavior",
		"AimWeaponBehavior",
		"BezierProjectileBehavior",
		"StancesBehavior",
		"RunOffMapBehavior",
		"ShareExperienceBehavior",
		"TerrainResourceBehavior",
		"RebuildHoleBehavior",
		"SupplyWarehouseCripplingBehavior",
		"SlaveWatcherBehavior",
		"PassiveAreaEffectBehavior",
		"SlowDeathBehavior",
		"StructureCollapseUpdate",
		"StructureToppleUpdate",
		"SupplyCenterDockUpdate",
		"GateOpenAndCloseBehavior",
		"FadeAndDieOrnamentUpdate",
		"GiveUpgradeUpdate",
		"OpenContain",
	};

	for (const char *name : citedBehaviorModules)
	{
		CAPTURE(name);
		CHECK(lw.w.modules.findModuleTemplate(name, MODULETYPE_BEHAVIOR) != nullptr);
	}
	CHECK(lw.w.modules.findModuleTemplate("RandomSoundSelectorClientBehavior", MODULETYPE_CLIENT_BEHAVIOR) != nullptr);
}

TEST_CASE("LOCAL-170 ShroudManager: setRegion and permanent reveal undo round-trip (RW 0xB4D8F0, RW 0xB4D960)")
{
	// Expectations from RW 0xB4D8F0 (-> 0xB520F0 setRegion) and RW 0xB4D960 (-> 0xB519B0 undoRevealMapForPlayerPermanently).
	logictest::LogicWorld lw;
	ShroudManager shroud(*lw.logic);
	shroud.setExtent(0.0f, 0.0f, 200.0f, 200.0f, 20.0f);
	CHECK(shroud.cellCountX() == 10);
	CHECK(shroud.cellCountY() == 10);

	CHECK(shroud.getStatusAt(0, 100.0f, 100.0f) == CELLSHROUD_SHROUDED);

	shroud.revealMapForPlayerPermanently(0);
	CHECK(shroud.getStatusAt(0, 100.0f, 100.0f) == CELLSHROUD_CLEAR);

	shroud.undoRevealMapForPlayerPermanently(0);
	CHECK(shroud.getStatusAt(0, 100.0f, 100.0f) == CELLSHROUD_FOGGED);
}

TEST_CASE("LOCAL-170 PartitionManager: setRegion initializes spatial grid (RW 0xA39040, RW 0xA39070)")
{
	// Expectations from RW 0xA39040 (-> 0xA3BBB0 quadtree init) and RW 0xA39070 (-> 0xA3BDB0 getClosestObject).
	logictest::LogicWorld lw;
	PartitionManager pm(*lw.logic);
	pm.setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	CHECK(pm.getClosestObject(Coord3D{500.0f, 500.0f, 0.0f}, 100.0f, FROM_CENTER_2D, PartitionFilterList{}, nullptr, nullptr) == nullptr);
}

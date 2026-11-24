// OpenBFME unit tests, Local 163: Pathfinder::ChooseBestLocomotorForPosition (RW 0x6E8937, table RW 0xDA2444).
// Synthetic INI data (no retail files). Expectations from RotWK 2.01 disassembly (RW 0x6E8937..0x6E89A7, RW 0xDA2444).
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"

#include <string>

namespace
{
const char kLocal163Ini[] = R"ini(
Locomotor L163DeepWater
  Surfaces = DEEP_WATER
  TurnTime = 500
  TurnTimeDamaged = 500
  Acceleration = 100
  Braking = 100
  MinSpeed = 0
  ZAxisBehavior = NO_Z_MOTIVE_FORCE
  Appearance = TWO_LEGS
End

Locomotor L163Ground
  Surfaces = GROUND
  TurnTime = 500
  TurnTimeDamaged = 500
  Acceleration = 100
  Braking = 100
  MinSpeed = 0
  ZAxisBehavior = NO_Z_MOTIVE_FORCE
  Appearance = TWO_LEGS
End

Object L163AmphibShip
  KindOf = SHIP SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 10
  GeometryHeight = 10
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  Behavior = PhysicsBehavior ModuleTag_Physics
  End
  LocomotorSet
    Locomotor = L163DeepWater
    Condition = SET_NORMAL
    Speed = 20
  End
  LocomotorSet
    Locomotor = L163Ground
    Condition = SET_NORMAL
    Speed = 20
  End
End
)ini";
} // namespace

// RW 0x6E8937 (0x6E899E xor eax, eax -> RW 0xDA2444[0] = 0x09 = GROUND|AIR):
// When getCell(layer, ix, iy) returns null (off-grid), retail treats the cell type as CELL_CLEAR (0)
// and picks a GROUND|AIR locomotor rather than skipping selection and falling back to SHIP's DEEP_WATER.
TEST_CASE("LOCAL-163 Pathfinder: ChooseBestLocomotorForPosition (RW 0x6E8937) falls back to CELL_CLEAR when off-grid")
{
	combattest::CombatWorld f;
	const std::string err = f.w.load(kLocal163Ini, INI_LOAD_OVERWRITE, "local163_loco.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	f.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);

	// Place the SHIP off the pathfinder grid (-500, -500) so getCell() is nullptr.
	Object *ship = f.unit("L163AmphibShip", 'A', -500.0f, -500.0f);
	REQUIRE(ship != nullptr);
	AIUpdateInterface *ai = ship->getAIUpdateInterface();
	REQUIRE(ai != nullptr);

	const Coord3D offGrid{ -500.0f, -500.0f, 0.0f };
	REQUIRE(f.ai->pathfinder().getCell(LAYER_GROUND, &offGrid) == nullptr);
	CHECK(f.ai->pathfinder().chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &offGrid) == 0x09u);

	const Locomotor *chosen = ai->curLocomotor();
	REQUIRE(chosen != nullptr);
	CHECK(chosen->getTemplate().m_name == "L163Ground");
}

// RW 0x6E8937 + RW 0xDA2444 surface table: CLEAR=0x09, WATER=0x0A, CLIFF=0x0C, RUBBLE=0x18, OBSTACLE=0x28, IMPASSABLE=0x48, DEEP_WATER=0x88.
TEST_CASE("LOCAL-163 Pathfinder: ChooseBestLocomotorForPosition (RW 0x6E8937) maps on-grid cell types via RW 0xDA2444")
{
	combattest::CombatWorld f;
	const std::string err = f.w.load(kLocal163Ini, INI_LOAD_OVERWRITE, "local163_cells.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	f.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);

	Pathfinder &pf = f.ai->pathfinder();
	const Coord3D pos{ 105.0f, 105.0f, 0.0f };
	PathfindCell *cell = pf.getCell(LAYER_GROUND, &pos);
	REQUIRE(cell != nullptr);

	cell->setType(PathfindCell::CELL_CLEAR);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x09u);
	cell->setType(PathfindCell::CELL_WATER);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x0Au);
	cell->setType(PathfindCell::CELL_CLIFF);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x0Cu);
	cell->setType(PathfindCell::CELL_RUBBLE);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x18u);
	cell->setType(PathfindCell::CELL_OBSTACLE);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x28u);
	cell->setType(PathfindCell::CELL_IMPASSABLE);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x48u);
	cell->setType(PathfindCell::CELL_DEEP_WATER);
	CHECK(pf.chooseBestLocomotorSurfacesForPosition(LAYER_GROUND, &pos) == 0x88u);
}

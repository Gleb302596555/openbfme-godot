// OpenBFME unit tests. GPL-3.0.
// LOCAL-190 (fork issue 983): FireLogicSystem fuel span paint (RW 0x6864EA / DONOR BFME2 0x00285DC5).
// Expectations come directly from RotWK 2.01 disassembly of RW 0x6864EA:
//   - RW 0x686532..0x686552: maxBurnRate clamped <= 0xfff, decay <= 0x3ff, resistance >= 0
//   - RW 0x68660E..0x686623: decay (m_flammability) raised when cell->m_flammability < decay
//   - RW 0x686626..0x686649: resistance lowered when cell->m_resistance > resistance (including 0)
//   - RW 0x68664C: cell->m_type reset to 0 (neutral)

#include "doctest.h"

#include "CombatTestUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/FireLogicSystem.h"

using namespace combattest;

namespace
{
const char kLocal190FireObjects[] =
	"Object DummyTarget\n"
	"  KindOf = STRUCTURE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 5\n"
	"End\n";
} // namespace

// RW 0x6864EA (0x68660E..0x68664C): changeFuelInArea raises decay, allows resistance=0, and resets burnt-out type to 0.
TEST_CASE("LOCAL-190 FireLogicSystem::changeFuelInArea RW 0x6864EA decay, zero resistance and type reset")
{
	CombatWorld w(kLocal190FireObjects, 50);
	FireLogicSystem &fire = w.logic->fireLogic();
	fire.newMap(0.0f, 0.0f, 200.0f, 200.0f);

	// 1. Decay update (RW 0x68660E..0x686623): grass default decay is 4; passing decay=50 raises it to 50.
	//    When ignited at burn rate 15, a full 10-frame slice cycle decays 15 - 50 <= 0 -> extinguishes.
	//    (Without the fix, decay < 4 was false so decay stayed 4 and burn rate remained 11.)
	const Coord3D decayPos{ 40.0f, 40.0f, 0.0f };
	fire.changeFuelInArea(decayPos, 4.0f, 10, 20, 50, 10, false);
	fire.changeBurnRateInArea(decayPos, 4.0f, 15, false);
	REQUIRE(fire.isCellBurning(4, 4));
	for (int i = 0; i < 10; ++i)
	{
		fire.update();
	}
	CHECK(!fire.isCellBurning(4, 4));

	// 2. Resistance=0 update (RW 0x686626..0x686649): grass default resistance is 10.
	//    Lowering neighbor (11, 10) resistance to 0 lets a fire of burn rate 6 at (10, 10) spread to (11, 10)
	//    because 6 > 0 (whereas 6 > 10 would not spread).
	const Coord3D neighborPos{ 110.0f, 100.0f, 0.0f };
	fire.changeFuelInArea(neighborPos, 4.0f, 10, 20, 4, 0, false);
	fire.changeBurnRate(10, 10, 10, 6, false);
	REQUIRE(fire.isCellBurning(10, 10));
	REQUIRE(!fire.isCellBurning(11, 10));
	for (int i = 0; i < 10; ++i)
	{
		fire.update();
	}
	CHECK(fire.isCellBurning(11, 10));

	// 3. Type reset (RW 0x68664C): burning out cell (8, 8) sets m_type = 2; refueling via changeFuelInArea resets m_type = 0.
	const Coord3D burnoutPos{ 80.0f, 80.0f, 0.0f };
	fire.changeFuelInArea(burnoutPos, 4.0f, -95, 20, 0, 10, false);
	fire.changeBurnRateInArea(burnoutPos, 4.0f, 10, false);
	for (int i = 0; i < 30; ++i)
	{
		fire.update();
	}
	REQUIRE(fire.cellTypeAt(80, 80) == 2);
	fire.changeFuelInArea(burnoutPos, 4.0f, 50, 20, 0, 10, false);
	CHECK(fire.cellTypeAt(80, 80) == 0);
}

// Neighboring behavior (RW 0x6864EA, 0x6865B7..0x6865C2): onlyBurning=true skips neutral cells and leaves fuel/type untouched.
TEST_CASE("LOCAL-190 FireLogicSystem::changeFuelInArea RW 0x6864EA onlyBurning skips unlit cells")
{
	CombatWorld w(kLocal190FireObjects, 50);
	FireLogicSystem &fire = w.logic->fireLogic();
	fire.newMap(0.0f, 0.0f, 200.0f, 200.0f);

	const Coord3D pos{ 60.0f, 60.0f, 0.0f };
	const int fuelBefore = fire.getCellFuel(6, 6);
	fire.changeFuelInArea(pos, 4.0f, 50, 200, 100, 0, true);
	CHECK(fire.getCellFuel(6, 6) == fuelBefore);
	CHECK(!fire.isCellBurning(6, 6));
}

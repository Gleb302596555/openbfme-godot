// OpenBFME unit tests for LOCAL-178 (fork issue 959): AI and pathfinding binary-map ports.
// Expectations come from RotWK 2.01 game.dat disassembly:
//   - RW 0x94E2E7: MoveToGroupOrder copy constructor initializes m_positions empty and m_stuck = 0.
//   - RW 0x94D9E9 / RW 0x94D9C7: AttackObjectGroupOrder::activate forwards aiAttackObject to contained passengers of non-HORDE containers.

#include "doctest.h"
#include "MoveTestUtil.h"
#include "GameLogic/AI/AIGroupOrders.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Object.h"

using namespace movetest;

TEST_CASE("LOCAL-178 MoveToGroupOrder::clone resets cached positions and stuck counter (RW 0x94E2E7)")
{
	MoveWorld mw;
	mw.buildMap();
	Object *unit = mw.spawn("Walker", 305.0f, 305.0f);
	const Coord3D dest{ 620.0f, 540.0f, 0.0f };
	MoveToGroupOrder orig(*mw.logic, { unit->getID() }, dest, true);

	MoveToGroupOrder::State st;
	st.destination = dest;
	st.attackMove = true;
	st.positions.emplace(unit->getID(), Coord3D{ 625.0f, 545.0f, 0.0f });
	st.stuck = 2;
	orig.setState(st);

	// RW 0x94E2E7: copy constructor preserves destination (+0x18..+0x20) and attackMove (+0x25),
	// initializes m_positions (+0x28, RW 0x94E213) empty, and zeroes m_stuck (+0x3C).
	auto clonedBase = orig.clone();
	const auto *cloned = dynamic_cast<const MoveToGroupOrder *>(clonedBase.get());
	REQUIRE(cloned != nullptr);
	CHECK(cloned->destination().x == dest.x);
	CHECK(cloned->destination().y == dest.y);
	CHECK(cloned->attackMove() == true);
	CHECK(cloned->positions().empty());
	CHECK(cloned->stuckCount() == 0);
}

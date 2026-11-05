// OpenBFME unit tests: TheTaintManager's grid (TaintManager.h; stops S-5260 / S-921 / S-856). Every expected value is read off the RotWK bodies named in
// each case (tools/mod/rw.py RotWK\game.dat dis <address>), not off the port.

#include "doctest.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/System/TaintManager.h"

#include <set>
#include <utility>
#include <vector>

namespace
{
Coord3D at(float x, float y)
{
	Coord3D c;
	c.x = x;
	c.y = y;
	c.z = 0.0f;
	return c;
}

TaintManager::Region region(float hiX, float hiY)
{
	TaintManager::Region r;
	r.hiX = hiX;
	r.hiY = hiY;
	return r;
}

// the cells whose value is not 0x80
std::set<std::pair<int, int>> painted(const TaintManager &t)
{
	std::set<std::pair<int, int>> s;
	for (int y = 0; y < t.countY(); ++y)
	{
		for (int x = 0; x < t.countX(); ++x)
		{
			if (t.cell(x, y).value != TaintManager::NEUTRAL)
			{
				s.insert({ x, y });
			}
		}
	}
	return s;
}
} // namespace

TEST_CASE("taint manager: the constructor's grid, the region and its size (RW 0xAD55F0, 0xAD5660, 0xAD5340)")
{
	TaintManager t;
	CHECK(t.countX() == 1); // RW 0xAD55F0: the region of zeros widened to 1.0 at size 1.0
	CHECK(t.countY() == 1);
	CHECK(t.cellSize() == 1.0f);
	CHECK(t.cellSizeInv() == 1.0f);
	CHECK(t.cell(0, 0).value == 0x80); // RW 0xAD4D00
	CHECK(t.cell(0, 0).state == 0);
	CHECK(t.cell(0, 0).owner == -1);
	t.setRegion(region(96.0f, 40.0f), 8.0f);
	CHECK(t.countX() == 12); // ceil(96 * 0.125)
	CHECK(t.countY() == 5);
	CHECK(t.cellSizeInv() == 0.125f);
	t.setRegion(region(96.0f, 40.0f), 0.0f); // RW 0xAD5672: size 0 keeps + 0x18
	CHECK(t.cellSize() == 8.0f);
	TaintManager::Region neg = region(-1.0f, 40.0f);
	t.setRegion(neg, 4.0f); // RW 0xAD568D: a negative extent changes nothing
	CHECK(t.cellSize() == 8.0f);
	CHECK(t.countX() == 12);
}

TEST_CASE("taint manager: paint's cell, radius and the midpoint circle (RW 0xAD4C10, 0xAD56E0, 0xAD52B0)")
{
	TaintManager t;
	t.setRegion(region(96.0f, 80.0f), 8.0f);
	// radius 0: r = ceil(0) = 0, one cell at floor(20 * 0.125) = 2
	t.paint(at(20.0f, 20.0f), 0.0f, 0, true, 7);
	CHECK(painted(t) == std::set<std::pair<int, int>>{ { 2, 2 } });
	CHECK(t.cell(2, 2).value == 0);
	CHECK(t.cell(2, 2).state == TaintManager::STATE_TAINT); // RW 0xAD5792: below 0x80 -> 2
	CHECK(t.cell(2, 2).owner == 7);
	// r = 1 (radius 8 * 0.125): the plus RW 0xAD52B0 draws (rows cy + 1, cy - 1, then cy three wide)
	TaintManager p;
	p.setRegion(region(96.0f, 80.0f), 8.0f);
	p.paint(at(44.0f, 44.0f), 8.0f, 0xFF, true, -1);
	CHECK(painted(p) == std::set<std::pair<int, int>>{ { 5, 4 }, { 5, 6 }, { 4, 5 }, { 5, 5 }, { 6, 5 } });
	CHECK(p.cell(5, 5).state == TaintManager::STATE_ELVEN_WOOD);
	// r = 2: rows cy +- 2 three wide, rows cy +- 1 and cy five wide
	TaintManager q;
	q.setRegion(region(96.0f, 80.0f), 8.0f);
	q.paint(at(44.0f, 44.0f), 9.0f, 0x10, true, 3); // ceil(9 * 0.125) = 2
	std::set<std::pair<int, int>> want;
	for (int x = 4; x <= 6; ++x)
	{
		want.insert({ x, 3 });
		want.insert({ x, 7 });
	}
	for (int y = 4; y <= 6; ++y)
	{
		for (int x = 3; x <= 7; ++x)
		{
			want.insert({ x, y });
		}
	}
	CHECK(painted(q) == want);
	// RW 0xAD56F8: a value of 0 without `set` paints nothing
	TaintManager z;
	z.setRegion(region(96.0f, 80.0f), 8.0f);
	z.paint(at(44.0f, 44.0f), 8.0f, 0, false, 1);
	CHECK(painted(z).empty());
	CHECK_FALSE(z.anyPainted());
}

TEST_CASE("taint manager: the cell write adds, clamps and keeps another owner's cell (RW 0xAD5740)")
{
	TaintManager t;
	t.setRegion(region(96.0f, 80.0f), 8.0f);
	const Coord3D c = at(20.0f, 20.0f);
	t.paint(c, 0.0f, 0x30, false, 4); // 0x80 + 0x30
	CHECK(t.cell(2, 2).value == 0xB0);
	CHECK(t.cell(2, 2).state == TaintManager::STATE_ELVEN_WOOD);
	t.paint(c, 0.0f, 0x90, false, 4); // 0x140 clamped to 0xFF
	CHECK(t.cell(2, 2).value == 0xFF);
	t.paint(c, 0.0f, -0x200, false, 4); // below 0: 0
	CHECK(t.cell(2, 2).value == 0);
	CHECK(t.cell(2, 2).state == TaintManager::STATE_TAINT);
	// 0x80 from another owner leaves the cell; from its owner it goes back to neutral, owner -1, state 0
	t.paint(c, 0.0f, 0x80, true, 9);
	CHECK(t.cell(2, 2).value == 0);
	CHECK(t.cell(2, 2).owner == 4);
	t.paint(c, 0.0f, 0x80, true, 4);
	CHECK(t.cell(2, 2).value == 0x80);
	CHECK(t.cell(2, 2).owner == -1);
	CHECK(t.cell(2, 2).state == 0);
	// owner -1 paints over any owner
	t.paint(c, 0.0f, 0, true, 4);
	t.paint(c, 0.0f, 0x80, true, -1);
	CHECK(t.cell(2, 2).value == 0x80);
}

TEST_CASE("taint manager: the state at a position and the callback's unclamped x (RW 0xAD4FA0, 0xAD5040)")
{
	TaintManager t;
	t.setRegion(region(96.0f, 80.0f), 8.0f);
	std::vector<std::pair<int, int>> calls;
	t.setCallback([&](int x, int y, int) { calls.emplace_back(x, y); });
	t.paint(at(4.0f, 20.0f), 8.0f, 0, true, 6); // cx 0, r 1: the row cy = 2 runs x0 = -1 .. 1
	std::int32_t owner = 55;
	CHECK(t.stateAt(at(4.0f, 20.0f), &owner) == TaintManager::STATE_TAINT);
	CHECK(owner == 6);
	owner = 55;
	CHECK(t.stateAt(at(-4.0f, 20.0f), &owner) == 0); // RW 0xAD4D30: outside, the owner untouched
	CHECK(owner == 55);
	CHECK(t.valueAt(at(500.0f, 20.0f)) == 0x80); // RW 0xAD502A
	// rows cy + 1 and cy - 1 (x0 = x1 = 0), then cy with x0 = -1: the cells 0 and 1, the callback's x -1 and 0
	const std::vector<std::pair<int, int>> want = { { 0, 3 }, { 0, 1 }, { -1, 2 }, { 0, 2 } };
	CHECK(calls == want);
	CHECK(t.cell(1, 2).value == 0);
}

TEST_CASE("taint manager: a new region keeps only the value byte, the reset keeps the size (RW 0xAD5340, 0xAD4BB0)")
{
	TaintManager t;
	t.setRegion(region(96.0f, 80.0f), 8.0f);
	t.paint(at(20.0f, 20.0f), 0.0f, 0, true, 7);
	t.setRegion(region(96.0f, 80.0f), 4.0f);
	CHECK(t.countX() == 24);
	// floor((4 * 4 + 0 - 0) * 0.125) = 2 and floor(5 * 4 * 0.125) = 2: both under the old cell (2, 2)
	CHECK(t.cell(4, 4).value == 0);
	CHECK(t.cell(5, 5).value == 0);
	CHECK(t.cell(4, 4).state == 0);   // RW 0xAD5534: only the byte
	CHECK(t.cell(4, 4).owner == -1);
	CHECK(t.cell(6, 4).value == 0x80);
	t.paint(at(0.0f, 0.0f), 0.0f, 0x20, true, 1);
	t.reset();
	CHECK(t.countX() == 1); // the region of zeros widened to 1.0: ceil(1 * 0.25)
	CHECK(t.cellSize() == 4.0f);
	CHECK(t.cell(0, 0).value == 0x20);
}

TEST_CASE("taint manager: the save keeps retail's layout and a load calls the callback (RW 0xAD4B80, 0xAD50B0, 0xAD57E0)")
{
	TaintManager t;
	t.setRegion(region(16.0f, 8.0f), 8.0f);
	t.paint(at(12.0f, 4.0f), 0.0f, 0xC0, true, 5);
	XferSave save;
	t.xfer(save);
	// versions 1 and 1, six reals, two reals, two ints, the uint 0, then two cells of version, byte, state, owner
	CHECK(save.data().size() == 1 + 1 + 24 + 8 + 8 + 4 + 2 * (1 + 1 + 4 + 4));
	CHECK(save.data()[46] == 2);    // the first cell's version
	CHECK(save.data()[47] == 0x80); // its value
	CHECK(save.data()[57] == 0xC0); // the second cell's value
	TaintManager u;
	int calls = 0;
	u.setCallback([&](int, int, int) { ++calls; });
	XferLoad load(save.data());
	u.xfer(load);
	CHECK(u.countX() == 2);
	CHECK(u.countY() == 1);
	CHECK(u.cell(1, 0).value == 0xC0);
	CHECK(u.cell(1, 0).owner == 5);
	CHECK(u.cell(1, 0).state == TaintManager::STATE_ELVEN_WOOD);
	CHECK(calls == 2); // RW 0xAD5290
	CHECK(u.anyPainted());
}

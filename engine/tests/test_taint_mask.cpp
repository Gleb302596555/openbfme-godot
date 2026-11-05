// OpenBFME unit tests. GPL-3.0.
// The terrain's taint mask (W3DTaintMask.h, stop S-5261): the texel of a cell (RW 0x4735AB, the shader path), the fire colour at the cell's centre
// (TheFireManager slot 0x3C RW 0x497F60), the texture with its border (RW 0x47321C) and the version the client rebuilds on. Expectations from the RW
// bodies and the Fire block's constructor (RW 0x70C637).

#include "doctest.h"

#include "GameClient/FireSetting.h"
#include "GameEngineDevice/W3DDevice/GameClient/W3DTaintMask.h"
#include "GameLogic/System/TaintManager.h"

#include <cstdint>
#include <vector>

TEST_CASE("taint mask: the fire colour of a cell type, the texel and the cell centre (RW 0x497F60, 0x4735AB)")
{
	const FireSetting fire; // the constructor's colours: BurntTerrainColor 0.2 each, FuelIndicatorColor 0.8 / 0.7 / 0.6
	// type 1 (burning) -> FuelIndicatorColor (+ 0x20), 2 (burnt out) -> BurntTerrainColor (+ 0x14), else white
	CHECK(W3DTaintMask::fireColour(fire, 0).red == 1.0f);
	CHECK(W3DTaintMask::fireColour(fire, 1).green == 0.7f);
	CHECK(W3DTaintMask::fireColour(fire, 2).blue == 0.2f);
	CHECK(W3DTaintMask::fireColour(fire, 3).blue == 1.0f);
	// value << 24 | channels * 255 truncated: 0.8 -> 204, 0.7 -> 178 (178.49999 truncates), 0.6 -> 153, 0.2 -> 51
	CHECK(W3DTaintMask::texel(0x80, W3DTaintMask::fireColour(fire, 0)) == 0x80FFFFFFu);
	CHECK(W3DTaintMask::texel(0x00, W3DTaintMask::fireColour(fire, 1)) == 0x00CCB299u);
	CHECK(W3DTaintMask::texel(0xFF, W3DTaintMask::fireColour(fire, 2)) == 0xFF333333u);
	// trunc((n + 0.5) * cellSize)
	CHECK(W3DTaintMask::cellCentre(0, 10.0f) == 5);
	CHECK(W3DTaintMask::cellCentre(3, 10.0f) == 35);
	CHECK(W3DTaintMask::cellCentre(1, 7.5f) == 11);
}

TEST_CASE("taint mask: the texture is the cells plus a one-texel border at TaintAlpha in every byte (RW 0x47321C, 0x473E83)")
{
	const FireSetting fire;
	TaintView view;
	view.countX = 2;
	view.countY = 1;
	view.values = { 0x00, 0xFF };
	view.fireTypes = { 0, 2 };
	std::vector<std::uint32_t> texels;
	int w = 0, h = 0;
	W3DTaintMask::build(view, fire, 0x80, texels, w, h);
	REQUIRE(w == 4);
	REQUIRE(h == 3);
	REQUIRE(texels.size() == 12);
	CHECK(texels[0] == 0x80808080u);
	CHECK(texels[11] == 0x80808080u);
	CHECK(texels[4] == 0x80808080u); // row 1, column 0: the border
	CHECK(texels[5] == 0x00FFFFFFu); // cell (0, 0) at texel (1, 1)
	CHECK(texels[6] == 0xFF333333u); // cell (1, 0), burnt out
	CHECK(texels[7] == 0x80808080u);
	// Ennorath's TaintAlpha 0
	W3DTaintMask::build(view, fire, 0, texels, w, h);
	CHECK(texels[0] == 0u);
}

TEST_CASE("taint mask: TheTaintManager's version moves with every cell write and region change, not with a read")
{
	TaintManager tm;
	TaintManager::Region r;
	r.hiX = 100.0f;
	r.hiY = 100.0f;
	const std::uint64_t v0 = tm.version();
	tm.setRegion(r, 10.0f);
	const std::uint64_t v1 = tm.version();
	CHECK(v1 != v0);
	Coord3D at{ 50.0f, 50.0f, 0.0f };
	CHECK(tm.valueAt(at) == 0x80);
	CHECK(tm.version() == v1);
	tm.paint(at, 10.0f, 0x00, true, -1);
	CHECK(tm.version() != v1);
}

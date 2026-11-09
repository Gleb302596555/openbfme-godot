// OpenBFME unit tests. GPL-3.0.
// PORT-73 (issue #205): W3DTreeDraw's DoTopple, MoveOutwardTime, MoveInwardTime, MoveOutwardDistanceFactor, DarkeningFactor — the shrub push aside.
//
// Binary verification against RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f):
//   * the field table RW 0xBE2E80 (24 rows): MoveOutwardTime -> +0x10, MoveInwardTime -> +0x14, MoveOutwardDistanceFactor -> +0x18,
//     DarkeningFactor -> +0x1C, DoTopple -> +0x3D (the rows RW 0xBE2EA0 .. 0xBE2F20).
//   * the shrub buffer's addShrub RW 0x4E8B8F: RW 0x4E8C66 `cmp dword ptr [data+0x10], 0; ja` OR RW 0x4E8C6C
//     `cmp byte ptr [data+0x3d], 0; jne` puts the record into the area partition (+0x5C0), else its link is -1 (RW 0x4E8C7E).
//   * the record's partition link is the only way the walk below finds a shrub: RW 0x4E70F5 (W3DShrubBuffer::unitMoved, reached from
//     TheTerrainVisual slot 0x6C RW 0x491977 -> BaseHeightMap RW 0x467819 -> the buffer at +0x3854) walks the 50 x 50 cells of the unit's
//     box (bounds +0x1948 .. +0x1954, 49.9 at RW 0xBE4AD0) for a record whose 3D squared distance is below (major radius + 7.0, RW
//     0xBE4AD4) squared AND whose type's +0x10 is above 0 (RW 0x4E73AE) — so MoveOutwardTime is the effective gate and DoTopple alone
//     pushes nothing.
//   * the push RW 0x4E6C74: the same pusher within three logic frames (RW 0x4E6CC9 `cmp eax, 3; jb`), a record already bent (push aside
//     +0x5C not 0, RW 0x4E6CD7), the direction from the sign of dir.x * dy - dir.y * dx (RW 0x4E6D0E: cos +0x68 = -dir.y, sin +0x64 =
//     dir.x when above 0), the rate 1.0f / (MoveOutwardTime * 6) (RW 0x4E6D4E .. 0x4E6D6D; 6 = the buffer's +0x5BC, 30 client frames per
//     5 logic frames).
//   * the per-drawn-frame step RW 0x4E8519: push aside += rate; at 1.0f (RW 0xBD1908) the rate becomes -1.0 / (MoveInwardTime * 6), a
//     double division (RW 0xBDFC10); at 0 both are cleared (RW 0x4E857C).
//   * the bend RW 0x4E7B25 / RW 0x4E7B4D (the same pair in the partial rebuild RW 0x4E7F73 / RW 0x4E7F9B): x += vertex z * push aside
//     * cos * MoveOutwardDistanceFactor (data +0x18), y += the same with sin.
//   * DarkeningFactor (+0x1C) has no reader in RotWK 2.01: the three functions that read the type's data (+0x10 at RW 0x4E73AE and
//     RW 0x4E6D4E, +0x14 at RW 0x4E8550, +0x18 at RW 0x4E7B2D / RW 0x4E7B4D / RW 0x4E7F7B / RW 0x4E7F9B) never touch +0x1C, and no
//     instruction in .text reads a float at +0x1C of a shrub type (the byte scan in docs/ennorath/PORT73_RW_ANALYSIS.md). The tests pin
//     that the port darkens nothing.
//
// The numbers come from the mod's data (Ennorath 1.9.8.2, data\ini\object\nature\natureprop.ini: MoveOutwardTime = 333 ms, MoveInwardTime
// = 1666 ms, MoveOutwardDistanceFactor = 1.0, DarkeningFactor = 0.3) through the port's parseDurationUnsignedInt (ceil(ms * 0.005) logic
// frames, RW 0x73A429, pinned by tests/test_mapobj_draw_data.cpp): 333 ms -> 2 frames, 1666 ms -> 9 frames. They are set on the module
// data here directly so the test needs no game files.

#include "doctest.h"

#include "GameClient/TreeTopple.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"

#include <cmath>

namespace
{
// a shrub of the mod's data: MoveOutwardTime 333 ms, MoveInwardTime 1666 ms, MoveOutwardDistanceFactor 1.0, DarkeningFactor 0.3
W3DTreeDrawModuleData shrubData(float distanceFactor = 1.0f)
{
	W3DTreeDrawModuleData d;
	d.m_modelName = "exseerosen";
	d.m_framesToMoveOutward = 2;  // 333 ms
	d.m_framesToMoveInward = 9;   // 1666 ms
	d.m_maxOutwardMovement = distanceFactor;
	d.m_darkening = 0.3f;
	d.m_doTopple = false;
	return d;
}

// one drawn frame of the client layer (RW 0x4E8519 runs once per drawn frame; the port's advance steps at 1/30 s, lane SPELL-3's S-930)
void frames(TreeTopple &topple, int count)
{
	for (int i = 0; i < count; ++i)
	{
		topple.advance(TreeTopple::kUpdateSeconds);
	}
}
} // namespace

TEST_CASE("PORT-73: a shrub the walker pushes bends out over MoveOutwardTime * 6 frames and back over MoveInwardTime * 6")
{
	const W3DTreeDrawModuleData d = shrubData();
	TreeTopple topple;
	// a shrub one metre to the walker's left (+y) as it heads +x: dir.x * dy - dir.y * dx = 1 * 1 - 0 * 0 = 1 > 0, so cos = -dir.y = 0, sin = dir.x = 1
	const size_t i = topple.add(7, Coord3D{ 0.0f, 1.0f, 0.0f }, &d);
	REQUIRE(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
	CHECK(topple.pushAsideAt(i) == 0.0f); // the push starts the bend, the step comes from the draw loop

	// the rate is 1.0 / (2 frames * 6 frames per logic frame) = 1/12 (RW 0x4E6D67)
	frames(topple, 1);
	CHECK(topple.pushAsideAt(i) == doctest::Approx(1.0f / 12.0f));
	frames(topple, 5);
	CHECK(topple.pushAsideAt(i) == doctest::Approx(6.0f / 12.0f));
	frames(topple, 6); // 12 steps: 12 / 12 = 1.0
	CHECK(topple.pushAsideAt(i) == doctest::Approx(1.0f));
	// RW 0x4E852D .. 0x4E8569 tests the sum AFTER adding the step: the frame that pushes it past 1.0 adds once more and only then turns the
	// rate inward (-1.0 / (9 * 6) = -1/54), so the bend peaks at 13/12
	frames(topple, 1);
	CHECK(topple.pushAsideAt(i) == doctest::Approx(13.0f / 12.0f));
	frames(topple, 6);
	CHECK(topple.pushAsideAt(i) == doctest::Approx(13.0f / 12.0f - 6.0f / 54.0f));
	frames(topple, 120); // the rest of MoveInwardTime * 6 and the steps that land on 0
	CHECK(topple.pushAsideAt(i) == 0.0f);
	frames(topple, 60); // and it stays upright: the rate is cleared (RW 0x4E857C)
	CHECK(topple.pushAsideAt(i) == 0.0f);
}

TEST_CASE("PORT-73: the bend follows the walker's heading (RW 0x4E6D0E) and stops 7.0 past its radius (RW 0x4E713C)")
{
	const W3DTreeDrawModuleData d = shrubData();
	TreeTopple topple;
	const size_t left = topple.add(1, Coord3D{ 0.0f, 1.0f, 0.0f }, &d);   // the record to the walker's left
	const size_t right = topple.add(2, Coord3D{ 0.0f, -1.0f, 0.0f }, &d); // to its right
	const size_t far = topple.add(3, Coord3D{ 0.0f, 8.01f, 0.0f }, &d);   // 8.01 > 1.0 + 7.0 away
	CHECK(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 2);
	CHECK(topple.pushAsideAt(far) == 0.0f);

	frames(topple, 12);
	const float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float basis[9];
	Coord3D position;
	topple.placement(left, identity, Coord3D{ 0.0f, 1.0f, 0.0f }, 1.0f, basis, position);
	// cross = dir.x * dy - dir.y * dx = 1 * 1 - 0 * 0 = 1 > 0: cos = -dir.y = 0, sin = dir.x = 1 -> the bend is along +y
	CHECK(basis[0 * 3 + 2] == doctest::Approx(0.0f));
	CHECK(basis[1 * 3 + 2] == doctest::Approx(1.0f)); // push aside 1.0 * MoveOutwardDistanceFactor 1.0
	CHECK(basis[2 * 3 + 2] == doctest::Approx(1.0f)); // the model's own up axis is untouched
	CHECK(position.x == 0.0f);
	CHECK(position.y == 1.0f);

	float other[9];
	Coord3D otherPos;
	topple.placement(right, identity, Coord3D{ 0.0f, -1.0f, 0.0f }, 1.0f, other, otherPos);
	// cross = 1 * (-1) - 0 * 0 = -1: cos = dir.y = 0, sin = -dir.x = -1 -> the other way
	CHECK(other[1 * 3 + 2] == doctest::Approx(-1.0f));
}

TEST_CASE("PORT-73: the same walker pushes at most every three logic frames and a bent record keeps its direction (RW 0x4E6CC9 / RW 0x4E6CD7)")
{
	const W3DTreeDrawModuleData d = shrubData();
	TreeTopple topple;
	const size_t i = topple.add(1, Coord3D{ 0.0f, 1.0f, 0.0f }, &d);
	REQUIRE(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
	frames(topple, 3); // the record is bent now: a second push would need it upright again
	// the same walker two frames later is inside the three-frame window (RW 0x4E6CC9 `cmp eax, 3; jb`)
	CHECK(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f }, 1.0f, 102u) == 0);
	frames(topple, 200); // back to upright
	REQUIRE(topple.pushAsideAt(i) == 0.0f);
	CHECK(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 1.0f, 0.0f }, 1.0f, 170u) == 1);
	frames(topple, 12);
	const float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float basis[9];
	Coord3D position;
	topple.placement(i, identity, Coord3D{ 0.0f, 1.0f, 0.0f }, 1.0f, basis, position);
	// cross = dir.x * dy - dir.y * dx = 0 * 1 - 1 * 0 = 0 -> the else branch: cos = dir.y = 1, sin = -dir.x = 0
	CHECK(basis[0 * 3 + 2] == doctest::Approx(1.0f));
	CHECK(basis[1 * 3 + 2] == doctest::Approx(0.0f));
}

TEST_CASE("PORT-73: MoveOutwardTime 0 pushes nothing even with DoTopple (RW 0x4E73AE; DoTopple only joins the partition, RW 0x4E8C6C)")
{
	W3DTreeDrawModuleData d = shrubData();
	d.m_framesToMoveOutward = 0;   // a tree of the mod's naturetrees.ini: DoTopple = Yes, MoveOutwardTime 0
	d.m_doTopple = true;
	TreeTopple topple;
	topple.add(1, Coord3D{ 0.0f, 0.0f, 0.0f }, &d);
	CHECK(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 0);
	frames(topple, 12);
	CHECK(topple.pushAsideAt(0) == 0.0f);

	// and a shrub with MoveOutwardTime above 0 needs DoTopple neither: the partition takes it on its own (RW 0x4E8C66)
	const W3DTreeDrawModuleData shrub = shrubData();
	TreeTopple other;
	other.add(1, Coord3D{ 0.0f, 0.0f, 0.0f }, &shrub);
	CHECK(other.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
}

TEST_CASE("PORT-73: MoveOutwardDistanceFactor scales the bend; a scale of the instance keeps the drawn offset (RW 0x4E7B2D / RW 0x4E7F7B)")
{
	const W3DTreeDrawModuleData d = shrubData(0.5f); // the mod's other shrub value
	TreeTopple topple;
	const size_t i = topple.add(1, Coord3D{ 0.0f, 1.0f, 0.0f }, &d);
	REQUIRE(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
	frames(topple, 12);
	const float base[9] = { 2, 0, 0, 0, 2, 0, 0, 0, 2 }; // an instance drawn at scale 2
	float basis[9];
	Coord3D position;
	topple.placement(i, base, Coord3D{ 0.0f, 1.0f, 0.0f }, 2.0f, basis, position);
	// the drawn offset of a vertex one model unit up is push aside * factor * (cos, sin) = 0.5 along +y whatever the instance scale is:
	// the draw multiplies the basis by the scale, so column 2 of the basis holds 0.5 / 2 on top of its own 0
	CHECK(2.0f * basis[1 * 3 + 2] == doctest::Approx(0.5f));
	CHECK(2.0f * basis[0 * 3 + 2] == doctest::Approx(0.0f));
}

TEST_CASE("PORT-73: the bend leaves the fall alone (a bent shrub still topples, an upright tree does not bend)")
{
	const W3DTreeDrawModuleData shrub = shrubData();
	const W3DTreeDrawModuleData tree = [] {
		W3DTreeDrawModuleData d;
		d.m_framesToMoveOutward = 0; // a real tree: naturetrees.ini's DoTopple = Yes, no bend
		d.m_doTopple = true;
		d.m_minimumToppleSpeed = 0.5f;
		d.m_initialVelocityPercent = 0.2f;
		d.m_killWhenToppled = false;
		return d;
	}();
	TreeTopple topple;
	const size_t bent = topple.add(1, Coord3D{ 0.0f, 1.0f, 0.0f }, &shrub);
	const size_t fell = topple.add(2, Coord3D{ 20.0f, 0.0f, 0.0f }, &tree);
	REQUIRE(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
	REQUIRE(topple.topple(2, Coord3D{ 1.0f, 0.0f, 0.0f }, 0.0f)); // RW 0x4E9E4E, the existing topple path
	CHECK(topple.trees()[fell].state == TreeTopple::FALLING);
	frames(topple, 12);
	CHECK(topple.trees()[fell].angle > 0.0f);
	CHECK(topple.trees()[bent].angle == 0.0f);
	CHECK(topple.pushAsideAt(bent) == doctest::Approx(1.0f));
	CHECK(topple.pushAsideAt(fell) == 0.0f);
}

TEST_CASE("PORT-73: DarkeningFactor is stored and applied by nothing (no reader in RotWK 2.01)")
{
	const W3DTreeDrawModuleData d = shrubData();
	CHECK(d.m_darkening == doctest::Approx(0.3f));
	TreeTopple topple;
	const size_t i = topple.add(1, Coord3D{ 0.0f, 1.0f, 0.0f }, &d);
	REQUIRE(topple.unitMoved(40, Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 1.0f, 0.0f, 0.0f }, 1.0f, 100u) == 1);
	frames(topple, 12);
	// the only state a bend produces is the shear: the record keeps its model, its angle and its sink (nothing darkens the draw)
	const float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float basis[9];
	Coord3D position;
	topple.placement(i, identity, Coord3D{ 0.0f, 1.0f, 0.0f }, 1.0f, basis, position);
	CHECK(topple.trees()[i].angle == 0.0f);
	CHECK(topple.trees()[i].sink == 0.0f);
	CHECK(position.x == 0.0f);
	CHECK(position.y == 1.0f);
	CHECK(position.z == 0.0f);
}

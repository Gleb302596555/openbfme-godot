// OpenBFME unit tests, lane RENDER-1: W3DStreakDraw (RW 0x4CFD13 defaults, RW 0xBE39F8 fields, RW 0x4CF884 trail points) and the
// ribbon geometry (inference S-391).

#include "doctest.h"
#include "HudTestUtil.h"
#include "RetailTestMount.h"

#include "Common/INI.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DStreakDraw.h"

#include <cmath>
#include <cstring>
#include <string>

TEST_CASE("render streak: module data defaults and field table (RW 0x4CFD13, 0xBE39F8)")
{
	W3DStreakDrawModuleData d;
	CHECK(d.m_length == 50.0f);
	CHECK(d.m_width == 0.5f);
	CHECK(d.m_additive);
	CHECK(d.m_color.red == 1.0f);
	CHECK(d.m_color.green == 1.0f);
	CHECK(d.m_color.blue == 1.0f);
	CHECK(d.m_numSegments == 5u);
	CHECK(d.m_texture.empty());
	const char *const names[] = { "Length", "Width", "Additive", "Color", "Texture", "NumSegments", "WeatherTexture" };
	const FieldParse *t = W3DStreakDrawTables::streak();
	int n = 0;
	for (; t[n].token; ++n)
	{
		REQUIRE(n < 7);
		CHECK(std::strcmp(t[n].token, names[n]) == 0);
	}
	CHECK(n == 7);
}

TEST_CASE("render streak: the trail follows RW 0x4CF884 (append past Length / NumSegments, drop points older than Length)")
{
	W3DStreakDrawModuleData d;
	d.m_length = 15.0f;
	d.m_numSegments = 1;
	W3DStreakTrail t;
	const float p0[3] = { 0, 0, 0 };
	t.update(d, p0);
	REQUIRE(t.points().size() == 1);
	CHECK(t.points()[0].length == 0.0f);
	const float p1[3] = { 3, 4, 0 }; // 5 away
	t.update(d, p1);
	REQUIRE(t.points().size() == 2);
	CHECK(t.points()[1].length == 5.0f);
	// the last point moves with the head; 5 + 5 = 10 from point 0, the segment 10 - 0 = 10 < 15: no new point
	const float p2[3] = { 6, 8, 0 };
	t.update(d, p2);
	REQUIRE(t.points().size() == 2);
	CHECK(t.points()[1].pos[0] == 6.0f);
	CHECK(t.points()[1].length == 10.0f);
	// 20 from point 0 > 15: a point is appended at the head; then 20 - 15 = 5 > len[1] = 20 is false: nothing dropped
	const float p3[3] = { 12, 16, 0 };
	t.update(d, p3);
	REQUIRE(t.points().size() == 3);
	CHECK(t.points()[2].length == 20.0f);
	// the head moves on: d = |p - p[1]| = 10 -> len 30; 10 < 15 no append; 30 - 15 = 15 > len[1] = 20 false
	const float p4[3] = { 18, 24, 0 };
	t.update(d, p4);
	REQUIRE(t.points().size() == 3);
	CHECK(t.points()[2].length == 30.0f);
	// d = 20 > 15: append (len 40); 40 - 15 = 25 > 20: point 0 dropped; then 25 > len[1] (now 40)? no
	const float p5[3] = { 24, 32, 0 };
	t.update(d, p5);
	REQUIRE(t.points().size() == 3);
	CHECK(t.points()[0].length == 20.0f);
	CHECK(t.points()[2].length == 40.0f);
}

namespace
{
// tolerance: float32 geometry of the closed forms below, relative 1e-4
bool near(double a, double b)
{
	return std::fabs(a - b) <= 1e-4 * (1.0 + std::fabs(b));
}

W3DStreakCamera viewSpaceCamera()
{
	W3DStreakCamera c = {};
	c.right[0] = 1.0f;
	c.up[1] = 1.0f;
	c.forward[2] = 1.0f;
	return c; // the eye at the origin looking along z: the points are in view space already
}

W3DStreakPoint streakPoint(float x, float y, float z, float length)
{
	W3DStreakPoint p;
	p.pos[0] = x;
	p.pos[1] = y;
	p.pos[2] = z;
	p.length = length;
	return p;
}
} // namespace

// The ribbon of SimpleStreakRendererClass::RenderStreak (BFME2 Rva 0x126433; RW 0x525E1F). The expected values are the closed form of its equations for a
// straight trail along x at depth d with the radius r = Width / 2: the segment's nearest point to the eye is (0, 0, d), its offset is -y, the two edge planes
// through the eye have the normals (0, +-d, r) / L with L^2 = d^2 + r^2, and a vertex is the point projected onto its edge plane: (x, -+r d^2 / L^2, d^3 / L^2).
TEST_CASE("render streak: the ribbon is the mitred strip of SimpleStreakRendererClass::RenderStreak, Width wide; V is 1 at the head and 0 one Length behind it (S-1002)")
{
	const W3DStreakCamera cam = viewSpaceCamera();
	const double d = 100.0, r = 1.0, L2 = d * d + r * r;
	const double y = r * d * d / L2, z = d * d * d / L2;
	const W3DStreakPoint a = streakPoint(0, 0, 100, 0), b = streakPoint(15, 0, 100, 15);
	const W3DStreakMesh m = W3DStreakRibbon({ a, b }, 2.0f, 15.0f, cam);
	REQUIRE(m.vertices.size() == 4);
	REQUIRE(m.indices == std::vector<unsigned>{ 0, 1, 2, 1, 3, 2 }); // (last top, last bottom, top), (last bottom, bottom, top)
	// the top vertex (u 0) is on the -y side (offset = segdir x nearest), the bottom (u 1) on +y
	CHECK(near(m.vertices[0].pos[0], 0.0));
	CHECK(near(m.vertices[0].pos[1], -y));
	CHECK(near(m.vertices[0].pos[2], z));
	CHECK(near(m.vertices[1].pos[1], y));
	CHECK(near(m.vertices[2].pos[0], 15.0));
	CHECK(near(m.vertices[3].pos[1], y));
	CHECK(m.vertices[0].u == 0.0f);
	CHECK(m.vertices[1].u == 1.0f);
	CHECK(m.vertices[2].u == 0.0f);
	CHECK(m.vertices[3].u == 1.0f);
	CHECK(m.vertices[0].v == 0.0f); // the tail, one Length behind the head: the fletching (V 0) of the retail arrow textures
	CHECK(m.vertices[1].v == 0.0f);
	CHECK(m.vertices[2].v == 1.0f); // the head (the newest point): the arrowhead (V 1)
	CHECK(m.vertices[3].v == 1.0f);
	CHECK(W3DStreakRibbon({ a }, 2.0f, 15.0f, cam).vertices.empty());
	CHECK(W3DStreakRibbon({ a, b }, 2.0f, 0.0f, cam).vertices.empty()); // no Length: nothing (retail would divide by zero)
	CHECK(W3DStreakDrawStopLine().rfind("[S-391]", 0) == 0);
	// a trail shorter than Length (just launched): the texture keeps its scale, the tail shows its middle (v = (len - (head - Length)) / Length)
	const W3DStreakMesh s2 = W3DStreakRibbon({ a, streakPoint(15, 0, 100, 5) }, 2.0f, 15.0f, cam);
	REQUIRE(s2.vertices.size() == 4);
	CHECK(near(s2.vertices[0].v, 10.0 / 15.0));
	CHECK(s2.vertices[2].v == 1.0f);
	// a longer trail (the oldest point is kept while the next one is within Length): V below 0 at the oldest point
	const W3DStreakMesh s3 = W3DStreakRibbon({ a, streakPoint(10, 0, 100, 10), streakPoint(30, 0, 100, 30) }, 2.0f, 15.0f, cam);
	REQUIRE(s3.vertices.size() == 6);
	CHECK(near(s3.vertices[0].v, -1.0));
	CHECK(near(s3.vertices[2].v, -5.0 / 15.0));
	CHECK(s3.vertices[4].v == 1.0f);
}

TEST_CASE("render streak: the ribbon is built in the camera's view space and mapped back to the points' space")
{
	// the camera at (10, 20, 30) looking along +x, its right along +y and its up along +z; the trail along the camera's right at depth d
	W3DStreakCamera cam = {};
	cam.eye[0] = 10.0f; cam.eye[1] = 20.0f; cam.eye[2] = 30.0f;
	cam.right[1] = 1.0f;
	cam.up[2] = 1.0f;
	cam.forward[0] = 1.0f;
	const double d = 100.0, r = 1.0, L2 = d * d + r * r;
	const double y = r * d * d / L2, z = d * d * d / L2;
	const W3DStreakMesh m = W3DStreakRibbon({ streakPoint(110, 20, 30, 0), streakPoint(110, 35, 30, 15) }, 2.0f, 15.0f, cam);
	REQUIRE(m.vertices.size() == 4);
	// view space (x, y, z) -> eye + right * x + up * y + forward * z
	CHECK(near(m.vertices[0].pos[0], 10.0 + z));
	CHECK(near(m.vertices[0].pos[1], 20.0 + 0.0));
	CHECK(near(m.vertices[0].pos[2], 30.0 - y));
	CHECK(near(m.vertices[1].pos[2], 30.0 + y));
	CHECK(near(m.vertices[2].pos[1], 20.0 + 15.0));
}

TEST_CASE("render streak: a fold keeps the edges on their sides (switch_edges), a mitre between segments, degenerate points and chunks of 129")
{
	const W3DStreakCamera cam = viewSpaceCamera();
	const double d = 100.0, r = 1.0, L2 = d * d + r * r;
	const double y = r * d * d / L2, z = d * d * d / L2;
	// the trail runs to x = 10 and comes back to x = 5: the second segment's edge planes swap, so the top vertex of every point stays on the -y side
	const W3DStreakMesh fold = W3DStreakRibbon({ streakPoint(0, 0, 100, 0), streakPoint(10, 0, 100, 10), streakPoint(5, 0, 100, 15) }, 2.0f, 50.0f, cam);
	REQUIRE(fold.vertices.size() == 6);
	const double xs[3] = { 0.0, 10.0, 5.0 };
	for (int i = 0; i < 3; ++i)
	{
		CHECK(near(fold.vertices[2 * i].pos[0], xs[i]));
		CHECK(near(fold.vertices[2 * i].pos[1], -y));
		CHECK(near(fold.vertices[2 * i].pos[2], z));
		CHECK(near(fold.vertices[2 * i + 1].pos[1], y));
	}
	// a corner: the trail turns by 90 degrees at the middle point; the mitre ray through the eye is where the two top planes cross, and its vertex is the
	// projection of the point on both top planes' crossing: its depth stays at the point's own
	const W3DStreakMesh corner = W3DStreakRibbon({ streakPoint(-10, 0, 100, 0), streakPoint(0, 0, 100, 10), streakPoint(0, 10, 100, 20) }, 2.0f, 50.0f, cam);
	REQUIRE(corner.vertices.size() == 6);
	CHECK(corner.indices.size() == 12);
	for (const W3DStreakVertex &v : corner.vertices)
	{
		CHECK(std::isfinite(v.pos[0]));
		CHECK(std::isfinite(v.pos[1]));
		CHECK(std::isfinite(v.pos[2]));
	}
	// two equal points are pulled 0.001 apart (Equal_Within_Epsilon): no NaN
	const W3DStreakMesh same = W3DStreakRibbon({ streakPoint(3, 4, 100, 0), streakPoint(3, 4, 100, 0) }, 2.0f, 50.0f, cam);
	REQUIRE(same.vertices.size() == 4);
	for (const W3DStreakVertex &v : same.vertices)
	{
		CHECK(std::isfinite(v.pos[0]));
		CHECK(std::isfinite(v.pos[1]));
		CHECK(std::isfinite(v.pos[2]));
	}
	// 130 points are two chunks of 129 and 2 points, each its own strip
	std::vector<W3DStreakPoint> many;
	for (int i = 0; i < 130; ++i)
	{
		many.push_back(streakPoint((float)i, 0, 100, (float)i));
	}
	const W3DStreakMesh chunks = W3DStreakRibbon(many, 2.0f, 50.0f, cam);
	CHECK(chunks.vertices.size() == 2 * 129 + 2 * 2);
	CHECK(chunks.indices.size() == 128 * 6 + 1 * 6);
	CHECK(chunks.indices.back() < chunks.vertices.size()); // tolerance: an index range check, not a measurement
}

TEST_CASE("render streak: the texture of the map's weather - the WeatherTexture entry of the weather, else Texture (RW 0x4CFA9A, lane PROJ-2)")
{
	W3DStreakDrawModuleData d;
	d.m_texture = "EXArrowStreak01.tga";
	d.m_weatherTextures = { { 1, "EXArrowStreak_Snow.tga" } };
	CHECK(W3DStreakTexture(d, 0) == "EXArrowStreak01.tga");
	CHECK(W3DStreakTexture(d, 1) == "EXArrowStreak_Snow.tga");
	d.m_weatherTextures.clear();
	CHECK(W3DStreakTexture(d, 1) == "EXArrowStreak01.tga");
}

TEST_CASE("render streak: the retail arrow templates parse into the typed module data")
{
	if (!hudtest::haveWorld("render streak retail arrows"))
	{
		return;
	}
	const ThingTemplate *tt = hudtest::shared().world->things().findTemplate("GoodFactionArrow");
	REQUIRE(tt != nullptr);
	const W3DStreakDrawModuleData *found = nullptr;
	for (const ThingTemplate::Nugget &n : tt->drawModules().nuggets())
	{
		if (n.name == "W3DStreakDraw")
		{
			found = dynamic_cast<const W3DStreakDrawModuleData *>(n.data.get());
		}
	}
	REQUIRE(found != nullptr);
	// goodfactionsubobjects.ini GoodFactionArrow: Length 15, Width 2, NumSegments 1, Color 255 255 255, Additive No, Texture EXArrowStreak01.tga,
	// WeatherTexture SNOWY EXArrowStreak_Snow.tga
	CHECK(found->m_length == 15.0f);
	CHECK(found->m_width == 2.0f);
	CHECK(found->m_numSegments == 1u);
	CHECK_FALSE(found->m_additive);
	CHECK(found->m_texture == "EXArrowStreak01.tga");
	REQUIRE(found->m_weatherTextures.size() == 1);
	CHECK(found->m_weatherTextures[0].first == 1);
	CHECK(found->m_weatherTextures[0].second == "EXArrowStreak_Snow.tga");
}

TEST_CASE("render streak: a failed texture is skipped and reported, and reported again after the next load (review r2)")
{
	// GameWorld's use: refresh_streaks asks skipped(key) before loading a material's texture and records a failure; clear_scene
	// (every load_map) calls beginScene(). Two consecutive loads with the same missing texture must both report it.
	W3DStreakDiagnostics d;
	const std::string key = "B|Missing.tga";
	for (int load = 0; load < 2; ++load)
	{
		CAPTURE(load);
		d.beginScene();
		CHECK_FALSE(d.skipped(key));          // the new scene asks for the texture again
		CHECK(d.errors().empty());
		d.recordFailure(key, "W3DStreakDraw: texture Missing.tga did not load");
		CHECK(d.skipped(key));                // the following frames skip the surface
		REQUIRE(d.errors().size() == 1);      // and the report carries the failure once
		CHECK(d.errors()[0] == "W3DStreakDraw: texture Missing.tga did not load");
		CHECK(d.failedCount() == 1);
	}
}

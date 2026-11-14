// OpenBFME unit tests. PORT-176 (issue #429, P-107): Mordor's "Desecrated earth" (SpellBookMordorTaintNoTime -> FX_TaintSpellFX1 ->
// DynamicDecal exmordorquake02, Size 420) lays a terrain-conforming decal mesh with packed JPG + PNG soft round alpha instead of a flat
// 4-corner unmasked quad z-clipping through uneven 10-unit heightmap cells.

#include "doctest.h"

#include "GameClient/DynamicDecalMesh.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/ParticleDraw.h"
#include "GameClient/ParticleTexture.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

TerrainScorchBuffer::Terrain steppedTerrain(int w, int h, int border)
{
	TerrainScorchBuffer::Terrain t;
	t.xExtent = w;
	t.yExtent = h;
	t.border = border;
	t.heights.resize((size_t)w * (size_t)h, 0);
	t.flip.resize((size_t)w * (size_t)h, 0);
	for (int j = 0; j < h; ++j)
	{
		for (int i = 0; i < w; ++i)
		{
			// Raised central ridge + stepped slope: a flat 4-corner quad would sink beneath the ridge and clip hard along 10x10 cell edges
			const int dx = i - w / 2;
			const int dy = j - h / 2;
			const int ridge = (dx * dx + dy * dy < 36) ? 512 : 0;
			const int step = ((i / 3) * 64) + ((j / 4) * 32);
			t.heights[(size_t)j * (size_t)w + (size_t)i] = (std::uint16_t)(256 + ridge + step);
			t.flip[(size_t)j * (size_t)w + (size_t)i] = (std::uint8_t)((i + j) & 1);
		}
	}
	return t;
}

} // namespace

TEST_SUITE_BEGIN("PORT-176");

TEST_CASE("PORT-176: FX_TaintSpellFX1's 420-unit DynamicDecal subdivides stepped heightmap cells and matches cell flip diagonals (RW 0x50C24B .. 0x50D414)")
{
	Win32BIGFileSystem fs;
	FXPlayback playback(fs, RandomAlgorithm::RotWK_GameDat_LCG);
	// Exact DynamicDecal block from Ennorath data/ini/fxlist.ini:11097..11125 (FX_TaintSpellFX1, TriggerFX of SpellBookMordorTaintNoTime)
	const std::vector<std::string> errs = playback.loadIniText(
		"fxlist.ini",
		"FXList FX_TaintSpellFX1\n"
		"  DynamicDecal\n"
		"    DecalName = exmordorquake02\n"
		"    ;Shader = SUBTRACT\n"
		"    Size = 420\n"
		"    Color = R:255 G:255 B:255\n"
		"    OpacityStart = 0\n"
		"    OpacityFadeTimeOne = 5000\n"
		"    OpacityPeak = 200\n"
		"    OpacityPeakTime = 82000\n"
		"    OpacityFadeTimeTwo = 8000\n"
		"    OpacityEnd = 0\n"
		"    StartingDelay = 0\n"
		"    Lifetime = 90000\n"
		"  End\n"
		"End\n");
	REQUIRE(errs.empty());
	REQUIRE(playback.playFXList("FX_TaintSpellFX1", Coord3D{ 250.0f, 250.0f, 0.0f }, false));
	REQUIRE(playback.decals().size() == 1u);
	CHECK(playback.decals()[0].desc.shader == DYNAMIC_DECAL_SHADER_ALPHA);

	// trunc(5000 * 0.03) = 150 fade-in frames -> frame 150 holds OpacityPeak = 200
	for (int s = 0; s < 150; ++s)
	{
		playback.step();
	}
	REQUIRE(playback.decals().size() == 1u);
	CHECK(playback.decals()[0].opacityAt(playback.clientFrame()) == 200);

	const TerrainScorchBuffer::Terrain terrain = steppedTerrain(64, 64, 4);
	const std::vector<const DecalFx *> group = { &playback.decals()[0] };
	DynamicDecalMesh built;
	BuildDynamicDecalMesh(group, playback.clientFrame(), &terrain, nullptr, built);

	CHECK(built.decalsBuilt == 1);
	// [250 - 210, 250 + 210] = [40, 460] -> cells [4 + 4 .. 46 + 4] = [8 .. 50]: 43 x 43 = 1849 vertices, 42 x 42 * 6 = 10584 indices
	REQUIRE(built.vertices.size() == 43u * 43u);
	REQUIRE(built.indices.size() == 42u * 42u * 6u);

	const float expectedAlpha = 200.0f / 255.0f;
	for (int row = 0; row < 43; ++row)
	{
		for (int col = 0; col < 43; ++col)
		{
			const int i = 8 + col;
			const int j = 8 + row;
			const DynamicDecalVertex &v = built.vertices[(size_t)row * 43u + (size_t)col];
			const float wx = (float)(i - 4) * 10.0f;
			const float wy = (float)(j - 4) * 10.0f;
			const float wz = (float)terrain.heights[(size_t)j * 64u + (size_t)i] * (10.0f / 256.0f);
			CHECK(v.position.x == wx);
			CHECK(v.position.y == wy);
			CHECK(v.position.z == wz);
			CHECK(v.r == 1.0f);
			CHECK(v.g == 1.0f);
			CHECK(v.b == 1.0f);
			CHECK(v.a == expectedAlpha);
		}
	}

	// Exact corner & center planar UVs (uVector = (1, 0, 0), vVector = (0, -1, 0)):
	// SW (40, 40) -> (0, 1); SE (460, 40) -> (1, 1); NW (40, 460) -> (0, 0); NE (460, 460) -> (1, 0); Center (250, 250) -> (0.5, 0.5)
	CHECK(built.vertices[0].u == 0.0f);
	CHECK(built.vertices[0].v == 1.0f);
	CHECK(built.vertices[42].u == 1.0f);
	CHECK(built.vertices[42].v == 1.0f);
	CHECK(built.vertices[42 * 43].u == 0.0f);
	CHECK(built.vertices[42 * 43].v == 0.0f);
	CHECK(built.vertices[42 * 43 + 42].u == 1.0f);
	CHECK(built.vertices[42 * 43 + 42].v == 0.0f);
	CHECK(built.vertices[21 * 43 + 21].u == 0.5f);
	CHECK(built.vertices[21 * 43 + 21].v == 0.5f);

	// Cell (8, 8) has (8 + 8) & 1 == 0 (unflipped): { 0, 44, 43, 0, 1, 44 }
	CHECK(built.indices[0] == 0);
	CHECK(built.indices[1] == 44);
	CHECK(built.indices[2] == 43);
	CHECK(built.indices[3] == 0);
	CHECK(built.indices[4] == 1);
	CHECK(built.indices[5] == 44);

	// Cell (9, 8) has (9 + 8) & 1 == 1 (flipped, RW 0x50D334 -> RW 0x4AD985): { 2, 44, 1, 2, 45, 44 }
	CHECK(built.indices[6] == 2);
	CHECK(built.indices[7] == 44);
	CHECK(built.indices[8] == 1);
	CHECK(built.indices[9] == 2);
	CHECK(built.indices[10] == 45);
	CHECK(built.indices[11] == 44);
}

TEST_CASE("PORT-176: DynamicDecal rotated axes, map-edge clamping, symmetric 104-vertex trim and zero-opacity skip follow queueDecal (RW 0x508E7A, 0x50CE63 .. 0x50CF88)")
{
	// 1. 90-degree rotation (yaw = pi / 2): RW 0x508E7A gives uVector = (0, 1, 0), vVector = (1, 0, 0)
	DecalFx rot;
	rot.desc.name = "exmordorquake02";
	rot.desc.size = 40.0f;
	rot.desc.position = Coord3D{ 50.0f, 50.0f, 0.0f };
	rot.desc.yaw = 3.14159265358979323846f * 0.5f;
	rot.desc.opacityStart = 255;
	rot.desc.opacityPeak = 255;
	rot.desc.opacityEnd = 255;
	rot.desc.lifetimeFrames = 100;

	DynamicDecalMesh builtRot;
	BuildDynamicDecalMesh({ &rot }, 0, nullptr, [](float x, float y) { return x * 0.25f + y * 0.5f; }, builtRot);
	// [30 .. 70] -> [3 .. 7] = 5 x 5 vertices on the viewer fallback 10-unit grid
	REQUIRE(builtRot.vertices.size() == 25u);
	// East midpoint (70, 50): dx = +20, dy = 0 -> u = 0.5, v = +20/40 + 0.5 = 1.0
	const DynamicDecalVertex &eastMid = builtRot.vertices[2 * 5 + 4];
	CHECK(eastMid.position.x == 70.0f);
	CHECK(eastMid.position.y == 50.0f);
	CHECK(eastMid.position.z == 70.0f * 0.25f + 50.0f * 0.5f);
	CHECK(eastMid.u == doctest::Approx(0.5f)); // tolerance: IEEE-754 cos(pi/2) rounding
	CHECK(eastMid.v == doctest::Approx(1.0f)); // tolerance: IEEE-754 sin(pi/2) rounding
	// North midpoint (50, 70): dx = 0, dy = +20 -> u = +20/40 + 0.5 = 1.0, v = 0.5
	const DynamicDecalVertex &northMid = builtRot.vertices[4 * 5 + 2];
	CHECK(northMid.u == doctest::Approx(1.0f)); // tolerance: IEEE-754 sin(pi/2) rounding
	CHECK(northMid.v == doctest::Approx(0.5f)); // tolerance: IEEE-754 cos(pi/2) rounding

	// 2. Symmetric 104-vertex cap per axis (RW 0x50CEFC .. 0x50CF88): Size = 1500 -> 151 raw verts trimmed to 104 (start += 23, end -= 24)
	TerrainScorchBuffer::Terrain wide = steppedTerrain(200, 200, 0);
	DecalFx huge = rot;
	huge.desc.yaw = 0.0f;
	huge.desc.size = 1500.0f;
	huge.desc.position = Coord3D{ 1000.0f, 1000.0f, 0.0f };
	DynamicDecalMesh builtHuge;
	BuildDynamicDecalMesh({ &huge }, 0, &wide, nullptr, builtHuge);
	// Raw [25 .. 175] (151 verts): extra = 47 -> start = 25 + 23 = 48 (480.0), end = 175 - 24 = 151 (1510.0) -> 104 x 104 = 10816 verts
	REQUIRE(builtHuge.vertices.size() == 104u * 104u);
	CHECK(builtHuge.vertices.front().position.x == 480.0f);
	CHECK(builtHuge.vertices.front().position.y == 480.0f);
	CHECK(builtHuge.vertices.back().position.x == 1510.0f);
	CHECK(builtHuge.vertices.back().position.y == 1510.0f);

	// 3. Map-edge clamping (RW 0x50CE63 .. 0x50CEEB) and zero-opacity skip during StartingDelay
	TerrainScorchBuffer::Terrain small = steppedTerrain(12, 12, 2);
	DecalFx delayed = huge;
	delayed.desc.size = 200.0f;
	delayed.desc.position = Coord3D{ 0.0f, 0.0f, 0.0f };
	delayed.desc.startDelayFrames = 5;
	DynamicDecalMesh builtDelayed;
	BuildDynamicDecalMesh({ &delayed }, 3, &small, nullptr, builtDelayed);
	CHECK(builtDelayed.decalsBuilt == 0);
	CHECK(builtDelayed.vertices.empty());

	BuildDynamicDecalMesh({ &delayed }, 5, &small, nullptr, builtDelayed);
	CHECK(builtDelayed.decalsBuilt == 1);
	// [-100 .. 100] -> [-10 + 2 .. 10 + 2] = [-8 .. 12], clamped to [0 .. 11] = 12 x 12 = 144 vertices
	CHECK(builtDelayed.vertices.size() == 144u);
	CHECK(builtDelayed.vertices.front().position.x == -20.0f);
	CHECK(builtDelayed.vertices.back().position.x == 90.0f);
}

TEST_CASE("PORT-176: DynamicDecal shader modes, projection depth bias 9.0 and render priority below particles follow flushDecals (RW 0x5E05EA, 0x5098F6, 0x509FCF, 0x4E2F85)")
{
	Win32BIGFileSystem fs;
	FXPlayback playback(fs, RandomAlgorithm::RotWK_GameDat_LCG);
	const std::vector<std::string> errs = playback.loadIniText(
		"fxlist.ini",
		"FXList FX_AllDecalShaders\n"
		"  DynamicDecal\n"
		"    DecalName = d_alpha\n"
		"    Shader = ALPHA\n"
		"    Size = 20\n"
		"    OpacityPeak = 200\n"
		"    Lifetime = 1000\n"
		"  End\n"
		"  DynamicDecal\n"
		"    DecalName = d_add\n"
		"    Shader = ADDITIVE\n"
		"    Size = 20\n"
		"    OpacityPeak = 200\n"
		"    Lifetime = 1000\n"
		"  End\n"
		"  DynamicDecal\n"
		"    DecalName = d_sub\n"
		"    Shader = SUBTRACT\n"
		"    Size = 20\n"
		"    OpacityPeak = 200\n"
		"    Lifetime = 1000\n"
		"  End\n"
		"End\n");
	REQUIRE(errs.empty());
	REQUIRE(playback.playFXList("FX_AllDecalShaders", Coord3D{ 0.0f, 0.0f, 0.0f }, false));
	REQUIRE(playback.decals().size() == 3u);
	CHECK(NormalizeDynamicDecalShader(playback.decals()[0].desc.shader) == DYNAMIC_DECAL_SHADER_ALPHA);
	CHECK(NormalizeDynamicDecalShader(playback.decals()[1].desc.shader) == DYNAMIC_DECAL_SHADER_ADDITIVE);
	CHECK(NormalizeDynamicDecalShader(playback.decals()[2].desc.shader) == DYNAMIC_DECAL_SHADER_SUBTRACT);
	CHECK(NormalizeDynamicDecalShader(99) == DYNAMIC_DECAL_SHADER_ALPHA);

	CHECK(DynamicDecalMesh::kDepthBias == 9.0f);
	CHECK(DynamicDecalMesh::depthBiasViewScale() == TerrainScorchBuffer::depthBiasViewScale(9.0f));
	CHECK(DynamicDecalMesh::kRenderPriority == FXParticleSystem::kTrackRenderPriority);
	CHECK(DynamicDecalMesh::kRenderPriority == -123);
	CHECK(FXParticleSystem::kParticleRenderPriorityMin == -122);
}

TEST_CASE("PORT-176: packed particle/decal JPG + PNG alpha merge copies L8, LA8 and RGBA8 mask bytes into alpha without altering JPG RGB (RW 0x530D29, 0x530E48 .. 0x530FB8)")
{
	const std::set<std::string> files = {
		"art\\compiledtextures\\ex\\exmordorquake02.jpg",
		"art\\compiledtextures\\ex\\exmordorquake02.png",
	};
	const ParticleTextureResolution res = ResolveParticleTexture("exmordorquake02", [&](const std::string &p) { return files.count(p) != 0; });
	CHECK(res.Found == true);
	CHECK(res.Kind == ParticleTextureResolution::JPG);
	CHECK(res.Path == "art\\compiledtextures\\ex\\exmordorquake02.jpg");
	CHECK(res.AuxPngPath == "art\\compiledtextures\\ex\\exmordorquake02.png");

	// 2x2 soft-edged mask: corner 0 (transparent), soft edge 96, soft edge 180, center 255
	const std::uint8_t maskValues[4] = { 0, 96, 180, 255 };
	auto makeBaseRgba = []() {
		return std::vector<std::uint8_t>{
			40, 30, 20, 255,
			50, 35, 25, 255,
			60, 45, 30, 255,
			70, 55, 35, 255,
		};
	};

	// 1. L8 / A8 (1 byte/texel: RW 0x530F11 / 0x530F16) — preserves soft circular mask when PNG is 8-bit grayscale
	{
		std::vector<std::uint8_t> rgba = makeBaseRgba();
		const std::vector<std::uint8_t> l8(maskValues, maskValues + 4);
		REQUIRE(MergeParticleJpgAlpha(rgba.data(), 2, 2, l8.data(), l8.size(), ParticleAuxAlphaFormat::L8_OR_A8));
		for (size_t i = 0; i < 4; ++i)
		{
			CHECK(rgba[i * 4 + 0] == (std::uint8_t)(40 + i * 10));
			CHECK(rgba[i * 4 + 3] == maskValues[i]);
		}
	}

	// 2. LA8 (2 bytes/texel: luminance + alpha)
	{
		std::vector<std::uint8_t> rgba = makeBaseRgba();
		const std::vector<std::uint8_t> la8 = { 12, maskValues[0], 34, maskValues[1], 56, maskValues[2], 78, maskValues[3] };
		REQUIRE(MergeParticleJpgAlpha(rgba.data(), 2, 2, la8.data(), la8.size(), ParticleAuxAlphaFormat::LA8));
		for (size_t i = 0; i < 4; ++i)
		{
			CHECK(rgba[i * 4 + 0] == (std::uint8_t)(40 + i * 10));
			CHECK(rgba[i * 4 + 3] == maskValues[i]);
		}
	}

	// 3. RGBA8 (4 bytes/texel: RW 0x530F1B — non-white PNG RGB must NOT multiply JPG RGB, RW 0x530F84 .. 0x530F9F)
	{
		std::vector<std::uint8_t> rgba = makeBaseRgba();
		const std::vector<std::uint8_t> rgbaMask = {
			0, 0, 0, maskValues[0],
			10, 20, 30, maskValues[1],
			40, 50, 60, maskValues[2],
			70, 80, 90, maskValues[3],
		};
		REQUIRE(MergeParticleJpgAlpha(rgba.data(), 2, 2, rgbaMask.data(), rgbaMask.size(), ParticleAuxAlphaFormat::RGBA8));
		for (size_t i = 0; i < 4; ++i)
		{
			CHECK(rgba[i * 4 + 0] == (std::uint8_t)(40 + i * 10));
			CHECK(rgba[i * 4 + 3] == maskValues[i]);
		}
	}

	// 4. Reject mismatched sizes without touching RGB/alpha
	{
		std::vector<std::uint8_t> rgba = makeBaseRgba();
		const std::vector<std::uint8_t> bad = { 0, 1, 2 };
		CHECK(MergeParticleJpgAlpha(rgba.data(), 2, 2, bad.data(), bad.size(), ParticleAuxAlphaFormat::L8_OR_A8) == false);
		CHECK(rgba[3] == 255);
	}
}

TEST_CASE("PORT-176: stop S-7344 for unported DynamicDecal bridge-deck projection and viewer ground-grid fallback is registered in docs/STOPS.md")
{
	std::ifstream in(std::string(OPENBFME_DOCS_DIR) + "/STOPS.md", std::ios::binary);
	REQUIRE_MESSAGE(static_cast<bool>(in), "cannot open docs/STOPS.md");
	std::ostringstream ss;
	ss << in.rdbuf();
	const std::string doc = ss.str();
	CHECK(doc.find("| S-7344 | code |") != std::string::npos);
	CHECK(doc.find("RW 0x50C880 .. 0x50CD20") != std::string::npos);
}

TEST_SUITE_END();

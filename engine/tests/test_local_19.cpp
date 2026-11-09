// OpenBFME unit tests: LOCAL-19 object shadows parity (issue #146, TOP20 #5).
//
// RotWK addresses:
//   W3DProjectedShadowManager::addShadow: RW 0x50BF6A
//     RW 0x50BF7B: UseShadowDecals or ShadowOverrideLODVisibility gate
//     RW 0x50BFF3..0x50C013: test ShadowSizeX == 0 and ShadowSizeY == 0
//     RW 0x50C01D: vslot 0x110 RenderObjClass::Get_Obj_Space_Bounding_Box(box)
//     RW 0x50C034..0x50C041: sizeX = box.Extent.X * 2.0 (constant RW 0xBD889C)
//     RW 0x50C054..0x50C061: sizeY = box.Extent.Y * -2.0 (constant RW 0xBDAC78, negative Y)
//   Decal opacity fades:
//     RW 0x4B1BC6: tests style 0x400 (ALPHA_DYNAMIC), 0x800 (ADDITIVE_DYNAMIC), 0x10000
//     RW 0x73228B: setOpacity (start / peak / end)
//     RW 0x7323CE: setOpacityCurve(0, -1, start, fadeInFrames, peak, -1, fadeOutFrames, end)
//     RW 0xD9F624: 0.03f ms to frames conversion (30 logic fps / 1000 ms)
//     RW 0x50B860: W3DProjectedShadowManager::update() per-frame curve evaluator
//   Shadow-map casters:
//     RW 0x601D4C: UseShadowMapping = Yes clears UseShadowVolumes and UseShadowDecals
//     RW 0x449DF5, RW 0x47D5C9: W3DShadowMapManager::UpdateShadowMap calls SceneClass::Render
//     RW 0x46FE84..0x47009F: SceneClass::Render_Objects renders all visible opaque 3D objects
//       with Shadow.fx into the shadow map; no separate whitelist. At High, all units with
//       valid shadow (MapMask | kDecalMask) cast into the shadow map unless ShadowForceDisable.

#include "doctest.h"

#include "EnnorathScenario.h"
#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "Common/ModelState.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cmath>
#include <string>
#include <vector>

namespace
{

struct MockDecalAtlas
{
	static constexpr int kW = 64, kH = 64;
	std::vector<std::uint8_t> px = std::vector<std::uint8_t>((size_t)kW * kH * 4, 128);

	DecalImage image() const { return { kW, kH, px.data() }; }

	static bool rect(const std::string &name, MergeDecalAtlasRect &r)
	{
		if (name == "shadowi" || name == "shadowi.tga" || name == "shadow" || name == "shadow.tga")
		{
			r = { 0, 0, 64, 64 };
			return true;
		}
		return false;
	}
};

} // namespace

TEST_CASE("LOCAL-19 decal size from model box with negative Y (RW 0x50BF6A, RW 0x50BFF3..0x50C066)")
{
	objtest::World w;
	const std::string ini =
		"Object PalantirShardMock\n"
		"  Shadow = SHADOW_DECAL\n"
		"End\n"
		"Object ExplicitSizesMock\n"
		"  Shadow = SHADOW_DECAL\n"
		"  ShadowSizeX = 35.0\n"
		"  ShadowSizeY = 25.0\n"
		"End\n"
		"Object ZeroSizeYMock\n"
		"  Shadow = SHADOW_DECAL\n"
		"  ShadowSizeX = 40.0\n"
		"End\n";

	REQUIRE(w.load(ini).empty());

	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;

	// 1. PalantirShard has Shadow = SHADOW_DECAL and no ShadowSizeX / ShadowSizeY.
	// resolveDecalShadow parses sizes as 0.0f without substituting sizeX for sizeY (RW 0x4C502B..0x4C509D).
	const ThingTemplate *shard = w.get("PalantirShardMock");
	REQUIRE(shard != nullptr);
	ObjectShadow::DecalShadowDescriptor desc;
	CHECK(ObjectShadow::resolveDecalShadow(shard, nullptr, nullptr, desc));
	CHECK(desc.enabled);
	CHECK(desc.texture == "shadow");
	CHECK(desc.sizeX == 0.0f);
	CHECK(desc.sizeY == 0.0f);

	// 2. buildDrawableDecalShadow calls fallbackSize when sizeX or sizeY == 0.
	// Fallback queries box extents: Extent.X = 46.417995f, Extent.Y = 8.264244f (from EXPalantirShard).
	// sizeX becomes box.Extent.X * 2.0 = 92.83599f (constant RW 0xBD889C).
	// sizeY becomes box.Extent.Y * -2.0 = -16.528488f (constant RW 0xBDAC78, negative!).
	Drawable dShard(services, shard, (DrawableID)1, (ObjectID)10, {}, problems);
	const Coord3D pos{ 100.0f, 200.0f, 10.0f };
	dShard.setPosition(&pos);

	SelectionDecal sd;
	auto fallback = [](float &sx, float &sy) {
		const float extX = 46.417995f;
		const float extY = 8.264244f;
		if (sx == 0.0f) sx = extX * 2.0f;
		if (sy == 0.0f) sy = extY * -2.0f;
	};

	CHECK(ObjectShadow::buildDrawableDecalShadow(dShard, sd, true, fallback));
	CHECK(sd.size == doctest::Approx(92.83599f)); // tolerance: float bounding box extent multiply by 2.0
	CHECK(sd.sizeY == doctest::Approx(-16.528488f)); // tolerance: float bounding box extent multiply by -2.0
	CHECK(sd.sizeY < 0.0f); // tolerance: negative Y convention in RW 0x50C054

	// 3. Grid building preserves negative sizeY in texel record and uses abs for extent and circumcircle
	MockDecalAtlas atlas;
	MergeDecalGrid grid;
	std::string gridErr;
	CHECK(BuildMergeDecalGrid({ sd }, MockDecalAtlas::rect, SelectionDecalSettings{}, grid, &gridErr));
	// p[14] is sizeY (texel di + 3, z component)
	CHECK(grid.texels[3 * 4 + 2] == doctest::Approx(-16.528488f)); // tolerance: float bounding box extent in texel

	// 4. Object with explicit sizes does not query fallback
	const ThingTemplate *explicitObj = w.get("ExplicitSizesMock");
	REQUIRE(explicitObj != nullptr);
	Drawable dExplicit(services, explicitObj, (DrawableID)2, (ObjectID)11, {}, problems);
	dExplicit.setPosition(&pos);

	SelectionDecal sdExplicit;
	bool fallbackCalled = false;
	auto unusedFallback = [&fallbackCalled](float &sx, float &sy) {
		fallbackCalled = true;
	};
	CHECK(ObjectShadow::buildDrawableDecalShadow(dExplicit, sdExplicit, true, unusedFallback));
	CHECK_FALSE(fallbackCalled);
	CHECK(sdExplicit.size == 35.0f);
	CHECK(sdExplicit.sizeY == 25.0f);

	// 5. Object with only ShadowSizeX queries fallback for sizeY only
	const ThingTemplate *zeroY = w.get("ZeroSizeYMock");
	REQUIRE(zeroY != nullptr);
	Drawable dZeroY(services, zeroY, (DrawableID)3, (ObjectID)12, {}, problems);
	dZeroY.setPosition(&pos);

	SelectionDecal sdZeroY;
	CHECK(ObjectShadow::buildDrawableDecalShadow(dZeroY, sdZeroY, true, fallback));
	CHECK(sdZeroY.size == 40.0f); // kept from INI
	CHECK(sdZeroY.sizeY == doctest::Approx(-16.528488f)); // tolerance: float bounding box extent multiply by -2.0
}

TEST_CASE("LOCAL-19 getModelBoundingBox calculation from mesh vertices (RW 0x50C01D)")
{
	// Build synthetic prototype with mesh vertices
	RenderObjPrototype proto;
	proto.Name = "TestModel";
	proto.Type = RenderObjPrototype::PROTO_MESH;
	proto.DefaultTree = HTreeClass::Make_Default();
	proto.Tree = &proto.DefaultTree;

	MeshModelClass mesh;
	mesh.MeshName = "TestMesh";
	// Define vertices matching EXPalantirShard bounding box:
	// X: [-45.9961f, 46.8399f], Y: [-10.8752f, 5.6533f], Z: [-7.912f, 7.720f]
	mesh.Vertices.push_back({ -45.9961f, -10.8752f, -7.912f });
	mesh.Vertices.push_back({  46.8399f,   5.6533f,  7.720f });
	mesh.Header.Min = { -45.9961f, -10.8752f, -7.912f };
	mesh.Header.Max = {  46.8399f,   5.6533f,  7.720f };

	RenderSubObject sub;
	sub.Type = RenderSubObject::SUB_MESH;
	sub.Mesh = &mesh;
	sub.BoneIndex = 0;
	proto.SubObjects.push_back(sub);

	float extX = 0.0f, extY = 0.0f;
	CHECK(ObjectShadow::getModelBoundingBox(&proto, extX, extY));
	CHECK(extX == doctest::Approx((46.8399f - (-45.9961f)) * 0.5f)); // tolerance: float vertex bounds half-extent
	CHECK(extY == doctest::Approx((5.6533f - (-10.8752f)) * 0.5f)); // tolerance: float vertex bounds half-extent

	float sizeX = 0.0f, sizeY = 0.0f;
	if (sizeX == 0.0f) sizeX = extX * 2.0f;
	if (sizeY == 0.0f) sizeY = extY * -2.0f;
	CHECK(sizeX == doctest::Approx(92.836f)); // tolerance: float bounding box extent multiply by 2.0
	CHECK(sizeY == doctest::Approx(-16.5285f)); // tolerance: float bounding box extent multiply by -2.0
	CHECK(sizeY < 0.0f); // tolerance: negative Y convention in RW 0x50C054
}

TEST_CASE("LOCAL-19 decal opacity fade curve for dynamic shadow decals (RW 0x4B1BC6, RW 0x7323CE, RW 0x50B860)")
{
	ObjectShadow::DecalShadowDescriptor dynamicDesc;
	dynamicDesc.enabled = true;
	dynamicDesc.style = 0x0400u; // SHADOW_ALPHA_DECAL_DYNAMIC (RW 0x4B1BC6)
	dynamicDesc.opacityStart = 50;
	dynamicDesc.opacityPeak = 250;
	dynamicDesc.opacityEnd = 10;
	dynamicDesc.opacityFadeInTime = 1000.0f; // 1000 ms * 0.03 = 30 frames (RW 0xD9F624)
	dynamicDesc.opacityFadeOutTime = 500.0f;  // 500 ms * 0.03 = 15 frames

	// 1. Static decals (0x1, 0x20, 0x40) ignore curves and stay at opacityPeak (RW 0x73228B)
	ObjectShadow::DecalShadowDescriptor staticDesc = dynamicDesc;
	staticDesc.style = 0x0001u; // SHADOW_DECAL
	CHECK(ObjectShadow::evaluateDecalOpacity(staticDesc, 0) == 250u);
	CHECK(ObjectShadow::evaluateDecalOpacity(staticDesc, 15) == 250u);
	CHECK(ObjectShadow::evaluateDecalOpacity(staticDesc, 100) == 250u);

	// 2. Dynamic decal: frame 0 starts at opacityStart (50)
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 0, 0) == 50u);

	// 3. Frame 15 (halfway through 30 frames fade-in): 50 + 0.5 * (250 - 50) = 150
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 15, 0) == 150u);

	// 4. Frame 30 (end of fade-in): peak opacity (250)
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 30, 0) == 250u);

	// 5. Frame 100: holds at peak indefinitely (peakHold == -1, RW 0x7323CE)
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 100, 0) == 250u);

	// 6. Fade-out triggered at frame 100: fades from peak (250) to end (10) over 15 frames
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 100, 0, 100) == 250u);
	// Frame 107 (approx half of 15 frames): 250 + (7/15) * (10 - 250) = 250 - 112 = 138
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 107, 0, 100) == 138u);
	// Frame 115 (end of fade-out): end opacity (10)
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 115, 0, 100) == 10u);
	// Frame 200: remains at end opacity
	CHECK(ObjectShadow::evaluateDecalOpacity(dynamicDesc, 200, 0, 100) == 10u);
}

TEST_CASE("LOCAL-19 shadow-map casters at High vs Medium (RW 0x46FE84..0x47009F, TOP20 #5)")
{
	objtest::World w;
	const std::string ini =
		"Object DecalInfantry\n"
		"  Shadow = SHADOW_DECAL\n"
		"End\n"
		"Object VolumeCaster\n"
		"  Shadow = SHADOW_VOLUME\n"
		"End\n"
		"Object AlphaDecalHero\n"
		"  Shadow = SHADOW_ALPHA_DECAL\n"
		"End\n"
		"Object NoShadowObj\n"
		"End\n";

	REQUIRE(w.load(ini).empty());

	const ThingTemplate *infantry = w.get("DecalInfantry");
	const ThingTemplate *volume = w.get("VolumeCaster");
	const ThingTemplate *alphaHero = w.get("AlphaDecalHero");
	const ThingTemplate *none = w.get("NoShadowObj");
	REQUIRE(infantry != nullptr);
	REQUIRE(volume != nullptr);
	REQUIRE(alphaHero != nullptr);
	REQUIRE(none != nullptr);

	// 1. Medium / Low detail (useShadowMapping = false):
	// Only volume types cast shadow map; decal shadow objects do NOT cast shadow map (they draw decals).
	CHECK_FALSE(ObjectShadow::castsMap(infantry, nullptr, nullptr, false));
	CHECK(ObjectShadow::castsMap(volume, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(alphaHero, nullptr, nullptr, false));
	CHECK_FALSE(ObjectShadow::castsMap(none, nullptr, nullptr, false));

	// 2. High / UltraHigh detail (useShadowMapping = true, RW 0x601D4C, RW 0x46FE84..0x47009F):
	// Decal shadows are disabled (except ShadowOverrideLODVisibility); ALL objects with shadows
	// cast into the directional shadow map!
	CHECK(ObjectShadow::castsMap(infantry, nullptr, nullptr, true));
	CHECK(ObjectShadow::castsMap(volume, nullptr, nullptr, true));
	CHECK(ObjectShadow::castsMap(alphaHero, nullptr, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(none, nullptr, nullptr, true)); // object with no shadow still casts nothing

	// 3. ShadowForceDisable = Yes skips shadow map even at High (RW 0x4C4EF8 / RW 0x4C5013)
	W3DModelDrawModuleData disabledModule;
	disabledModule.m_shadowForceDisable = true;
	CHECK_FALSE(ObjectShadow::castsMap(infantry, &disabledModule, nullptr, true));
	CHECK_FALSE(ObjectShadow::castsMap(volume, &disabledModule, nullptr, true));
}

TEST_CASE("LOCAL-19 Ennorath scenario: infantry casts shadow map at High, draws decal at Medium")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-19 Ennorath scenario decal and shadow map tests"))
	{
		return;
	}

	// 1. PalantirShard in real game data: resolves decal shadow with sizeX = 0, sizeY = 0
	const ThingTemplate *shard = world.shared.world->things().findTemplate("PalantirShard");
	if (shard != nullptr)
	{
		ObjectShadow::DecalShadowDescriptor shardDesc;
		CHECK(ObjectShadow::resolveDecalShadow(shard, nullptr, nullptr, shardDesc));
		CHECK(shardDesc.enabled);
		CHECK(shardDesc.sizeX == 0.0f);
		CHECK(shardDesc.sizeY == 0.0f);
	}

	// 2. Ennorath infantry (GondorFighter):
	// Has Shadow = SHADOW_DECAL.
	// At Medium: does not cast shadow map (draws decal shadow instead).
	// At High: casts shadow map (TOP20 #5 fix)!
	const ThingTemplate *fighter = world.shared.world->things().findTemplate("GondorFighter");
	REQUIRE(fighter != nullptr);

	CHECK_FALSE(ObjectShadow::castsMap(fighter, nullptr, nullptr, false)); // Medium: decal only
	CHECK(ObjectShadow::castsMap(fighter, nullptr, nullptr, true));       // High: casts shadow map!
}

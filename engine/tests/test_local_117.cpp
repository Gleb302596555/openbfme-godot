// OpenBFME unit tests: LOCAL-117 (issue #801) — shadows on Ultra: trees (P-166), Aragorn (P-170), mumak body (P-83).
//
// TARGET FACTS (RotWK game.dat 2.01, read with tools/mod/rw.py):
//   1. Trees on Ultra (P-166):
//      - W3DShadowMapManager::UpdateShadowMap (RW 0x47D5C9, called from RW 0x449DF5 when GlobalData+0x62
//        UseShadowMapping != 0) raises WW3D::IsCurrentlyRenderingShadowMap = 1 (RW 0x47D6A8, global 0xdd1e44)
//        and renders the scene (RW 0x47D70C -> RTS3DScene::Customized_Render RW 0x46FE84 -> RTS3DScene::Flush RW 0x470F44).
//      - RTS3DScene::Flush calls BaseHeightMap::RenderTrees (RW 0x47156D -> RW 0x46B0A0) unconditionally during
//        the shadow map pass, drawing W3DTreeBuffer::drawTrees (RW 0x46B20A -> RW 0x4EC387 -> W3DVegetationBufferBase::draw
//        RW 0x4E5F64) and W3DShrubBuffer::drawShrubs (RW 0x46B233) regardless of ThingTemplate::Shadow.
//      - Ennorath naturetrees.ini: GenericInteractableOptimizedTree uses Shadow = SHADOW_DECAL, and 17 tree templates
//        (e.g. Tree06, TreeLothlorien01) omit Shadow altogether; all cast shadow-map shadows on Ultra.
//   2. Blended depth-writing meshes in shadow map (P-170 Aragorn, P-83 mumak body):
//      - DX8TextureCategoryClass::Render (RW 0x525E1F -> RW 0x5284C0..0x5284F7): when WW3D::IsCurrentlyRenderingShadowMap
//        (0xdd1e44) is set, depth-writing meshes switch to an alpha-test shadow shader ([0xdd42ac] if AlphaTest == ENABLE
//        (ShaderBits & 0xC000) == 0x4000 and Texturing == ENABLE (ShaderBits & 0xE0) == 0x20) or opaque shadow shader
//        ([0xdd42b0]), ignoring SRC_ALPHA / INV_SRC_ALPHA blend mode.
//      - Ennorath art/w3d/gu/guara_skn.w3d (GondorAragorn_mod, 6 body meshes) and art/w3d/ha/hamumak3.w3d (MUMAKIL_MESH,
//        HaradMumakilForOliphant) use depthMask=1, SRC_ALPHA / ONE_MINUS_SRC_ALPHA, alphaTest=1 (while HOUDA uses FX opaque).
//      - W3D_Shadow_Pass_Key converts such depth-writing blended surfaces to an opaque (alpha-tested) shadow-pass shader
//        for W3DInstancer::ShadowMM so Godot 4 renders them in the directional shadow pass.

#include "doctest.h"

#include "EnnorathScenario.h"
#include "ObjectTestUtil.h"

#include "GameClient/ObjectShadow.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/meshrender.h"
#include "Libraries/WWVegas/WW3D2/w3dshadergen.h"

#include <string>

using namespace objtest;

namespace
{
const char kLocal117Objects[] =
	"Object Local117TreeDecal\n"
	"  Shadow = SHADOW_DECAL\n"
	"End\n"
	"Object Local117TreeNoShadow\n"
	"End\n"
	"Object Local117TreeVolume\n"
	"  Shadow = SHADOW_VOLUME\n"
	"End\n"
	"Object Local117PropNoShadow\n"
	"End\n"
	"Object Local117InfantryDecal\n"
	"  Shadow = SHADOW_DECAL\n"
	"End\n";
} // namespace

TEST_CASE("LOCAL-117 tree shadow-map casting at Ultra vs Medium (RW 0x46B0A0, RW 0x4EC387, RW 0x47D5C9)")
{
	World w;
	REQUIRE(w.load(kLocal117Objects).empty());

	const ThingTemplate *treeDecal = w.get("Local117TreeDecal");
	const ThingTemplate *treeNoShadow = w.get("Local117TreeNoShadow");
	const ThingTemplate *treeVolume = w.get("Local117TreeVolume");
	const ThingTemplate *propNoShadow = w.get("Local117PropNoShadow");
	const ThingTemplate *infantryDecal = w.get("Local117InfantryDecal");
	REQUIRE(treeDecal != nullptr);
	REQUIRE(treeNoShadow != nullptr);
	REQUIRE(treeVolume != nullptr);
	REQUIRE(propNoShadow != nullptr);
	REQUIRE(infantryDecal != nullptr);

	// Ultra / High (UseShadowMapping = true, RW 0x47D5C9 -> RW 0x46B0A0 -> RW 0x4EC387):
	// W3DTreeDraw trees cast into the shadow map regardless of ThingTemplate::Shadow.
	CHECK(ObjectShadow::castsMap(treeDecal, nullptr, nullptr, true, true));
	CHECK(ObjectShadow::castsMap(treeNoShadow, nullptr, nullptr, true, true));
	CHECK(ObjectShadow::castsMap(treeVolume, nullptr, nullptr, true, true));

	// Medium (UseShadowMapping = false): tree draw falls back to MapMask (only volume shadows cast map).
	CHECK_FALSE(ObjectShadow::castsMap(treeDecal, nullptr, nullptr, false, true));
	CHECK_FALSE(ObjectShadow::castsMap(treeNoShadow, nullptr, nullptr, false, true));
	CHECK(ObjectShadow::castsMap(treeVolume, nullptr, nullptr, false, true));

	// Neighbouring behaviour: non-tree objects without Shadow still cast nothing on Ultra.
	CHECK_FALSE(ObjectShadow::castsMap(propNoShadow, nullptr, nullptr, true, false));
	CHECK(ObjectShadow::castsMap(infantryDecal, nullptr, nullptr, true, false));
	CHECK_FALSE(ObjectShadow::castsMap(infantryDecal, nullptr, nullptr, false, false));
}

TEST_CASE("LOCAL-117 shadow-pass shader key converts blended depth-writing surfaces to opaque (RW 0x525E1F, RW 0x5284C0..0x5284F7)")
{
	// Classic W3D shader with depthMask=1, SRC_ALPHA/INV_SRC_ALPHA, alphaTest=1 (Aragorn guara_skn, mumak MUMAKIL_MESH)
	W3DShaderKey blendedDepth;
	blendedDepth.Blend = W3D_GODOT_BLEND_MIX;
	blendedDepth.DepthWrite = true;
	blendedDepth.AlphaTest = true;
	blendedDepth.Skin = true;
	blendedDepth.Texturing = true;
	blendedDepth.Tex[0] = true;

	W3DShaderKey shadowKey;
	REQUIRE(W3D_Shadow_Pass_Key(blendedDepth, shadowKey));
	CHECK(shadowKey.Blend == W3D_GODOT_BLEND_OPAQUE);
	CHECK(shadowKey.DepthWrite);
	CHECK(shadowKey.AlphaTest);
	CHECK(shadowKey.Skin);
	CHECK(shadowKey.Tex[0]);
	CHECK_FALSE(shadowKey.AlphaOverride);
	CHECK_FALSE(shadowKey.SelectionDecals);

	const std::string visibleCode = Generate_W3D_Shader_Code(blendedDepth);
	const std::string shadowCode = Generate_W3D_Shader_Code(shadowKey);
	CHECK(visibleCode.find("blend_mix") != std::string::npos);
	CHECK(visibleCode.find("ALPHA =") != std::string::npos);
	// Shadow-pass shader must be opaque (no blend_mix, no ALPHA =) while preserving alpha-test discard (RW 0x5284DB..0x5284EB)
	CHECK(shadowCode.find("blend_mix") == std::string::npos);
	CHECK(shadowCode.find("ALPHA =") == std::string::npos);
	CHECK(shadowCode.find("discard;") != std::string::npos);

	// Neighbouring behaviour: already opaque surfaces need no shadow-pass override
	W3DShaderKey opaque;
	opaque.Blend = W3D_GODOT_BLEND_OPAQUE;
	opaque.DepthWrite = true;
	CHECK_FALSE(W3D_Shadow_Pass_Key(opaque, shadowKey));

	// Non-depth-writing additive/blended surfaces (e.g. Aragorn FORGED_BLADE glow, depthMask=0) do not get an opaque shadow pass
	W3DShaderKey additiveGlow;
	additiveGlow.Blend = W3D_GODOT_BLEND_ADD;
	additiveGlow.DepthWrite = false;
	CHECK_FALSE(W3D_Shadow_Pass_Key(additiveGlow, shadowKey));

	W3DShaderKey softAlpha;
	softAlpha.Blend = W3D_GODOT_BLEND_MIX;
	softAlpha.DepthWrite = false;
	CHECK_FALSE(W3D_Shadow_Pass_Key(softAlpha, shadowKey));

	// Floor decals and occlusion passes are excluded
	W3DShaderKey floorKey = blendedDepth;
	floorKey.Floor = true;
	CHECK_FALSE(W3D_Shadow_Pass_Key(floorKey, shadowKey));

	W3DShaderKey occKey = blendedDepth;
	occKey.OcclusionPass = W3D_OCCLUSION_PASS_MARK;
	CHECK_FALSE(W3D_Shadow_Pass_Key(occKey, shadowKey));

	// Opaque surface with AlphaOverride needs shadow-pass override (clears AlphaOverride so Godot renders shadow)
	W3DShaderKey fadedOpaque = opaque;
	fadedOpaque.AlphaOverride = true;
	REQUIRE(W3D_Shadow_Pass_Key(fadedOpaque, shadowKey));
	CHECK(shadowKey.Blend == W3D_GODOT_BLEND_OPAQUE);
	CHECK_FALSE(shadowKey.AlphaOverride);
}

TEST_CASE("LOCAL-117 ENNORATH scenario: GondorAragorn, HaradMumakil and map trees shadow casting on Ultra (RW 0x46B0A0, RW 0x525E1F)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-117 Ennorath scenario: Aragorn, mumak body, and trees shadows on Ultra"))
	{
		return;
	}

	// 1. Trees (naturetrees.ini): GenericInteractableOptimizedTree (Shadow = SHADOW_DECAL) and Tree06 (no Shadow field)
	const ThingTemplate *treeDecal = world.shared.world->things().findTemplate("GenericInteractableOptimizedTree");
	const ThingTemplate *treeNoShadow = world.shared.world->things().findTemplate("Tree06");
	REQUIRE(treeDecal != nullptr);
	REQUIRE(treeNoShadow != nullptr);
	CHECK(ObjectShadow::castsMap(treeDecal, nullptr, nullptr, true, true));
	CHECK(ObjectShadow::castsMap(treeNoShadow, nullptr, nullptr, true, true));
	CHECK_FALSE(ObjectShadow::castsMap(treeDecal, nullptr, nullptr, false, true));

	// 2. Aragorn (data/ini/object/goodfaction/units/gondor/gondoraragorn.ini -> guara_skn.w3d)
	const ThingTemplate *aragorn = world.shared.world->things().findTemplate("GondorAragorn");
	REQUIRE(aragorn != nullptr);
	CHECK(ObjectShadow::castsMap(aragorn, nullptr, nullptr, true, false));

	// 3. Mumak (data/ini/object/evilfaction/units/harad/haradmumakil.ini -> hamumak3.w3d)
	const ThingTemplate *mumak = world.shared.world->things().findTemplate("HaradMumakil");
	REQUIRE(mumak != nullptr);
	CHECK(ObjectShadow::castsMap(mumak, nullptr, nullptr, true, false));

	// Inspect real Ennorath W3D meshes for Aragorn (guara_skn) and Mumak (hamumak3)
	ArchiveW3DFileSource w3dSource(*world.merged.fs);
	WW3DAssetManager assets(w3dSource);
	auto checkModel = [&](const char *modelName, int &overrideCount, int &opaqueCount, int &nonDepthCount) {
		overrideCount = opaqueCount = nonDepthCount = 0;
		std::string loadErr;
		const RenderObjPrototype *proto = assets.Create_Render_Obj(modelName, &loadErr);
		REQUIRE_MESSAGE(proto != nullptr, "Failed to load Ennorath W3D " << modelName << ": " << loadErr);
		for (const RenderSubObject &sub : proto->SubObjects)
		{
			if (!sub.Mesh) continue;
			MeshRenderData rd;
			std::string err;
			REQUIRE_MESSAGE(Build_Mesh_Render_Data(*sub.Mesh, rd, &err), err);
			for (const MeshDrawSurface &surf : rd.Surfaces)
			{
				if (surf.Kind == MATERIAL_CLASSIC && surf.State.NeverDraws) continue;
				W3DShaderKey key;
				key.Skin = rd.Skin;
				key.Dual = rd.DualBone;
				if (surf.Kind == MATERIAL_CLASSIC)
				{
					key.Blend = surf.State.Blend;
					key.DepthWrite = surf.State.DepthWrite;
					key.DepthTestDisabled = surf.State.DepthTestDisabled;
					key.AlphaTest = surf.State.AlphaTest;
					key.Texturing = surf.State.Texturing;
				}
				else
				{
					key.Fx = true;
					key.Blend = W3D_GODOT_BLEND_OPAQUE;
					key.DepthWrite = true;
				}
				W3DShaderKey out;
				if (W3D_Shadow_Pass_Key(key, out))
				{
					++overrideCount;
				}
				else if (key.Blend == W3D_GODOT_BLEND_OPAQUE && key.DepthWrite)
				{
					++opaqueCount;
				}
				else if (!key.DepthWrite)
				{
					++nonDepthCount;
				}
			}
		}
	};

	// Aragorn guara_skn.w3d: 6 body meshes use blend_mix + depthMask=1 + alphaTest=1 -> shadow override; FORGED_BLADE has depthMask=0 -> skipped
	int araOverride = 0, araOpaque = 0, araNonDepth = 0;
	checkModel("guara_skn", araOverride, araOpaque, araNonDepth);
	CHECK(araOverride == 6);
	CHECK(araNonDepth == 1);

	// Mumak hamumak3.w3d: MUMAKIL_MESH (body) uses blend_mix + depthMask=1 + alphaTest=1 -> shadow override; HOUDA (howdah) is FX opaque
	int mumOverride = 0, mumOpaque = 0, mumNonDepth = 0;
	checkModel("hamumak3", mumOverride, mumOpaque, mumNonDepth);
	CHECK(mumOverride == 1);
	CHECK(mumOpaque == 2);
}

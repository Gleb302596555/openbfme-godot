// OpenBFME unit tests for Local 65 (issue #510, P-130):
//   * Isengard building foundations go under the black ground decal of Isengard citadels / fortresses
//     (IsenFloorEffect / IsenFloorEffectBig, SHADOW_ALPHA_DECAL_DYNAMIC fl_floor_isengard):
//     RTS3DScene::Flush (RW 0x470F44) runs W3DFloorBuffer::RenderFloors (RW 0x4E2F3A -> 0x4E53D3,
//     donor BFME2 W3DFloorBuffer::drawFloors) inside SceneClass::Render (RW 0x470F5E) BEFORE
//     W3DProjectedShadowManager::Render_Shadows (RW 0x471025 -> 0x50D92C). Thus k.Floor shaders
//     must composite selection_decals (sel_alpha_at / sel_merge_at) in gamma space without adding varyings.
//   * All 11 Ennorath factions' foundations fit terrain cleanly:
//     - k.Floor flift scales with MODEL_MATRIX (Scale != 1.0 on *BaseDefenceFoundation / BlueMtnWell_Edain)
//     - k.Floor heightmap sampling matches HeightMapMesh cell triangle planes (kUnflipped / kFlipped),
//       avoiding bilinear sag under terrain ridges
//     - All 11 factions' foundation W3Ds load and subdivide cleanly to <= 5.0 XY edges.

#include "doctest.h"
#include "EnnorathScenario.h"

#include "Common/AsciiString.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/ParticleDraw.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/HeightMapMesh.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/meshrender.h"
#include "Libraries/WWVegas/WW3D2/w3dshadergen.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{

int countVaryingLines(const std::string &shader)
{
	int count = 0;
	std::istringstream in(shader);
	std::string ln;
	while (std::getline(in, ln))
	{
		if (ln.rfind("varying ", 0) == 0)
		{
			++count;
		}
	}
	return count;
}

float evalTriangleHeight(float fx, float fy, float h00, float h10, float h01, float h11, bool flip)
{
	if (flip)
	{
		if (fx + fy <= 1.0f)
		{
			return h00 + fx * (h10 - h00) + fy * (h01 - h00);
		}
		return h11 + (1.0f - fx) * (h01 - h11) + (1.0f - fy) * (h10 - h11);
	}
	if (fx <= fy)
	{
		return h00 + fx * (h11 - h01) + fy * (h01 - h00);
	}
	return h00 + fx * (h10 - h00) + fy * (h11 - h10);
}

void checkMaxEdgeXY(const MeshRenderData &m, float limit, const std::string &label)
{
	REQUIRE(m.Surfaces.size() >= 1); // tolerance: renderable floor mesh has >= 1 surface
	for (const MeshDrawSurface &s : m.Surfaces)
	{
		REQUIRE(s.Indices.size() % 3 == 0);
		for (size_t i = 0; i + 2 < s.Indices.size(); i += 3)
		{
			const std::uint32_t idx[3] = { s.Indices[i], s.Indices[i + 1], s.Indices[i + 2] };
			for (int k = 0; k < 3; ++k)
			{
				const std::uint32_t a = idx[k];
				const std::uint32_t b = idx[(k + 1) % 3];
				const float dx = m.Position[b * 3 + 0] - m.Position[a * 3 + 0];
				const float dy = m.Position[b * 3 + 1] - m.Position[a * 3 + 1];
				const float len = std::sqrt(dx * dx + dy * dy);
				INFO(label << " edge=" << len);
				CHECK(len <= limit + 1e-3f); // tolerance: float32 subdivision edge bound
			}
		}
	}
}

} // namespace

TEST_CASE("LOCAL-65 #510: W3DShaderKey::Floor composites projected ground decals (IsenFloorEffect) without extra varyings")
{
	W3DShaderKey base;
	base.Blend = W3D_GODOT_BLEND_MIX;
	base.DepthWrite = false;
	base.Texturing = true;
	base.Tex[0] = true;

	W3DShaderKey floorKey = base;
	floorKey.Floor = true;

	const std::string baseCode = Generate_W3D_Shader_Code(base, W3DSamplerPolicy::Anisotropic);
	const std::string floorCode = Generate_W3D_Shader_Code(floorKey, W3DSamplerPolicy::Anisotropic);

	// Baseline non-floor transparent shader must not emit selection decals.
	CHECK(baseCode.find("selection_decals.gdshaderinc") == std::string::npos);

	// Floor decals must sample both alpha decals (SHADOW_ALPHA_DECAL_DYNAMIC IsenFloorEffect/Big) and merge decals in gamma space.
	CHECK(floorCode.find("#include \"res://shaders/selection_decals.gdshaderinc\"") != std::string::npos);
	CHECK(floorCode.find("sel_alpha_at(sel_at, false, ALBEDO)") != std::string::npos);
	CHECK(floorCode.find("sel_merge_at(sel_at, false)") != std::string::npos);
	CHECK(floorCode.find("ALBEDO = clamp(ALBEDO * (1.0 - sela.a) + sela.rgb, 0.0, 1.0);") != std::string::npos);
	CHECK(floorCode.find("ALBEDO = mix(ALBEDO, clamp(sel.rgb, 0.0, 1.0), sel.a);") != std::string::npos);
	CHECK(countVaryingLines(floorCode) == countVaryingLines(baseCode));

	// Occlusion pass must stay free of selection decal compositing and heightmap conformation.
	W3DShaderKey occlFloor = floorKey;
	occlFloor.OcclusionPass = W3D_OCCLUSION_PASS_COLOR;
	const std::string occlCode = Generate_W3D_Shader_Code(occlFloor, W3DSamplerPolicy::Anisotropic);
	CHECK(occlCode.find("selection_decals.gdshaderinc") == std::string::npos);
	CHECK(occlCode.find("sel_alpha_at") == std::string::npos);
}

TEST_CASE("LOCAL-65 #510 P-130: W3DShaderKey::Floor respects instance Scale in flift and samples exact terrain triangles")
{
	W3DShaderKey floorKey;
	floorKey.Blend = W3D_GODOT_BLEND_MIX;
	floorKey.DepthWrite = false;
	floorKey.Texturing = true;
	floorKey.Tex[0] = true;
	floorKey.Floor = true;

	const std::string floorCode = Generate_W3D_Shader_Code(floorKey, W3DSamplerPolicy::Anisotropic);

	// flift must account for MODEL_MATRIX scale (e.g. *BaseDefenceFoundation Scale = 0.7), not raw unscaled VERTEX.y.
	CHECK(floorCode.find("max(fw.y - MODEL_MATRIX[3].y, 0.0) + 0.15") != std::string::npos);
	CHECK(floorCode.find("max(VERTEX.y, 0.0)") == std::string::npos);

	// Exact cell triangle interpolation matching HeightMapMesh (kUnflipped / kFlipped) via texelFetch, preserving w3d_floor_hm sampler declaration.
	CHECK(floorCode.find("global uniform sampler2D w3d_floor_hm : filter_linear, repeat_disable;") != std::string::npos);
	CHECK(floorCode.find("texelFetch(w3d_floor_hm") != std::string::npos);
	CHECK(floorCode.find("fc00.g > 0.5") != std::string::npos);

	// Verify triangle vs bilinear height difference on a ridged cell (h00=10, h11=10, h10=0, h01=0):
	// bilinear averages to 5.0 (causing terrain ridge at 10.0 to poke 5 units through a floor), while exact triangle returns 10.0 on the diagonal.
	const float triUnflipped = evalTriangleHeight(0.5f, 0.5f, 10.0f, 0.0f, 0.0f, 10.0f, false);
	const float triFlipped = evalTriangleHeight(0.5f, 0.5f, 0.0f, 10.0f, 10.0f, 0.0f, true);
	CHECK(triUnflipped == doctest::Approx(10.0f)); // tolerance: float32 interpolation epsilon
	CHECK(triFlipped == doctest::Approx(10.0f)); // tolerance: float32 interpolation epsilon
}

TEST_CASE("LOCAL-65 #510: Ennorath IsenFloorEffect & IsenFloorEffectBig cover Isengard foundations as alpha decals")
{
	ennorathscenario::World ew;
	if (!ew.load("LOCAL-65 IsenFloorEffect over Isengard foundations"))
	{
		return;
	}

	structtest::Arena arena(ew.shared, "FactionIsengard", "FactionMen");

	// Verify Ennorath Isengard citadel/fortress decal templates and Isengard foundation templates (isengardcitadel.ini, isengardfortress.ini, campsandcastles.ini).
	const ThingTemplate *floorSmall = ew.shared.world->things().findTemplate("IsenFloorEffect");
	const ThingTemplate *floorBig = ew.shared.world->things().findTemplate("IsenFloorEffectBig");
	const ThingTemplate *bldgFound = ew.shared.world->things().findTemplate("IsengardBuildingFoundation");
	const ThingTemplate *defFound = ew.shared.world->things().findTemplate("IsengardBaseDefenceFoundation");
	const ThingTemplate *expPad = ew.shared.world->things().findTemplate("IsengardFortressExpansionPadCorner");
	REQUIRE(floorSmall != nullptr);
	REQUIRE(floorBig != nullptr);
	REQUIRE(bldgFound != nullptr);
	REQUIRE(defFound != nullptr);
	REQUIRE(expPad != nullptr);

	const FieldValue *scaleField = defFound->findField("Scale");
	REQUIRE(scaleField != nullptr);
	const float *defScale = std::get_if<float>(scaleField);
	REQUIRE(defScale != nullptr);
	CHECK(*defScale == doctest::Approx(0.7f)); // tolerance: float32 INI Scale precision

	ArchiveW3DFileSource w3dSource(*ew.merged.fs);
	WW3DAssetManager assets(w3dSource);
	DrawableManager drawables(assets, arena.logic);
	ClientEventRecorder events(arena.logic);
	arena.logic.setClientHooks(&events);
	struct UnbindHooks
	{
		GameLogic &logic;
		~UnbindHooks() { logic.setClientHooks(nullptr); }
	} unbind{ arena.logic };

	Object *decalBigObj = ew.place(arena, "IsenFloorEffectBig", 0, 500.0f, 500.0f);
	Object *padObj = ew.place(arena, "IsengardFortressExpansionPadCorner", 0, 620.0f, 620.0f);
	Object *bldgObj = ew.place(arena, "IsengardBuildingFoundation", 0, 580.0f, 500.0f);
	REQUIRE(decalBigObj != nullptr);
	REQUIRE(padObj != nullptr);
	REQUIRE(bldgObj != nullptr);

	arena.logic.runLogicFrame();
	drawables.applyEvents(events.take());
	std::shared_ptr<const LogicSnapshot> snap = LogicSnapshot::build(arena.logic, nullptr, false, 0);
	drawables.advance(200.0);
	drawables.syncTransforms(*snap, 0.5, true);

	const Drawable *decalDrw = drawables.findByObject(decalBigObj->getID());
	const Drawable *padDrw = drawables.findByObject(padObj->getID());
	const Drawable *bldgDrw = drawables.findByObject(bldgObj->getID());
	REQUIRE(decalDrw != nullptr);
	REQUIRE(padDrw != nullptr);
	REQUIRE(bldgDrw != nullptr);

	bool padHasFloor = false;
	bool bldgHasFloor = false;
	for (const DrawEntry &e : padDrw->entries())
	{
		if (e.kind == W3D_DRAWKIND_FLOOR)
		{
			padHasFloor = true;
		}
	}
	for (const DrawEntry &e : bldgDrw->entries())
	{
		if (e.kind == W3D_DRAWKIND_FLOOR)
		{
			bldgHasFloor = true;
		}
	}
	CHECK(padHasFloor);
	CHECK(bldgHasFloor);

	SelectionDecal sd;
	REQUIRE(ObjectShadow::buildDrawableDecalShadow(*decalDrw, sd, true, nullptr, 30, decalBigObj));
	CHECK(AsciiStringUtil::lowered(sd.texture) == "fl_floor_isengard.tga");
	CHECK(sd.size == doctest::Approx(685.0f)); // tolerance: float32 ShadowSizeX precision
	CHECK(sd.sizeY == doctest::Approx(685.0f)); // tolerance: float32 ShadowSizeY precision
	CHECK(sd.style == 0x400u); // SHADOW_ALPHA_DECAL_DYNAMIC
	CHECK((sd.color >> 24) == 240u);

	CHECK(std::abs(padObj->getPosition()->x - decalBigObj->getPosition()->x) < sd.size * 0.5f); // tolerance: pad sits inside decal half-extent
	CHECK(std::abs(padObj->getPosition()->y - decalBigObj->getPosition()->y) < sd.sizeY * 0.5f); // tolerance: pad sits inside decal half-extent
	CHECK(std::abs(bldgObj->getPosition()->x - decalBigObj->getPosition()->x) < sd.size * 0.5f); // tolerance: foundation sits inside decal half-extent
	CHECK(std::abs(bldgObj->getPosition()->y - decalBigObj->getPosition()->y) < sd.sizeY * 0.5f); // tolerance: foundation sits inside decal half-extent

	const std::uint8_t blackTex[4] = { 0, 0, 0, 255 };
	DecalImage atlas{ 1, 1, blackTex };
	SelectionDecalSettings settings;
	settings.useSimpleMergeDecals = false;
	MergeDecalGrid grid;
	std::string gridErr;
	REQUIRE_MESSAGE(BuildSelectionDecalGrid({ sd }, [&](const std::string &name, MergeDecalAtlasRect &r) {
		if (AsciiStringUtil::lowered(name) == "fl_floor_isengard.tga")
		{
			r = { 0, 0, 1, 1 };
			return true;
		}
		return false;
	}, settings, grid, &gridErr), gridErr);

	const std::uint8_t foundationColor[3] = { 180, 140, 100 };
	std::uint8_t shaded[3] = { 0, 0, 0 };
	REQUIRE(EvaluateSelectionDecals(grid, atlas, 580.0f, 500.0f, 0.0f, DecalReceiver::Terrain, foundationColor, shaded));
	CHECK(shaded[0] < 16); // tolerance: 240/255 alpha black decal leaves <= 15/255 of 180
	CHECK(shaded[1] < 12); // tolerance: 240/255 alpha black decal leaves <= 11/255 of 140
	CHECK(shaded[2] < 10); // tolerance: 240/255 alpha black decal leaves <= 9/255 of 100
}

TEST_CASE("LOCAL-65 #510 P-130: All 11 Ennorath factions' foundation W3Ds load and subdivide cleanly for terrain conformation")
{
	ennorathscenario::World ew;
	if (!ew.load("LOCAL-65 all 11 factions foundations"))
	{
		return;
	}

	ArchiveW3DFileSource w3dSource(*ew.merged.fs);
	WW3DAssetManager assets(w3dSource);

	struct FoundationCase
	{
		const char *faction;
		const char *templateName;
		const char *modelName;
		bool isW3DFloorDraw;
	};
	const FoundationCase cases[] = {
		{ "Angmar", "AngmarFortressExpansionPadCorner", "KBFoundation", true },
		{ "Arnor", "ArnorBuildingFoundation", "GBFoundationX", true },
		{ "Arnor", "ArnorBaseDefenceFoundation", "GBFoundationM", true },
		{ "Arnor", "ArnorFortressExpansionPadCorner", "arfoundation", true },
		{ "Arnor", "ArnorWallFoundation", "GBFoundationL", false },
		{ "Dwarves", "DwarvenFortressExpansionPadCorner", "DBFoundation", true },
		{ "Dwarves", "DwarvenWallFoundation", "GBFoundationL", false },
		{ "Elves", "EntBuildingFoundation", "GBFoundationX", true },
		{ "Elves", "LothlorienElbenwald", "EBFoundationx", true },
		{ "Evilmen", "HaradFortressExpansionPadCorner", "hafoundation", true },
		{ "Imladris", "ElvenFortressExpansionPadCorner", "imlafoundatn", true },
		{ "Imladris", "ImladrisFortressExpansionPadCorner", "EBFoundation", true },
		{ "Isengard", "IsengardBuildingFoundation", "IBFoundationX", true },
		{ "Isengard", "IsengardBaseDefenceFoundation", "IBFoundationM", true },
		{ "Isengard", "IsengardFortressExpansionPadCorner", "IBFoundation", true },
		{ "Men", "GondorBuildingFoundation", "GBFoundationX", true },
		{ "Men", "GondorBaseDefenceFoundation", "GBFoundationM", true },
		{ "Men", "MenFortressExpansionPadCorner", "GBFoundationB", true },
		{ "Men", "GondorWallFoundation", "GBFoundationL", false },
		{ "Men", "GondorGBCastleFloorEdain", "GBCastFloor", false },
		{ "Mordor", "MordorFortressExpansionPadCorner", "MBFoundation", true },
		{ "Rohan", "RohanBuildingFoundation", "RBFoundationX", true },
		{ "Rohan", "RohanBaseDefenceFoundation", "RBFoundationM", true },
		{ "Rohan", "RohanFortressExpansionPadCorner", "rbfoundation", true },
		{ "Rohan", "RohanWallFoundation", "GBFoundationL", false },
		{ "Wild", "WildFortressExpansionPadCorner", "WBFoundation", true },
	};

	for (const FoundationCase &c : cases)
	{
		INFO("faction=" << c.faction << " template=" << c.templateName << " model=" << c.modelName);
		const ThingTemplate *tt = ew.shared.world->things().findTemplate(c.templateName);
		REQUIRE(tt != nullptr);

		std::string err;
		const RenderObjPrototype *proto = assets.Create_Render_Obj(c.modelName, &err);
		REQUIRE_MESSAGE(proto != nullptr, err);
		bool hasMesh = false;
		for (const RenderSubObject &sub : proto->SubObjects)
		{
			if (sub.Type != RenderSubObject::SUB_MESH || !sub.Mesh)
			{
				continue;
			}
			hasMesh = true;
			MeshRenderData rd;
			REQUIRE_MESSAGE(Build_Mesh_Render_Data(*sub.Mesh, rd, &err), err);
			REQUIRE(rd.NumVertices > 0); // tolerance: mesh has at least one vertex
			REQUIRE(rd.NumTriangles > 0); // tolerance: mesh has at least one triangle

			Subdivide_Floor_Mesh_Data(rd, 5.0f);
			checkMaxEdgeXY(rd, 5.0f, std::string(c.faction) + ":" + c.modelName);
		}
		CHECK(hasMesh);
	}
}

TEST_CASE("LOCAL-65 #510 P-130: WorldHeightMap::isCellFlipped & getTriangleHeightAt match HeightMapMesh diagonal split")
{
	WorldHeightMap hm;
	hm.m_width = 3;
	hm.m_height = 3;
	hm.m_dataSize = 9;
	hm.m_borderSize = 0;
	// Ridge along SW-NE on cell (0,0) and SE-NW on cell (1,0):
	// Cell (0,0): unflipped SW=(0,0)=256 (z=10), SE=(1,0)=0, NW=(0,1)=0, NE=(1,1)=256 (z=10).
	// Cell (1,0): flipped SE=(2,0)=256 (z=10), SW=(1,0)=0, NE=(2,1)=0, NW=(1,1)=256 (z=10).
	hm.m_data = {
		256, 0, 256,
		0, 256, 0,
		0, 0, 0
	};
	hm.m_tileNdxes.assign(9, 0);
	hm.m_blendTileNdxes.assign(9, 0);
	// Blend entry 1: uninverted right diagonal -> needFlip = true at cell (1,0)
	TBlendTileInfo zero{};
	TBlendTileInfo flipBlend{};
	flipBlend.rightDiagonal = 1;
	flipBlend.inverted = 0;
	hm.m_blendedTiles = { zero, flipBlend };
	hm.m_blendTileNdxes[0 * 3 + 1] = 1;

	CHECK_FALSE(hm.isCellFlipped(0, 0));
	CHECK(hm.isCellFlipped(1, 0));

	// Center of unflipped ridged cell (0,0) at (5,5) lies on SW-NE diagonal at height 10 (not bilinear 5).
	CHECK(hm.getTriangleHeightAt(5.0f, 5.0f) == doctest::Approx(10.0f)); // tolerance: float32 height conversion
	// Center of flipped ridged cell (1,0) at (15,5) lies on SE-NW diagonal at height 10 (not bilinear 5).
	CHECK(hm.getTriangleHeightAt(15.0f, 5.0f) == doctest::Approx(10.0f)); // tolerance: float32 height conversion
	// Point (2, 8) lies in NW half (fx=0.2 <= fy=0.8) where SW=10, NW=0, NE=10 -> 10 + 0.2*10 + 0.8*(-10) = 4.0.
	CHECK(hm.getTriangleHeightAt(2.0f, 8.0f) == doctest::Approx(4.0f)); // tolerance: float32 height conversion
}

TEST_CASE("LOCAL-65 #510 P-130: Is_Flat_Ground_Floor_Model promotes non-W3DFloorDraw slab foundations and excludes 3D buildings")
{
	ennorathscenario::World ew;
	if (!ew.load("LOCAL-65 flat floor slab detection"))
	{
		return;
	}

	ArchiveW3DFileSource w3dSource(*ew.merged.fs);
	WW3DAssetManager assets(w3dSource);

	for (const char *flatSlab : { "GBFoundationL", "GBCastFloor" })
	{
		INFO("flat slab=" << flatSlab);
		std::string err;
		const RenderObjPrototype *proto = assets.Create_Render_Obj(flatSlab, &err);
		REQUIRE_MESSAGE(proto != nullptr, err);
		bool anyVisible = false;
		for (const RenderSubObject &sub : proto->SubObjects)
		{
			if (sub.Type == RenderSubObject::SUB_MESH && sub.Mesh && !sub.Mesh->Is_Hidden())
			{
				anyVisible = true;
				CHECK(Is_Flat_Ground_Floor_Model(*sub.Mesh));
				MeshRenderData rd;
				REQUIRE_MESSAGE(Build_Mesh_Render_Data(*sub.Mesh, rd, &err), err);
				CHECK(Is_Flat_Ground_Floor_Mesh(rd));
			}
		}
		CHECK(anyVisible);
	}

	// A 3D structure (IBurukPit) must never be misclassified as a flat ground floor slab.
	{
		std::string err;
		const RenderObjPrototype *proto = assets.Create_Render_Obj("IBUrukPit_SKN", &err);
		REQUIRE_MESSAGE(proto != nullptr, err);
		bool hasNonFlatMesh = false;
		for (const RenderSubObject &sub : proto->SubObjects)
		{
			if (sub.Type == RenderSubObject::SUB_MESH && sub.Mesh && !sub.Mesh->Is_Hidden() && !Is_Flat_Ground_Floor_Model(*sub.Mesh))
			{
				hasNonFlatMesh = true;
			}
		}
		CHECK(hasNonFlatMesh);
	}
}

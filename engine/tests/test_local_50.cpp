// OpenBFME unit tests. GPL-3.0.
//
// Narrow tests for lane LOCAL-50 (issue #406):
//   * P-103: Ground decals (W3DFloorDraw) missing under buildings (Goblin and all factions):
//     - StartHidden floor draws keep staticModel and start moduleHidden so SubObjectsUpgrade / Drawable::showModule can toggle them (RW 0x4CE91B / 0x6789B4)
//     - Module tag matching in Drawable::showModule and ClientEventRecorder::templateHasDrawModuleTag is case-insensitive (RW 0x6789B4 -> 0x49F474)
//     - Subdivide_Floor_Mesh_Data splits sparse floor quads into <= 10-unit triangles preserving winding, UVs, normals, DCG, and shared edges
//     - W3DShaderKey::Floor injects terrain heightmap conformation & depth bias cleanly with distinct Id and unchanged varying count
//   * P-97: Magma cauldron trail (FX_MordorFortGround) flickers black:
//     - DynamicDecalOrder puts Alpha decals (0x400, EXmagmapuddle01) strictly before Additive decals (0x800, EXmagmapuddle02), after Tracks and before Particles,
//       with RW 0x509FCF / 0x51EB60 depth bias 9.0f

#include "doctest.h"
#include "EconTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/AsciiString.h"
#include "GameClient/ClientEffects.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/FXList.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveGame.h"
#include "GameClient/ParticleDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/meshrender.h"
#include "Libraries/WWVegas/WW3D2/w3dshadergen.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{

const DrawEntry *findEntryNoCase(const Drawable &d, const std::string &tag)
{
	for (const DrawEntry &e : d.entries())
	{
		if (AsciiStringUtil::compareNoCase(e.tag, tag) == 0)
		{
			return &e;
		}
	}
	return nullptr;
}

MeshRenderData makeQuadPlusSmallTriangle()
{
	// 140 x 110 quad (diagonal ~178.045 -> N=18 subdivisions along edge) + disjoint 4 x 4 small triangle (N=1 untouched)
	MeshRenderData m;
	m.Name = "TEST_BIB";
	m.NumVertices = 7;
	m.NumTriangles = 3;
	m.Position = {
		0.0f, 0.0f, 0.0f,
		140.0f, 0.0f, 0.0f,
		140.0f, 110.0f, 0.0f,
		0.0f, 110.0f, 0.0f,
		200.0f, 0.0f, 1.0f,
		204.0f, 0.0f, 1.0f,
		200.0f, 4.0f, 1.0f,
	};
	m.Normal = {
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
	};
	MeshUVStream uv;
	uv.Pass = 0;
	uv.Stage = 0;
	uv.UV = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
	m.PassUV.push_back(std::move(uv));
	m.PassDCG.push_back({
		0, 0, 0, 255,
		255, 0, 0, 255,
		255, 255, 0, 255,
		0, 255, 0, 255,
		10, 20, 30, 255,
		40, 50, 60, 255,
		70, 80, 90, 255,
	});
	MeshDrawSurface s;
	s.Pass = 0;
	s.Indices = { 0, 1, 2, 0, 2, 3, 4, 5, 6 };
	s.Triangles = { 0, 1, 2 };
	m.Surfaces.push_back(std::move(s));
	return m;
}

int countVaryingLines(const std::string &glsl)
{
	int count = 0;
	size_t pos = 0;
	while ((pos = glsl.find("varying ", pos)) != std::string::npos)
	{
		if (pos == 0 || glsl[pos - 1] == '\n')
		{
			++count;
		}
		pos += 8;
	}
	return count;
}

} // namespace

TEST_CASE("LOCAL-50 P-103: StartHidden W3DFloorDraw keeps staticModel, starts moduleHidden, and SubObjectsUpgrade toggles floors case-insensitively")
{
	const char *kFloorBuildingIni =
		"Object Local50FloorBuilding\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  Draw = W3DDefaultDraw ModuleTag_01\n"
		"  End\n"
		"  Draw = W3DFloorDraw ModuleTag_02\n"
		"    ModelName = BIB_L1\n"
		"  End\n"
		"  Draw = W3DFloorDraw ModuleTag_03\n"
		"    ModelName = BIB_L2\n"
		"    StartHidden = Yes\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"  End\n"
		"  Behavior = SubObjectsUpgrade ModuleTag_UpgradeFloor\n"
		"    TriggeredBy = Upgrade_Barracks2\n"
		"    ShowSubObjects = moduletag_03\n"
		"    HideSubObjects = MODULETAG_02\n"
		"  End\n"
		"End\n";

	econtest::Fx fx("");
	W3DDrawModules::registerTypedDrawModuleData(fx.w.modules);
	const std::string objErr = fx.w.load(kFloorBuildingIni, INI_LOAD_OVERWRITE, "floor_l50.ini");
	REQUIRE_MESSAGE(objErr.empty(), objErr);
	const UpgradeTemplate *upgrade = fx.upgradeCenter.findUpgrade("Upgrade_Barracks2");
	REQUIRE(upgrade != nullptr);
	ArchiveW3DFileSource source(fx.w.fx.fsys);
	WW3DAssetManager assets(source);
	DrawableManager drawables(assets, *fx.logic);
	ClientEventRecorder events(*fx.logic);
	fx.logic->setClientHooks(&events);

	const ThingTemplate *tmpl = fx.w.things.findTemplate("Local50FloorBuilding");
	REQUIRE(tmpl != nullptr);
	CHECK(ClientEventRecorder::templateHasDrawModuleTag(*tmpl, "ModuleTag_02"));
	CHECK(ClientEventRecorder::templateHasDrawModuleTag(*tmpl, "moduletag_03"));
	CHECK(ClientEventRecorder::templateHasDrawModuleTag(*tmpl, "MODULETAG_03"));
	CHECK_FALSE(ClientEventRecorder::templateHasDrawModuleTag(*tmpl, "NoSuchTag"));

	Object *obj = fx.makeAt("Local50FloorBuilding", "Alice", 200.0f, 200.0f);
	REQUIRE(obj != nullptr);
	fx.logic->runLogicFrame();
	drawables.applyEvents(events.take());

	Drawable *d = drawables.findByObject(obj->getID());
	REQUIRE(d != nullptr);
	const DrawEntry *eL1 = findEntryNoCase(*d, "ModuleTag_02");
	const DrawEntry *eL2 = findEntryNoCase(*d, "ModuleTag_03");
	REQUIRE(eL1 != nullptr);
	REQUIRE(eL2 != nullptr);

	CHECK(eL1->kind == W3D_DRAWKIND_FLOOR);
	CHECK(eL1->staticModel == "BIB_L1");
	CHECK(eL1->notDrawnReason.empty());
	CHECK_FALSE(eL1->moduleHidden);

	CHECK(eL2->kind == W3D_DRAWKIND_FLOOR);
	CHECK(eL2->staticModel == "BIB_L2");
	CHECK(eL2->notDrawnReason.empty());
	CHECK(eL2->moduleHidden);

	// SubObjectsUpgrade toggles both modules case-insensitively (RW 0x6789B4 -> 0x49F474)
	obj->giveUpgrade(upgrade);
	fx.logic->runLogicFrame();
	drawables.applyEvents(events.take());

	CHECK(eL1->moduleHidden);
	CHECK_FALSE(eL2->moduleHidden);
	CHECK(eL2->staticModel == "BIB_L2");

	fx.logic->setClientHooks(nullptr);
}

TEST_CASE("LOCAL-50 P-103: Subdivide_Floor_Mesh_Data subdivides sparse floor quads to <= 10-unit edges cleanly")
{
	MeshRenderData skinned = makeQuadPlusSmallTriangle();
	skinned.Skin = true;
	CHECK_FALSE(Subdivide_Floor_Mesh_Data(skinned, 10.0f));
	CHECK(skinned.NumTriangles == 3);

	MeshRenderData m = makeQuadPlusSmallTriangle();
	REQUIRE(Subdivide_Floor_Mesh_Data(m, 10.0f));

	// 140x110 diagonal = 178.045 -> N=18; each quad tri splits into 324 -> 648 + 1 small untouched = 649 triangles.
	// 19x19 deduplicated grid vertices (361) + 3 small triangle vertices = 364 vertices.
	CHECK(m.NumTriangles == 649);
	CHECK(m.Surfaces[0].Indices.size() == 649 * 3);
	CHECK(m.Surfaces[0].Triangles.size() == 649);
	CHECK(m.NumVertices == 364);
	CHECK(m.Position.size() == 364 * 3);
	CHECK(m.Normal.size() == 364 * 3);
	CHECK(m.PassUV[0].UV.size() == 364 * 2);
	CHECK(m.PassDCG[0].size() == 364 * 4);

	for (size_t t = 0; t < m.NumTriangles; ++t)
	{
		const std::uint32_t i0 = m.Surfaces[0].Indices[t * 3];
		const std::uint32_t i1 = m.Surfaces[0].Indices[t * 3 + 1];
		const std::uint32_t i2 = m.Surfaces[0].Indices[t * 3 + 2];
		const bool validIndices = (i0 < m.NumVertices && i1 < m.NumVertices && i2 < m.NumVertices);
		REQUIRE(validIndices);

		auto edgeXY = [&](std::uint32_t a, std::uint32_t b) {
			const float dx = m.Position[a * 3] - m.Position[b * 3];
			const float dy = m.Position[a * 3 + 1] - m.Position[b * 3 + 1];
			return std::sqrt(dx * dx + dy * dy);
		};
		const float maxE = std::max({ edgeXY(i0, i1), edgeXY(i1, i2), edgeXY(i2, i0) });
		CHECK(maxE <= 10.01f); // tolerance: binary32 sqrt rounding on subdivided grid edges

		const float ax = m.Position[i1 * 3] - m.Position[i0 * 3];
		const float ay = m.Position[i1 * 3 + 1] - m.Position[i0 * 3 + 1];
		const float bx = m.Position[i2 * 3] - m.Position[i0 * 3];
		const float by = m.Position[i2 * 3 + 1] - m.Position[i0 * 3 + 1];
		const bool ccwWinding = (ax * by - ay * bx) > 0.0f;
		CHECK(ccwWinding);
	}

	for (size_t v = 0; v < m.NumVertices; ++v)
	{
		CHECK(m.Normal[v * 3] == 0.0f);
		CHECK(m.Normal[v * 3 + 1] == 0.0f);
		CHECK(m.Normal[v * 3 + 2] == 1.0f);
		if (m.Position[v * 3] <= 140.01f)
		{
			CHECK(m.PassUV[0].UV[v * 2] == doctest::Approx(m.Position[v * 3] / 140.0f).epsilon(1e-4f)); // tolerance: binary32 barycentric UV lerp
			CHECK(m.PassUV[0].UV[v * 2 + 1] == doctest::Approx(m.Position[v * 3 + 1] / 110.0f).epsilon(1e-4f)); // tolerance: binary32 barycentric UV lerp
		}
	}
}

TEST_CASE("LOCAL-50 P-103: W3DShaderKey::Floor generates distinct Id, conforms to w3d_floor_hm, biases depth, and adds zero varyings")
{
	W3DShaderKey base;
	base.Blend = W3D_GODOT_BLEND_MIX;
	base.DepthWrite = false;
	base.Texturing = true;
	base.Tex[0] = true;
	base.PriGradient = W3D_GRADIENT_MODULATE;
	base.Fog = 1;

	W3DShaderKey floorKey = base;
	floorKey.Floor = true;

	CHECK(base.Id() != floorKey.Id());
	CHECK(floorKey.Id().find("_flr") != std::string::npos);

	const std::string baseCode = Generate_W3D_Shader_Code(base, W3DSamplerPolicy::Anisotropic);
	const std::string floorCode = Generate_W3D_Shader_Code(floorKey, W3DSamplerPolicy::Anisotropic);

	CHECK(baseCode.find("w3d_floor_hm") == std::string::npos);
	CHECK(floorCode.find("global uniform sampler2D w3d_floor_hm : filter_linear, repeat_disable;") != std::string::npos);
	CHECK(floorCode.find("global uniform vec4 w3d_floor_hm_rect;") != std::string::npos);
	CHECK(floorCode.find("w3d_floor_hm_rect.z > 0.0") != std::string::npos);
	CHECK(floorCode.find("POSITION = PROJECTION_MATRIX * vec4((MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz * 0.999375, 1.0);") != std::string::npos);
	CHECK(countVaryingLines(floorCode) == countVaryingLines(baseCode));

	W3DShaderKey occlFloor = floorKey;
	occlFloor.OcclusionPass = W3D_OCCLUSION_PASS_MARK;
	const std::string occlCode = Generate_W3D_Shader_Code(occlFloor, W3DSamplerPolicy::Anisotropic);
	CHECK(occlCode.find("w3d_floor_hm") == std::string::npos);
}

TEST_CASE("LOCAL-50 P-97: DynamicDecalOrder puts Alpha before Additive after Tracks and before Particles with depth bias 9.0")
{
	const DynamicDecalBatchOrder alpha = DynamicDecalOrder(0);
	const DynamicDecalBatchOrder add = DynamicDecalOrder(1);

	CHECK(alpha.priority == FXParticleSystem::kTrackRenderPriority);
	CHECK(add.priority == FXParticleSystem::kTrackRenderPriority);
	CHECK(FXParticleSystem::kTrackRenderPriority == FXParticleSystem::kScorchRenderPriority + 1);
	CHECK(FXParticleSystem::kTrackRenderPriority == FXParticleSystem::kParticleRenderPriorityMin - 1);
	CHECK(alpha.sortOffset == 100000.0f);
	CHECK(add.sortOffset == 200000.0f);
	const float expectedBias = 1.0f / (1.0f + 9.0f * 0.000625f);
	CHECK(alpha.depthBias == expectedBias);
	CHECK(add.depthBias == expectedBias);
}

TEST_CASE("LOCAL-50 ennorath: GoblinCave floor draws preserve StartHidden bib models and FX_MordorFortGround orders Alpha before Additive")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-50 ennorath"))
	{
		return;
	}

	// P-103: GoblinCave floor modules preserve staticModel and start moduleHidden when StartHidden = Yes
	{
		hudtest::Rig rig(world.shared, "map mp fall back 4p", "FactionWild");
		std::string err;
		const Coord3D site = rig.freeSpot(1800, 1800, 200.0f);
		Object *cave = rig.game->createObject("GoblinCave", rig.index(), site, 0.0f, &err);
		REQUIRE_MESSAGE(cave != nullptr, err);
		rig.frame(1);

		Drawable *d = rig.game->drawables().findByObject(cave->getID());
		REQUIRE(d != nullptr);
		std::vector<const DrawEntry *> floors;
		for (const DrawEntry &e : d->entries())
		{
			if (e.kind == W3D_DRAWKIND_FLOOR)
			{
				floors.push_back(&e);
			}
		}
		REQUIRE(floors.size() == 2);
		int visibleCount = 0;
		int hiddenCount = 0;
		for (const DrawEntry *f : floors)
		{
			CHECK_MESSAGE(!f->staticModel.empty(), f->tag);
			CHECK_MESSAGE(f->notDrawnReason.empty(), f->tag);
			if (f->moduleHidden)
			{
				++hiddenCount;
			}
			else
			{
				++visibleCount;
			}
		}
		CHECK(visibleCount == 1);
		CHECK(hiddenCount == 1);
	}

	// P-97: FX_MordorFortGround emits Alpha (EXmagmapuddle01) + Additive (EXmagmapuddle02) decals ordered by DynamicDecalOrder
	{
		FXPlayback playback(*world.shared.mount->fs, RandomAlgorithm::RotWK_GameDat_LCG);
		const std::vector<std::string> fxErrors = playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
		REQUIRE(fxErrors.empty());
		REQUIRE(playback.playFXList("FX_MordorFortGround", Coord3D{ 500.0f, 500.0f, 0.0f }, false));
		const std::vector<DecalFx> &decals = playback.decals();
		const DecalFx *alphaPuddle = nullptr;
		const DecalFx *addPuddle = nullptr;
		for (const DecalFx &df : decals)
		{
			if (AsciiStringUtil::compareNoCase(df.desc.name, "EXmagmapuddle01") == 0) alphaPuddle = &df;
			if (AsciiStringUtil::compareNoCase(df.desc.name, "EXmagmapuddle02") == 0) addPuddle = &df;
		}
		REQUIRE(alphaPuddle != nullptr);
		REQUIRE(addPuddle != nullptr);
		CHECK(alphaPuddle->desc.shader == 0);
		CHECK(addPuddle->desc.shader == 1);
		const DynamicDecalBatchOrder oAlpha = DynamicDecalOrder(alphaPuddle->desc.shader);
		const DynamicDecalBatchOrder oAdd = DynamicDecalOrder(addPuddle->desc.shader);
		CHECK(oAlpha.priority == oAdd.priority);
		CHECK(oAlpha.sortOffset == 100000.0f);
		CHECK(oAdd.sortOffset == 200000.0f);
	}
}

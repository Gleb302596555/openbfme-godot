// OpenBFME. GPL-3.0.
// LOCAL-119 (issue #803): effects drawn on objects — P-158 (DefaultUpdate + RenderObjectDraw uniform scale),
// P-160 (DDS header dwPitchOrLinearSize without DDSD_LINEARSIZE/DDSD_PITCH fallback),
// P-167 (W3DBuffBuffer::Draw additive / no-depth-write render state override).
// Editions: both RotWK 2.01 and BFME2 1.06.
// Sources:
//   * P-158: RW 0x0096AF99 (DefaultUpdate::update), RW 0x006AA274 (DefaultUpdate getSizeX/Y/Z at vtable RW 0x00C32BD0 / 0x00C333C4 slots +0x14..+0x1C),
//            RW 0x00964C00 (RenderObjectDrawModule::doParticles), RW 0x00965DA7 (RenderObjectUpdate::update);
//            Ennorath data\ini\includes\fxlist.inc (FX_ImladrisUpgrade), data\ini\includes\fxparticlesystem.inc (UpgradeImladrisPillar).
//   * P-160: Ennorath data\ini\fxlist.ini (FX_EregionSummon -> DynamicDecal sccbeaconground),
//            _______enrth_textures3.big art\compiledtextures\sc\sccbeaconground.dds (512x512 DXT5, flags=0x1007, pitch=131072).
//   * P-167: RW 0x004D20C6 (W3DBuffBuffer::Draw: D3DRS_ZWRITEENABLE=0 at RW 0x004D246C, D3DRS_ALPHATESTENABLE=0 at RW 0x004D252C,
//            D3DRS_SRCBLEND=ONE at RW 0x004D26D4, D3DRS_DESTBLEND=ONE at RW 0x004D2734, D3DRS_FOGENABLE=0 at RW 0x004D2794);
//            Ennorath data\ini\includes\fxlist.inc (FX_PalantirLeadershipAlatar, FX_AbwehrLeadership), ________enrth_w3d.big art\w3d\ex\expalantir.W3D.

#include "doctest.h"
#include "EnnorathScenario.h"
#include "FxTestUtil.h"

#include "Common/INI.h"
#include "GameClient/DdsDecode.h"
#include "GameClient/FXList.h"
#include "GameClient/FXParticleSystem.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/ParticleDraw.h"
#include "GameClient/ParticleSys.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/meshmdl.h"
#include "Libraries/WWVegas/WW3D2/meshrender.h"
#include "Libraries/WWVegas/WW3D2/shader.h"
#include "Libraries/WWVegas/WW3D2/w3dshadergen.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace FXParticleSystem;

namespace
{

float colLength(const Matrix3D &m, int col)
{
	const float x = m.Row[0][col];
	const float y = m.Row[1][col];
	const float z = m.Row[2][col];
	return std::sqrt(x * x + y * y + z * z);
}

void writeU32(std::vector<std::uint8_t> &buf, size_t off, std::uint32_t v)
{
	buf[off + 0] = (std::uint8_t)(v & 0xFFu);
	buf[off + 1] = (std::uint8_t)((v >> 8) & 0xFFu);
	buf[off + 2] = (std::uint8_t)((v >> 16) & 0xFFu);
	buf[off + 3] = (std::uint8_t)((v >> 24) & 0xFFu);
}

std::vector<std::uint8_t> makeSyntheticDxt5(int w, int h, std::uint32_t flags, std::uint32_t pitchOrLinearSize)
{
	const size_t blocks = (size_t)((w + 3) / 4) * (size_t)((h + 3) / 4);
	std::vector<std::uint8_t> buf(128 + blocks * 16, 0);
	std::memcpy(buf.data(), "DDS ", 4);
	writeU32(buf, 4, 124);
	writeU32(buf, 8, flags);
	writeU32(buf, 12, (std::uint32_t)h);
	writeU32(buf, 16, (std::uint32_t)w);
	writeU32(buf, 20, pitchOrLinearSize);
	writeU32(buf, 28, 1);   // mipMapCount
	writeU32(buf, 76, 32);  // ddspf.dwSize
	writeU32(buf, 80, 0x4); // DDPF_FOURCC
	std::memcpy(buf.data() + 84, "DXT5", 4);
	writeU32(buf, 108, 0x1000); // DDSCAPS_TEXTURE
	if (blocks > 0)
	{
		buf[128 + 0] = 255;
		buf[128 + 1] = 255;
		buf[128 + 8] = 0xFF;
		buf[128 + 9] = 0xFF;
		buf[128 + 10] = 0xFF;
		buf[128 + 11] = 0xFF;
	}
	return buf;
}

} // namespace

TEST_CASE("LOCAL-119 P-158 synthetic: DefaultUpdate + RenderObjectDraw grows all three scale axes uniformly (RW 0x0096AF99 / RW 0x006AA274 / RW 0x00964C00)")
{
	const char *kIni =
		"FXParticleSystem Local119UniformPillar\n"
		"  System\n"
		"    Priority = ALWAYS_RENDER\n"
		"    Shader = W3D_EMISSIVE\n"
		"    Type = DRAWABLE\n"
		"    ParticleName = exlight01.w3d\n"
		"    Lifetime = 90 90\n"
		"    SystemLifetime = 60\n"
		"    SortLevel = 1\n"
		"    Size = 0.1 0.1\n"
		"    BurstDelay = 1 1\n"
		"    BurstCount = 1 1\n"
		"  End\n"
		"  Color = DefaultColor\n"
		"    Color1 = R:224 G:188 B:26 0\n"
		"  End\n"
		"  Alpha = DefaultAlpha\n"
		"    Alpha2 = 0.2 0.2 10\n"
		"    Alpha3 = 0 0 90\n"
		"  End\n"
		"  Update = DefaultUpdate\n"
		"    SizeRate = 0.03 0.03\n"
		"    SizeRateDamping = 0.99 0.99\n"
		"    AngleZ = 0 0\n"
		"    AngularDamping = 1 1\n"
		"    Rotation = ROTATE_Z\n"
		"    AngularDampingXY = 1 1\n"
		"  End\n"
		"  Physics = DefaultPhysics\n"
		"    VelocityDamping = 1 1\n"
		"  End\n"
		"  EmissionVelocity = OrthoEmissionVelocity\n"
		"  End\n"
		"  EmissionVolume = CylinderEmissionVolume\n"
		"    Radius = 30\n"
		"  End\n"
		"  Draw = RenderObjectDraw\n"
		"  End\n"
		"End\n"
		"FXParticleSystem Local119AnisoPillar\n"
		"  System\n"
		"    Priority = ALWAYS_RENDER\n"
		"    Shader = W3D_ALPHA\n"
		"    Type = DRAWABLE\n"
		"    ParticleName = exgrass02.w3d\n"
		"    Lifetime = 60 60\n"
		"    SystemLifetime = 30\n"
		"    Size = 1 1\n"
		"    BurstDelay = 1 1\n"
		"    BurstCount = 1 1\n"
		"  End\n"
		"  Alpha = DefaultAlpha\n"
		"    Alpha1 = 1 1 0\n"
		"  End\n"
		"  Update = RenderObjectUpdate\n"
		"    StartSizeX = 1 1\n"
		"    StartSizeY = 2 2\n"
		"    StartSizeZ = 3 3\n"
		"    SizeRateX = 0.05 0.05\n"
		"    SizeRateY = 0.0 0.0\n"
		"    SizeRateZ = 0.1 0.1\n"
		"    SizeDampingX = 1 1\n"
		"    SizeDampingY = 1 1\n"
		"    SizeDampingZ = 1 1\n"
		"    AngleZ = 0 0\n"
		"    AngularDamping = 1 1\n"
		"    Rotation = ROTATE_Z\n"
		"  End\n"
		"  Physics = DefaultPhysics\n"
		"    VelocityDamping = 1 1\n"
		"  End\n"
		"  EmissionVelocity = OrthoEmissionVelocity\n"
		"  End\n"
		"  EmissionVolume = PointEmissionVolume\n"
		"  End\n"
		"  Draw = RenderObjectDraw\n"
		"  End\n"
		"End\n";

	fxtest::Sim sim(kIni);
	sim.spawn("Local119UniformPillar");
	sim.spawn("Local119AnisoPillar");
	for (int f = 0; f < 25; ++f)
	{
		sim.step();
	}

	DrawCamera cam;
	cam.position = Coord3D{ 0.0f, -100.0f, 100.0f };
	cam.right = Coord3D{ 1.0f, 0.0f, 0.0f };
	cam.up = Coord3D{ 0.0f, 0.0f, 1.0f };
	cam.forward = Coord3D{ 0.0f, 1.0f, 0.0f };

	W3DClientRandom renderRng(RandomAlgorithm::RotWK_GameDat_LCG);
	ParticleDrawBuilder builder(renderRng);
	const std::vector<DrawBatch> batches = builder.build(*sim.mgr, cam);

	bool foundUniform = false;
	bool foundAniso = false;
	for (const DrawBatch &b : batches)
	{
		if (b.kind != DrawBatch::DRAWABLES) continue;
		if (b.modelName == "exlight01")
		{
			foundUniform = true;
			REQUIRE(!b.drawables.empty());
			float maxSx = 0.0f;
			for (const DrawableInstance &inst : b.drawables)
			{
				const float sx = colLength(inst.transform, 0);
				const float sy = colLength(inst.transform, 1);
				const float sz = colLength(inst.transform, 2);
				CHECK(sy == doctest::Approx(sx).epsilon(1e-4f)); // tolerance: rotation matrix column norm float rounding
				CHECK(sz == doctest::Approx(sx).epsilon(1e-4f)); // tolerance: rotation matrix column norm float rounding
				if (sx > maxSx) maxSx = sx;
			}
			CHECK(maxSx > 0.5f); // tolerance: oldest particle grows from 0.1 at SizeRate 0.03 with damping 0.99 over 25 frames
		}
		else if (b.modelName == "exgrass02")
		{
			foundAniso = true;
			REQUIRE(!b.drawables.empty());
			bool sawAniso = false;
			for (const DrawableInstance &inst : b.drawables)
			{
				const float sx = colLength(inst.transform, 0);
				const float sy = colLength(inst.transform, 1);
				const float sz = colLength(inst.transform, 2);
				if (sz > sy + 1.0f && sx > 1.5f) sawAniso = true;
			}
			CHECK(sawAniso);
		}
	}
	CHECK(foundUniform);
	CHECK(foundAniso);
}

TEST_CASE("LOCAL-119 P-160 synthetic: DdsDecode::inspect flags non-zero dwPitchOrLinearSize without DDSD_LINEARSIZE and decodes level 0")
{
	const std::vector<std::uint8_t> badDds = makeSyntheticDxt5(4, 4, 0x1007u, 131072u);
	bool fallback = false;
	std::string err;
	REQUIRE(DdsDecode::inspect(badDds.data(), badDds.size(), fallback, &err));
	CHECK(fallback == true);

	DdsImage decoded;
	REQUIRE(DdsDecode::decodeLevel0(badDds.data(), badDds.size(), decoded, &err));
	CHECK(decoded.width == 4);
	CHECK(decoded.height == 4);
	REQUIRE(decoded.rgba.size() == 4u * 4u * 4u);
	CHECK(decoded.rgba[0] == 255);
	CHECK(decoded.rgba[3] == 255);

	const std::vector<std::uint8_t> goodDds = makeSyntheticDxt5(4, 4, 0x81007u, 16u);
	bool goodFallback = true;
	REQUIRE(DdsDecode::inspect(goodDds.data(), goodDds.size(), goodFallback, &err));
	CHECK(goodFallback == false);
}

TEST_CASE("LOCAL-119 P-167 synthetic: W3D_Apply_Buff_Render_State forces additive no-depth-write no-alpha-test no-fog (RW 0x004D20C6)")
{
	W3DShaderKey key;
	key.Blend = W3D_GODOT_BLEND_ADD;
	key.DepthWrite = true;
	key.AlphaTest = true;
	key.Fog = 1;
	key.Tex[0] = true;

	const std::string beforeCode = Generate_W3D_Shader_Code(key);
	CHECK(beforeCode.find("depth_draw_always") != std::string::npos);

	W3D_Apply_Buff_Render_State(key);
	CHECK(key.Blend == W3D_GODOT_BLEND_ADD);
	CHECK(key.DepthWrite == false);
	CHECK(key.AlphaTest == false);
	CHECK(key.Fog == 0);

	const std::string afterCode = Generate_W3D_Shader_Code(key);
	CHECK(afterCode.find("blend_add") != std::string::npos);
	CHECK(afterCode.find("depth_draw_never") != std::string::npos);
	CHECK(afterCode.find("depth_draw_always") == std::string::npos);

	W3DShaderKey opaqueKey;
	opaqueKey.Blend = W3D_GODOT_BLEND_OPAQUE;
	opaqueKey.DepthWrite = true;
	opaqueKey.Tex[0] = true;
	W3D_Apply_Buff_Render_State(opaqueKey);
	CHECK(opaqueKey.Blend == W3D_GODOT_BLEND_ADD);
	CHECK(opaqueKey.DepthWrite == false);
}

TEST_CASE("LOCAL-119 ENNORATH scenario P-158 / P-160 / P-167: Wisdom UpgradeImladrisPillar, Eregion sccbeaconground.dds, and Alatar expalantir.W3D")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-119 ENNORATH scenario P-158 / P-160 / P-167"))
	{
		return;
	}

	FXPlayback playback(*world.merged.fs, RandomAlgorithm::RotWK_GameDat_LCG);
	playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
	playback.seedClientRandom(42);

	// ---- P-158: FX_ImladrisUpgrade -> UpgradeImladrisPillar (exlight01.w3d) ----
	// Source: Ennorath data\ini\includes\fxlist.inc:20843, fxparticlesystem.inc:66742; RW 0x0096AF99 / RW 0x006AA274 / RW 0x00964C00.
	{
		REQUIRE(playback.playFXList("FX_ImladrisUpgrade", Coord3D{ 100.0f, 100.0f, 0.0f }, false));
		for (int f = 0; f < 25; ++f)
		{
			playback.step();
		}
		DrawCamera cam;
		cam.position = Coord3D{ 100.0f, 0.0f, 100.0f };
		cam.right = Coord3D{ 1.0f, 0.0f, 0.0f };
		cam.up = Coord3D{ 0.0f, 0.0f, 1.0f };
		cam.forward = Coord3D{ 0.0f, 1.0f, 0.0f };
		W3DClientRandom renderRng(RandomAlgorithm::RotWK_GameDat_LCG);
		ParticleDrawBuilder builder(renderRng);
		const std::vector<DrawBatch> batches = builder.build(playback.particles(), cam);
		bool foundPillar = false;
		for (const DrawBatch &b : batches)
		{
			if (b.kind == DrawBatch::DRAWABLES && b.modelName == "exlight01")
			{
				foundPillar = true;
				REQUIRE(!b.drawables.empty());
				float maxScale = 0.0f;
				for (const DrawableInstance &inst : b.drawables)
				{
					const float sx = colLength(inst.transform, 0);
					const float sy = colLength(inst.transform, 1);
					const float sz = colLength(inst.transform, 2);
					CHECK(sy == doctest::Approx(sx).epsilon(1e-4f)); // tolerance: rotation matrix column norm float rounding
					CHECK(sz == doctest::Approx(sx).epsilon(1e-4f)); // tolerance: rotation matrix column norm float rounding
					if (sx > maxScale) maxScale = sx;
				}
				CHECK(maxScale > 0.5f); // tolerance: oldest UpgradeImladrisPillar particle grows from 0.1 over 25 frames
			}
		}
		CHECK(foundPillar);
	}

	// ---- P-160: FX_EregionSummon -> DynamicDecal sccbeaconground -> art\compiledtextures\sc\sccbeaconground.dds ----
	// Source: Ennorath data\ini\fxlist.ini:22961, _______enrth_textures3.big.
	{
		REQUIRE(playback.playFXList("FX_EregionSummon", Coord3D{ 200.0f, 200.0f, 0.0f }, false));
		bool foundDecal = false;
		for (const DecalFx &d : playback.decals())
		{
			if (d.desc.name == "sccbeaconground")
			{
				foundDecal = true;
				CHECK(d.desc.size == 250.0f);
			}
		}
		CHECK(foundDecal);

		std::vector<std::uint8_t> ddsBytes;
		std::string readErr;
		REQUIRE(world.merged.fs->readFile("art\\compiledtextures\\sc\\sccbeaconground.dds", ddsBytes, &readErr));
		bool needsFallback = false;
		std::string ddsErr;
		REQUIRE(DdsDecode::inspect(ddsBytes.data(), ddsBytes.size(), needsFallback, &ddsErr));
		CHECK(needsFallback == true);

		DdsImage img;
		REQUIRE(DdsDecode::decodeLevel0(ddsBytes.data(), ddsBytes.size(), img, &ddsErr));
		CHECK(img.width == 512);
		CHECK(img.height == 512);
		REQUIRE(img.rgba.size() == 512u * 512u * 4u);
		CHECK(img.rgba[3] <= 10); // tolerance: DXT5 corner block alpha endpoints in sccbeaconground.dds are 0..5
		std::uint8_t maxAlpha = 0;
		for (size_t i = 3; i < img.rgba.size(); i += 4)
		{
			if (img.rgba[i] > maxAlpha) maxAlpha = img.rgba[i];
		}
		CHECK(maxAlpha >= 240); // tolerance: DXT5 ring pixels in sccbeaconground.dds reach alpha 255
	}

	// ---- P-167: Alatar's arrow-protection buff (FX_PalantirLeadershipAlatar -> expalantir.W3D) ----
	// Source: Ennorath data\ini\includes\fxlist.inc:22400, ________enrth_w3d.big art\w3d\ex\expalantir.W3D; RW 0x004D20C6.
	{
		std::vector<std::uint8_t> w3dBytes;
		std::string readErr;
		REQUIRE(world.merged.fs->readFile("art\\w3d\\ex\\expalantir.w3d", w3dBytes, &readErr));
		W3DFileContents contents;
		std::string loadErr;
		REQUIRE(Load_W3D_File(w3dBytes.data(), w3dBytes.size(), contents, &loadErr));
		REQUIRE(!contents.Meshes.empty());
		bool sawDepthMask1 = false;
		for (const MeshModelClass &mesh : contents.Meshes)
		{
			MeshRenderData rd;
			std::string err;
			REQUIRE(Build_Mesh_Render_Data(mesh, rd, &err));
			for (const MeshDrawSurface &surf : rd.Surfaces)
			{
				if (surf.State.DepthWrite) sawDepthMask1 = true;
				W3DShaderKey key;
				key.Blend = W3D_GODOT_BLEND_ADD;
				key.DepthWrite = surf.State.DepthWrite;
				key.Tex[0] = true;
				W3D_Apply_Buff_Render_State(key);
				const std::string glsl = Generate_W3D_Shader_Code(key);
				CHECK(glsl.find("blend_add") != std::string::npos);
				CHECK(glsl.find("depth_draw_never") != std::string::npos);
				CHECK(glsl.find("depth_draw_always") == std::string::npos);
			}
		}
		CHECK(sawDepthMask1);
	}
}

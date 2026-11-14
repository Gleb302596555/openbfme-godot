// OpenBFME. GPL-3.0.
//
// Terrain-conforming dynamic decal mesh builder and render parameters.
//
// TARGET FACTS (RotWK 2.01 game.dat, caveat S-001) and DONOR facts (BFME2 1.06 decomp):
//   * DynamicDecalFXNugget::doFXPos (TARGET RW 0x5E05AB): maps DynamicDecal Shader (RW 0xD9DE90: 0 ALPHA, 1 ADDITIVE,
//     2 SUBTRACT) to ShadowType 0x400 (ALPHA), 0x800 (ADDITIVE) or 0x10000 (SUBTRACT; RW 0x5E05FB, added in RotWK —
//     DONOR BFME2 0x1E0E43 had only 0x800 vs 0x400) and calls W3DProjectedShadowManager::addDecal (RW 0x50BA57) with
//     allowWorldAlign = true (RW 0x5E0728).
//   * W3DProjectedShadowManager::getTexture (TARGET RW 0x50B67B): sets clamp UV addressing on both U and V
//     (RW 0x50B76A, SET_ADDRESS_CLAMP).
//   * W3DProjectedShadowManager::queueDecal (TARGET RW 0x50C24B .. 0x50D414; DONOR BFME2 0x10C841
//     W3DProjectedShadowDecalHelpers.cpp):
//       - skips entries with decalSize <= 0 (RW 0x50C276 .. 0x50C2DE);
//       - builds world-XY axes from Rotate_Z(yaw) via RW 0x508E7A (DONOR BFME2 0x108E7A decalAxesRva00108E7A):
//         uVector = (cos(yaw), sin(yaw), 0), vVector = (uVector.Y, -uVector.X, 0) = (sin(yaw), -cos(yaw), 0);
//       - projects the four rotated decal corners to world X/Y bounds via RW 0x508FBD (DONOR BFME2 0x108FBD
//         projectRangesRva00108FBD) and scales uVec = uVector / size, vVec = vVector / size with UV center (0.5, 0.5)
//         (RW 0x50C6D0 .. 0x50C729);
//       - subdivides the terrain heightmap cells [floor(min * 0.1) + border .. ceil(max * 0.1) + border] (MAP_XY_FACTOR
//         10.0, 1 / 10 = 0.1 at RW 0xBD83D4), clamped to [0 .. extent - 1] and trimmed symmetrically when either axis
//         exceeds 104 vertices (RW 0x50CE63 .. 0x50CF88);
//       - places each grid vertex on the terrain surface ((i - border) * 10, (j - border) * 10, height(i, j) * 0.0390625)
//         (RW 0x50D10F .. 0x50D25B, MAP_HEIGHT_SCALE = 10 / 256 at RW 0xBD18AC) and emits two triangles per cell
//         matching WorldHeightMap::getFlipState (RW 0x50D334 -> RW 0x4AD985).
//   * W3DProjectedShadowManager::flushDecals (TARGET RW 0x50983D .. 0x50A0E4; DONOR BFME2 0x109C70):
//       - applies projection depth bias 9.0f (RW 0x509FCF [0xBE5958] = 9.0f -> bfmeSetProjectionDepthBias RW 0x51EB60)
//         and resets to 0.0f afterwards (RW 0x50A0BE);
//       - selects _PresetAlphaShader (RW 0xD9B2F0, SRCALPHA / INVSRCALPHA) for 0x400 (ALPHA), _PresetAdditiveShader
//         (RW 0xD9B2E4, ONE / ONE with RGB scaled by opacity / 255 in Shadow::setOpacity DONOR BFME2 0x3308F6) for
//         0x800 (ADDITIVE), and D3DBLENDOP_REVSUBTRACT (RW 0x5098F6 -> RW 0x537BF0) with RW 0xD9B2E8 for 0x10000
//         (SUBTRACT);
//       - renders in the scene's terrain decal pass (RW 0x4E2F85 -> RW 0x50D8BD) after terrain, water, roads and
//         scorches and before particle systems (kDynamicDecalRenderPriority = -123 < kParticleRenderPriorityMin -122).
//
// INFERENCE: when no WorldHeightMap terrain grid is bound (e.g. standalone FXPlayer viewer), the same 10-unit world-XY
// grid is subdivided and sampled through the caller's groundHeight(x, y) callback with unflipped cell diagonals.

#pragma once

#include "GameClient/ClientEffects.h"
#include "GameClient/ParticleDraw.h"
#include "GameEngineDevice/W3DDevice/GameClient/TerrainScorches.h"

#include <cstdint>
#include <functional>
#include <vector>

enum DynamicDecalShaderMode : int
{
	DYNAMIC_DECAL_SHADER_ALPHA = 0,    ///< RW 0xD9DE90[0] "ALPHA" -> ShadowType 0x400 -> _PresetAlphaShader (blend_mix, tex.a * color.a)
	DYNAMIC_DECAL_SHADER_ADDITIVE = 1, ///< RW 0xD9DE90[1] "ADDITIVE" -> ShadowType 0x800 -> _PresetAdditiveShader (blend_add, color.a)
	DYNAMIC_DECAL_SHADER_SUBTRACT = 2, ///< RW 0xD9DE90[2] "SUBTRACT" -> ShadowType 0x10000 -> REVSUBTRACT (blend_sub, color.a)
	DYNAMIC_DECAL_SHADER_COUNT = 3
};

// RW 0x5E05EA .. 0x5E0607: any value outside ADDITIVE (1) and SUBTRACT (2) falls back to ALPHA (0).
inline DynamicDecalShaderMode NormalizeDynamicDecalShader(int rawShader)
{
	if (rawShader == DYNAMIC_DECAL_SHADER_ADDITIVE)
	{
		return DYNAMIC_DECAL_SHADER_ADDITIVE;
	}
	if (rawShader == DYNAMIC_DECAL_SHADER_SUBTRACT)
	{
		return DYNAMIC_DECAL_SHADER_SUBTRACT;
	}
	return DYNAMIC_DECAL_SHADER_ALPHA;
}

struct DynamicDecalVertex
{
	Coord3D position; ///< SAGE world space (x east, y north, z up = terrain surface)
	float u = 0.0f;
	float v = 0.0f;
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

struct DynamicDecalMesh
{
	static constexpr int kMaxVertsPerAxis = 104;             ///< RW 0x50CEFC: max terrain vertices per axis per decal
	static constexpr float kCellWorldSize = 10.0f;           ///< MAP_XY_FACTOR (1 / 10 = 0.1f at RW 0xBD83D4)
	static constexpr float kHeightScale = 10.0f / 256.0f;    ///< MAP_HEIGHT_SCALE (0.0390625f at RW 0xBD18AC)
	static constexpr float kDepthBias = 9.0f;                ///< RW 0x509FCF ([0xBE5958] = 9.0f) -> RW 0x51EB60
	static constexpr int kRenderPriority = FXParticleSystem::kTrackRenderPriority; ///< -123: above scorches (-124), below particles (-122 .. -9)

	static float depthBiasViewScale() { return TerrainScorchBuffer::depthBiasViewScale(kDepthBias); }

	std::vector<DynamicDecalVertex> vertices;
	std::vector<std::int32_t> indices;
	int decalsBuilt = 0;

	void clear()
	{
		vertices.clear();
		indices.clear();
		decalsBuilt = 0;
	}
};

// Builds one batched terrain-conforming mesh for `decals` at `clientFrame` (RW 0x50C24B .. 0x50D414).
// When `terrain` is valid, vertices sit on `terrain->heights` and cell diagonals follow `terrain->flip` (RW 0x4AD985);
// otherwise `fallbackGroundHeight(x, y)` is sampled on the same 10-unit world-XY grid.
void BuildDynamicDecalMesh(
	const std::vector<const DecalFx *> &decals,
	std::uint32_t clientFrame,
	const TerrainScorchBuffer::Terrain *terrain,
	const std::function<float(float, float)> &fallbackGroundHeight,
	DynamicDecalMesh &out);

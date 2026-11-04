// OpenBFME. GPL-3.0.
//
// Device layer of the terrain's scorch marks (TOP20 "Fire, scorches", stops S-117 / S-033): uploads the buffer the core builds
// (TerrainScorchBuffer::updateScorches, RW 0x467073) as one mesh and draws it as RotWK's drawScorches does (RW 0x468BCB): the scorch texture
// (exscorch01.tga, RW 0x46D1D5) with U and V clamped, the vertex colour modulating it, alpha blended (the shader RW 0xD9B2F0, BFME2 decomp
// _PresetAlphaShader: src alpha / inverse src alpha, no depth write), the hardware fog, a depth bias (RW 0x51EB60 with 1.0), after the roads
// (the terrain render's pass order RW 0x4E2DBF .. 0x4E2FED: RenderTerrain, RenderRoads, RenderFloors, RenderScorches). It decides nothing: the
// list, the grid, the colour and the cell diagonals are the core's.
//
// Who feeds it: the terrain builder makes the layer with the map's terrain (TerrainScorchBuffer::terrainFrom) and lights (shadeFrom) and an own
// buffer holding the map's Scorch objects (the map viewer); a live game (GameWorld) presents the live effect player's buffer instead
// (LiveFX::scorches: the map's Scorch objects, the FXList TerrainScorch nugget, the burning terrain), every frame.

#pragma once

#include "GameEngineDevice/W3DDevice/GameClient/TerrainScorches.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <cstdint>

namespace godot
{

class TerrainScorchLayer : public MeshInstance3D
{
	GDCLASS(TerrainScorchLayer, MeshInstance3D)

public:
	void setup(TerrainScorchBuffer::Terrain terrain, const TerrainScorchBuffer::Shade &shade, const Ref<ShaderMaterial> &material);
	// the map viewer's own buffer (the map's Scorch objects)
	TerrainScorchBuffer &ownBuffer() { return m_own; }
	// a live game presents its buffer from here on (the own buffer is no longer drawn)
	void setLive(bool live) { m_live = live; }
	// drawScorches RW 0x468BCB: updateScorches (RW 0x467073) when the core says so, the mesh replaced when the buffer was rebuilt
	void present(TerrainScorchBuffer &buffer);
	void _process(double delta) override;
	// { live, scorches, drawn, vertices, indices, rebuilds, uploads }
	Dictionary get_scorch_report() const;

protected:
	static void _bind_methods();

private:
	TerrainScorchBuffer::Terrain m_terrain;
	TerrainScorchBuffer::Shade m_shade;
	TerrainScorchBuffer m_own;
	Ref<ShaderMaterial> m_material;
	bool m_live = false;
	const TerrainScorchBuffer *m_shown = nullptr; ///< the buffer the mesh was made from (compared, never read through)
	std::uint64_t m_shownRevision = 0;
	int m_scorches = 0, m_drawn = 0, m_vertices = 0, m_indices = 0;
	std::uint64_t m_rebuilds = 0, m_uploads = 0;
};

} // namespace godot

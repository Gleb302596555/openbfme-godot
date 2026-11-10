// OpenBFME. GPL-3.0. PORT-69: client-only terrain tracks, drawn after scorches.
#pragma once

#include "GameEngineDevice/W3DDevice/GameClient/TerrainTracks.h"
#include "GodotDevice/GodotW3DMaterial.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>

namespace godot
{
class TerrainTrackLayer : public MeshInstance3D
{
	GDCLASS(TerrainTrackLayer, MeshInstance3D)
public:
	void setup(ArchiveFileSystem &fs, const AssetPacks *packs, const TerrainTracksShade &shade, const Ref<ShaderMaterial> &base);
	void present(const TerrainTracksSystem &tracks);
	Dictionary get_track_report() const;
protected:
	static void _bind_methods();
private:
	std::unique_ptr<W3DMaterialFactory> m_textures;
	TerrainTracksShade m_shade;
	Ref<ShaderMaterial> m_base;
	const TerrainTracksSystem *m_shown = nullptr;
	std::uint64_t m_revision = 0;
	int m_batches = 0, m_vertices = 0, m_indices = 0;
	Array m_errors;
};
}

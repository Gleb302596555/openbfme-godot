// OpenBFME. GPL-3.0. See GodotTerrainTracks.h.
#include "GodotDevice/GodotTerrainTracks.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot
{
void TerrainTrackLayer::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("get_track_report"), &TerrainTrackLayer::get_track_report);
}
void TerrainTrackLayer::setup(ArchiveFileSystem &fs, const AssetPacks *packs, const TerrainTracksShade &shade, const Ref<ShaderMaterial> &base)
{
	m_textures.reset(new W3DMaterialFactory(fs));
	m_textures->Set_Asset_Packs(packs);
	m_shade = shade;
	m_base = base;
	set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
}
void TerrainTrackLayer::present(const TerrainTracksSystem &tracks)
{
	if (m_shown == &tracks && m_revision == tracks.revision()) return;
	m_shown = &tracks;
	m_revision = tracks.revision();
	std::vector<TerrainTracksSystem::FlushBatch> batches;
	tracks.buildFlushBatches(m_shade, batches);
	m_batches = m_vertices = m_indices = 0;
	m_errors.clear();
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	for (const auto &batch : batches)
	{
		std::vector<std::string> errors;
		Ref<Texture2D> tex = m_textures->Get_Texture(batch.textureName, errors);
		for (const std::string &e : errors) m_errors.push_back(String::utf8(e.c_str()));
		if (tex.is_null() || m_base.is_null()) continue;
		PackedVector3Array vtx;
		PackedColorArray col;
		PackedVector2Array uv;
		PackedInt32Array idx;
		vtx.resize((int64_t)batch.vertices.size());
		col.resize((int64_t)batch.vertices.size());
		uv.resize((int64_t)batch.vertices.size());
		for (size_t i = 0; i < batch.vertices.size(); ++i)
		{
			const auto &p = batch.vertices[i];
			vtx.set((int64_t)i, Vector3(p.x, p.z, -p.y));
			col.set((int64_t)i, Color(((p.diffuse >> 16) & 255) / 255.0f, ((p.diffuse >> 8) & 255) / 255.0f,
				(p.diffuse & 255) / 255.0f, ((p.diffuse >> 24) & 255) / 255.0f));
			uv.set((int64_t)i, Vector2(p.u, p.v));
		}
		idx.resize((int64_t)batch.indices.size());
		for (size_t i = 0; i < batch.indices.size(); ++i) idx.set((int64_t)i, batch.indices[i]);
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vtx;
		arrays[Mesh::ARRAY_COLOR] = col;
		arrays[Mesh::ARRAY_TEX_UV] = uv;
		arrays[Mesh::ARRAY_INDEX] = idx;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		Ref<ShaderMaterial> mat = m_base->duplicate();
		mat->set_shader_parameter("scorch_tex", tex);
		mesh->surface_set_material(m_batches, mat);
		++m_batches;
		m_vertices += (int)batch.vertices.size();
		m_indices += (int)batch.indices.size();
	}
	set_mesh(mesh);
}
Dictionary TerrainTrackLayer::get_track_report() const
{
	Dictionary d;
	d["batches"] = m_batches;
	d["vertices"] = m_vertices;
	d["indices"] = m_indices;
	d["errors"] = m_errors;
	return d;
}
}

// OpenBFME. GPL-3.0. See GodotTerrainScorches.h.

#include "GodotDevice/GodotTerrainScorches.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <utility>

namespace godot
{

void TerrainScorchLayer::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("get_scorch_report"), &TerrainScorchLayer::get_scorch_report);
}

void TerrainScorchLayer::setup(TerrainScorchBuffer::Terrain terrain, const TerrainScorchBuffer::Shade &shade, const Ref<ShaderMaterial> &material)
{
	m_terrain = std::move(terrain);
	m_shade = shade;
	m_material = material;
	set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	set_process(true);
}

void TerrainScorchLayer::present(TerrainScorchBuffer &buffer)
{
	// RW 0x468BD3: updateScorches first
	if (buffer.updateScorches(m_terrain, m_shade))
	{
		++m_rebuilds;
	}
	m_scorches = buffer.numScorches();
	m_drawn = buffer.scorchesInBuffer();
	if (m_shown == &buffer && m_shownRevision == buffer.revision())
	{
		return;
	}
	m_shown = &buffer;
	m_shownRevision = buffer.revision();
	const std::vector<TerrainScorchBuffer::Vertex> &v = buffer.vertices();
	const std::vector<std::uint16_t> &ix = buffer.indices();
	m_vertices = (int)v.size();
	m_indices = (int)ix.size();
	// RW 0x468BDA: no draw without indices
	if (ix.empty() || v.empty())
	{
		set_mesh(Ref<Mesh>());
		return;
	}
	PackedVector3Array vtx;
	PackedColorArray col;
	PackedVector2Array uv;
	PackedInt32Array idx;
	vtx.resize((int64_t)v.size());
	col.resize((int64_t)v.size());
	uv.resize((int64_t)v.size());
	for (size_t i = 0; i < v.size(); ++i)
	{
		const TerrainScorchBuffer::Vertex &p = v[i];
		vtx.set((int64_t)i, Vector3(p.x, p.z, -p.y)); // SAGE (x, y, z) -> Godot (x, z, -y), spec maps-and-terrain.md 2.1
		col.set((int64_t)i, Color(((p.diffuse >> 16) & 0xFF) / 255.0f, ((p.diffuse >> 8) & 0xFF) / 255.0f, (p.diffuse & 0xFF) / 255.0f,
			((p.diffuse >> 24) & 0xFF) / 255.0f));
		uv.set((int64_t)i, Vector2(p.u, p.v));
	}
	idx.resize((int64_t)ix.size());
	for (size_t i = 0; i < ix.size(); ++i)
	{
		idx.set((int64_t)i, (int32_t)ix[i]);
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vtx;
	arrays[Mesh::ARRAY_COLOR] = col;
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	arrays[Mesh::ARRAY_INDEX] = idx;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	if (m_material.is_valid())
	{
		mesh->surface_set_material(0, m_material);
	}
	set_mesh(mesh);
	++m_uploads;
}

void TerrainScorchLayer::_process(double)
{
	if (!m_live)
	{
		present(m_own);
	}
}

Dictionary TerrainScorchLayer::get_scorch_report() const
{
	Dictionary d;
	d["live"] = m_live;
	d["scorches"] = m_scorches;
	d["drawn"] = m_drawn;
	d["vertices"] = m_vertices;
	d["indices"] = m_indices;
	d["rebuilds"] = (int64_t)m_rebuilds;
	d["uploads"] = (int64_t)m_uploads;
	return d;
}

} // namespace godot

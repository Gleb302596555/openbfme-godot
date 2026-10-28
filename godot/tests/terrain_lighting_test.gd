## Integration: the terrain effect receives the overbright-scaled SAS sun colour, and the map's terrain light scale (GlobalLighting v8) only at
## the low terrain tier (RW 0x4E2B3A, guarded by the terrain rendering mode < 1; TerrainLightScale.h).
extends SceneTree
func _initialize() -> void:
	call_deferred("_run")
func _run() -> void:
	var fs = ClassDB.instantiate("RetailFileSystem")
	assert(fs.mount_retail().ok)
	var builder = ClassDB.instantiate("MapTerrainBuilder")
	var map = builder.build_map(fs, "map mp harlindon", {"markers":false,"water":false})
	assert(map != null)
	var report: Dictionary = builder.get_report()
	assert(report.lighting.effect_color_scale == 2.0)
	var expected: Vector3 = report.lighting.terrain_lights[0].diffuse * 2.0
	var checked := 0
	for mesh in map.find_children("*", "MeshInstance3D", true, false):
		for surface in mesh.mesh.get_surface_count():
			var mat = mesh.mesh.surface_get_material(surface)
			if mat is ShaderMaterial and mat.shader.code.contains("terrain_common"):
				assert(mat.get_shader_parameter("sun_color") == expected)
				assert(mat.get_shader_parameter("overbright") == true)
				checked += 1
	assert(checked == report.mesh.chunks, "Every terrain material must receive the SAS colour")
	assert(checked == 324, "Harlindon terrain chunk coverage changed")
	map.free()
	_check_light_scale(builder, fs)
	print("TERRAIN LIGHTING PASS: ", checked, " materials")
	quit()

func _sun_colors(map: Node) -> Array:
	var out: Array = []
	for mesh in map.find_children("*", "MeshInstance3D", true, false):
		for surface in mesh.mesh.get_surface_count():
			var mat = mesh.mesh.surface_get_material(surface)
			if mat is ShaderMaterial and mat.shader.code.contains("terrain_common"):
				out.append(mat.get_shader_parameter("sun_color"))
	return out

func _check_light_scale(builder, fs) -> void:
	# wor mirkwood stores the scale (0.5, 0.5, 0.45): the High tier (the default) draws the sun unscaled, the lowest terrain tier scales it
	var high = builder.build_map(fs, "map wor mirkwood", {"markers":false,"water":false,"roads":false})
	assert(high != null)
	var rep: Dictionary = builder.get_report()
	var scale: Vector3 = rep.lighting.terrain_light_scale
	assert(scale == Vector3(0.5, 0.5, 0.45), "wor mirkwood light scale")
	assert(rep.lighting.terrain_rendering_mode == 2)
	assert(rep.lighting.terrain_light_scale_applied == Vector3(1, 1, 1))
	var sun: Vector3 = rep.lighting.terrain_lights[0].diffuse * rep.lighting.effect_color_scale
	var colors: Array = _sun_colors(high)
	assert(colors.size() == rep.mesh.chunks, "every terrain material carries the sun colour")
	for c in colors:
		assert(c == sun, "High tier: the sun is not scaled")
	high.free()
	var low = builder.build_map(fs, "map wor mirkwood", {"markers":false,"water":false,"roads":false,
			"use_terrain_normal_map":false, "terrain_lod":0, "shader_lod":0})
	assert(low != null)
	rep = builder.get_report()
	assert(rep.lighting.terrain_rendering_mode == 0)
	assert(rep.lighting.terrain_light_scale_applied == scale)
	var d: Vector3 = rep.lighting.terrain_lights[0].diffuse
	var expected: Vector3 = Vector3(scale.x * d.x, scale.y * d.y, scale.z * d.z) * rep.lighting.effect_color_scale
	colors = _sun_colors(low)
	assert(colors.size() == rep.mesh.chunks, "every terrain material carries the sun colour")
	for c in colors:
		assert(c == expected, "lowest tier: the sun is scaled by the map's light scale")
	assert(expected != sun)
	low.free()

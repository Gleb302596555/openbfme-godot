## P-9 lines: the queued orders of the selected and hovered own objects drawn as lines while Alt is held, as RotWK's order line renderer (constructor
## RW 0x4E03C7, frame RW 0x4E013B). The HUD builds the polylines from the live game (GameClient/OrderLines.h) and turns them into WW3D's
## SegmentedLineClass ribbons for this camera (GameClient/SegLineGeometry.h: the edges' planes, the joints, the merges, TILED V, the colour and
## opacity per vertex); this node only uploads the triangles. It never changes the game.
## The material is the renderer's shader preset (RW 0xD9B2F0 = 0x1198B3 with depth compare 7, RW 0x4E049E): blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA,
## the texture times the vertex colour, depth test always, no depth write, no culling (seglinerenderer Render sets CULL_MODE_DISABLE).
## The node model SCMoveHintSml stands at every order's point (RW 0x4E0427, 0x4E007E); its opacity is the lines' (RW 0x4DF983), 1 in the order mode
## Alt gives; RotWK's 0.4 belongs to planning mode 2 only, which retail never enables (S-1831), so the instancer's full opacity is the same picture.
extends Node3D

const NODE_MODEL := "SCMoveHintSml"

var hud: Node
var errors: Array = []
var surfaces_drawn := 0  # mesh surfaces of the last frame (tests, reports)
var markers_drawn := 0

var _mesh := ArrayMesh.new()
var _mesh_node := MeshInstance3D.new()
var _materials: Array = []  # [EXLaser, EXLaser2]
var _inst: Node3D
var _model := -1
var _marker_ids: Array = []


func setup(p_hud: Node, fs: RefCounted) -> Dictionary:
	hud = p_hud
	var builder = ClassDB.instantiate("W3DModelBuilder")
	for tex_name in ["EXLaser.tga", "EXLaser2.tga"]:
		var tex: Texture2D = builder.load_texture(fs, tex_name)
		if tex == null:
			errors.append("order line texture %s did not load" % tex_name)
		var m := ShaderMaterial.new()
		m.shader = _shader()
		m.set_shader_parameter("tex", tex)
		_materials.append(m)
	_mesh_node.mesh = _mesh
	_mesh_node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_mesh_node)
	_inst = ClassDB.instantiate("W3DInstancer")
	add_child(_inst)
	var r: Dictionary = _inst.setup(fs)
	if not r.get("ok", false):
		errors.append("order line instancer: %s" % [r.get("errors", [])])
	else:
		_model = _inst.add_model(NODE_MODEL)
		if _model < 0:
			errors.append("order line node model %s: %s" % [NODE_MODEL, _inst.get_model_report(_model)])
	return {"ok": errors.is_empty(), "errors": errors}


## The preset 0x1198B3 | 7 as the port draws W3D's alpha preset elsewhere (the streaks' DAT 0xD9B2F0 material, GodotGameWorld): the gamma-space
## transparent pass takes the texel and the vertex colour as they are (RENDER-3, S-831; a source_color sampler would linearise EXLaser's greys and
## darken the line), texture times colour (primary gradient 6 = GRADIENT_MODULATE without the overbright, BFME2 Apply RVA 0x137590 default case),
## SRC_ALPHA / INV_SRC_ALPHA, depth compare always (| 7), no depth write, no culling; V repeats (TILED, the point index * 4).
static var _shared_shader: Shader

static func _shader() -> Shader:
	if _shared_shader == null:
		_shared_shader = Shader.new()
		_shared_shader.code = """shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, depth_test_disabled, shadows_disabled, blend_mix;
uniform sampler2D tex : hint_default_white, filter_linear_mipmap, repeat_enable;
void fragment() {
	vec4 t = texture(tex, UV) * COLOR;
	ALBEDO = clamp(t.rgb, 0.0, 1.0);
	ALPHA = t.a;
}
"""
	return _shared_shader


func _process(_delta: float) -> void:
	_mesh.clear_surfaces()
	surfaces_drawn = 0
	if hud == null:
		return
	var cam := get_viewport().get_camera_3d()
	if cam == null:
		return
	var state: Dictionary = hud.build_order_line_mesh(cam.global_transform)
	for m in state.get("meshes", []):
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = m.vertices
		arrays[Mesh.ARRAY_TEX_UV] = m.uvs
		arrays[Mesh.ARRAY_COLOR] = m.colors
		arrays[Mesh.ARRAY_INDEX] = m.indices
		_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		_mesh.surface_set_material(surfaces_drawn, _materials[int(m.texture)])
		surfaces_drawn += 1
	_place_markers(state.get("markers", []))


func _place_markers(markers: Array) -> void:
	markers_drawn = 0
	if _model < 0:
		return
	while _marker_ids.size() > markers.size():
		_inst.remove_instance(_marker_ids.pop_back())
	for i in range(markers.size()):
		var t := Transform3D(Basis.IDENTITY, markers[i].position)
		if i < _marker_ids.size():
			_inst.set_instance_transform(_marker_ids[i], t)
		else:
			var id: int = _inst.add_instance(_model, t, "", 0.0, 1.0)
			if id < 0:
				break
			_marker_ids.append(id)
		markers_drawn += 1

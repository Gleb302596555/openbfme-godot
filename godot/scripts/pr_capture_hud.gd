## PR capture viewer, HUD side: the hud_viewer (the live game, the real HUD and input) with scripted scenarios for the HUD rows of the PR candidates.
## It extends hud_viewer.gd, so every hud_viewer option works (--map, --spawn=<Template>:<dx>,<dy>, --enemy=..., --audio, ...).
##
##   godot --path godot res://scenes/pr_capture_hud.tscn -- --scenario=pr_<mode> --spawn=... --shots=<dir> --prefix=<before|after> [--pr-frames=3,10,25] [--pr-caption=...]
##
## Modes (--scenario=):
##   pr_click    click the first --spawn object (select it), save <prefix>-click-<n>.png at each frame count of --pr-frames after the click; prints the selection and the command buttons
##   pr_box      drag a box over the --spawn objects (centre of the first), save the same way; prints the selection (the objects the box took)
##   pr_garrison order every --spawn object but the first into the first (a building), wait, click the building, save the frames (the pips above it)
##   pr_dolly    the tactical camera at three heights over the first --spawn object (320 / 200 / 120): the camera-distance fade of a model (PORT-49)
##   pr_shell    the first --enemy object attacks the first --spawn object; frames of --pr-frames (the HUD's tactical camera: scorch marks, fires)
##   pr_lines    select the first --spawn object, queue three move orders with Shift, hold Alt: the order lines (P-9); the lines are only drawn when
##               this build has them (hud.build_order_line_mesh + scripts/order_lines.gd); otherwise the picture shows the plain selection
##   pr_voice    audio on: select the first --spawn object, then order it to move / attack the first --enemy object; every sound request is in the OPENBFME_AUDIO_LOG file
extends "res://scripts/hud_viewer.gd"

var _pr_frames: Array = [3, 10, 25]
var _pr_prefix := "after"
var _pr_caption := ""
var _pr_shots := ""
var _pr_cam_h := 330.0   # --pr-cam-h=<h>: hud_viewer's camera height (clamped by the retail limits)
var _pr_cam_dy := -150.0   # --pr-cam-dy=<d>: the camera's look-at point is the first object shifted by this much along y
var _pr_objs: Array = []   # --pr-obj=<Template>,<dx>,<dy> (repeatable): more objects of the local player, offsets from the first --spawn object


func _pr_args() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--pr-frames="):
			_pr_frames = []
			for f in arg.substr(12).split(","):
				_pr_frames.append(int(f))
		elif arg.begins_with("--pr-caption="):
			_pr_caption = arg.substr(13)
		elif arg.begins_with("--prefix="):
			_pr_prefix = arg.substr(9)
		elif arg.begins_with("--shots="):
			_pr_shots = arg.substr(8)
		elif arg.begins_with("--pr-cam-h="):
			_pr_cam_h = float(arg.substr(11))
		elif arg.begins_with("--pr-cam-dy="):
			_pr_cam_dy = float(arg.substr(12))
		elif arg.begins_with("--pr-obj="):
			_pr_objs.append(arg.substr(9))


func _pr_save(name: String) -> void:
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	_save(_pr_shots.path_join("%s-%s.png" % [_pr_prefix, name]))


func _pr_frames_after(tag: String) -> void:
	var done := 0
	for f in _pr_frames:
		await _frames(int(f) - done)
		done = int(f)
		await _pr_save("%s-%d" % [tag, done])


## the HUD camera's look-at point is not the screen centre: move it until the world point is drawn at the given pixel
func _center_on(world: Vector2, pixel: Vector2) -> void:
	for i in 12:
		await _frames(2)
		var px: Vector2 = _hud.world_to_pixel(world)
		var err := px - pixel
		print("PRCAP centre step ", i, " px ", px, " target ", _cam_target, " cam ", JSON.stringify(_hud.get_camera()))
		if err.length() < 12.0:
			break
		_cam_target += Vector2(err.x, -err.y) * 1.2 * 0.7
		_hud.camera_look_at(_cam_target)
	print("PRCAP centred: ", _hud.world_to_pixel(world), " target ", _cam_target)


func _run_scenario() -> void:
	if not _scenario.begins_with("pr_"):
		await super()
		return
	_pr_args()
	_label.text = _pr_caption
	await _frames(30)
	var first: Dictionary = _world.get_object(_spawn_ids[0])
	if not first.get("ok", false):
		_fail("the first --spawn object does not exist")
		return
	for po in _pr_objs:
		var pf: PackedStringArray = po.split(",")
		var nid: int = _world.create_object(pf[0], _local_index, first.x + float(pf[1]), first.y + float(pf[2]), 0.0)
		print("PRCAP created ", pf[0], " as ", nid)
		_spawn_ids.append(nid)
	_cam_target = Vector2(first.x, first.y)
	_cam_height = _pr_cam_h
	_cam_back = 260.0
	_place_camera()
	await _frames(10)
	await _center_on(Vector2(first.x, first.y), Vector2(640, 300))
	match _scenario:
		"pr_click":
			await _click_world(Vector2(first.x, first.y))
			await _pr_frames_after("click")
		"pr_box":
			# the box takes every --spawn object: centred on their centroid, 1.2 x as wide as they spread
			var cen := Vector2.ZERO
			for sid in _spawn_ids:
				var so: Dictionary = _world.get_object(sid)
				cen += Vector2(so.x, so.y)
			cen /= _spawn_ids.size()
			var half := Vector2(60, 50)
			for sid in _spawn_ids:
				var so2: Dictionary = _world.get_object(sid)
				half = Vector2(maxf(half.x, absf(so2.x - cen.x) + 50.0), maxf(half.y, absf(so2.y - cen.y) + 50.0))
			_cam_target = cen
			_cam_height = 330.0 + half.length() * 0.5
			_place_camera()
			await _frames(20)
			var pa: Vector2 = _hud.world_to_pixel(cen + Vector2(-half.x, half.y))
			var pb: Vector2 = _hud.world_to_pixel(cen + Vector2(half.x, -half.y))
			_hud.inject_mouse_move(pa)
			await _frames(3)
			_hud.inject_mouse_button(1, true, pa, false)
			for t in 12:
				_hud.inject_mouse_move(pa.lerp(pb, (t + 1) / 12.0))
				await _frames(2)
			_hud.inject_mouse_button(1, false, pb, false)
			await _frames(3)
			print("PRCAP selection ", JSON.stringify(_hud.get_selection()))
			await _pr_frames_after("box")
		"pr_garrison":
			var ids: Array = _spawn_ids.slice(1)
			print("PRCAP garrison order ", JSON.stringify(_world.order_garrison(ids, _spawn_ids[0])))
			await _frames(200)
			print("PRCAP garrison ", JSON.stringify(_world.get_garrison(_spawn_ids[0])))
			await _click_world(Vector2(first.x, first.y))
			await _pr_frames_after("garrison")
		"pr_dolly":
			# the game camera (the HUD's tactical camera: it feeds the alpha camera fade) moves toward the first --spawn object
			for h in [300.0, 200.0, 120.0]:
				_hud.camera_look_at(Vector2(first.x, first.y - 150.0))
				_hud.camera_set_height(h)
				await _frames(40)
				await _pr_save("dolly-%d" % int(h))
		"pr_shell":
			# the first --enemy object (a catapult) shells the first --spawn object; the HUD's own camera sees the scorch marks
			print("PRCAP order ", JSON.stringify(_world.order_attack([_enemy_ids[0]], _spawn_ids[0])))
			await _pr_frames_after("shell")
		"pr_lines":
			var lines: Node3D = null
			if ResourceLoader.exists("res://scripts/order_lines.gd") and _hud.has_method("build_order_line_mesh"):
				lines = load("res://scripts/order_lines.gd").new()
				add_child(lines)
				print("PRCAP order lines setup ", JSON.stringify(lines.setup(_hud, _fs)))
			else:
				print("PRCAP this build has no order lines (no build_order_line_mesh / order_lines.gd)")
			await _click_world(Vector2(first.x, first.y))
			_hud.inject_key(0x2A, true) # Shift: queue the orders
			await _frames(2)
			var points := [Vector2(first.x + 160, first.y + 60), Vector2(first.x + 260, first.y - 80), Vector2(first.x + 120, first.y - 160)]
			for p in points:
				await _click_world(p)
			_hud.inject_key(0x2A, false)
			await _frames(10)
			_hud.inject_key(0x38, true) # Alt: the order lines
			await _frames(5)
			print("PRCAP selection ", JSON.stringify(_hud.get_selection()))
			await _pr_frames_after("lines")
			_hud.inject_key(0x38, false)
		"pr_voice":
			await _click_world(Vector2(first.x, first.y))
			await _frames(20)
			print("PRCAP selection ", JSON.stringify(_hud.get_selection()))
			await _pr_save("selected")
			if not _enemy_ids.is_empty():
				var eo: Dictionary = _world.get_object(_enemy_ids[0])
				await _click_world(Vector2(eo.x, eo.y), 1) # the default mouse: the left click on an enemy = the attack order
				await _frames(60)
				await _pr_save("attack-order")
			var mv := Vector2(first.x + 200, first.y + 80)
			await _click_world(Vector2(first.x, first.y))
			await _frames(10)
			await _click_world(mv)
			await _frames(60)
			await _pr_save("move-order")
		_:
			_fail("unknown pr scenario %s" % _scenario)
			return
	_finish()

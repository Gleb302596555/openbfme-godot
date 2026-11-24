## PR capture viewer: ONE generic scene for the "before / after" pictures upstream asks for (docs/ennorath/PR_CANDIDATES.md, visual rows).
## The scene is driven by a small JSON config per candidate (godot/pr_capture/<key>.json): a map, objects to create, a camera, and a list of steps
## (wait N game frames, call a GameWorld method, save a screenshot, set a caption). The same script runs on the fork's main ("after") and on upstream's main
## ("before"; the files are copied there for the run only), so both sides see the same map, objects, camera and frames.
##
##   godot --path godot res://scenes/pr_capture_viewer.tscn -- --config=<json> --shots=<dir> --prefix=<before|after>
##
## Options (after `--`):
##   --config=<path>   the candidate's JSON (an absolute path, or res://...)
##   --shots=<dir>     where the PNGs go (<prefix>-<shot>.png); also <prefix>-calls.txt with the results of the "log" calls
##   --prefix=<name>   "before" or "after"
##
## Config (all keys optional but "steps"):
##   map: string (default "map mp fall back 4p"); slots: [{faction, human}] (default Player_1 FactionMen human, Player_2 FactionMordor)
##   spot_mode: "trees" = the map's densest tree cluster;  spot: [x, y] (default: an open place near the middle of the map's trees); seed: int; weather etc. via "call" steps
##   audio: bool        attach the game audio (the audio event log goes to the file in the OPENBFME_AUDIO_LOG environment variable)
##   objects: [{name, template, player (0 = local, 1 = enemy, -1 = neutral), dx, dy, angle (degrees), complete (debug_build_complete)}]
##   camera: {eye: [dx, dy, dz], focus: [dx, dy, dz]}   dx/dy relative to the spot, dz above the ground there
##   steps: list of dictionaries, run in order, each may hold:
##     cam: {eye, focus}      move the camera;  follow: "<name>" with eye / focus relative to that object
##     caption: "text"        the label in the scene (what to look at)
##     wait: N                N more game frames
##     spawn: {object}        create an object now
##     call: "method", args: [...]   a GameWorld method; "@name" = that object's id, "#local" / "#enemy" = the player index,
##                                  "x:N" / "y:N" = spot.x / spot.y + N; "tx:N" / "ty:N" = the N-th nearest map tree's x / y; "log": true prints/writes the result
##     damage_frac: ["name", f]  removes f of that object's max health
##     shot: "name"           saves <prefix>-<name>.png
extends Node3D

var _cfg: Dictionary = {}
var _shots := ""
var _prefix := "after"
var _world: Node3D
var _camera: Camera3D
var _label: Label
var _local := -1
var _enemy := -1
var _spot := Vector2.ZERO
var _ids := {}
var _follow := ""
var _follow_eye := Vector3(0, -230, 150)
var _follow_focus := Vector3(0, 0, 20)
var _calls_log := PackedStringArray()
var _fs: RefCounted


func _ready() -> void:
	var cfg_path := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--config="):
			cfg_path = arg.substr(9)
		elif arg.begins_with("--shots="):
			_shots = arg.substr(8)
		elif arg.begins_with("--prefix="):
			_prefix = arg.substr(9)
	var text := FileAccess.get_file_as_string(cfg_path)
	if text.is_empty():
		_fail("cannot read config %s" % cfg_path)
		return
	var parsed: Variant = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		_fail("config is not a JSON object: %s" % cfg_path)
		return
	_cfg = parsed
	var map_name: String = _cfg.get("map", "map mp fall back 4p")
	var fs: RefCounted = ClassDB.instantiate("RetailFileSystem")
	_fs = fs
	var mount: Dictionary = fs.mount_retail()
	if not mount.ok:
		_fail("mount failed: %s" % [mount.errors])
		return
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.45, 0.55, 0.7)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.6, 0.6, 0.6)
	env.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)
	_camera = Camera3D.new()
	_camera.fov = 45
	_camera.near = 2.0
	_camera.far = 60000.0
	add_child(_camera)
	var builder: RefCounted = ClassDB.instantiate("MapTerrainBuilder")
	var root: Node3D = builder.build_map(fs, map_name, {"markers": false, "fog": false})
	if root == null:
		_fail("build_map failed: %s" % [builder.get_report().errors])
		return
	add_child(root)
	_world = ClassDB.instantiate("GameWorld")
	var setup: Dictionary = _world.setup(fs)
	if not setup.ok:
		_fail("object world failed: %s" % [setup.errors])
		return
	var slots: Array = _cfg.get("slots", [
		{"player": "Player_1", "faction": "FactionMen", "human": true, "team": 0, "start_index": 0},
		{"player": "Player_2", "faction": "FactionMordor", "human": false, "team": 1, "start_index": 1},
	])
	var rep: Dictionary = _world.load_map(map_name, {"slots": slots, "seed": int(_cfg.get("seed", 4711)), "test_hooks": true})
	if not rep.ok:
		_fail("load_map failed: %s" % [rep.errors.slice(0, 5)])
		return
	root.add_child(_world)
	var idx := 0
	for pl in rep.players:
		if "Player_1" in str(pl) and _local < 0:
			_local = idx
		if "Player_2" in str(pl) and _enemy < 0:
			_enemy = idx
		idx += 1
	_world.set_logic_thread(false)
	_world.set_shroud_drawn(false)
	_world.set_auto_advance(true)
	_world.set_local_player(_local)
	var layer := CanvasLayer.new()
	add_child(layer)
	_label = Label.new()
	_label.position = Vector2(12, 34)
	_label.add_theme_font_size_override("font_size", 22)
	_label.add_theme_color_override("font_outline_color", Color.BLACK)
	_label.add_theme_constant_override("outline_size", 5)
	layer.add_child(_label)
	_run()


func _fail(message: String) -> void:
	push_error(message)
	print("PRCAP FAIL: ", message)
	get_tree().quit(1)


func _sage_to_godot(p: Vector3) -> Vector3:
	return Vector3(p.x, p.z, -p.y)


func _aim(focus: Vector3, eye: Vector3) -> void:
	_camera.position = _sage_to_godot(eye)
	_camera.look_at(_sage_to_godot(focus), Vector3.UP)


## an open spot near the middle of the map's trees: no tree within 300 of it
func _pick_spot() -> Vector2:
	var trees: PackedVector3Array = _world.get_terrain_trees(-1)
	var lo := Vector2(1e9, 1e9)
	var hi := Vector2(-1e9, -1e9)
	for t in trees:
		lo = Vector2(minf(lo.x, t.x), minf(lo.y, t.y))
		hi = Vector2(maxf(hi.x, t.x), maxf(hi.y, t.y))
	if trees.is_empty():
		return Vector2(2000, 2000)
	var centre := (lo + hi) * 0.5
	var best := centre
	var best_d := 1e18
	for gx in range(-20, 21):
		for gy in range(-20, 21):
			var p := centre + Vector2(gx, gy) * 100.0
			var ok := true
			for t in trees:
				if Vector2(t.x, t.y).distance_to(p) < 300.0:
					ok = false
					break
			if ok and p.distance_squared_to(centre) < best_d:
				best = p
				best_d = p.distance_squared_to(centre)
	return best


## the map's densest tree cluster: the tree with the most other trees within 500
func _pick_tree_cluster() -> Vector2:
	var trees: PackedVector3Array = _world.get_terrain_trees(-1)
	if trees.is_empty():
		return _pick_spot()
	var best := Vector2(trees[0].x, trees[0].y)
	var best_n := -1
	var step := maxi(1, trees.size() / 400)
	for i in range(0, trees.size(), step):
		var p := Vector2(trees[i].x, trees[i].y)
		var n := 0
		for t in trees:
			if Vector2(t.x, t.y).distance_to(p) < 500.0:
				n += 1
		if n > best_n:
			best_n = n
			best = p
	print("PRCAP tree cluster ", best, " with ", best_n, " of ", trees.size(), " trees")
	return best


## the steepest open ground: the grid point whose ground height differs most from its neighbours 60 units away, with no tree within 200
func _pick_slope() -> Vector2:
	var trees: PackedVector3Array = _world.get_terrain_trees(-1)
	var best := _pick_spot()
	var best_g := -1.0
	for gx in range(-15, 16):
		for gy in range(-15, 16):
			var p := best + Vector2(gx, gy) * 80.0
			var h0: float = _world.get_ground_height(p.x, p.y)
			var g := absf(_world.get_ground_height(p.x + 60.0, p.y) - h0) + absf(_world.get_ground_height(p.x, p.y + 60.0) - h0)
			var ok := g > best_g and g < 45.0 and h0 > 0.0 # a real slope a vehicle can stand on: at most 45 units of rise over 60 + 60
			if ok:
				for t in trees:
					if Vector2(t.x, t.y).distance_to(p) < 200.0:
						ok = false
						break
			if ok:
				best_g = g
				best = p
	print("PRCAP slope spot ", best, " gradient ", best_g)
	return best


func _pos(id: int) -> Vector3:
	var o: Dictionary = _world.get_object(id)
	return Vector3(o.get("x", 0.0), o.get("y", 0.0), o.get("z", 0.0))


func _vec3(a: Array) -> Vector3:
	return Vector3(float(a[0]), float(a[1]), float(a[2]))


func _place_camera(c: Dictionary) -> void:
	var g: float = _world.get_ground_height(_spot.x, _spot.y)
	var e := _vec3(c["eye"])
	var f := _vec3(c["focus"])
	_aim(Vector3(_spot.x + f.x, _spot.y + f.y, g + f.z), Vector3(_spot.x + e.x, _spot.y + e.y, g + e.z))


func _resolve(v: Variant) -> Variant:
	if typeof(v) == TYPE_STRING:
		var s: String = v
		if s.begins_with("@"):
			return int(_ids.get(s.substr(1), -1))
		if s == "#local":
			return _local
		if s == "#enemy":
			return _enemy
		if s.begins_with("tx:") or s.begins_with("ty:"):
			# the N-th nearest map tree / shrub to the spot
			var trees: PackedVector3Array = _world.get_terrain_trees(-1)
			var order := []
			for t in trees:
				order.append([Vector2(t.x, t.y).distance_to(_spot), t])
			order.sort_custom(func(a, b): return a[0] < b[0])
			var tr: Vector3 = order[mini(int(s.substr(3)), order.size() - 1)][1]
			return tr.x if s.begins_with("tx:") else tr.y
		if s.begins_with("x:"):
			return _spot.x + float(s.substr(2))
		if s.begins_with("y:"):
			return _spot.y + float(s.substr(2))
		return s
	if typeof(v) == TYPE_ARRAY:
		var out := []
		for e in v:
			out.append(_resolve(e))
		return out
	if typeof(v) == TYPE_DICTIONARY:
		var d := {}
		for k in v:
			d[k] = _resolve(v[k])
		return d
	return v


func _spawn(o: Dictionary) -> int:
	var pk := int(o.get("player", 0))
	var player: int = _local if pk == 0 else (_enemy if pk == 1 else -1)
	var id: int = _world.create_object(str(o["template"]), player, _spot.x + float(o.get("dx", 0.0)), _spot.y + float(o.get("dy", 0.0)), deg_to_rad(float(o.get("angle", 0.0))))
	if id <= 0:
		_fail("create_object(%s) returned %d" % [o["template"], id])
		return id
	if o.has("name"):
		_ids[str(o["name"])] = id
	print("PRCAP created %s as %d" % [o["template"], id])
	if o.get("complete", false):
		print("PRCAP build complete ", JSON.stringify(_world.debug_build_complete(id)))
	return id


func _wait_frames(n: int) -> void:
	var f0: int = _world.get_frame()
	while _world.get_frame() < f0 + n:
		await get_tree().process_frame
		_after_frame()


func _after_frame() -> void:
	if _follow != "" and _ids.has(_follow):
		var p := _pos(int(_ids[_follow]))
		var g: float = _world.get_ground_height(p.x, p.y)
		_aim(Vector3(p.x, p.y, g) + _follow_focus, Vector3(p.x, p.y, g) + _follow_eye)


func _log(line: String) -> void:
	print("PRCAP ", line)
	_calls_log.append(line)


func _run() -> void:
	await get_tree().process_frame
	_spot = _pick_spot()
	if _cfg.get("spot_mode", "") == "trees":
		_spot = _pick_tree_cluster()
	if _cfg.get("spot_mode", "") == "slope":
		_spot = _pick_slope()
	if _cfg.has("spot"):
		_spot = Vector2(float(_cfg["spot"][0]), float(_cfg["spot"][1]))
	print("PRCAP spot ", _spot, " ground ", _world.get_ground_height(_spot.x, _spot.y))
	print("PRCAP auto-acquire ", JSON.stringify(_world.debug_set_auto_acquire(bool(_cfg.get("auto_acquire", false)))))
	if _cfg.get("audio", false):
		# the retail audio manager first (GameAudio.boot), then the game's audio side; every request goes to the OPENBFME_AUDIO_LOG file
		var ga: Node = ClassDB.instantiate("GameAudio")
		ga.name = "GameAudio"
		add_child(ga)
		print("PRCAP audio boot ", JSON.stringify(ga.boot(_fs, {"seed": 4711})))
		print("PRCAP audio attach ", JSON.stringify(_world.attach_audio()))
	for o in _cfg.get("objects", []):
		if _spawn(o) <= 0:
			return
	if _cfg.has("camera"):
		_place_camera(_cfg["camera"])
	await _wait_frames(int(_cfg.get("settle", 5)))
	for step in _cfg.get("steps", []):
		if step.has("caption"):
			_label.text = str(step["caption"])
		if step.has("cam"):
			_follow = ""
			_place_camera(step["cam"])
		if step.has("follow"):
			_follow = str(step["follow"])
			if step.has("eye"):
				_follow_eye = _vec3(step["eye"])
			if step.has("focus"):
				_follow_focus = _vec3(step["focus"])
			_after_frame()
		if step.has("spawn"):
			if _spawn(step["spawn"]) <= 0:
				return
		if step.has("call"):
			var args: Array = _resolve(step.get("args", []))
			var res: Variant = _world.callv(str(step["call"]), args)
			if step.get("log", false) or (typeof(res) == TYPE_DICTIONARY and not res.get("ok", true)):
				_log("%s(%s) -> %s" % [step["call"], JSON.stringify(args), JSON.stringify(res)])
		if step.has("damage_frac"):
			# ["name", fraction of max health to remove]
			var oid := int(_ids[str(step["damage_frac"][0])])
			var od: Dictionary = _world.get_object(oid)
			var amount := float(od.get("max_health", 0.0)) * float(step["damage_frac"][1])
			_log("damage_frac %s %s -> %s" % [step["damage_frac"][0], amount, JSON.stringify(_world.debug_damage_object(oid, amount, -1))])
		if step.has("wait"):
			await _wait_frames(int(step["wait"]))
		if step.has("shot"):
			await _save(str(step["shot"]))
	if _shots != "":
		var f := FileAccess.open(_shots.path_join("%s-calls.txt" % _prefix), FileAccess.WRITE)
		if f:
			f.store_string("\n".join(_calls_log) + "\n")
	print("PRCAP DONE")
	get_tree().quit(0)


func _save(shot: String) -> void:
	await get_tree().process_frame
	await RenderingServer.frame_post_draw
	if _shots.is_empty():
		return
	var img := get_viewport().get_texture().get_image()
	var path := _shots.path_join("%s-%s.png" % [_prefix, shot])
	img.save_png(path)
	print("PRCAP SAVED ", path, " frame ", _world.get_frame())

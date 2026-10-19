## Placement selection must reuse the renderer resources loaded during HUD setup.
## Run headless or windowed with ROTWK_INSTALL and BFME2_INSTALL set.
extends SceneTree
var _failures := 0
func _initialize() -> void:
	call_deferred("_run")
func _check(ok: bool, message: String) -> void:
	print("  ", "ok   " if ok else "FAIL ", message)
	if not ok: _failures += 1
func _frames(count: int = 4) -> void:
	for i in count: await process_frame
func _check_hidden(world: Node) -> void:
	for node in world.get_children():
		if node is Node3D and (str(node.name).begins_with("PlacementGhost") or str(node.name).begins_with("PlacementBib") or str(node.name).begins_with("PlacementShadowDecal")):
			_check(not node.visible, "inactive preview node %s is hidden" % node.name)
func _run() -> void:
	if OS.get_environment("ROTWK_INSTALL").is_empty() or OS.get_environment("BFME2_INSTALL").is_empty():
		print("SKIP: retail install paths required")
		quit(77)
		return
	root.size = Vector2i(1280, 720)
	var fs: RefCounted = ClassDB.instantiate("RetailFileSystem")
	assert(fs.mount_retail().ok)
	var world: Node3D = ClassDB.instantiate("GameWorld")
	root.add_child(world)
	assert(world.setup(fs).ok)
	var report: Dictionary = world.load_map("map mp evendim", {"seed": 314, "slots": [
		{"player": "Player_1", "faction": "FactionMen", "human": true, "team": 0},
		{"player": "Player_2", "faction": "FactionMordor", "human": false, "team": 1}]})
	assert(report.ok)
	world.set_logic_thread(false)
	var player := -1
	for i in report.players.size():
		if str(report.players[i]).contains("'Player_1'"): player = i
	assert(player != -1)
	var builder: int = world.create_object("MenPorter", player, 4300.0, 1300.0, 0.0)
	assert(builder != -1, "Men porter fixture must spawn")
	world.give_money(player, 100000)
	world.advance(0.2)
	var camera := Camera3D.new()
	root.add_child(camera)
	var hud: Node = ClassDB.instantiate("InGameHudNode")
	root.add_child(hud)
	await _frames()
	var before_hash: int = world.get_state_hash()
	var setup: Dictionary = hud.setup(fs, world, camera, "Player_1", {"camera_start": Vector3(4300, 0, -1300), "edge_scroll": false})
	assert(setup.ok)
	_check(world.get_state_hash() == before_hash, "preview preload leaves authoritative state unchanged")
	var initial: Dictionary = hud.get_placement_ghost_state()
	_check(initial.preloaded_templates != 0, "local construction menu preloaded")
	_check(initial.icons.is_empty(), "preloading leaves no active placement")
	_check_hidden(world)
	_check(initial.errors.is_empty(), "preload reports no resource errors: %s" % [initial.errors])
	hud.select_object(builder)
	await _frames()
	var builds: int = initial.model_builds
	var instance_records: int = initial.instance_records
	for iteration in 24:
		var name: String = ["GondorBarracks", "GondorFarm"][iteration % 2]
		var start := Time.get_ticks_usec()
		_check(hud.press_command_button(name), "%s placement starts" % name)
		hud.inject_mouse_move(Vector2(640, 260))
		await _frames()
		var ghost: Dictionary = hud.get_placement_ghost_state()
		_check(ghost.template == name and not ghost.icons.is_empty(), "%s preview is active" % name)
		_check(ghost.instance_records == instance_records, "%s reuses preloaded instance records" % name)
		_check(ghost.model_builds == builds, "%s selects without building new GPU models" % name)
		print("PLACEMENT selection ", name, " elapsed_ms=", (Time.get_ticks_usec() - start) / 1000.0)
		hud.inject_key(1, true) # DIK_ESCAPE: cancel via the real input path
		hud.inject_key(1, false)
		await _frames()
		_check(hud.get_placement_ghost_state().icons.is_empty(), "cancel hides placement")
		_check(hud.get_placement_ghost_state().model_builds == builds, "cancel retains model cache")
		_check_hidden(world)
	hud.free()
	world.free()
	camera.free()
	print("PLACEMENT CACHE ", "PASS" if _failures == 0 else "FAIL")
	quit(0 if _failures == 0 else 1)

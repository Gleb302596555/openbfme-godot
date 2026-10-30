## Lane VISUAL-2: the owner's playtest bugs (2026-10-17), checked and captured in the real client. game.gd runs it after the start with
## --visual2=<scenario>[,...] (with --auto); screenshots go to --screens as visual2-<name>.png; every scenario prints "VISUAL2 <scenario>: ok" or
## "VISUAL2 <scenario>: FAIL <why>" and "VISUAL2 RESULT: <n> ok, <m> failed" ends the run.
##   fog    the enemy as the local player sees it: every enemy object whose status for the local player is SHROUDED in two samples 3 s apart must not
##          be drawn at the second (VIS-1 with RW 0x64849E's 2 s grace); the particles live around the enemy objects the shroud hides (retail: their
##          systems emit nothing, RW 0x5FA2CE / 0x5E2282: only what was emitted while seen is left, and it dies); the camera on the enemy builder (or
##          unit, or structure) in the fog: visual2-fog-<n>.png. FAIL: a hidden enemy object drawn
##   trees  the camera close over the trees nearest the local base, two shots 0.6 s apart (visual2-trees-a / -b.png) and the SwayOffsets globals moving
##          (W3DVegetationSway: zero when nothing sways). FAIL: the globals do not move
##   ghost  a structure of another player (neutral) that is FOGGED and drawn (RW 0x68EDD0: a neutral IMMOBILE object is not hidden in the fog): the
##          camera on it, lit by the fogged light (RW 0x46F48E: visual2-ghost-fogged.png). FAIL: no such structure
##   reveal (lane FOGFX-1, FB-0061) a cavalry unit of the local player made at its base's sight rides toward the nearest enemy structure, the
##          camera raised and following it, for 14 s (420 frames: a Movie Maker clip's frames): the shroud texture fades as RotWK's W3DShroud does (RW 0x472CF2); shots
##          visual2-reveal-<n>.png. reveal_snap: the same with the fade off (GameWorld.set_shroud_fade(false): each 40 ft cell lands at once, the "before")
##   dust   (lane FOGFX-1, FB-0056) a building rises on a fortress pad of an enemy in the fog (build2_construct_on_plot; its worker walks over first,
##          up to 900 logic frames): the camera on it 5 s (nothing
##          drawn, no dust: RW 0x5FA2CE), then a unit of the local player made beside it reveals it: the building and its construction dust 6 s;
##          shots visual2-dust-fog.png / visual2-dust-seen.png. FAIL: particles near it while fogged, none once seen
##   look   the local base and the map's middle at the default camera (visual2-look-base / -middle.png) for the colour comparison
extends "res://scripts/scenario_player.gd"

var game: Node
var hud: Node
var world: Node


func _run(p_game: Node, scenarios: PackedStringArray) -> void:
	game = p_game
	hud = game._hud
	world = game._world
	await _run_scenarios(scenarios)


func _frames(n: int) -> void:
	for i in n:
		await game.get_tree().process_frame


func _seconds(s: float) -> void:
	await game.get_tree().create_timer(s).timeout


func _enemy_names() -> Array:
	var out := []
	for p in world.get_economy().players:
		if p.playable and p.name != game._local_name:
			out.append(p.name)
	return out


func _local_base() -> Vector2:
	var sum := Vector2()
	var n := 0
	for o in world.get_player_objects(game._local_name):
		if o.get("structure", false):
			sum += Vector2(o.x, o.y)
			n += 1
	return sum / n if n > 0 else Vector2()


## the enemy objects with their status for the local player (3 FOGGED, 4 SHROUDED) and the render view's hiding
func _enemy_sample() -> Array:
	var out := []
	for name in _enemy_names():
		for o in world.get_player_objects(name):
			var id: int = o.id
			var full: Dictionary = world.get_object(id)
			if not full.get("ok", false) or full.get("destroyed", false) or full.get("contained_by", 0) != 0:
				continue
			var pose: Dictionary = world.get_render_pose(id)
			if not pose.get("ok", false) or not pose.get("view_valid", false):
				continue
			out.append({"id": id, "template": full.template, "x": full.x, "y": full.y, "structure": full.get("structure", false),
				"dozer": full.has("dozer_task"), "status": world.get_object_shroud_status(id), "hidden": pose.get("view_hidden", false),
				"obscured": pose.get("fully_obscured", null)})
	return out


func _scenario_fog() -> String:
	var shroud: Dictionary = world.get_shroud_report()
	print("VISUAL2 fog shroud: %s" % JSON.stringify(shroud))
	if not shroud.get("ok", false) or int(shroud.get("fogged", 0)) + int(shroud.get("shrouded", 0)) == 0:
		return "the local player has no fogged or shrouded cell (the shroud is not displayed)"
	var first := _enemy_sample()
	await _seconds(3.0)
	var second := _enemy_sample()
	var shrouded_then := {}
	for e in first:
		if e.status == 4:
			shrouded_then[e.id] = true
	var leaks := []
	var hidden_enemies := 0
	var drawn_in_fog := 0
	var particles_near_hidden := 0
	var systems_near_hidden := 0
	var shot_target = null
	for e in second:
		if e.status == 4:
			hidden_enemies += 1
			if shrouded_then.has(e.id) and not e.hidden:
				leaks.append("%s #%d" % [e.template, e.id])
			var pn: Dictionary = world.get_particles_near(e.x, e.y, 40.0)
			particles_near_hidden += int(pn.particles)
			systems_near_hidden += int(pn.systems)
		elif e.status == 3 and not e.hidden:
			drawn_in_fog += 1 # a fogged structure the local player saw: drawn as its last sight (the ghost, S-561)
		if e.status >= 3 and not e.structure:
			if shot_target == null or (e.dozer and not shot_target.dozer):
				shot_target = e
	if shot_target == null:
		for e in second:
			if e.status >= 3:
				shot_target = e
				break
	print("VISUAL2 fog: %d enemy objects, %d hidden by the shroud, %d leaks (drawn while shrouded 3 s: %s), %d fogged drawn as last seen, particles near hidden enemies %d in %d systems" %
		[second.size(), hidden_enemies, leaks.size(), ", ".join(leaks.slice(0, 12)), drawn_in_fog, particles_near_hidden, systems_near_hidden])
	if shot_target != null:
		hud.camera_look_at(Vector2(shot_target.x, shot_target.y))
		await _frames(20)
		game._save_named("visual2-fog-1")
		print("VISUAL2 fog shot: %s #%d at (%d, %d) status %d hidden %s" % [shot_target.template, shot_target.id, shot_target.x, shot_target.y, shot_target.status, shot_target.hidden])
		# a second spot: the most crowded hidden enemy area (an expansion being built)
		var best = null
		var best_n := 0
		for e in second:
			if e.status != 4:
				continue
			var n := 0
			for f in second:
				if f.status == 4 and Vector2(f.x, f.y).distance_to(Vector2(e.x, e.y)) < 300.0:
					n += 1
			if n > best_n:
				best_n = n
				best = e
		if best != null:
			hud.camera_look_at(Vector2(best.x, best.y))
			await _frames(20)
			game._save_named("visual2-fog-2")
			print("VISUAL2 fog shot 2: around %s #%d at (%d, %d), %d hidden enemy objects within 300" % [best.template, best.id, best.x, best.y, best_n])
	if not leaks.is_empty():
		return "%d enemy objects drawn while the shroud hides them: %s" % [leaks.size(), ", ".join(leaks.slice(0, 12))]
	# r2: the claims are measured, not printed: an enemy hidden by the shroud exists, and no particle is drawn near any (RW 0x5FA2CE, 0x5E2282)
	if hidden_enemies == 0:
		return "no enemy object is hidden by the shroud: nothing measured"
	if particles_near_hidden > 0:
		return "%d particles in %d systems near enemy objects the shroud hides" % [particles_near_hidden, systems_near_hidden]
	return ""


func _sway_globals() -> Array:
	if not world.has_method("get_tree_sway"):
		return []
	return world.get_tree_sway().get("offsets", [])


func _scenario_trees() -> String:
	var base := _local_base()
	var trees: Array = world.get_logic_trees(base.x, base.y, 3000.0)
	if trees.is_empty():
		return "no tree within 3000 of the local base"
	trees.sort_custom(func(a, b): return Vector2(a.x, a.y).distance_to(base) < Vector2(b.x, b.y).distance_to(base))
	var t: Dictionary = trees[0]
	var at := Vector2(t.x, t.y)
	hud.camera_look_at(at)
	hud.camera_set_height(180.0)
	hud.camera_look_at(at)
	await _seconds(3.0) # the camera settles
	if world.has_method("get_tree_sway"):
		print("VISUAL2 trees sway: %s" % JSON.stringify(world.get_tree_sway()).left(400))
	var a := _sway_globals()
	game._save_named("visual2-trees-a")
	await _seconds(0.6)
	var b := _sway_globals()
	game._save_named("visual2-trees-b")
	await _seconds(2.4)
	var c := _sway_globals()
	game._save_named("visual2-trees-c")
	if a.is_empty() or b.is_empty():
		return "no tree sway in this build (get_tree_sway)"
	print("VISUAL2 trees: tree #%d at (%d, %d); SwayOffsets[1] %s -> %s -> %s" % [t.id, t.x, t.y, str(a[0]), str(b[0]), str(c[0])])
	var moved := false
	for i in 10:
		if (a[i] as Vector3).distance_to(b[i]) > 1e-6: # tolerance: any movement of the sway vector over 0.6 s is the claim
			moved = true
	if not moved:
		return "the SwayOffsets did not move in 0.6 s (%s)" % str(a[0])
	return ""


func _scenario_look() -> String:
	var base := _local_base()
	hud.camera_look_at(base)
	await _frames(30)
	game._save_named("visual2-look-base")
	var shroud: Dictionary = world.get_shroud_report()
	var mid := Vector2(float(shroud.get("cells_x", 0)), float(shroud.get("cells_y", 0))) * float(shroud.get("cell_size", 0.0)) * 0.5
	if mid != Vector2():
		hud.camera_look_at(mid)
		await _frames(30)
		game._save_named("visual2-look-middle")
	return ""


func _scenario_ghost() -> String:
	# a structure that is not the local player's or an enemy's (a neutral camp / ruin) and is FOGGED and drawn: retail lights it by the fogged light
	var mine: String = game._local_name
	var enemies := _enemy_names()
	var pick = null
	var count := 0
	for id in world.get_object_ids():
		var o: Dictionary = world.get_object(id)
		if not o.get("ok", false) or not o.get("structure", false) or o.get("owner", "") == mine or enemies.has(o.get("owner", "")):
			continue
		if int(world.get_object_shroud_status(id)) != 3:
			continue
		var pose: Dictionary = world.get_render_pose(id)
		if not pose.get("ok", false) or pose.get("view_hidden", true):
			continue
		count += 1
		if pick == null:
			pick = o
	print("VISUAL2 ghost: %d fogged structures of other players drawn" % count)
	if pick == null:
		return "no fogged structure of another player is drawn"
	hud.camera_look_at(Vector2(pick.x, pick.y))
	await _seconds(3.0)
	var pose_now: Dictionary = world.get_render_pose(pick.id)
	game._save_named("visual2-ghost-fogged")
	print("VISUAL2 ghost: %s #%d (%s) at (%d, %d) status %d hidden %s fully obscured %s instances %s fog lit %s" % [pick.template, pick.id, pick.get("owner", ""), pick.x, pick.y,
		int(world.get_object_shroud_status(pick.id)), pose_now.get("view_hidden"), str(pose_now.get("fully_obscured")), str(pose_now.get("instances")),
		str(pose_now.get("fog_lit_instances"))])
	# r2: the claim is measured: the fogged structure is drawn fully obscured, and every drawn instance of it carries the fogged light (RW 0x46F48E)
	if not pose_now.get("ok", false) or int(world.get_object_shroud_status(pick.id)) != 3:
		return "the fogged structure #%d is no longer fogged / presented" % pick.id
	if pose_now.get("fully_obscured") != true:
		return "the fogged structure #%d is not fully obscured" % pick.id
	if int(pose_now.get("instances", 0)) == 0:
		return "the fogged structure #%d draws no instance: nothing measured" % pick.id
	if int(pose_now.get("fog_lit_instances", 0)) != int(pose_now.get("instances", 0)):
		return "the fogged structure #%d: %d of %d instances have the fogged light" % [pick.id, int(pose_now.get("fog_lit_instances", 0)), int(pose_now.get("instances", 0))]
	return ""


## lane FOGFX-1: a unit of the local player at `at` (the first template of the list the game makes); -1 when none
func _make_local_unit(at: Vector2, templates := ["GondorFighterHorde", "MordorFighterHorde", "IsengardFighterHorde", "MordorMumakil"]) -> int:
	var me := -1
	var i := 0
	for p in world.get_report().get("players", []): # the player indices create_object takes (play1_player.gd's ghosts)
		if me < 0 and str(p).contains(game._local_name):
			me = i
		i += 1
	for t in templates:
		var id: int = world.create_object(t, me, at.x, at.y, 0.0)
		if id >= 0:
			return id
	return -1


func _scenario_reveal() -> String:
	return await _reveal(true)


func _scenario_reveal_snap() -> String:
	return await _reveal(false)


func _reveal(fade: bool) -> String:
	world.set_shroud_fade(fade)
	var base := _local_base()
	var shroud: Dictionary = world.get_shroud_report()
	var mid := Vector2(float(shroud.get("cells_x", 0)), float(shroud.get("cells_y", 0))) * float(shroud.get("cell_size", 0.0)) * 0.5
	# toward the nearest enemy structure (a land path: the map's middle can be water)
	var goal := mid
	for n in _enemy_names():
		for o in world.get_player_objects(n):
			if o.get("structure", false) and (goal == mid or Vector2(o.x, o.y).distance_to(base) < goal.distance_to(base)):
				goal = Vector2(o.x, o.y)
	var dir := (goal - base).normalized()
	# cavalry, past the base's sight (its own look is the only one around it)
	var from := base + dir * 600.0
	var id := _make_local_unit(from, ["RohanRohirrimHorde", "GondorKnightHorde", "MordorHaradrimRiderHorde", "IsengardWargRiderHorde"])
	if id < 0:
		return "no unit template made for the local player"
	# the horde makes its members over its first logic frames: the order after them
	var made_at: int = world.get_frame()
	while world.get_frame() < made_at + 5:
		await game.get_tree().process_frame
	var to := goal
	var ordered: Variant = world.order_move([id], to.x, to.y)
	print("VISUAL2 reveal: order_move to (%d, %d): %s" % [int(to.x), int(to.y), str(ordered)])
	# the camera raised over the default (its view wider than the unit's look, so the fog edges around it are in view) and following it
	hud.camera_set_zoom_limited(false)
	hud.camera_set_height(700.0)
	print("VISUAL2 reveal: clip from drawn frame %d" % Engine.get_frames_drawn())
	var shots := 0
	for i in 420:
		var o: Dictionary = world.get_object(id)
		if not o.is_empty():
			hud.camera_look_at(Vector2(o.x, o.y))
		await game.get_tree().process_frame
		if i % 60 == 0:
			print("VISUAL2 reveal: frame %d logic %d at (%d, %d)" % [i, world.get_frame(), int(o.get("x", 0)), int(o.get("y", 0))])
		if i % 140 == 139:
			shots += 1
			game._save_named("visual2-reveal%s-%d" % ["" if fade else "-snap", shots])
	print("VISUAL2 reveal: clip to drawn frame %d" % Engine.get_frames_drawn())
	var o: Dictionary = world.get_object(id)
	print("VISUAL2 reveal (fade %s): %s #%d walked from %s to (%d, %d)" % [fade, o.get("template", ""), id, str(from), int(o.get("x", 0)), int(o.get("y", 0))])
	world.set_shroud_fade(true)
	return ""


## every structure template named in VISUAL2_STRUCTURES (comma list), made finished for the local player on one free spot toward the map's middle, shot
## and removed one by one (visual2-struct-<template>.png); camera height from VISUAL2_HEIGHT (default: the game's). FAIL: nothing named
func _scenario_structures() -> String:
	var names := OS.get_environment("VISUAL2_STRUCTURES").split(",", false)
	if names.is_empty():
		return "VISUAL2_STRUCTURES names no template"
	var me := -1
	var i := 0
	for p in world.get_report().get("players", []):
		if me < 0 and str(p).contains(game._local_name):
			me = i
		i += 1
	var height := OS.get_environment("VISUAL2_HEIGHT").to_float() if not OS.get_environment("VISUAL2_HEIGHT").is_empty() else 0.0
	var base := _local_base()
	var shroud: Dictionary = world.get_shroud_report()
	var mid := Vector2(float(shroud.get("cells_x", 0)), float(shroud.get("cells_y", 0))) * float(shroud.get("cell_size", 0.0)) * 0.5
	var dir := (mid - base).normalized() if mid != base else Vector2.RIGHT
	var refused := []
	var n := 0
	for t in names:
		var at := base + dir * 700.0
		var id: int = world.create_object(t, me, at.x, at.y, 0.0)
		if id < 0:
			refused.append(t)
			continue
		n += 1
		hud.camera_look_at(at)
		if height > 0.0:
			hud.camera_set_height(height)
		await _frames(90)
		game._save_named("visual2-struct-" + t)
		world.destroy_object(id)
		await _frames(5)
	print("VISUAL2 structures: %d shot, %d refused %s" % [n, refused.size(), str(refused)])
	return ""


## VISUAL2_UNITS (comma list of templates): each made for the local player and for the first other player, side by side, one shot per template
## (visual2-units-<template>.png); camera height from VISUAL2_HEIGHT. The team colours of the two owners must differ on the drawn units
func _scenario_units() -> String:
	var names := OS.get_environment("VISUAL2_UNITS").split(",", false)
	if names.is_empty():
		return "VISUAL2_UNITS names no template"
	var players: Array = world.get_report().get("players", [])
	var me := -1
	var other := -1
	var enemies := _enemy_names()
	for i in players.size():
		if me < 0 and str(players[i]).contains(game._local_name):
			me = i
		elif other < 0 and not enemies.is_empty() and str(players[i]).contains(str(enemies[0])):
			other = i
	print("VISUAL2 units: players %s, local index %d, other index %d" % [str(players), me, other])
	var height := OS.get_environment("VISUAL2_HEIGHT").to_float() if not OS.get_environment("VISUAL2_HEIGHT").is_empty() else 0.0
	var base := _local_base()
	var shroud: Dictionary = world.get_shroud_report()
	var mid := Vector2(float(shroud.get("cells_x", 0)), float(shroud.get("cells_y", 0))) * float(shroud.get("cell_size", 0.0)) * 0.5
	var dir := (mid - base).normalized() if mid != base else Vector2.RIGHT
	var side := Vector2(-dir.y, dir.x)
	var at := base + dir * 700.0
	var refused := []
	for t in names:
		var a: int = world.create_object(t, me, at.x - side.x * 60.0, at.y - side.y * 60.0, 0.0)
		var b: int = world.create_object(t, other, at.x + side.x * 60.0, at.y + side.y * 60.0, 0.0)
		if a < 0 or b < 0:
			refused.append(t)
		hud.camera_look_at(at)
		if height > 0.0:
			hud.camera_set_height(height)
		await _frames(25)
		game._save_named("visual2-units-" + t)
		var defects := []
		var dd: Dictionary = world.get_report().get("drawables", {}).get("data_defects", {})
		for k in dd.keys():
			if str(k).to_lower().contains(t.to_lower()) or str(k).to_lower().contains("oliph") or str(k).to_lower().contains("mumak"):
				defects.append(str(k) + " x" + str(dd[k]))
		print("VISUAL2 units %s: pose %s, draw defects %s" % [t, str(world.get_render_pose(a)), str(defects).left(1500)])
		for id in [a, b]:
			if id >= 0:
				world.destroy_object(id)
		await _frames(5)
		var hits := []
		var text := JSON.stringify(world.get_report())
		var hit_at := text.to_lower().find(t.to_lower())
		while hit_at >= 0 and hits.size() < 12:
			hits.append(text.substr(maxi(0, hit_at - 200), 500))
			hit_at = text.to_lower().find(t.to_lower(), hit_at + 1)
		print("VISUAL2 units %s: report mentions %s" % [t, str(hits)])
	print("VISUAL2 units: %d shot, %d refused %s (local %s, other player index %d)" % [names.size() - refused.size(), refused.size(), str(refused), game._local_name, other])
	return ""


## VISUAL2_HERO (comma list of templates): each made for the local player and selected, the camera on it: the Palantir shows its ability buttons
## (the locked ones disabled), shot visual2-hero-<template>.png
func _scenario_hero() -> String:
	var names := OS.get_environment("VISUAL2_HERO").split(",", false)
	if names.is_empty():
		return "VISUAL2_HERO names no template"
	var me := -1
	var i := 0
	for p in world.get_report().get("players", []):
		if me < 0 and str(p).contains(game._local_name):
			me = i
		i += 1
	var at := _local_base() + Vector2(300.0, 0.0)
	var refused := []
	for t in names:
		var id: int = world.create_object(t, me, at.x, at.y, 0.0)
		if id < 0:
			refused.append(t)
			continue
		hud.camera_look_at(at)
		await _frames(10)
		hud.select_object(id)
		await _frames(30)
		game._save_named("visual2-hero-" + t)
		world.destroy_object(id)
		await _frames(5)
	return "" if refused.is_empty() else "could not make " + str(refused)


## VISUAL2_SPELL=<science>:<special power>:<target template>: the science bought (points given), the target made for the local player beside the
## base, finished and damaged to half; the spell bar's button of that power pressed and clicked on the target; the target's health and the bar
## printed before and 3 s after (visual2-spell-<power>.png). FAIL: the press or the click is refused, or the target's health does not rise
func _scenario_spell() -> String:
	var parts := OS.get_environment("VISUAL2_SPELL").split(":")
	if parts.size() != 3:
		return "VISUAL2_SPELL is not <science>:<button>:<target>"
	var me := -1
	var i := 0
	for p in world.get_report().get("players", []):
		if me < 0 and str(p).contains(game._local_name):
			me = i
		i += 1
	world.debug_add_science_points(me, 100)
	print("VISUAL2 spell: purchase %s -> %s" % [parts[0], str(world.purchase_science(me, parts[0]))])
	var at := _local_base() + Vector2(400.0, 0.0)
	var target: int = world.create_object(parts[2], me, at.x, at.y, 0.0)
	if target < 0:
		return "could not make " + parts[2]
	world.debug_build_complete(target)
	await _frames(10)
	var before: Dictionary = world.get_object(target)
	world.damage_object(target, float(before.get("max_health", before.get("health", 1000.0))) * 0.5)
	await _frames(5)
	before = world.get_object(target)
	var bar: Variant = world.get_spell_bar(me)
	print("VISUAL2 spell: target %s health %s / %s; bar %s" % [parts[2], str(before.get("health")), str(before.get("max_health")), JSON.stringify(bar).left(1500)])
	var index := -1
	for b in (bar.get("buttons", []) if bar is Dictionary else []):
		if str(b.get("power", "")) == parts[1]:
			index = int(b.get("index", -1))
	if index < 0:
		return "the bar has no " + parts[1]
	hud.camera_look_at(at)
	var pressed: Variant = world.spell_bar_press(me, index)
	print("VISUAL2 spell: press %d -> %s; bar after %s" % [index, str(pressed), JSON.stringify(world.get_spell_bar(me)).left(800)])
	var needs_object := false
	for b in (bar.get("buttons", []) if bar is Dictionary else []):
		if int(b.get("index", -1)) == index:
			needs_object = bool(b.get("needs_object", false)) and not bool(b.get("needs_position", false))
	var clicked: Variant = world.spell_bar_click_object(me, target) if needs_object else world.spell_bar_click_world(me, at.x, at.y)
	print("VISUAL2 spell: click -> %s" % str(clicked))
	await _seconds(3.0)
	var after: Dictionary = world.get_object(target)
	print("VISUAL2 spell: health after %s / %s; bar %s" % [str(after.get("health")), str(after.get("max_health")), JSON.stringify(world.get_spell_bar(me)).left(800)])
	game._save_named("visual2-spell-" + parts[1])
	if float(after.get("health", 0.0)) <= float(before.get("health", 0.0)):
		return "the target's health did not rise"
	return ""


## VISUAL2_PRODUCE=<producer>:<unit>[,...]: the producer made for the local player toward the map's middle, money given, the unit queued; every logic
## object of the unit's template (and the queue) printed each 2 s for 90 s, a shot when it is out (visual2-produce-<unit>.png). FAIL: it never appears
func _scenario_produce() -> String:
	var pairs := OS.get_environment("VISUAL2_PRODUCE").split(",", false)
	if pairs.is_empty():
		return "VISUAL2_PRODUCE names no producer:unit"
	var me := -1
	var i := 0
	for p in world.get_report().get("players", []):
		if me < 0 and str(p).contains(game._local_name):
			me = i
		i += 1
	var base := _local_base()
	var shroud: Dictionary = world.get_shroud_report()
	var mid := Vector2(float(shroud.get("cells_x", 0)), float(shroud.get("cells_y", 0))) * float(shroud.get("cell_size", 0.0)) * 0.5
	var dir := (mid - base).normalized() if mid != base else Vector2.RIGHT
	var missing := []
	for pair in pairs:
		var parts := pair.split(":")
		var at := base + dir * 700.0
		var producer: int = world.create_object(parts[0], me, at.x, at.y, 0.0)
		if producer < 0:
			return "could not make " + parts[0] + ": " + str(world.get_report().get("errors", []))
		world.debug_build_complete(producer)
		world.give_money(me, 100000)
		hud.camera_look_at(at)
		await _frames(10)
		world.queue_unit(me, producer, parts[1], false)
		var seen := false
		for t in 45:
			await _seconds(2.0)
			var found := []
			for o in world.get_player_objects(game._local_name):
				if str(o.get("template", "")).to_lower() == parts[1].to_lower():
					found.append(world.get_object(o.id))
			print("VISUAL2 produce %s t=%ds: %d out %s queue %s" % [parts[1], (t + 1) * 2, found.size(), str(found).left(600), str(world.get_production(me, producer)).left(400)])
			if not found.is_empty():
				if not seen:
					hud.camera_look_at(Vector2(float(found[0].get("x", at.x)), float(found[0].get("y", at.y))))
					await _frames(20)
					game._save_named("visual2-produce-" + parts[1])
				seen = true
				break
		if not seen:
			missing.append(parts[1])
		world.destroy_object(producer)
	return "" if missing.is_empty() else "never appeared: " + str(missing)


func _scenario_dust() -> String:
	var enemy := ""
	for n in _enemy_names():
		enemy = n
		break
	if enemy.is_empty():
		return "no enemy player"
	var site: Dictionary = {}
	for t in ["GondorBarracks", "MordorOrcPit", "IsengardUrukPit", "ElvenBarracks", "DwarvenBarracks", "WildGoblinCave", "AngmarBarracks"]:
		site = world.build2_construct_on_plot(enemy, t)
		if site.get("ok", false):
			break
	if not site.get("ok", false):
		return "no enemy fortress pad took a building: %s" % str(site)
	var at := Vector2(float(site.x), float(site.y))
	# its worker walks over from the enemy's fortress first: wait (in logic frames) until it rises
	var start: int = world.get_frame()
	while world.get_frame() < start + 900 and float(world.get_object(int(site.id)).get("construction_percent", 0.0)) <= 0.0:
		await game.get_tree().process_frame
	print("VISUAL2 dust: the building rises after %d logic frames" % (world.get_frame() - start))
	hud.camera_look_at(at)
	print("VISUAL2 dust: clip from drawn frame %d" % Engine.get_frames_drawn())
	var fogged_particles := 0
	for i in 150:
		await game.get_tree().process_frame
		fogged_particles += int(world.get_particles_near(at.x, at.y, 200.0).particles)
	var status := int(world.get_object_shroud_status(int(site.id)))
	game._save_named("visual2-dust-fog")
	# the enemy objects drawn near the fogged site and their status (printed: status 0 is an object without a shroud record)
	var drawn_near := []
	for e in _enemy_sample():
		if Vector2(e.x, e.y).distance_to(at) < 900.0 and not e.hidden:
			drawn_near.append("%s #%d status %d" % [e.template, e.id, e.status])
	print("VISUAL2 dust: enemy objects drawn within 900 of the fogged site: %s" % str(drawn_near))
	var scout := _make_local_unit(at + Vector2(140.0, -140.0))
	if scout < 0:
		return "no unit template made for the local player"
	var so: Dictionary = world.get_object(scout)
	print("VISUAL2 dust: local %s, enemy %s, scout #%d %s owner %s at (%d, %d)" % [game._local_name, enemy, scout, so.get("template", ""), so.get("owner", ""),
		int(so.get("x", 0)), int(so.get("y", 0))])
	var seen_particles := 0
	for i in 180:
		await game.get_tree().process_frame
		seen_particles += int(world.get_particles_near(at.x, at.y, 200.0).particles)
		if i == 120:
			game._save_named("visual2-dust-seen")
	var seen_status := int(world.get_object_shroud_status(int(site.id)))
	print("VISUAL2 dust: clip to drawn frame %d" % Engine.get_frames_drawn())
	print("VISUAL2 dust: %s's building #%d at (%d, %d) status %d while fogged: %d particles over 150 frames; seen (status %d): %d over 180 frames, now %s" % [enemy,
		int(site.id), int(at.x), int(at.y), status, fogged_particles, seen_status, seen_particles, JSON.stringify(world.get_object(int(site.id)).get("construction_percent", -2))])
	if fogged_particles > 0:
		return "%d particles near the fogged enemy building" % fogged_particles
	if seen_particles == 0:
		return "no particle near the building once seen"
	return ""

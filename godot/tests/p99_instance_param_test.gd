## P-99: every W3D draw node's material declares instance uniforms (w3d_tint, w3d_receive_shadows), so the rendering server gives each node 16
## slots of the global shader buffer. With Godot's default buffer (65536) only 4096 nodes fit; a long game kept adding model draw nodes, the
## server returned -1 for the rest and then printed "instance_buffer_pos.has(p_instance)" at every update of those instances (the owner's flood
## and lag). Checks: project.godot raises the buffer to its maximum; W3DInstancer counts its draw nodes in get_stats (instance_param_nodes,
## instance_param_capacity) and gives them back when it goes.
##
##   ROTWK_INSTALL=... BFME2_INSTALL=... godot --path godot --script res://tests/p99_instance_param_test.gd
extends SceneTree

const EXIT_PASS := 0
const EXIT_FAIL := 1
const EXIT_SKIP := 77

var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok:
		_failures += 1


func _run() -> void:
	var buffer: int = ProjectSettings.get_setting("rendering/limits/global_shader_variables/buffer_size", 65536)
	_check(buffer == 1048576, "rendering/limits/global_shader_variables/buffer_size is the maximum 1048576 (got %d)" % buffer)
	if OS.get_environment("ROTWK_INSTALL").is_empty() or OS.get_environment("BFME2_INSTALL").is_empty():
		print("SKIP: set ROTWK_INSTALL and BFME2_INSTALL for the instancer count")
		quit(EXIT_FAIL if _failures > 0 else EXIT_SKIP)
		return
	var fs: RefCounted = ClassDB.instantiate("RetailFileSystem")
	var mount: Dictionary = fs.mount_retail()
	_check(mount.ok, "mount_retail %s" % [mount.get("errors", [])])

	var probe: Node3D = ClassDB.instantiate("W3DInstancer")
	var base: Dictionary = probe.get_stats()
	probe.free()
	_check(base.get("instance_param_capacity", -1) == 65536, "instance_param_capacity = 1048576 / 16 = 65536 (got %s)" % base.get("instance_param_capacity"))
	var before: int = base.get("instance_param_nodes", -1)

	var inst: Node3D = ClassDB.instantiate("W3DInstancer")
	root.add_child(inst)
	inst.setup(fs)
	var m1: int = inst.add_model("GUMAArms_SKN")
	_check(m1 >= 0, "add_model GUMAArms_SKN")
	var s1: Dictionary = inst.get_stats()
	var draws: int = s1.get("draw_items", 0)
	_check(draws > 0, "the model has draw nodes (%d)" % draws)
	_check(s1.get("instance_param_nodes", -1) == before + draws, "instance_param_nodes counts the model's draw nodes (%s == %d + %d)" % [s1.get("instance_param_nodes"), before, draws])

	# vanilla MUSauronEye's HLOD lists MUSAURONEYE.BOX01, which musauroneye.w3d does not have: retail's HLodClass skips it (a note, not an error)
	var eye: int = inst.add_model("MUSauronEye")
	var eye_report: Dictionary = inst.get_model_report(eye) if eye >= 0 else {}
	var eye_notes := str(eye_report.get("notes", []))
	_check(eye >= 0 and (eye_report.get("errors", ["?"]) as Array).is_empty(), "MUSauronEye loads with no error (%s)" % [eye_report.get("errors")])
	_check(eye_notes.contains("MUSAURONEYE.BOX01 is in no file"), "MUSauronEye's missing BOX01 is a note")
	var eye_draws: int = inst.get_stats().get("draw_items", 0) - draws
	draws += eye_draws

	var second: Node3D = ClassDB.instantiate("W3DInstancer")
	root.add_child(second)
	second.setup(fs)
	second.add_model("GUMAArms_SKN")
	var second_draws: int = second.get_stats().get("draw_items", 0)
	_check(second.get_stats().get("instance_param_nodes", -1) == before + draws + second_draws, "the count is shared by every instancer (%s)" % second.get_stats().get("instance_param_nodes"))
	second.free()
	_check(inst.get_stats().get("instance_param_nodes", -1) == before + draws, "a freed instancer gives its nodes back (%s)" % inst.get_stats().get("instance_param_nodes"))
	inst.free()
	var probe2: Node3D = ClassDB.instantiate("W3DInstancer")
	_check(probe2.get_stats().get("instance_param_nodes", -1) == before, "all instancers gone: back to %d (%s)" % [before, probe2.get_stats().get("instance_param_nodes")])
	probe2.free()

	print("P-99 instance param test: %s" % ("PASS" if _failures == 0 else "FAIL (%d)" % _failures))
	quit(EXIT_PASS if _failures == 0 else EXIT_FAIL)

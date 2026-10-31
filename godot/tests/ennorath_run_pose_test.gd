## P-30: test the Godot instancer's bone palettes using installed Ennorath assets.
extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var fs: RefCounted = ClassDB.instantiate("RetailFileSystem")
	fs.set_install_mod(OS.get_environment("OPENBFME_ENNORATH_INSTALL"))
	var mount: Dictionary = fs.mount_retail()
	if not mount.ok:
		push_error("P-30 mount failed: %s" % [mount.get("errors", [])])
		quit(1)
		return
	var inst: Node3D = ClassDB.instantiate("W3DInstancer")
	root.add_child(inst)
	inst.set_auto_update(false)
	inst.set_playing(false)
	if not inst.setup(fs).ok:
		push_error("P-30 instancer setup failed")
		quit(1)
		return
	var failures := 0
	for entry in [["guwar_dot", "gumaarms_skl.GUManMocap_RUNC"], ["guarch_dot", "guarcher_skl.GUArcher_RUNA"]]:
		var model: int = inst.add_model(entry[0])
		if model < 0:
			push_error("P-30 cannot load model %s: %s" % [entry[0], inst.get_errors()])
			failures += 1
			continue
		var id: int = inst.add_instance(model, Transform3D.IDENTITY, "", 0.0, 1.0)
		var poses: Array = []
		for frame in [0.0, 4.0, 8.0, 12.0]:
			if not inst.set_instance_pose(id, entry[1], frame, "", 0.0, 0.0):
				push_error("P-30 cannot set pose %s" % [entry])
				failures += 1
				break
			inst.update_now()
			poses.append(inst.get_instance_palette(id).to_byte_array())
		if poses.size() == 4:
			var varies: bool = poses[0] != poses[1] and poses[1] != poses[2] and poses[2] != poses[3]
			print("P30 palette model=%s clip=%s changes=%s bytes=%d" % [entry[0], entry[1], varies, poses[0].size()])
			if not varies or poses[0].is_empty():
				failures += 1
	# Trace the palette actually selected by the draw buffer across a loop.
	# Independent poses are the control; a group's changing leader is observable
	# in the per-instance custom-data palette offset, even in headless Godot.
	var group: Node3D = ClassDB.instantiate("W3DInstancer")
	root.add_child(group)
	group.set_auto_update(false)
	group.set_playing(false)
	if not group.setup(fs).ok:
		push_error("P-30 group instancer setup failed")
		quit(1)
		return
	var group_model: int = group.add_model("guwar_dot")
	var ids: Array[int] = []
	for i in 2:
		var id: int = group.add_instance(group_model, Transform3D(Basis.IDENTITY, Vector3(float(i), 0, 0)), "", 0.0, 1.0)
		ids.append(id)
		group.set_instance_pose_share(id, 2.0)
	if not "[S-6009]" in " ".join(group.get_warnings()):
		push_error("P-30 missing S-6009 report")
		failures += 1
	group.set_pose_culling(true)
	# The default presentation must not switch a moving unit between another
	# unit's frame and its own at a loop boundary (P-30).
	for step in 28:
		for i in 2:
			var frame := fmod(float(step) + float(i) * 1.5, 23.0)
			group.set_instance_pose(ids[i], "gumaarms_skl.GUManMocap_RUNC", frame, "", 0.0, 0.0)
		group.update_now()
		var offsets: Array = []
		for child in group.get_children():
			if child is MultiMeshInstance3D and child.multimesh.visible_instance_count == 2:
				var buffer: PackedFloat32Array = child.multimesh.buffer
				offsets = [buffer[12], buffer[28]]
				break
		if offsets.size() != 2:
			push_error("P-30 missing group draw buffer")
			failures += 1
			break
		if offsets[0] == offsets[1]:
			failures += 1
		print("P30 share step=%d frames=%.1f,%.1f offsets=%s shared=%s" % [step, fmod(float(step), 23.0), fmod(float(step) + 1.5, 23.0), offsets, group.get_stats().pose_shared])
	group.queue_free()
	inst.queue_free()
	quit(1 if failures else 0)

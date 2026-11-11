## P-88: a mesh with W3D_MESH_FLAG_HIDDEN starts hidden and is drawn once a draw module shows it (Ennorath's HaradBarracks, model HABRKS:
## UPGR3 and UPG2 carry the flag; SubObjectsUpgrade ShowSubObjects shows them at levels 2 and 3, haradbarracks.ini:339 / :345).
## Before P-88 the instancer built no draw item for a flagged mesh at all, so ShowSubObjects had nothing to show (the owner: the level-2 part
## of the Harad barracks is missing). Needs the Ennorath install (OPENBFME_ENNORATH_INSTALL) and retail assets; no GPU.
extends SceneTree

var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String) -> void:
	if not ok:
		_failures += 1
		print("FAIL ", what)


func _visible(inst: Node3D, sub: String) -> int:
	for child in inst.get_children():
		# the draw item's node is "<model>_<sub object>"; Godot turns the sub object's '.' into '_' in a node name
		if child is MultiMeshInstance3D and str(child.name).to_upper().ends_with("_" + sub.to_upper().replace(".", "_")):
			return child.multimesh.visible_instance_count
	return -1 # no draw item for that sub object


func _run() -> void:
	var fs: RefCounted = ClassDB.instantiate("RetailFileSystem")
	fs.set_install_mod(OS.get_environment("OPENBFME_ENNORATH_INSTALL"))
	var mount: Dictionary = fs.mount_retail()
	if not mount.ok:
		push_error("P-88 mount failed: %s" % [mount.get("errors", [])])
		quit(1)
		return
	var inst: Node3D = ClassDB.instantiate("W3DInstancer")
	root.add_child(inst)
	inst.set_auto_update(false)
	inst.set_playing(false)
	if not inst.setup(fs).ok:
		push_error("P-88 instancer setup failed")
		quit(1)
		return
	var model: int = inst.add_model("habrks")
	if model < 0:
		push_error("P-88 cannot load habrks: %s" % [inst.get_errors()])
		quit(1)
		return
	var flagged: Array = []
	for so in inst.get_model_report(model).sub_objects:
		if so.get("hidden", false):
			flagged.append(str(so.name).to_upper())
	# the model file: UPGR3 and UPG2 carry W3D_MESH_FLAG_HIDDEN (attributes 0x1000), the other four meshes do not
	_check(flagged == ["HABRKS.UPGR3", "HABRKS.UPG2"], "flagged meshes %s" % [flagged])
	var id: int = inst.add_instance(model, Transform3D.IDENTITY, "", 0.0, 1.0)
	inst.update_now()
	# a new instance shows the meshes' own flags: the four plain meshes drawn, the two flagged ones built but hidden
	_check(_visible(inst, "HABRKS.UPG1") == 1, "UPG1 drawn")
	_check(_visible(inst, "HABRKS.UPGR3") == 0, "UPGR3 built and hidden at start (got %d)" % _visible(inst, "HABRKS.UPGR3"))
	_check(_visible(inst, "HABRKS.UPG2") == 0, "UPG2 built and hidden at start (got %d)" % _visible(inst, "HABRKS.UPG2"))
	# a caller that knows nothing of the flags hides names on top of them: an empty list does not reveal a flagged mesh
	_check(inst.set_instance_hidden_subobjects(id, PackedStringArray()), "plain set")
	inst.update_now()
	_check(_visible(inst, "HABRKS.UPGR3") == 0, "a plain empty set keeps UPGR3 hidden")
	# the draw module's complete set (level 2: SubObjectsUpgrade showed UPGR3 and hid UPG1; UPG2 still flagged): UPGR3 is drawn
	_check(inst.set_instance_hidden_subobjects_exact(id, PackedStringArray(["HABRKS.UPG1", "HABRKS.UPG2"])), "exact set")
	inst.update_now()
	_check(_visible(inst, "HABRKS.UPGR3") == 1, "UPGR3 drawn once the draw module shows it (got %d)" % _visible(inst, "HABRKS.UPGR3"))
	_check(_visible(inst, "HABRKS.UPG1") == 0, "UPG1 hidden by the draw module")
	_check(_visible(inst, "HABRKS.UPG2") == 0, "UPG2 still hidden")
	print("P88 hidden meshes: failures=%d" % _failures)
	quit(1 if _failures > 0 else 0)

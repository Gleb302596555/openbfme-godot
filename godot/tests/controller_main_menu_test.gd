## Inspect each mounted game's own main-menu entries and activate Credits
## through the same authored button action as a mouse click.
extends SceneTree
var failures := 0
func _initialize() -> void: call_deferred("_run")
func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		printerr("CONTROLLER MAIN MENU FAIL: ", message)
func step(shell: Node, count := 20) -> void:
	for i in count:
		shell.tick(0.033)
		shell.render(true)
func key(shell: Node, code: int) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	shell.menu_input(event)
	event.pressed = false
	shell.menu_input(event)
	step(shell)
func cycle(fs: RefCounted, presentation: RefCounted, style: String) -> void:
	var world = ClassDB.instantiate("GameWorld")
	root.add_child(world)
	var setup: Dictionary = world.setup(fs)
	if not setup.ok:
		print(style, " setup errors ", setup.errors)
	if not setup.ok:
		check(false, style + " setup")
		world.free()
		return
	var shell = ClassDB.instantiate("AptMenuPlayer")
	root.add_child(shell)
	shell.auto_process = false
	var config := {"options_file": OS.get_temp_dir().path_join("controller-menu-options-%d.ini" % OS.get_process_id()), "skirmish_ini": OS.get_temp_dir().path_join("controller-menu-skirmish-%d.ini" % OS.get_process_id())}
	if presentation != null: config.presentation_fs = presentation
	check(shell.boot_shell(fs, world, config).ok, style + " boot")
	check(shell.shell_push("MainMenu.apt"), style + " main menu")
	step(shell)
	check(shell.shell_invoke(shell.shell_top_level(), "ShowMainMenu", PackedStringArray()).ok, style + " show")
	step(shell, 120)
	var campaigns: Array = []
	var credits_opened := false
	for group_index in 8:
		key(shell, KEY_RIGHT)
		var state: Dictionary = shell.menu_nav_state()
		if group_index == 0: print(style, " groups ", state.main_groups)
		if group_index == state.main_groups.size(): break
		var group: String = state.main_focus
		# Up enumerates the currently shown authored entries, without launching.
		key(shell, KEY_UP)
		state = shell.menu_nav_state()
		print(style, " ", group, " entries ", state.main_entries)
		for entry in state.main_entries:
			if "Campaign" in entry: campaigns.append(entry)
		if "Credits" in state.main_entries:
			for i in state.main_entries.size():
				if shell.menu_nav_state().main_focus == "Credits": break
				key(shell, KEY_UP)
			key(shell, KEY_ENTER)
			step(shell, 120)
			var visible: Dictionary = shell.get_member(shell.shell_top_level(), "ShowCreditsMovie", "_visible")
			print(style, " credits ", visible)
			check(visible.get("found", false) and str(visible.get("value", "")).to_lower() in ["true", "1"], style + " Credits page visible")
			credits_opened = true
			break
	if style == "rotwk":
		check(campaigns.has("Expansion1Campaign"), "RotWK Angmar campaign reachable")
	if style == "bfme2":
		check(campaigns.has("GoodCampaign") and campaigns.has("EvilCampaign"), "BFME2 campaigns reachable")
	check(credits_opened, style + " Credits reachable")
	# Profile pages share the skirmish movie, but cannot expose Start.
	if style != "bfme1":
		check(shell.shell_push("Skirmish.apt"), style + " skirmish")
		step(shell, 90)
		for command in ["OnNewProfileMenu", "OnChangeProfileMenu", "OnDeleteProfileMenu"]:
			shell.shell_fscommand("AptSkirmish::" + command, "")
			check(not shell.screen_state().start_available, style + " Start blocked during " + command)
			shell.shell_fscommand("AptSkirmish::OnProfilePopupCancel", "")

	shell.free()
	world.free()
func _run() -> void:
	if OS.get_environment("ROTWK_INSTALL").is_empty():
		print("CONTROLLER MAIN MENU SKIP: ROTWK_INSTALL is not set")
		quit(77)
		return
	var fs = ClassDB.instantiate("RetailFileSystem")
	check(fs.mount_retail().ok, "retail mount")
	cycle(fs, null, "rotwk")
	cycle(fs, fs.create_bfme2_shell(), "bfme2")
	var bfme1 := OS.get_environment("BFME1_INSTALL")
	if not bfme1.is_empty():
		var first = ClassDB.instantiate("RetailFileSystem")
		check(first.mount_bfme1_path(bfme1).ok, "BFME1 mount")
		cycle(first, null, "bfme1")
	else: print("CONTROLLER MAIN MENU BFME1 SKIP: BFME1_INSTALL is not set")
	print("CONTROLLER MAIN MENU ", "PASS" if failures == 0 else "FAIL")
	quit(0 if failures == 0 else 1)

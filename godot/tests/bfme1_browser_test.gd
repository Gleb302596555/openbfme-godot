## BFME1's separate map library can preview and import into the playable catalog.
extends SceneTree
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var install := OS.get_environment("BFME1_INSTALL")
	if install.is_empty(): quit(77); return
	create_timer(120).timeout.connect(func(): push_error("BFME1 browser timed out"); quit(1))
	root.size = Vector2i(1280,720)
	var fs = ClassDB.instantiate("RetailFileSystem")
	assert(fs.mount_bfme1_path(install).ok)
	var world = ClassDB.instantiate("GameWorld")
	root.add_child(world)
	assert(world.setup(fs).ok)
	var directory := OS.get_temp_dir().path_join("bfme1-browser-"+str(OS.get_process_id()))
	assert(DirAccess.make_dir_recursive_absolute(directory.path_join("library")) == OK)
	var file := FileAccess.open(directory.path_join("library/fixture.map"),FileAccess.WRITE)
	file.store_buffer(fs.read_file("maps/map mp fords of isen/map mp fords of isen.map"))
	file.close()
	var shell = ClassDB.instantiate("AptMenuPlayer")
	root.add_child(shell)
	assert(shell.boot_shell(fs,world,{"skirmish_ini":directory.path_join("Skirmish.ini")}).ok)
	assert(shell.shell_push("MainMenu.apt"))
	for i in 90: shell.tick(0.033)
	assert(shell.shell_fscommand("AptMainMenu::Skirmish",""))
	for i in 90: shell.tick(0.033)
	assert(shell.lobby_apply({"profile":"BrowserTest"}).ok)
	for i in 90: shell.tick(0.033)
	var browser = load("res://scripts/skirmish_map_browser.gd").new()
	browser.shell = shell
	browser.fs = fs
	browser.library_path = directory.path_join("library")
	browser.install_root = directory.path_join("installed")
	browser.metadata_cache_root = directory.path_join("metadata")
	browser.favorites_path = directory.path_join("favorites.cfg")
	root.add_child(browser)
	browser.show_browser()
	assert(browser.open)
	assert(not browser.loaded_catalog.is_empty())
	browser._change_source(1)
	for i in 30: await process_frame
	assert(browser.library_catalog.size() == 1)
	browser._select(0)
	assert(browser.selected.library)
	var previous_count: int = fs.installed_maps(browser.install_root).size()
	browser._install_selected()
	assert(fs.installed_maps(browser.install_root).size() == previous_count + 1, browser.message.text)
	var imported := false
	for entry in shell.lobby_map_catalog():
		if entry.get("installed",false): imported = true
	assert(imported)
	browser.close_browser()
	assert(not browser.open)
	print("BFME1 BROWSER PASS")
	quit()

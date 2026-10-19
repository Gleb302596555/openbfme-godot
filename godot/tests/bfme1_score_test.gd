## BFME1 ends a match with ScoreScreen.apt and its own Continue command.
extends SceneTree
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var install := OS.get_environment("BFME1_INSTALL")
	if install.is_empty(): quit(77); return
	create_timer(60).timeout.connect(func(): push_error("BFME1 score test timed out"); quit(1))
	var fs = ClassDB.instantiate("RetailFileSystem")
	assert(fs.mount_bfme1_path(install).ok)
	var world = ClassDB.instantiate("GameWorld")
	root.add_child(world)
	assert(world.setup(fs).ok)
	var slots: Array = []
	for i in 8: slots.append({"state":1,"name":"Closed","accepted":true,"color":-1,"start_pos":-1,"player_template":-1,"team":-1})
	slots[0] = {"state":6,"name":"ScoreTest","accepted":true,"color":0,"start_pos":0,"player_template":4,"team":0}
	slots[1] = {"state":2,"name":"Enemy","accepted":true,"color":1,"start_pos":1,"player_template":5,"team":1}
	assert(world.prepare_new_game({"mode":"skirmish","difficulty":1,"rank_points":0,"map_crc":0,"map_size":0,"map_mask":0,"map":"maps/map mp fords of isen/map mp fords of isen.map","seed":9001,"starting_cash":2000,"slots":slots}).ok)
	assert(world.start_new_game({"logic_thread":false,"test_hooks":true}).ok)
	assert(world.debug_kill_player_objects("Player_2", "").ok)
	for i in 30: world.advance(0.2)
	assert(world.clear_game_data(true).ok)
	var shell = ClassDB.instantiate("AptMenuPlayer")
	root.add_child(shell)
	assert(shell.boot_shell(fs,world,{"skirmish_ini":OS.get_temp_dir().path_join("bfme1-score-test.ini")}).ok)
	assert(shell.set_score_screen_from_world(world))
	assert(shell.shell_push("ScoreScreen.apt"), str(shell.get_shell_report()))
	for i in 90: shell.tick(0.033)
	assert(shell.get_shell_report().notes.get("bfme1-score-inference",0) == 1)
	assert(shell.get_shell_report().errors.is_empty(), str(shell.get_shell_report().errors))
	var requests: Array = []
	shell.shell_request.connect(func(action, _argument): requests.append(action))
	assert(shell.shell_fscommand("AptScoreScreen::Continue", ""))
	shell.tick(0.033)
	assert(requests == ["ScoreScreenContinue"], str(requests))
	print("BFME1 SCORE PASS")
	quit()

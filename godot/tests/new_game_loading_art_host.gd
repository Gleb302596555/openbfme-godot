extends "res://scripts/game.gd"
func _last_replay_path() -> String:
	return OS.get_environment("RESUME_TEST_DIR").path_join("record.BfME2Replay")
var checked_loading_art := false
func _on_load_progress(percent: int) -> void:
	super._on_load_progress(percent)
	var art: Dictionary = _shell.get_backdrop_state()
	assert(art.backdrop_image == "")
	assert(art.background_mode == 1)
	assert(not art.background_paths.is_empty())
	checked_loading_art = true

## Fork BFME1 game selector; RotWK mod management is outside this port.
extends CanvasLayer
const UI_SCALE := 1.5
var _game_picker: OptionButton
var _game := "bfme2"
var _bfme1_path := ""
var _status: Label
func _ready() -> void:
	layer = 120
	process_mode = Node.PROCESS_MODE_ALWAYS
	_game = Release.selected_game()
	_bfme1_path = Release.bfme1_install()
	_build()

func _build() -> void:
	var panel := PanelContainer.new()
	panel.name = "ModManager"
	panel.scale = Vector2(UI_SCALE, UI_SCALE)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.08, 0.07, 0.05, 0.96)
	style.border_color = Color(0.72, 0.6, 0.36)
	style.set_border_width_all(1)
	style.set_corner_radius_all(4)
	style.set_content_margin_all(12)
	panel.add_theme_stylebox_override("panel", style)
	add_child(panel)
	var viewport := get_viewport().get_visible_rect().size / UI_SCALE
	panel.custom_minimum_size = Vector2(minf(640, viewport.x - 32), minf(420, viewport.y - 32))
	panel.position = (get_viewport().get_visible_rect().size - panel.custom_minimum_size * UI_SCALE) / 2
	var col := VBoxContainer.new()
	col.add_theme_constant_override("separation", 8)
	panel.add_child(col)
	var title := Label.new()
	title.text = "Games"
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", Color(0.95, 0.85, 0.6))
	col.add_child(title)
	var game_row := HBoxContainer.new()
	col.add_child(game_row)
	var game_label := Label.new()
	game_label.text = "Game"
	game_row.add_child(game_label)
	_game_picker = OptionButton.new()
	_game_picker.name = "GamePicker"
	_game_picker.add_item("The Rise of the Witch-king / BFME II")
	_game_picker.add_item("Battle for Middle-earth I")
	_game_picker.select(1 if _game=="bfme1" else 0)
	_game_picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_game_picker.item_selected.connect(func(index: int):
		_game = "bfme1" if index==1 else "bfme2"
		_status.text = "Restart to switch games." if _game!="bfme1" or not _bfme1_path.is_empty() else "Choose the BFME1 installation folder below before restarting."
	)
	game_row.add_child(_game_picker)
	game_row.add_child(_button("BFME1 files…", _choose_bfme1))
	var note := Label.new()
	note.text = "Switch games with Restart to apply. BFME1 uses separate game files and settings."
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.add_theme_font_size_override("font_size", 12)
	note.modulate = Color(1, 1, 1, 0.75)
	col.add_child(note)
	_status = Label.new()
	_status.add_theme_font_size_override("font_size", 12)
	col.add_child(_status)
	var buttons := HFlowContainer.new()
	col.add_child(buttons)
	buttons.add_child(_button("Restart to apply", _restart))
	buttons.add_child(_button("Close", _close))

func _button(text: String, action: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.pressed.connect(action)
	return b


static func restart_arguments(args: PackedStringArray, game_name: String, bfme1_path := "") -> PackedStringArray:
	var result := PackedStringArray()
	for arg in args:
		if arg.begins_with("--game="): continue
		if not bfme1_path.is_empty() and arg.begins_with("--bfme1-install="): continue
		result.append(arg)
	if not result.has("--"): result.append("--")
	result.append("--game="+game_name)
	if not bfme1_path.is_empty(): result.append("--bfme1-install="+bfme1_path)
	return result


static func current_restart_arguments(game_name: String, bfme1_path := "") -> PackedStringArray:
	var args := OS.get_cmdline_args()
	args.append("--")
	args.append_array(OS.get_cmdline_user_args())
	return restart_arguments(args,game_name,bfme1_path)


func _choose_bfme1() -> void:
	var chooser := FileDialog.new()
	chooser.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	chooser.access = FileDialog.ACCESS_FILESYSTEM
	chooser.use_native_dialog = true
	chooser.title = "Choose the BFME1 installation folder"
	chooser.canceled.connect(chooser.queue_free)
	chooser.dir_selected.connect(func(path: String):
		chooser.queue_free()
		var has_big := false
		for file in DirAccess.get_files_at(path):
			if file.to_lower().ends_with(".big"): has_big=true
		if not has_big:
			_status.text="That folder contains no BFME1 BIG archives."
			return
		var file := FileAccess.open("user://install-paths.cfg",FileAccess.READ)
		var lines := PackedStringArray()
		if file!=null:
			for line in file.get_as_text().split("\n"):
				if not line.begins_with("BFME1_INSTALL="):lines.append(line)
			file.close()
		lines.append("BFME1_INSTALL="+path)
		file=FileAccess.open("user://install-paths.cfg",FileAccess.WRITE)
		if file==null: _status.text="Could not save BFME1 folder.";return
		file.store_string("\n".join(lines));file.close()
		_bfme1_path = path
		_status.text="BFME1 folder saved. Restart to switch games.")
	add_child(chooser)
	chooser.popup_centered_ratio(0.6)


func _restart() -> void:
	if _game=="bfme1" and _bfme1_path.is_empty():
		_status.text="Choose the BFME1 installation folder first."
		return
	var cfg := ConfigFile.new()
	cfg.set_value("game","selected",_game)
	if cfg.save("user://game-selection.cfg")!=OK:
		_status.text="Could not save the game selection."
		return
	# Godot separates engine arguments and arguments after `--`. Preserve both:
	# get_cmdline_args() alone drops player options such as --max-fps and paths.
	OS.set_restart_on_exit(true, current_restart_arguments(_game,_bfme1_path))
	get_tree().quit()


func _close() -> void:
	queue_free()

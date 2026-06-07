extends Node3D

const TorcsBridgeFallbackScript := preload("res://scripts/torcs_bridge_fallback.gd")
const TORCS_GDEXTENSION_PATH := "res://bin/torcs_gdextension.gdextension"

@onready var _car: Node3D = $Car
@onready var _camera: Camera3D = $Camera3D
@onready var _telemetry: Label = $CanvasLayer/Telemetry
@onready var _track_debug: Node3D = $TrackDebug

var _bridge = null
var _bridge_backend := ""
var _snapshot: Dictionary = {}
var _track_debug_built := false
var _scripted_drive := false
var _gear := 1
var _shift_up_pressed := false
var _shift_down_pressed := false
var _reset_pressed := false

func _ready() -> void:
	_scripted_drive = DisplayServer.get_name() == "headless"
	_load_race()

func _exit_tree() -> void:
	if _bridge != null:
		_bridge.shutdown()

func _physics_process(delta: float) -> void:
	if _scripted_drive:
		var race_time := float(_snapshot.get("race_time", 0.0))
		_bridge.set_human_input(0, _scripted_input(race_time))
	else:
		_handle_reset_input()
		_bridge.set_human_input(0, _interactive_input())

	_snapshot = _bridge.step(delta)
	_update_scene_from_snapshot(delta)

func _load_race() -> void:
	_clear_track_debug_overlay()

	var data_root := ProjectSettings.globalize_path("res://../data")
	var local_root := ProjectSettings.globalize_path("res://..")
	var race_config := {
		"track_xml": "data/tracks/road/wheel-2/wheel-2.xml",
		"car_xml": "data/cars/models/car1-trb1/car1-trb1.xml",
		"car_id": "car1-trb1",
		"laps": 0
	}

	if _bridge != null:
		_bridge.shutdown()

	var loaded := false
	for candidate in _bridge_candidates():
		var bridge = candidate["bridge"]
		if bridge == null:
			continue

		loaded = bridge.initialize(data_root, local_root, local_root)
		if loaded:
			loaded = bridge.load(race_config)

		if loaded:
			_bridge = bridge
			_bridge_backend = candidate["name"]
			print("TORCS bridge backend: " + _bridge_backend)
			break

		bridge.shutdown()

	if not loaded:
		_bridge = null
		_bridge_backend = ""
		push_error("Failed to initialize TORCS bridge smoke backend.")
		set_physics_process(false)
		return

	_gear = 1
	_snapshot = _bridge.get_snapshot()
	_update_scene_from_snapshot(0.0, true)
	_rebuild_track_debug_overlay()

func get_bridge_backend() -> String:
	return _bridge_backend

func _bridge_candidates() -> Array:
	var candidates: Array = []
	_ensure_native_bridge_loaded()
	if ClassDB.class_exists("TorcsBridgeNative"):
		candidates.append({
			"name": "native",
			"bridge": ClassDB.instantiate("TorcsBridgeNative")
		})
	candidates.append({
		"name": "fallback",
		"bridge": TorcsBridgeFallbackScript.new()
	})
	return candidates

func _ensure_native_bridge_loaded() -> void:
	if ClassDB.class_exists("TorcsBridgeNative"):
		return

	if ResourceLoader.exists(TORCS_GDEXTENSION_PATH):
		ResourceLoader.load(TORCS_GDEXTENSION_PATH)

func _scripted_input(time: float) -> Dictionary:
	var input := {
		"steer": 0.0,
		"throttle": 0.0,
		"brake": 0.0,
		"clutch": 0.0,
		"gear": 1,
		"lights": false,
		"pit_request": false,
		"brake_balance": 0.55
	}

	if time < 6.0:
		input["throttle"] = 0.85

	if time >= 1.5 and time < 4.0:
		input["steer"] = 0.35
	elif time >= 4.0 and time < 6.0:
		input["steer"] = -0.20

	if time >= 6.0 and time < 8.5:
		input["brake"] = 0.45

	return input

func _interactive_input() -> Dictionary:
	_update_gear_input()

	var steer := _keyboard_axis(KEY_A, KEY_LEFT, KEY_D, KEY_RIGHT)
	var throttle := 1.0 if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP) else 0.0
	var brake := 1.0 if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN) else 0.0

	var joypads := Input.get_connected_joypads()
	if not joypads.is_empty():
		var joypad: int = joypads[0]
		var joy_steer := Input.get_joy_axis(joypad, JOY_AXIS_LEFT_X)
		if absf(joy_steer) > 0.1:
			steer = joy_steer
		throttle = maxf(throttle, _normalized_trigger(Input.get_joy_axis(joypad, JOY_AXIS_TRIGGER_RIGHT)))
		brake = maxf(brake, _normalized_trigger(Input.get_joy_axis(joypad, JOY_AXIS_TRIGGER_LEFT)))

	return {
		"steer": steer,
		"throttle": throttle,
		"brake": brake,
		"clutch": 0.0,
		"gear": _gear,
		"lights": Input.is_key_pressed(KEY_L),
		"pit_request": false,
		"brake_balance": 0.55
	}

func _keyboard_axis(negative_key: Key, negative_alt_key: Key, positive_key: Key, positive_alt_key: Key) -> float:
	var value := 0.0
	if Input.is_key_pressed(negative_key) or Input.is_key_pressed(negative_alt_key):
		value -= 1.0
	if Input.is_key_pressed(positive_key) or Input.is_key_pressed(positive_alt_key):
		value += 1.0
	return value

func _normalized_trigger(value: float) -> float:
	if value < -0.05:
		return clampf((value + 1.0) * 0.5, 0.0, 1.0)
	return clampf(value, 0.0, 1.0)

func _update_gear_input() -> void:
	var shift_up_now := Input.is_key_pressed(KEY_E) or Input.is_key_pressed(KEY_PAGEUP)
	if shift_up_now and not _shift_up_pressed:
		_gear = mini(_gear + 1, 6)
	_shift_up_pressed = shift_up_now

	var shift_down_now := Input.is_key_pressed(KEY_Q) or Input.is_key_pressed(KEY_PAGEDOWN)
	if shift_down_now and not _shift_down_pressed:
		_gear = maxi(_gear - 1, -1)
	_shift_down_pressed = shift_down_now

func _handle_reset_input() -> void:
	var reset_now := Input.is_key_pressed(KEY_R)
	if reset_now and not _reset_pressed:
		_bridge.shutdown()
		_load_race()
	_reset_pressed = reset_now

func _update_scene_from_snapshot(delta: float, snap_camera := false) -> void:
	if _snapshot.is_empty() or _snapshot["cars"].is_empty():
		return

	var car: Dictionary = _snapshot["cars"][0]
	var position: Vector3 = car["godot_position"]
	var godot_yaw := float(car["godot_yaw"])
	var forward := _forward_from_godot_yaw(godot_yaw)
	_car.position = position
	_car.rotation.y = godot_yaw

	var camera_position := position - forward * 8.0 + Vector3(0.0, 4.0, 0.0)
	if snap_camera:
		_camera.position = camera_position
	else:
		_camera.position = _camera.position.lerp(camera_position, minf(delta * 6.0, 1.0))
	_camera.look_at(position + forward * 4.0 + Vector3(0.0, 0.6, 0.0))
	_telemetry.text = "time %.2f  speed %.2f m/s  rpm %.0f  substeps %d" % [
		_snapshot["race_time"],
		car["speed"],
		car["rpm"],
		_snapshot["completed_substeps"]
	]

func _forward_from_godot_yaw(godot_yaw: float) -> Vector3:
	return Vector3(-sin(godot_yaw), 0.0, -cos(godot_yaw))

func _rebuild_track_debug_overlay() -> void:
	if _track_debug_built or _snapshot.is_empty():
		return

	var track: Dictionary = _snapshot.get("track", {})
	var debug_points: Array = track.get("debug_points", [])
	if debug_points.size() < 2:
		return

	_add_debug_line("CenterLine", debug_points, "godot_center", Color(0.1, 0.65, 1.0))
	_add_debug_line("LeftBorder", debug_points, "godot_left_border", Color(0.95, 0.85, 0.2))
	_add_debug_line("RightBorder", debug_points, "godot_right_border", Color(0.95, 0.85, 0.2))
	_track_debug_built = true

func _clear_track_debug_overlay() -> void:
	for child in _track_debug.get_children():
		_track_debug.remove_child(child)
		child.queue_free()
	_track_debug_built = false

func _add_debug_line(node_name: String, debug_points: Array, point_key: String, color: Color) -> void:
	var vertices := PackedVector3Array()
	for index in range(debug_points.size() - 1):
		var start: Vector3 = debug_points[index][point_key]
		var finish: Vector3 = debug_points[index + 1][point_key]
		vertices.append(start + Vector3(0.0, 0.03, 0.0))
		vertices.append(finish + Vector3(0.0, 0.03, 0.0))

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays)

	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color

	var line := MeshInstance3D.new()
	line.name = node_name
	line.mesh = mesh
	line.material_override = material
	_track_debug.add_child(line)

extends Node3D

const TorcsBridgeFallbackScript := preload("res://scripts/torcs_bridge_fallback.gd")
const TORCS_GDEXTENSION_PATH := "res://bin/torcs_gdextension.gdextension"
const WHEEL_RADIUS := 0.33

@onready var _car: Node3D = $Car
@onready var _road: Node3D = $Road
@onready var _camera: Camera3D = $Camera3D
@onready var _telemetry: Label = $CanvasLayer/Telemetry
@onready var _track_debug: Node3D = $TrackDebug

var _bridge = null
var _bridge_backend := ""
var _snapshot: Dictionary = {}
var _road_built := false
var _track_debug_built := false
var _scripted_drive := false
var _gear := 1
var _shift_up_pressed := false
var _shift_down_pressed := false
var _reset_pressed := false
var _wheel_nodes: Array[Node3D] = []
var _wheel_spin := [0.0, 0.0, 0.0, 0.0]

func _ready() -> void:
	_ensure_debug_vehicle_rig()
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
	_clear_road_mesh()
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

		if loaded and not _snapshot_has_phase3_schema(bridge.get_snapshot()):
			push_warning("TORCS bridge backend missing Phase 3 snapshot schema: " + candidate["name"])
			loaded = false

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
	_wheel_spin = [0.0, 0.0, 0.0, 0.0]
	_snapshot = _bridge.get_snapshot()
	_update_scene_from_snapshot(0.0, true)
	_rebuild_road_mesh()
	_rebuild_track_debug_overlay()

func get_bridge_backend() -> String:
	return _bridge_backend

func get_debug_snapshot() -> Dictionary:
	return _snapshot.duplicate(true)

func _snapshot_has_phase3_schema(snapshot: Dictionary) -> bool:
	var track: Dictionary = snapshot.get("track", {})
	var debug_points: Array = track.get("debug_points", [])
	var cars: Array = snapshot.get("cars", [])
	if debug_points.is_empty() or cars.is_empty():
		return false
	if not debug_points[0].has("segment_id") or not debug_points[0].has("surface_name"):
		return false
	var car: Dictionary = cars[0]
	if not car.has("track_position"):
		return false
	var wheels: Array = car.get("wheels", [])
	return not wheels.is_empty() and wheels[0].has("godot_contact_point")

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

	var steer := Input.get_axis("torcs_steer_left", "torcs_steer_right")
	var throttle := Input.get_action_strength("torcs_throttle")
	var brake := Input.get_action_strength("torcs_brake")

	return {
		"steer": steer,
		"throttle": throttle,
		"brake": brake,
		"clutch": 0.0,
		"gear": _gear,
		"lights": Input.is_action_pressed("torcs_lights"),
		"pit_request": false,
		"brake_balance": 0.55
	}

func _update_gear_input() -> void:
	var shift_up_now := Input.is_action_pressed("torcs_shift_up")
	if shift_up_now and not _shift_up_pressed:
		_gear = mini(_gear + 1, 6)
	_shift_up_pressed = shift_up_now

	var shift_down_now := Input.is_action_pressed("torcs_shift_down")
	if shift_down_now and not _shift_down_pressed:
		_gear = maxi(_gear - 1, -1)
	_shift_down_pressed = shift_down_now

func _handle_reset_input() -> void:
	var reset_now := Input.is_action_pressed("torcs_reset")
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
	_update_debug_vehicle_rig(car, delta)

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

func _ensure_debug_vehicle_rig() -> void:
	if _car.has_node("Body"):
		return

	var body_mesh := BoxMesh.new()
	body_mesh.size = Vector3(1.9, 0.7, 4.2)

	var body_material := StandardMaterial3D.new()
	body_material.albedo_color = Color(0.9, 0.12, 0.08)
	body_material.roughness = 0.6

	var body := MeshInstance3D.new()
	body.name = "Body"
	body.mesh = body_mesh
	body.material_override = body_material
	body.position = Vector3(0.0, 0.45, 0.0)
	_car.add_child(body)

	var wheel_material := StandardMaterial3D.new()
	wheel_material.albedo_color = Color(0.04, 0.04, 0.04)
	wheel_material.roughness = 0.9

	var wheel_names := ["FrontLeftWheel", "FrontRightWheel", "RearLeftWheel", "RearRightWheel"]
	for wheel_name in wheel_names:
		var wheel_root := Node3D.new()
		wheel_root.name = wheel_name

		var wheel_mesh := CylinderMesh.new()
		wheel_mesh.top_radius = WHEEL_RADIUS
		wheel_mesh.bottom_radius = WHEEL_RADIUS
		wheel_mesh.height = 0.32
		wheel_mesh.radial_segments = 16
		wheel_mesh.rings = 1

		var wheel_visual := MeshInstance3D.new()
		wheel_visual.name = "Visual"
		wheel_visual.mesh = wheel_mesh
		wheel_visual.material_override = wheel_material
		wheel_visual.rotation.z = PI * 0.5
		wheel_root.add_child(wheel_visual)
		_car.add_child(wheel_root)
		_wheel_nodes.append(wheel_root)

func _update_debug_vehicle_rig(car: Dictionary, delta: float) -> void:
	if _wheel_nodes.size() != 4:
		return

	var wheels: Array = car.get("wheels", [])
	if wheels.size() != 4:
		return

	var car_inverse := _car.global_transform.affine_inverse()
	var steer_visual := float(car.get("input", {}).get("steer", 0.0)) * 0.45
	for index in range(4):
		var wheel: Dictionary = wheels[index]
		var contact_point: Vector3 = wheel.get("godot_contact_point", _car.global_position)
		var ride_height := maxf(float(wheel.get("ride_height", 0.0)), 0.0)
		var wheel_world_position := contact_point + Vector3(0.0, WHEEL_RADIUS + ride_height, 0.0)
		var wheel_node := _wheel_nodes[index]
		wheel_node.position = car_inverse * wheel_world_position
		_wheel_spin[index] += float(wheel.get("spin_velocity", 0.0)) * delta
		wheel_node.rotation.x = _wheel_spin[index]
		wheel_node.rotation.y = steer_visual if index < 2 else 0.0

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

func _rebuild_road_mesh() -> void:
	if _road_built or _snapshot.is_empty():
		return

	var track: Dictionary = _snapshot.get("track", {})
	var debug_points: Array = track.get("debug_points", [])
	if debug_points.size() < 2:
		return

	var vertices := PackedVector3Array()
	var indices := PackedInt32Array()
	for point in debug_points:
		vertices.append(Vector3(point["godot_left_border"].x, point["godot_left_border"].y, point["godot_left_border"].z))
		vertices.append(Vector3(point["godot_right_border"].x, point["godot_right_border"].y, point["godot_right_border"].z))

	for index in range(debug_points.size() - 1):
		var left_a := index * 2
		var right_a := left_a + 1
		var left_b := left_a + 2
		var right_b := left_a + 3
		indices.append_array([left_a, left_b, right_a, right_a, left_b, right_b])

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_INDEX] = indices

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)

	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.18, 0.18, 0.17)
	material.roughness = 0.95
	material.cull_mode = BaseMaterial3D.CULL_DISABLED

	var road_mesh := MeshInstance3D.new()
	road_mesh.name = "GeneratedRoadRibbon"
	road_mesh.mesh = mesh
	road_mesh.material_override = material
	_road.add_child(road_mesh)
	_road_built = true

func _clear_road_mesh() -> void:
	for child in _road.get_children():
		_road.remove_child(child)
		child.queue_free()
	_road_built = false

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

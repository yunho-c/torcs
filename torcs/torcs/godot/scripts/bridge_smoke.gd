extends Node3D

const TorcsBridgeFallbackScript := preload("res://scripts/torcs_bridge_fallback.gd")

@onready var _car: Node3D = $Car
@onready var _camera: Camera3D = $Camera3D
@onready var _telemetry: Label = $CanvasLayer/Telemetry

var _bridge = TorcsBridgeFallbackScript.new()
var _snapshot: Dictionary = {}

func _ready() -> void:
	var data_root := ProjectSettings.globalize_path("res://../data")
	var local_root := ProjectSettings.globalize_path("res://..")
	var loaded: bool = _bridge.initialize(data_root, local_root, local_root)
	if loaded:
		loaded = _bridge.load({
			"track_xml": "data/tracks/road/wheel-2/wheel-2.xml",
			"car_xml": "data/cars/models/car1-trb1/car1-trb1.xml",
			"car_id": "car1-trb1",
			"laps": 0
		})

	if not loaded:
		push_error("Failed to initialize TORCS bridge smoke fallback.")
		set_physics_process(false)
		return

	_snapshot = _bridge.get_snapshot()
	_update_scene_from_snapshot()

func _physics_process(delta: float) -> void:
	var race_time := float(_snapshot.get("race_time", 0.0))
	_bridge.set_human_input(0, _scripted_input(race_time))
	_snapshot = _bridge.step(delta)
	_update_scene_from_snapshot()

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

func _update_scene_from_snapshot() -> void:
	if _snapshot.is_empty() or _snapshot["cars"].is_empty():
		return

	var car: Dictionary = _snapshot["cars"][0]
	var position: Vector3 = car["godot_position"]
	_car.position = position
	_car.rotation.y = -float(car["yaw"])
	_camera.position = position + Vector3(-8.0, 4.0, 8.0)
	_camera.look_at(position + Vector3(4.0, 0.5, 0.0))
	_telemetry.text = "time %.2f  speed %.2f m/s  rpm %.0f  substeps %d" % [
		_snapshot["race_time"],
		car["speed"],
		car["rpm"],
		_snapshot["completed_substeps"]
	]

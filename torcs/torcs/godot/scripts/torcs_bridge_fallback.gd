class_name TorcsBridgeFallback
extends RefCounted

const SIM_STEP_SECONDS := 0.002
const ROBOT_STEP_SECONDS := 0.02
const MAX_SUBSTEPS_PER_STEP := 50

var _initialized := false
var _loaded := false
var _runtime_config := {}
var _race_config := {}
var _accumulator := 0.0
var _human_input := _default_input()
var _snapshot := {
	"race_time": 0.0,
	"completed_substeps": 0,
	"track": {},
	"cars": []
}

static func torcs_to_godot_position(position: Vector3) -> Vector3:
	return Vector3(position.x, position.z, position.y)

func initialize(data_root: String, local_root: String, library_root: String) -> bool:
	_runtime_config = {
		"data_root": data_root,
		"local_root": local_root,
		"library_root": library_root
	}
	_initialized = not data_root.is_empty()
	return _initialized

func shutdown() -> void:
	_initialized = false
	_loaded = false
	_runtime_config = {}
	_race_config = {}
	_accumulator = 0.0
	_human_input = _default_input()
	_snapshot = {
		"race_time": 0.0,
		"completed_substeps": 0,
		"track": {},
		"cars": []
	}

func load(config: Dictionary) -> bool:
	if not _initialized:
		return false

	_race_config = config.duplicate(true)
	_loaded = true
	_accumulator = 0.0
	_human_input = _default_input()
	_snapshot = {
		"race_time": 0.0,
		"completed_substeps": 0,
		"track": _default_track_snapshot(),
		"cars": [_default_car_snapshot()]
	}
	return true

func set_human_input(car_index: int, input: Dictionary) -> void:
	if car_index != 0:
		return

	_human_input = _clamp_input(input)

func step(seconds: float) -> Dictionary:
	_snapshot["completed_substeps"] = 0
	if not _loaded or is_nan(seconds) or is_inf(seconds) or seconds <= 0.0:
		return get_snapshot()

	_accumulator += seconds
	while _accumulator >= SIM_STEP_SECONDS and int(_snapshot["completed_substeps"]) < MAX_SUBSTEPS_PER_STEP:
		_step_one_substep()
		_accumulator -= SIM_STEP_SECONDS
		_snapshot["completed_substeps"] += 1

	return get_snapshot()

func get_snapshot() -> Dictionary:
	return _snapshot.duplicate(true)

static func _default_input() -> Dictionary:
	return {
		"steer": 0.0,
		"throttle": 0.0,
		"brake": 0.0,
		"clutch": 0.0,
		"gear": 1,
		"lights": false,
		"pit_request": false,
		"brake_balance": 0.5
	}

func _default_car_snapshot() -> Dictionary:
	var torcs_position := Vector3.ZERO
	return {
		"id": 0,
		"car_id": _race_config.get("car_id", "car1-trb1"),
		"torcs_position": torcs_position,
		"godot_position": torcs_to_godot_position(torcs_position),
		"torcs_linear_velocity": Vector3.ZERO,
		"torcs_angular_velocity": Vector3.ZERO,
		"yaw": 0.0,
		"speed": 0.0,
		"rpm": 0.0,
		"gear": 1,
		"fuel": 1.0,
		"damage": 0.0,
		"skid": 0.0,
		"collision": false,
		"input": _default_input(),
		"wheels": [
			_default_wheel_snapshot(),
			_default_wheel_snapshot(),
			_default_wheel_snapshot(),
			_default_wheel_snapshot()
		]
	}

static func _default_wheel_snapshot() -> Dictionary:
	return {
		"spin_velocity": 0.0,
		"ride_height": 0.0,
		"slip_side": 0.0,
		"slip_accel": 0.0,
		"skid": 0.0,
		"surface_id": 0
	}

func _default_track_snapshot() -> Dictionary:
	var debug_points: Array[Dictionary] = []
	var track_width := 10.0
	for index in range(13):
		var x := float(index) * 10.0
		var torcs_center := Vector3(x, 0.0, 0.0)
		var torcs_left_border := Vector3(x, track_width * 0.5, 0.0)
		var torcs_right_border := Vector3(x, -track_width * 0.5, 0.0)
		debug_points.append({
			"torcs_center": torcs_center,
			"godot_center": torcs_to_godot_position(torcs_center),
			"torcs_left_border": torcs_left_border,
			"godot_left_border": torcs_to_godot_position(torcs_left_border),
			"torcs_right_border": torcs_right_border,
			"godot_right_border": torcs_to_godot_position(torcs_right_border)
		})

	return {
		"track_id": _race_config.get("track_xml", "wheel-2"),
		"length": 120.0,
		"width": track_width,
		"debug_points": debug_points
	}

static func _clamp_input(input: Dictionary) -> Dictionary:
	var clamped := _default_input()
	for key in input:
		if clamped.has(key):
			clamped[key] = input[key]

	clamped["steer"] = _clamp_float(float(clamped["steer"]), -1.0, 1.0, 0.0)
	clamped["throttle"] = _clamp_float(float(clamped["throttle"]), 0.0, 1.0, 0.0)
	clamped["brake"] = _clamp_float(float(clamped["brake"]), 0.0, 1.0, 0.0)
	clamped["clutch"] = _clamp_float(float(clamped["clutch"]), 0.0, 1.0, 0.0)
	clamped["gear"] = clampi(int(clamped["gear"]), -1, 6)
	clamped["brake_balance"] = _clamp_float(float(clamped["brake_balance"]), 0.0, 1.0, 0.5)
	return clamped

static func _clamp_float(value: float, low: float, high: float, fallback: float) -> float:
	if is_nan(value):
		return fallback
	if is_inf(value):
		return high if value > 0.0 else low
	return clampf(value, low, high)

func _step_one_substep() -> void:
	var car: Dictionary = _snapshot["cars"][0]
	var dt := SIM_STEP_SECONDS
	var longitudinal_accel := float(_human_input["throttle"]) * 8.0 - float(_human_input["brake"]) * 12.0
	var new_speed: float = maxf(0.0, float(car["speed"]) + longitudinal_accel * dt)
	var yaw_rate: float = float(_human_input["steer"]) * minf(new_speed, 30.0) * 0.025
	var yaw: float = float(car["yaw"]) + yaw_rate * dt
	var torcs_velocity := Vector3(cos(yaw) * new_speed, sin(yaw) * new_speed, 0.0)
	var torcs_position: Vector3 = car["torcs_position"] + torcs_velocity * dt
	var skid: float = absf(float(_human_input["steer"])) * new_speed * 0.02

	car["yaw"] = yaw
	car["speed"] = new_speed
	car["rpm"] = 900.0 + new_speed * 120.0 + float(_human_input["throttle"]) * 1800.0
	car["gear"] = int(_human_input["gear"])
	car["input"] = _human_input.duplicate(true)
	car["torcs_linear_velocity"] = torcs_velocity
	car["torcs_angular_velocity"] = Vector3(0.0, 0.0, yaw_rate)
	car["torcs_position"] = torcs_position
	car["godot_position"] = torcs_to_godot_position(torcs_position)
	car["skid"] = skid

	for wheel in car["wheels"]:
		wheel["spin_velocity"] = new_speed / 0.33
		wheel["ride_height"] = 0.12
		wheel["slip_side"] = absf(float(_human_input["steer"])) * new_speed * 0.01
		wheel["slip_accel"] = absf(longitudinal_accel) * 0.05
		wheel["skid"] = skid
		wheel["surface_id"] = 0

	_snapshot["race_time"] += dt

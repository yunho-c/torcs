extends SceneTree

const SIM_STEP_SECONDS := 0.002
const ROBOT_STEP_SECONDS := 0.02
const TORCS_GDEXTENSION_PATH := "res://bin/torcs_gdextension.gdextension"

var _exit_code := 0

func _init() -> void:
	_run.call_deferred()

func _run() -> void:
	var options := _parse_options()
	if _exit_code != 0:
		quit(_exit_code)
		return

	_ensure_native_bridge_loaded()
	if not ClassDB.class_exists("TorcsBridgeNative"):
		push_error("TorcsBridgeNative is not registered. Build and load torcs_gdextension first.")
		quit(2)
		return

	var bridge = ClassDB.instantiate("TorcsBridgeNative")
	if bridge == null:
		push_error("Failed to instantiate TorcsBridgeNative.")
		quit(2)
		return

	var data_root: String = options["data_root"]
	var local_root: String = options["local_root"]
	if not bridge.initialize(data_root, local_root, local_root):
		push_error("Failed to initialize TorcsBridgeNative.")
		quit(1)
		return

	var race_config := {
		"track_xml": options["track_xml"],
		"car_xml": options["car_xml"],
		"car_id": options["car_id"],
		"laps": int(options["laps"])
	}
	if not bridge.load(race_config):
		push_error("Failed to load TorcsBridgeNative race.")
		bridge.shutdown()
		quit(1)
		return

	var initial_snapshot: Dictionary = bridge.get_snapshot()
	var samples: Array = []
	var elapsed := 0.0
	while elapsed < float(options["seconds"]):
		var step_seconds: float = minf(float(options["sample_seconds"]), float(options["seconds"]) - elapsed)
		bridge.set_human_input(0, _scripted_input(elapsed))
		var snapshot: Dictionary = bridge.step(step_seconds)
		samples.append(_sample_to_json(snapshot))
		elapsed += step_seconds

	bridge.shutdown()

	var output := {
		"track": _track_to_json(initial_snapshot.get("track", {})),
		"samples": samples
	}
	var json := JSON.stringify(output)
	if String(options["output"]).is_empty():
		print(json)
	else:
		var file := FileAccess.open(options["output"], FileAccess.WRITE)
		if file == null:
			push_error("Failed to open native capture output: " + String(options["output"]))
			quit(1)
			return
		file.store_string(json + "\n")

	quit(_exit_code)

func _ensure_native_bridge_loaded() -> void:
	if ClassDB.class_exists("TorcsBridgeNative"):
		return

	if ResourceLoader.exists(TORCS_GDEXTENSION_PATH):
		ResourceLoader.load(TORCS_GDEXTENSION_PATH)

func _parse_options() -> Dictionary:
	var options := {
		"data_root": ProjectSettings.globalize_path("res://../data"),
		"local_root": ProjectSettings.globalize_path("res://.."),
		"track_xml": "data/tracks/road/wheel-2/wheel-2.xml",
		"car_xml": "data/cars/models/car1-trb1/car1-trb1.xml",
		"car_id": "car1-trb1",
		"laps": 0,
		"seconds": 0.2,
		"sample_seconds": ROBOT_STEP_SECONDS,
		"output": ""
	}

	var args := OS.get_cmdline_user_args()
	var index := 0
	while index < args.size():
		var arg := String(args[index])
		if index + 1 >= args.size():
			push_error("Missing value for " + arg)
			_exit_code = 2
			return options

		var value := String(args[index + 1])
		match arg:
			"--data-root":
				options["data_root"] = value
			"--local-root":
				options["local_root"] = value
			"--track-xml":
				options["track_xml"] = value
			"--car-xml":
				options["car_xml"] = value
			"--car-id":
				options["car_id"] = value
			"--laps":
				options["laps"] = int(value)
			"--seconds":
				options["seconds"] = float(value)
			"--sample-seconds":
				options["sample_seconds"] = float(value)
			"--output":
				options["output"] = value
			_:
				push_error("Unknown option: " + arg)
				_exit_code = 2
				return options
		index += 2

	if _exit_code == 0 and (not is_finite(float(options["seconds"])) or float(options["seconds"]) <= 0.0):
		push_error("--seconds must be finite and positive.")
		_exit_code = 2
	if _exit_code == 0 and (not is_finite(float(options["sample_seconds"])) or float(options["sample_seconds"]) < SIM_STEP_SECONDS):
		push_error("--sample-seconds must be finite and at least %.3f." % SIM_STEP_SECONDS)
		_exit_code = 2
	return options

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

func _sample_to_json(snapshot: Dictionary) -> Dictionary:
	var cars: Array = []
	for car in snapshot.get("cars", []):
		cars.append(_car_to_json(car))
	return {
		"time": float(snapshot.get("race_time", 0.0)),
		"substeps": int(snapshot.get("completed_substeps", 0)),
		"cars": cars
	}

func _track_to_json(track: Dictionary) -> Dictionary:
	var debug_points: Array = []
	for point in track.get("debug_points", []):
		debug_points.append({
			"segment_id": int(point.get("segment_id", -1)),
			"segment_name": point.get("segment_name", ""),
			"distance_from_start": float(point.get("distance_from_start", 0.0)),
			"surface_id": int(point.get("surface_id", -1)),
			"surface_name": point.get("surface_name", ""),
			"start_line": bool(point.get("start_line", false)),
			"finish_line": bool(point.get("finish_line", false)),
			"torcs_center": _vec3_to_json(point["torcs_center"]),
			"godot_center": _vec3_to_json(point["godot_center"]),
			"torcs_left_border": _vec3_to_json(point["torcs_left_border"]),
			"godot_left_border": _vec3_to_json(point["godot_left_border"]),
			"torcs_right_border": _vec3_to_json(point["torcs_right_border"]),
			"godot_right_border": _vec3_to_json(point["godot_right_border"])
		})
	return {
		"track_id": track.get("track_id", ""),
		"length": float(track.get("length", 0.0)),
		"width": float(track.get("width", 0.0)),
		"debug_points": debug_points
	}

func _car_to_json(car: Dictionary) -> Dictionary:
	var wheels: Array = []
	for wheel in car.get("wheels", []):
		wheels.append({
			"spin_velocity": float(wheel.get("spin_velocity", 0.0)),
			"ride_height": float(wheel.get("ride_height", 0.0)),
			"slip_side": float(wheel.get("slip_side", 0.0)),
			"slip_accel": float(wheel.get("slip_accel", 0.0)),
			"skid": float(wheel.get("skid", 0.0)),
			"surface_id": int(wheel.get("surface_id", 0)),
			"surface_name": wheel.get("surface_name", ""),
			"has_contact": bool(wheel.get("has_contact", false)),
			"torcs_contact_point": _vec3_to_json(wheel.get("torcs_contact_point", Vector3.ZERO)),
			"godot_contact_point": _vec3_to_json(wheel.get("godot_contact_point", Vector3.ZERO)),
			"torcs_surface_normal": _vec3_to_json(wheel.get("torcs_surface_normal", Vector3.ZERO)),
			"godot_surface_normal": _vec3_to_json(wheel.get("godot_surface_normal", Vector3.ZERO))
		})

	return {
		"id": int(car.get("id", 0)),
		"car_id": car.get("car_id", ""),
		"torcs_position": _vec3_to_json(car.get("torcs_position", Vector3.ZERO)),
		"godot_position": _vec3_to_json(car.get("godot_position", Vector3.ZERO)),
		"torcs_linear_velocity": _vec3_to_json(car.get("torcs_linear_velocity", Vector3.ZERO)),
		"godot_linear_velocity": _vec3_to_json(car.get("godot_linear_velocity", Vector3.ZERO)),
		"torcs_angular_velocity": _vec3_to_json(car.get("torcs_angular_velocity", Vector3.ZERO)),
		"godot_angular_velocity": _vec3_to_json(car.get("godot_angular_velocity", Vector3.ZERO)),
		"yaw": float(car.get("yaw", 0.0)),
		"godot_yaw": float(car.get("godot_yaw", 0.0)),
		"speed": float(car.get("speed", 0.0)),
		"rpm": float(car.get("rpm", 0.0)),
		"gear": int(car.get("gear", 0)),
		"fuel": float(car.get("fuel", 0.0)),
		"damage": float(car.get("damage", 0.0)),
		"skid": float(car.get("skid", 0.0)),
		"collision": bool(car.get("collision", false)),
		"input": _input_to_json(car.get("input", {})),
		"track_position": _track_position_to_json(car.get("track_position", {})),
		"wheels": wheels
	}

func _track_position_to_json(position: Dictionary) -> Dictionary:
	return {
		"segment_id": int(position.get("segment_id", -1)),
		"segment_name": position.get("segment_name", ""),
		"distance_from_start": float(position.get("distance_from_start", 0.0)),
		"to_start": float(position.get("to_start", 0.0)),
		"to_right": float(position.get("to_right", 0.0)),
		"to_middle": float(position.get("to_middle", 0.0)),
		"to_left": float(position.get("to_left", 0.0)),
		"surface_id": int(position.get("surface_id", -1)),
		"surface_name": position.get("surface_name", ""),
		"start_line": bool(position.get("start_line", false)),
		"finish_line": bool(position.get("finish_line", false))
	}

func _input_to_json(input: Dictionary) -> Dictionary:
	return {
		"steer": float(input.get("steer", 0.0)),
		"throttle": float(input.get("throttle", 0.0)),
		"brake": float(input.get("brake", 0.0)),
		"clutch": float(input.get("clutch", 0.0)),
		"gear": int(input.get("gear", 0)),
		"lights": bool(input.get("lights", false)),
		"pit_request": bool(input.get("pit_request", false)),
		"brake_balance": float(input.get("brake_balance", 0.0))
	}

func _vec3_to_json(value: Vector3) -> Dictionary:
	return {
		"x": value.x,
		"y": value.y,
		"z": value.z
	}

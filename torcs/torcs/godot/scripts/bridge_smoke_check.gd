extends SceneTree

const TorcsBridgeFallbackScript := preload("res://scripts/torcs_bridge_fallback.gd")

var _failures := 0

func _init() -> void:
	_check_fallback_snapshot()
	quit(_failures)

func _check_fallback_snapshot() -> void:
	var bridge = TorcsBridgeFallbackScript.new()
	var data_root := ProjectSettings.globalize_path("res://../data")
	var local_root := ProjectSettings.globalize_path("res://..")

	_expect(bridge.initialize(data_root, local_root, local_root), "bridge initializes")
	_expect(bridge.load({
		"track_xml": "data/tracks/road/wheel-2/wheel-2.xml",
		"car_xml": "data/cars/models/car1-trb1/car1-trb1.xml",
		"car_id": "car1-trb1",
		"laps": 0
	}), "bridge loads")

	var snapshot: Dictionary = bridge.get_snapshot()
	var track: Dictionary = snapshot.get("track", {})
	_expect(snapshot.get("cars", []).size() == 1, "snapshot has one car")
	_expect(track.get("debug_points", []).size() == 13, "track has debug points")
	_expect(is_equal_approx(float(track.get("width", 0.0)), 10.0), "track width is stable")

	bridge.set_human_input(0, {
		"steer": 0.25,
		"throttle": 1.0,
		"brake": 0.0,
		"clutch": 0.0,
		"gear": 1,
		"lights": false,
		"pit_request": false,
		"brake_balance": 0.55
	})
	snapshot = bridge.step(TorcsBridgeFallbackScript.ROBOT_STEP_SECONDS)

	var car: Dictionary = snapshot["cars"][0]
	_expect(int(snapshot["completed_substeps"]) == 10, "robot step produces ten substeps")
	_expect(float(snapshot["race_time"]) > 0.0, "race time advances")
	_expect(float(car["speed"]) > 0.0, "car accelerates")
	_expect(car["wheels"].size() == 4, "car has four wheel snapshots")

	var torcs_position: Vector3 = car["torcs_position"]
	var godot_position: Vector3 = car["godot_position"]
	_expect(is_equal_approx(godot_position.y, torcs_position.z), "Godot Y maps from TORCS Z")
	_expect(is_equal_approx(godot_position.z, torcs_position.y), "Godot Z maps from TORCS Y")

	snapshot = bridge.step(1.0)
	_expect(
		int(snapshot["completed_substeps"]) == TorcsBridgeFallbackScript.MAX_SUBSTEPS_PER_STEP,
		"large delta is capped"
	)

	bridge.shutdown()

func _expect(condition: bool, message: String) -> void:
	if condition:
		return

	_failures += 1
	push_error("Bridge smoke check failed: " + message)

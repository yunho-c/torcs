extends SceneTree

var _failures := 0

func _init() -> void:
	_run.call_deferred()

func _run() -> void:
	var packed_scene: PackedScene = load("res://scenes/bridge_smoke.tscn")
	_expect(packed_scene != null, "bridge smoke scene loads")
	if packed_scene == null:
		quit(_failures)
		return

	var scene := packed_scene.instantiate()
	root.add_child(scene)

	await process_frame
	await physics_frame
	await create_timer(0.25).timeout

	var car := scene.get_node_or_null("Car") as Node3D
	var road := scene.get_node_or_null("Road") as Node3D
	var track_debug := scene.get_node_or_null("TrackDebug") as Node3D
	var telemetry := scene.get_node_or_null("CanvasLayer/Telemetry") as Label
	var backend: String = scene.call("get_bridge_backend")
	var snapshot: Dictionary = scene.call("get_debug_snapshot")
	var cars: Array = snapshot.get("cars", [])
	var track: Dictionary = snapshot.get("track", {})
	var debug_points: Array = track.get("debug_points", [])

	_expect(car != null, "scene has car node")
	_expect(road != null, "scene has road node")
	_expect(track_debug != null, "scene has track debug node")
	_expect(telemetry != null, "scene has telemetry label")
	_expect(backend == "native" or backend == "fallback", "scene reports active bridge backend")
	_expect(int(snapshot.get("completed_substeps", 0)) > 0, "bridge advances simulation substeps")
	_expect(cars.size() == 1, "scene snapshot has one car")
	_expect(debug_points.size() >= 2, "scene snapshot has road debug points")
	for action in [
		"torcs_steer_left",
		"torcs_steer_right",
		"torcs_throttle",
		"torcs_brake",
		"torcs_shift_up",
		"torcs_shift_down",
		"torcs_reset"
	]:
		_expect(InputMap.has_action(action), "project input action exists: " + action)

	if car != null:
		_expect(car.position.x > 0.0, "headless scripted smoke moves car")
		_expect(is_equal_approx(car.rotation.y, -PI * 0.5), "initial visual yaw faces bridge +X")
		_expect(car.get_node_or_null("Body") is MeshInstance3D, "debug vehicle has body mesh")
		_expect(car.get_node_or_null("FrontLeftWheel") is Node3D, "debug vehicle has front-left wheel")
		_expect(car.get_node_or_null("FrontRightWheel") is Node3D, "debug vehicle has front-right wheel")
		_expect(car.get_node_or_null("RearLeftWheel") is Node3D, "debug vehicle has rear-left wheel")
		_expect(car.get_node_or_null("RearRightWheel") is Node3D, "debug vehicle has rear-right wheel")
		if cars.size() == 1:
			var car_snapshot: Dictionary = cars[0]
			var snapshot_position: Vector3 = car_snapshot["godot_position"]
			_expect(car.position.distance_to(snapshot_position) < 0.01, "visual car position matches snapshot")
			var input: Dictionary = car_snapshot.get("input", {})
			_expect(float(input.get("throttle", 0.0)) > 0.8, "headless scripted input reaches bridge")
			var track_position: Dictionary = car_snapshot.get("track_position", {})
			_expect(int(track_position.get("segment_id", -1)) >= 0, "car exposes track-local segment")
			_expect(absf(float(track_position.get("to_middle", 0.0))) <= maxf(float(track.get("width", 0.0)), 10.0), "track-local offset stays on debug road")
			var velocity: Vector3 = car_snapshot.get("godot_linear_velocity", Vector3.ZERO)
			velocity.y = 0.0
			if velocity.length() > 0.1:
				var visual_forward := _forward_from_godot_yaw(car.rotation.y)
				_expect(visual_forward.dot(velocity.normalized()) > 0.75, "visual car is not sideways or mirrored")
			if debug_points.size() >= 2:
				var road_sample := _nearest_road_sample(car.position, debug_points)
				var road_width := maxf(float(road_sample["width"]), float(track.get("width", 0.0)))
				_expect(float(road_sample["lateral_distance"]) <= road_width * 0.75 + 2.0, "visual car stays near generated road")
				_expect(car.position.y >= float(road_sample["height"]) - 0.25, "visual car is not underground")
	if road != null:
		_expect(road.get_child_count() == 1, "road ribbon is generated from bridge track data")
		var road_mesh := road.get_node_or_null("GeneratedRoadRibbon") as MeshInstance3D
		_expect(road_mesh != null and road_mesh.mesh != null, "road ribbon has mesh")
		if road_mesh != null and road_mesh.mesh != null:
			_expect(road_mesh.mesh.get_surface_count() == 1, "road ribbon mesh has one surface")
	if track_debug != null:
		_expect(track_debug.get_child_count() >= 3, "track debug overlay is built")
		scene.call("_load_race")
		if road != null:
			_expect(road.get_child_count() == 1, "road ribbon rebuild does not duplicate meshes")
		_expect(track_debug.get_child_count() == 3, "track debug overlay rebuild does not duplicate lines")
	if telemetry != null:
		_expect(telemetry.text.contains("speed"), "telemetry updates from snapshot")

	root.remove_child(scene)
	scene.queue_free()
	quit(_failures)

func _expect(condition: bool, message: String) -> void:
	if condition:
		return

	_failures += 1
	push_error("Bridge scene check failed: " + message)

func _forward_from_godot_yaw(godot_yaw: float) -> Vector3:
	return Vector3(-sin(godot_yaw), 0.0, -cos(godot_yaw))

func _nearest_road_sample(position: Vector3, debug_points: Array) -> Dictionary:
	var best_distance := INF
	var best_height := 0.0
	var best_width := 0.0
	var query := Vector2(position.x, position.z)
	for index in range(debug_points.size() - 1):
		var point_a: Dictionary = debug_points[index]
		var point_b: Dictionary = debug_points[index + 1]
		var center_a: Vector3 = point_a["godot_center"]
		var center_b: Vector3 = point_b["godot_center"]
		var center_a_2d := Vector2(center_a.x, center_a.z)
		var center_b_2d := Vector2(center_b.x, center_b.z)
		var segment := center_b_2d - center_a_2d
		var segment_length_sq := segment.length_squared()
		if segment_length_sq <= 0.000001:
			continue
		var t := clampf((query - center_a_2d).dot(segment) / segment_length_sq, 0.0, 1.0)
		var nearest_2d := center_a_2d + segment * t
		var distance := query.distance_to(nearest_2d)
		if distance < best_distance:
			var left: Vector3 = point_a["godot_left_border"].lerp(point_b["godot_left_border"], t)
			var right: Vector3 = point_a["godot_right_border"].lerp(point_b["godot_right_border"], t)
			best_distance = distance
			best_height = center_a.lerp(center_b, t).y
			best_width = left.distance_to(right)
	return {
		"lateral_distance": best_distance,
		"height": best_height,
		"width": best_width
	}

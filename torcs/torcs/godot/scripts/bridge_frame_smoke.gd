extends SceneTree

var _failures := 0

func _init() -> void:
	_run.call_deferred()

func _run() -> void:
	var frame_count := _int_arg("--frames", 3600)
	var packed_scene: PackedScene = load("res://scenes/bridge_smoke.tscn")
	_expect(packed_scene != null, "bridge smoke scene loads")
	if packed_scene == null:
		quit(_failures)
		return

	var scene := packed_scene.instantiate()
	root.add_child(scene)

	await process_frame
	await physics_frame

	var initial_snapshot: Dictionary = scene.call("get_debug_snapshot")
	var initial_cars: Array = initial_snapshot.get("cars", [])
	var initial_position := Vector3.ZERO
	if initial_cars.size() == 1:
		initial_position = initial_cars[0].get("godot_position", Vector3.ZERO)

	for _frame in range(frame_count):
		await physics_frame

	var snapshot: Dictionary = scene.call("get_debug_snapshot")
	var cars: Array = snapshot.get("cars", [])
	var backend: String = scene.call("get_bridge_backend")
	_expect(backend == "native" or backend == "fallback", "frame smoke has active backend")
	_expect(cars.size() == 1, "frame smoke has one car")
	_expect(float(snapshot.get("race_time", 0.0)) > 0.0, "frame smoke advances race time")
	if cars.size() == 1:
		var car: Dictionary = cars[0]
		var position: Vector3 = car.get("godot_position", Vector3.ZERO)
		var min_distance := 1.0 if frame_count >= 600 else 0.001
		_expect(position.distance_to(initial_position) > min_distance, "frame smoke moves car visibly")
		_expect(float(car.get("speed", 0.0)) >= 0.0, "frame smoke keeps finite speed")
		_expect(car.has("track_position"), "frame smoke keeps Phase 3 track-local schema")

	root.remove_child(scene)
	scene.queue_free()
	quit(_failures)

func _int_arg(name: String, fallback: int) -> int:
	var args := OS.get_cmdline_user_args()
	for index in range(args.size()):
		if args[index] == name and index + 1 < args.size():
			return int(args[index + 1])
	return fallback

func _expect(condition: bool, message: String) -> void:
	if condition:
		return

	_failures += 1
	push_error("Bridge frame smoke failed: " + message)

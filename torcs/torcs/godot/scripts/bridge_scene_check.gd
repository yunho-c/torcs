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

	_expect(car != null, "scene has car node")
	_expect(road != null, "scene has road node")
	_expect(track_debug != null, "scene has track debug node")
	_expect(telemetry != null, "scene has telemetry label")
	_expect(backend == "native" or backend == "fallback", "scene reports active bridge backend")

	if car != null:
		_expect(car.position.x > 0.0, "headless scripted smoke moves car")
		_expect(is_equal_approx(car.rotation.y, -PI * 0.5), "initial visual yaw faces bridge +X")
	if road != null:
		_expect(road.get_child_count() == 1, "road ribbon is generated from bridge track data")
		var road_mesh := road.get_node_or_null("GeneratedRoadRibbon") as MeshInstance3D
		_expect(road_mesh != null and road_mesh.mesh != null, "road ribbon has mesh")
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

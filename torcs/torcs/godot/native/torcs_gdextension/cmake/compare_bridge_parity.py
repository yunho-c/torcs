#!/usr/bin/env python3
import argparse
import json
import math
import sys


DEFAULT_TOLERANCES = {
    "time": 1.0e-6,
    "position": 1.0e-4,
    "yaw": 1.0e-5,
    "speed": 1.0e-4,
    "rpm": 1.0e-2,
    "control": 1.0e-6,
}


def load_json(path):
    with open(path, "r", encoding="utf-8") as file:
        return json.load(file)


def fail(message):
    print(message, file=sys.stderr)
    return 1


def compare_float(label, expected, actual, tolerance, failures):
    if not math.isfinite(expected) or not math.isfinite(actual):
        failures.append(f"{label}: non-finite value expected={expected} actual={actual}")
        return
    if abs(expected - actual) > tolerance:
        failures.append(
            f"{label}: expected {expected:.9f}, actual {actual:.9f}, tolerance {tolerance:.9f}"
        )


def compare_vec3(label, expected, actual, tolerance, failures):
    for axis in ("x", "y", "z"):
        compare_float(f"{label}.{axis}", expected[axis], actual[axis], tolerance, failures)


def compare_controls(label, expected, actual, tolerance, failures):
    for key in ("steer", "throttle", "brake", "clutch", "brake_balance"):
        compare_float(f"{label}.{key}", expected[key], actual[key], tolerance, failures)
    for key in ("gear", "lights", "pit_request"):
        if expected[key] != actual[key]:
            failures.append(f"{label}.{key}: expected {expected[key]}, actual {actual[key]}")


def compare_sample(index, expected, actual, tolerances, failures):
    compare_float(f"samples[{index}].time", expected["time"], actual["time"], tolerances["time"], failures)
    if expected["substeps"] != actual["substeps"]:
        failures.append(
            f"samples[{index}].substeps: expected {expected['substeps']}, actual {actual['substeps']}"
        )

    if len(expected["cars"]) != len(actual["cars"]):
        failures.append(
            f"samples[{index}].cars: expected {len(expected['cars'])}, actual {len(actual['cars'])}"
        )
        return

    expected_car = expected["cars"][0]
    actual_car = actual["cars"][0]
    compare_vec3(
        f"samples[{index}].cars[0].torcs_position",
        expected_car["torcs_position"],
        actual_car["torcs_position"],
        tolerances["position"],
        failures,
    )
    compare_vec3(
        f"samples[{index}].cars[0].godot_position",
        expected_car["godot_position"],
        actual_car["godot_position"],
        tolerances["position"],
        failures,
    )
    compare_float(
        f"samples[{index}].cars[0].yaw",
        expected_car["yaw"],
        actual_car["yaw"],
        tolerances["yaw"],
        failures,
    )
    compare_float(
        f"samples[{index}].cars[0].godot_yaw",
        expected_car["godot_yaw"],
        actual_car["godot_yaw"],
        tolerances["yaw"],
        failures,
    )
    compare_float(
        f"samples[{index}].cars[0].speed",
        expected_car["speed"],
        actual_car["speed"],
        tolerances["speed"],
        failures,
    )
    compare_float(
        f"samples[{index}].cars[0].rpm",
        expected_car["rpm"],
        actual_car["rpm"],
        tolerances["rpm"],
        failures,
    )
    if expected_car["gear"] != actual_car["gear"]:
        failures.append(
            f"samples[{index}].cars[0].gear: expected {expected_car['gear']}, actual {actual_car['gear']}"
        )
    compare_controls(
        f"samples[{index}].cars[0].input",
        expected_car["input"],
        actual_car["input"],
        tolerances["control"],
        failures,
    )


def main():
    parser = argparse.ArgumentParser(description="Compare retained harness and Godot native bridge JSON output.")
    parser.add_argument("--harness", required=True)
    parser.add_argument("--godot", required=True)
    for name, value in DEFAULT_TOLERANCES.items():
        parser.add_argument(f"--{name}-tolerance", type=float, default=value)
    args = parser.parse_args()

    harness = load_json(args.harness)
    godot = load_json(args.godot)
    harness_samples = harness.get("samples", [])
    godot_samples = godot.get("samples", [])
    failures = []
    if len(harness_samples) != len(godot_samples):
        return fail(f"sample count mismatch: harness={len(harness_samples)} godot={len(godot_samples)}")

    tolerances = {
        "time": args.time_tolerance,
        "position": args.position_tolerance,
        "yaw": args.yaw_tolerance,
        "speed": args.speed_tolerance,
        "rpm": args.rpm_tolerance,
        "control": args.control_tolerance,
    }
    for index, (expected, actual) in enumerate(zip(harness_samples, godot_samples)):
        compare_sample(index, expected, actual, tolerances, failures)

    if failures:
        for failure in failures[:20]:
            print(failure, file=sys.stderr)
        if len(failures) > 20:
            print(f"... {len(failures) - 20} additional failures", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

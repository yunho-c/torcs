#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
NATIVE_SOURCE="$REPO_ROOT/torcs/torcs/godot/native/torcs_gdextension"
BUILD_ROOT="${TORCS_BRIDGE_VERIFY_BUILD_ROOT:-/private/tmp/torcs-bridge-phase3-verify}"
BUILD_DIR="$BUILD_ROOT/retained"
GODOT_HOME="${TORCS_BRIDGE_GODOT_HOME:-/private/tmp/torcs-godot-home}"
SMOKE_FRAMES="${TORCS_BRIDGE_PHASE3_SMOKE_FRAMES:-3600}"

cmake_args=(
	-S "$NATIVE_SOURCE"
	-B "$BUILD_DIR"
	-DTORCS_BRIDGE_ENABLE_RETAINED_CORE=ON
)

if [[ "${TORCS_BRIDGE_VERIFY_GDEXTENSION:-auto}" == "ON" ]]; then
	cmake_args+=(-DTORCS_BRIDGE_ENABLE_GDEXTENSION=ON)
elif [[ "${TORCS_BRIDGE_VERIFY_GDEXTENSION:-auto}" == "auto" ]]; then
	if [[ -n "${TORCS_BRIDGE_GODOT_CPP_ROOT:-}" \
		|| -n "${TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR:-}" \
		|| -n "${TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR:-}" \
		|| -n "${TORCS_BRIDGE_GODOT_CPP_LIBRARY:-}" ]]; then
		cmake_args+=(-DTORCS_BRIDGE_ENABLE_GDEXTENSION=ON)
	else
		echo "Skipping GDExtension target: set TORCS_BRIDGE_VERIFY_GDEXTENSION=ON and godot-cpp paths to require it."
	fi
fi

mkdir -p "$BUILD_ROOT" "$GODOT_HOME"

cmake "${cmake_args[@]}"
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure

HOME="$GODOT_HOME" godot --headless --path "$REPO_ROOT/torcs/torcs/godot" --check-only --script res://scripts/bridge_smoke.gd
HOME="$GODOT_HOME" godot --headless --path "$REPO_ROOT/torcs/torcs/godot" --script res://scripts/bridge_smoke_check.gd
HOME="$GODOT_HOME" godot --headless --path "$REPO_ROOT/torcs/torcs/godot" --script res://scripts/bridge_scene_check.gd
HOME="$GODOT_HOME" godot --headless --path "$REPO_ROOT/torcs/torcs/godot" --script res://scripts/bridge_frame_smoke.gd -- --frames "$SMOKE_FRAMES"

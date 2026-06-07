# TORCS Godot Project

This directory contains the Godot-owned runtime for the staged TORCS port. It
is separate from the legacy TORCS source so the original simulator remains
buildable while the retained simulation bridge and new presentation layer grow.

Current scope:

- Godot 4.x desktop prototype.
- One free-drive session.
- Native C++ bridge under `native/torcs_gdextension`.
- Debug visuals before converted legacy media.

Directory layout:

- `addons/torcs_importers/`: future editor import plugins.
- `assets/`: Godot-native or converted assets.
- `native/torcs_gdextension/`: shared native bridge, command-line harness, and
  opt-in GDExtension library.
- `scenes/`: Godot scenes.
- `scripts/`: GDScript runtime helpers.

Open `project.godot` with Godot 4.6 or a compatible 4.x stable build.

## Native Bridge Build

From the repository root:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-build
cmake --build /private/tmp/torcs-bridge-build
ctest --test-dir /private/tmp/torcs-bridge-build --output-on-failure
```

The default native bridge build is dependency-free. Its harness uses a
deterministic stub backend so DTO shape, fixed-step behavior, CSV/JSON output,
and repeatability can be tested without PLIB, Godot, or legacy TORCS module
loading.

Run the stub harness:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 10 --output /private/tmp/torcs-bridge.csv
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 10 --format json --output /private/tmp/torcs-bridge.json
/private/tmp/torcs-bridge-build/torcs_bridge_harness --backend stub --seconds 10 --output /private/tmp/torcs-bridge.csv
```

The harness emits CSV or JSON snapshots with time, substep count, TORCS/Godot
positions, TORCS/Godot linear and angular velocity, TORCS/Godot yaw, speed, RPM,
gear, input values, wheel state, damage, skid, and track debug geometry. It also
accepts `--track-xml`,
`--car-xml`, `--car-id`, and `--laps` so stub and retained-core runs use the
same standalone schema for different race configurations. CTest runs the stub
harness twice with the same scripted input and compares the CSV outputs to keep
repeatability covered.

## Retained Simuv2 Harness

The retained path is enabled only when CMake can find PLIB headers, `sg`, and
`ul` libraries, and can link required `sg` symbols such as `sgMakeCoordMat4`:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-retained -DTORCS_BRIDGE_ENABLE_RETAINED_CORE=ON
cmake --build /private/tmp/torcs-bridge-retained
ctest --test-dir /private/tmp/torcs-bridge-retained --output-on-failure
```

When those checks pass, the same harness can run through retained TORCS
`TrackBuildv1` plus `simuv2` for `wheel-2` and `car1-trb1`:

```bash
/private/tmp/torcs-bridge-retained/torcs_bridge_harness --backend retained --seconds 10 --output /private/tmp/torcs-retained.csv
/private/tmp/torcs-bridge-retained/torcs_bridge_harness --backend retained --seconds 10 --format json --output /private/tmp/torcs-retained.json
```

Retained CTest coverage is registered behind the same linkability gate as
`torcs_retained_smoke`. It checks CSV/JSON harness output, repeated-run CSV
determinism, runtime track/car loading, throttle, brake, steering, and retained
snapshot determinism. If `--backend retained` is requested in a build without
that gate, the harness exits with an explicit unavailable-backend error.

## Godot Smoke Scene

Run the current smoke scene:

```bash
godot --path torcs/torcs/godot
```

Windowed runs use Godot input actions from `project.godot`. Defaults are W/Up
throttle, S/Down brake, A/Left and D/Right steering, Q/E gear changes, R reset,
and L lights, with gamepad left stick steering and trigger throttle/brake mapped
through the same actions. Headless runs keep the deterministic scripted input
path so smoke checks remain repeatable.

In this sandboxed macOS environment, Godot cannot write to the normal user data
directory. Use a temporary home for headless verification:

```bash
mkdir -p /private/tmp/torcs-godot-home
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --check-only --script res://scripts/bridge_smoke.gd
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_smoke_check.gd
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_scene_check.gd
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --quit-after 3
```

Run the repeatable Phase 2/3 gate from the repository root:

```bash
torcs/torcs/godot/verify_phase3.sh
```

By default this configures a retained bridge build under
`/private/tmp/torcs-bridge-phase3-verify`, runs CTest, runs Godot check-only,
the fallback contract check, the scene acceptance check, and a 60-second
headless scene smoke. Set `TORCS_BRIDGE_PHASE3_SMOKE_SECONDS` to shorten local
iterations. To require the GDExtension target, set
`TORCS_BRIDGE_VERIFY_GDEXTENSION=ON` and provide the godot-cpp CMake variables
documented in `native/torcs_gdextension/README.md`; generated Godot `bin`
outputs remain uncommitted.

The smoke scene tries the native `TorcsBridgeNative` GDExtension first when it
is registered, then falls back to `scripts/torcs_bridge_fallback.gd`. The active
backend is logged as `native` or `fallback`; no committed `.gdextension` file is
required for fallback-only startup. A native candidate that lacks the current
Phase 3 snapshot schema is rejected so stale ignored native artifacts do not
silently drive the scene. The stub harness verifies dependency-free
DTO behavior, the retained harness verifies native `simuv2` stepping when PLIB
is available, and the fallback scene keeps Godot scene logic stable when the
native extension is not built. The scene also renders the bridge snapshot's
generated road ribbon, track debug centerline, road borders, and debug vehicle
rig so retained `tTrackSeg` output can be checked against Godot coordinates
early.

Native parity capture is available in native-enabled builds:

```bash
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_native_capture.gd -- --seconds 0.2 --sample-seconds 0.02 --output /private/tmp/torcs-godot-native.json
```

Without the built extension, this script exits with an explicit
`TorcsBridgeNative is not registered` error; the fallback smoke checks above
remain the expected local verification path.

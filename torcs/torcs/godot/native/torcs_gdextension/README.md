# TORCS Native Bridge

This native subtree will host two targets that share the same bridge core:

- `torcs_bridge_harness`: command-line executable for deterministic scripted
  stepping outside Godot.
- `torcs_gdextension`: Godot GDExtension library that exposes the same retained
  TORCS core through a Godot-friendly API.

The first implementation keeps TORCS structs internal and exports snapshots,
input values, and lifecycle methods. The bridge should avoid legacy graphics,
audio, GLUT, PLIB, SSG, and `tgfclient` dependencies.

## Current Targets

- `torcs_bridge_core`: static C++17 DTO/lifecycle/fixed-step bridge core.
- `torcs_bridge_harness`: command-line deterministic CSV snapshot harness.
- `torcs_bridge_tests`: dependency-free CTest regression checks.
- `torcs_retained_smoke`: retained-core-only smoke executable that loads
  `wheel-2` through `TorcsRetainedAdapter` when retained dependencies are
  enabled.

CTest also runs quick harness CLI smoke checks for `--help`, CSV output,
deterministic repeat-run CSV output, JSON output, bounded sample duration, and
custom race configuration so regressions in the standalone debugging path are
caught with the core tests.

Build from the repository root:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-build
cmake --build /private/tmp/torcs-bridge-build
ctest --test-dir /private/tmp/torcs-bridge-build --output-on-failure
```

CMake also runs a retained TORCS core preflight. On systems without PLIB
headers/libraries, the current stub targets still build, but retained-core
targets stay disabled. To require the retained core dependency check:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-build -DTORCS_BRIDGE_ENABLE_RETAINED_CORE=ON
```

If PLIB is installed in a nonstandard location, pass
`TORCS_BRIDGE_PLIB_INCLUDE_DIR`, `TORCS_BRIDGE_PLIB_SG_LIBRARY`, and
`TORCS_BRIDGE_PLIB_UL_LIBRARY`. On Windows, the preflight checks the bundled
`src/windows/include` and `src/windows/lib` or `src/windows/lib64` PLIB paths
before falling back to system search paths.

When that option and preflight both pass, CMake adds a build-only
`torcs_retained_core` target. It compiles `TorcsRetainedAdapter` plus the
retained TORCS source subset from `tgf`, `txml`, `robottools`, `track`,
`simuv2`, and SOLID so missing headers and legacy compile issues are isolated
before the public bridge behavior changes. The adapter initializes TGF paths,
loads one TORCS track through `TrackBuildv1`, copies track length/width and
sampled segment center/border points into `TorcsBridgeTrackSnapshot`, loads and
merges the car model/category XML, builds a one-car `tRmInfo`/`tSituation`,
calls `SimInit`/`SimConfig`, and extracts the initial car snapshot from
`tCarElt`. It is not used by the current smoke scene.

When PLIB can also link required `sg` symbols, the retained-core CTest set
includes `torcs_retained_smoke`, which links the retained adapter executable
path and verifies `wheel-2` track loading at runtime plus `car1-trb1`
simulator-backed car setup. Empty scratch archives are only enough for static
`torcs_retained_core` compile checks, so CMake leaves the smoke target disabled
if the PLIB files are present but the `sg` link probe fails.

CMake also runs a GDExtension dependency preflight. On systems without
`godot-cpp`, the current harness and tests still build, but the future
GDExtension target stays disabled. To require the binding dependency check:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-build -DTORCS_BRIDGE_ENABLE_GDEXTENSION=ON
```

If `godot-cpp` is installed or built in a nonstandard location, pass
`TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR`,
`TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR`, and
`TORCS_BRIDGE_GODOT_CPP_LIBRARY`.

Run a short harness sample:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --format json
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --track-xml data/tracks/road/wheel-2/wheel-2.xml --car-xml data/cars/models/car1-trb1/car1-trb1.xml --car-id car1-trb1 --laps 0
```

Snapshots include car transform, TORCS/Godot linear and angular velocity,
TORCS/Godot yaw, controls, wheel state, and a `TorcsBridgeTrackSnapshot` with
debug centerline and left/right road border points. The default bridge harness
still emits a deterministic straight placeholder; the retained adapter already
copies debug points from TORCS physics track segments when
`torcs_retained_core` is enabled.

The harness bounds its final step to the requested `--seconds` duration. For
example, `--seconds 0.01 --sample-seconds 0.02` emits one sample at 0.010000
seconds instead of advancing a full 0.02 seconds.

Each `step(seconds)` call consumes at most 50 fixed 0.002s substeps. Extra time
stays queued in the accumulator so a single slow Godot frame cannot run an
unbounded catch-up loop.

The current public bridge intentionally uses generated placeholder motion. The
next retained-core milestone is mapping `TorcsBridgeInputState` into `tCarCtrl`,
stepping with `SimUpdate`, and refreshing `TorcsBridgeCarSnapshot` from
`tCarElt` while preserving the public snapshot/input boundary.

See `GDEXTENSION_PLAN.md` for the Godot C++ binding dependency plan and the
thin binding shape to add once `godot-cpp` is available.

See `TORCS_CORE_SURVEY.md` for the retained TORCS track/sim integration path
and the recommended direct-call adapter before linking the full race engine.

The Godot fallback snapshot contract can be checked from the repository root:

```bash
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_smoke_check.gd
```

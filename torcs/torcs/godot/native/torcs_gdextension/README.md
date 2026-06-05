# TORCS Native Bridge

This native subtree will host two targets that share the same bridge core:

- `torcs_bridge_harness`: command-line executable for deterministic scripted
  stepping outside Godot.
- `torcs_gdextension`: Godot GDExtension library that exposes the same retained
  TORCS core through a Godot-friendly API.

The first implementation keeps TORCS structs internal and exports snapshots,
input values, and lifecycle methods. The dependency-free bridge core avoids
legacy graphics, audio, GLUT, PLIB, SSG, and `tgfclient` dependencies; retained
prototype targets add the TORCS track/simuv2 and PLIB dependencies behind CMake
gates.

## Current Targets

- `torcs_bridge_core`: static C++17 DTO/lifecycle/fixed-step bridge core.
- `torcs_bridge_harness`: command-line deterministic CSV/JSON snapshot harness
  with `--backend stub|retained`.
- `torcs_bridge_tests`: dependency-free CTest regression checks.
- `torcs_retained_smoke`: retained-core-only smoke executable that loads
  `wheel-2` through `TorcsRetainedAdapter` when retained dependencies are
  enabled.
- `torcs_gdextension`: opt-in Godot 4.6-compatible GDExtension library, built
  only when `TORCS_BRIDGE_ENABLE_GDEXTENSION=ON`, `godot-cpp` is supplied, and
  the retained simuv2 backend is linkable.

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
`TORCS_BRIDGE_PLIB_UL_LIBRARY`. The preflight checks the repo-local
`.cache/plib-1.8.5` source install first. On Windows, it also checks the
bundled `src/windows/include` and `src/windows/lib` or `src/windows/lib64`
PLIB paths before falling back to system search paths.

When that option and preflight both pass, CMake adds a build-only
`torcs_retained_core` target. It compiles `TorcsRetainedAdapter` plus the
retained TORCS source subset from `tgf`, `txml`, `robottools`, `track`,
`simuv2`, and SOLID so missing headers and legacy compile issues are isolated
before the public bridge behavior changes. The adapter initializes TGF paths,
loads one TORCS track through `TrackBuildv1`, copies track length/width and
sampled segment center/border points into `TorcsBridgeTrackSnapshot`, loads and
merges the car model/category XML, builds a one-car `tRmInfo`/`tSituation`,
calls `SimInit`/`SimConfig`, and extracts the initial car snapshot from
`tCarElt`. Retained `step(seconds)` maps bridge input into `tCarCtrl`, advances
bounded `SimUpdate` substeps, and refreshes the bridge snapshot from `tCarElt`.
It is not used by the current smoke scene.

When PLIB can also link required `sg` symbols, the retained-core CTest set
includes `torcs_retained_smoke` and retained harness tests. The smoke links the
retained adapter executable path and verifies `wheel-2` track loading at runtime
plus `car1-trb1` simulator-backed setup, throttle acceleration, braking after
acceleration, steering/yaw movement, and deterministic repeated snapshots. The
harness tests run `--backend retained` through the same CSV/JSON schema as the
stub backend and byte-compare repeated CSV runs. Empty scratch archives are only
enough for static `torcs_retained_core` compile checks, so CMake leaves the
smoke and retained harness targets disabled if the PLIB files are present but
the `sg` link probe fails.

CMake also runs a GDExtension dependency preflight. On systems without
`godot-cpp`, the current harness and tests still build, but the GDExtension
target stays disabled. To build the native Godot bridge, use official
`godot-cpp` headers/library compatible with Godot 4.6.x and enable both the
retained backend and the GDExtension target:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-native -DTORCS_BRIDGE_ENABLE_RETAINED_CORE=ON -DTORCS_BRIDGE_ENABLE_GDEXTENSION=ON -DTORCS_BRIDGE_GODOT_CPP_ROOT=/path/to/godot-cpp
cmake --build /private/tmp/torcs-bridge-native
ctest --test-dir /private/tmp/torcs-bridge-native --output-on-failure
```

If `godot-cpp` is installed or built in a nonstandard location, pass
`TORCS_BRIDGE_GODOT_CPP_ROOT` or the explicit
`TORCS_BRIDGE_GODOT_CPP_INCLUDE_DIR`,
`TORCS_BRIDGE_GODOT_CPP_GEN_INCLUDE_DIR`, and
`TORCS_BRIDGE_GODOT_CPP_LIBRARY`. The source tree does not vendor generated
`godot-cpp` artifacts. When the native target is enabled, CMake writes
`godot/bin/torcs_gdextension.gdextension` and the native library into the Godot
project `bin` directory. The descriptor is generated during the native build so
default fallback-only Godot startup does not try to load a missing library.

`TorcsBridgeNative` is the initial GDExtension class. It exposes the same
`initialize`, `load`, `set_human_input`, `step`, `get_snapshot`, and `shutdown`
methods as `scripts/torcs_bridge_fallback.gd`, returning the same dictionary
keys with Godot `Vector3` values for vector fields.

Run a short harness sample:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04
/private/tmp/torcs-bridge-build/torcs_bridge_harness --backend stub --seconds 0.04
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --format json
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --track-xml data/tracks/road/wheel-2/wheel-2.xml --car-xml data/cars/models/car1-trb1/car1-trb1.xml --car-id car1-trb1 --laps 0
```

In a retained-linkable build, add `--backend retained` to run the same harness
through retained TORCS track loading and `simuv2` stepping. In dependency-free
or retained-unlinkable builds, requesting `--backend retained` exits with an
explicit unavailable-backend error instead of silently falling back to the stub.

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

The Godot smoke scene tries `TorcsBridgeNative` first via `ClassDB` and falls
back to `scripts/torcs_bridge_fallback.gd` when the native class is unavailable
or cannot load. It logs the active backend as `native` or `fallback`.

Native-vs-retained parity is registered as CTest
`torcs_bridge_native_retained_parity` only in native-enabled builds where Godot
and Python are available. It runs:

```bash
torcs_bridge_harness --backend retained --seconds 0.2 --sample-seconds 0.02 --format json
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_native_capture.gd -- --seconds 0.2 --sample-seconds 0.02
```

The comparator checks every sample for time, substeps, TORCS/Godot position,
TORCS/Godot yaw, speed, RPM, gear, and controls. Default tolerances are
`1e-6` seconds, `1e-4` position/speed, `1e-5` yaw, `1e-2` RPM, and `1e-6`
controls. If native Godot output diverges, treat the retained harness JSON as
the source of truth and compare the generated parity files under the CTest
`native-retained-parity` output directory.

See `GDEXTENSION_PLAN.md` for the Godot C++ binding dependency plan and the
thin binding shape.

See `TORCS_CORE_SURVEY.md` for the retained TORCS track/sim integration path
and the recommended direct-call adapter before linking the full race engine.

The Godot fallback snapshot contract can be checked from the repository root:

```bash
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_smoke_check.gd
```

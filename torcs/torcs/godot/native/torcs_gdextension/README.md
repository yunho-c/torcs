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
`TORCS_BRIDGE_PLIB_UL_LIBRARY`.

When that option and preflight both pass, CMake adds a disabled-prototype
`torcs_retained_core` target containing `TorcsRetainedAdapter`. It is a
lifecycle skeleton for the future one-car direct TORCS adapter and is not used
by the current smoke scene.

Run a short harness sample:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --format json
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04 --track-xml data/tracks/road/wheel-2/wheel-2.xml --car-xml data/cars/models/car1-trb1/car1-trb1.xml --car-id car1-trb1 --laps 0
```

Snapshots include car state plus a `TorcsBridgeTrackSnapshot` with debug
centerline and left/right road border points. The current track data is a
deterministic straight placeholder; retained-core work should replace it with
points generated from TORCS physics track segments.

The harness bounds its final step to the requested `--seconds` duration. For
example, `--seconds 0.01 --sample-seconds 0.02` emits one sample at 0.010000
seconds instead of advancing a full 0.02 seconds.

Each `step(seconds)` call consumes at most 50 fixed 0.002s substeps. Extra time
stays queued in the accumulator so a single slow Godot frame cannot run an
unbounded catch-up loop.

The current bridge intentionally uses generated placeholder motion. The next
native milestone is replacing `TorcsRace::load()` and `TorcsRace::step()` with
retained TORCS track loading, car setup, and `simuv2` stepping while preserving
the public snapshot/input boundary.

See `GDEXTENSION_PLAN.md` for the Godot C++ binding dependency plan and the
thin binding shape to add once `godot-cpp` is available.

See `TORCS_CORE_SURVEY.md` for the retained TORCS track/sim integration path
and the recommended direct-call adapter before linking the full race engine.

The Godot fallback snapshot contract can be checked from the repository root:

```bash
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --script res://scripts/bridge_smoke_check.gd
```

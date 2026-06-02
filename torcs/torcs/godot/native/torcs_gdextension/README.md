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

Build from the repository root:

```bash
cmake -S torcs/torcs/godot/native/torcs_gdextension -B /private/tmp/torcs-bridge-build
cmake --build /private/tmp/torcs-bridge-build
ctest --test-dir /private/tmp/torcs-bridge-build --output-on-failure
```

Run a short harness sample:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 0.04
```

The current bridge intentionally uses generated placeholder motion. The next
native milestone is replacing `TorcsRace::load()` and `TorcsRace::step()` with
retained TORCS track loading, car setup, and `simuv2` stepping while preserving
the public snapshot/input boundary.

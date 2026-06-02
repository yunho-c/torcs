# GDExtension Binding Plan

The current bridge builds and tests without Godot headers. That is intentional:
the native harness should remain usable even when the Godot editor, extension
reloads, or export packaging fail.

## Current Dependency State

- Godot executable available locally: 4.6.3 stable.
- `godot-cpp` headers/library are not present in this repository or under the
  local Homebrew prefix.
- The repository has no network-dependent dependency bootstrap yet.
- CMake now has a `TORCS_BRIDGE_ENABLE_GDEXTENSION` preflight option. Default
  stub builds continue without `godot-cpp`; explicit GDExtension enable fails
  early until the include, generated include, and library paths are provided.

Do not add a non-compiling `.gdextension` resource or source file until the
Godot C++ binding dependency is available. A `.gdextension` file that points to
a missing library makes project startup noisier and weakens the current smoke
scene fallback.

## Preferred Binding Dependency

Use `godot-cpp` matching the target Godot minor version:

- Godot target: 4.6.x for current local development.
- Binding source: official `godot-cpp` release/branch compatible with Godot
  4.6.
- Build system: keep CMake for `torcs_bridge_core`; either integrate
  `godot-cpp` through CMake or add a narrowly scoped SCons step only for the
  binding library.

Avoid vendoring large generated artifacts unless needed for offline builds. If
vendoring is chosen, place it under a clear third-party path and document the
version and license.

## Binding Shape

Keep these layers separate:

```text
torcs_bridge_core
  Pure C++ DTOs, lifecycle, fixed-step logic, retained TORCS integration.

torcs_bridge_harness
  CLI executable linked to torcs_bridge_core.

torcs_bridge_tests
  Native CTest executable linked to torcs_bridge_core.

torcs_gdextension
  Thin Godot binding linked to torcs_bridge_core and godot-cpp.
```

Initial Godot classes:

- `TorcsRuntimeExtension`: wraps `TorcsRuntime`.
- `TorcsRaceExtension`: wraps `TorcsRace`.

Initial methods:

- `initialize(data_root: String, local_root: String, library_root: String) -> bool`
- `shutdown() -> void`
- `load(config: Dictionary) -> bool`
- `set_human_input(car_index: int, input: Dictionary) -> void`
- `step(seconds: float) -> Dictionary`
- `get_snapshot() -> Dictionary`

The returned snapshot should match the current fallback dictionary keys so
`scripts/bridge_smoke.gd` can switch from fallback to native with minimal
changes.

## Files To Add Once Dependency Exists

```text
native/torcs_gdextension/
  src/godot/
    torcs_runtime_extension.cpp
    torcs_runtime_extension.h
    torcs_race_extension.cpp
    torcs_race_extension.h
    register_types.cpp
    register_types.h
  bin/torcs_gdextension.gdextension
```

The `.gdextension` file should set:

- `entry_symbol` to the registration function from `register_types.cpp`.
- `compatibility_minimum` to the minimum supported Godot 4.x version.
- per-platform library paths for macOS, Linux, and Windows.
- `reloadable = true` during development.

## Acceptance Criteria

- `cmake --build` still builds `torcs_bridge_core`, `torcs_bridge_harness`, and
  `torcs_bridge_tests` without launching Godot.
- The GDExtension target compiles only when its dependency is present or an
  explicit enable option is set.
- The Godot smoke scene can choose native binding if available and fallback
  otherwise.
- Native snapshots from Godot match harness snapshots for the same scripted
  input and sample period.

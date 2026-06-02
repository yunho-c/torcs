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
  future GDExtension library.
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

The current native bridge is a deterministic stub. It does not yet load TORCS
`simuv2`, the track module, or car XML into the simulation core.

Run the harness:

```bash
/private/tmp/torcs-bridge-build/torcs_bridge_harness --seconds 10 --output /private/tmp/torcs-bridge.csv
```

The harness emits CSV snapshots with time, substep count, TORCS/Godot
positions, yaw, speed, RPM, gear, input values, damage, and skid.

## Godot Smoke Scene

Run the current smoke scene:

```bash
godot --path torcs/torcs/godot
```

In this sandboxed macOS environment, Godot cannot write to the normal user data
directory. Use a temporary home for headless verification:

```bash
mkdir -p /private/tmp/torcs-godot-home
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --check-only --script res://scripts/bridge_smoke.gd
HOME=/private/tmp/torcs-godot-home godot --headless --path torcs/torcs/godot --quit-after 3
```

The smoke scene currently uses `scripts/torcs_bridge_fallback.gd`, which mirrors
the native stub's DTOs and fixed-step behavior until a real GDExtension binding
is added. It also renders the bridge snapshot's track debug centerline and road
borders so future retained `tTrackSeg` output can be checked against Godot
coordinates early.

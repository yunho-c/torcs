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

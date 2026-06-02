# TORCS Native Bridge

This native subtree will host two targets that share the same bridge core:

- `torcs_bridge_harness`: command-line executable for deterministic scripted
  stepping outside Godot.
- `torcs_gdextension`: Godot GDExtension library that exposes the same retained
  TORCS core through a Godot-friendly API.

The first implementation keeps TORCS structs internal and exports snapshots,
input values, and lifecycle methods. The bridge should avoid legacy graphics,
audio, GLUT, PLIB, SSG, and `tgfclient` dependencies.

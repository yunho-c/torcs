# TORCS Godot Port Charter

This charter locks the first implementation scope for the TORCS-to-Godot port
described in `godot-port-strategy.md`. It is intentionally narrow: prove the
retained TORCS simulation in a Godot-owned runtime before replacing physics,
race rules, AI, or media pipelines.

## Initial Product Scope

- Target engine: Godot 4.x stable, using GDExtension for native integration.
- Target platform: desktop development builds first.
- Distribution assumption: GPL-compatible distribution while TORCS-derived code
  remains linked into the shipped runtime.
- Initial mode: free drive, with one human-controlled car.
- Initial simulator: retain `src/modules/simu/simuv2`.
- Initial presentation: generated debug road mesh, placeholder car, simple
  camera, and telemetry display.
- Initial controls: keyboard input mapped to a small bridge input DTO.
- Initial media policy: avoid carrying legacy graphics/audio code; convert
  legacy assets only after physics/render alignment is proven.

## First Car And Track

- Track: `data/tracks/road/wheel-2/wheel-2.xml`.
- Car: `data/cars/models/car1-trb1/car1-trb1.xml`.

`wheel-2` is a short road track with conventional XML, AC, and ACC assets. The
chosen car is in a common TORCS category and avoids the README-noted non-free
`pw-*` and `kc-*` artwork families.

## Architecture Commitments

- Keep the original TORCS source buildable and minimally modified.
- Place Godot-specific work under `godot/`, separate from legacy runtime code.
- Use a native command-line harness before depending on editor-side behavior.
- Share bridge core code between the command-line harness and GDExtension.
- Keep TORCS structs internal to native code; expose snapshots and input values
  at the Godot boundary.
- Run TORCS simulation synchronously from Godot physics ticks at `0.002` second
  substeps.
- Centralize TORCS Z-up to Godot Y-up conversion in one helper and test it.
- Support one race session at a time until global TORCS state is fully audited.

## Phase 1 Exit Criteria

- A command-line harness can initialize the bridge skeleton, run a deterministic
  scripted control sequence, and write snapshot rows.
- Snapshot rows include time, position, rotation/yaw, speed, RPM, gear, damage,
  and key control values.
- The harness has no dependency on TORCS graphics, `tgfclient`, PLIB, GLUT, SSG,
  or legacy OpenGL rendering.

## Phase 2 Exit Criteria

- A Godot project can load or gracefully fall back around the native bridge.
- A smoke scene steps the bridge from `_physics_process(delta)`.
- The scene displays speed/RPM and can run for at least 60 seconds.
- Snapshot values can be compared with the native harness for the same scripted
  input sequence.

## Deferred Until After The Prototype

- Full TORCS race manager UI parity.
- Dynamic loading of arbitrary robot modules.
- Real track and car conversion.
- Audio rigs.
- Multiplayer and full race rules.
- Custom Godot physics server integration.
- Any rewrite of TORCS vehicle dynamics in Godot physics.

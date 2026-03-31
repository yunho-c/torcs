---
name: torcs-track-authoring
description: Author and iterate TORCS v4 tracks safely (XML, trackgen, accc, raceline, and AC3D/Blender alignment).
---

## When to load this skill

Load this skill whenever a task involves:

- creating or editing TORCS v4 track XML
- tuning pits, side/border/barrier behavior, banking, or interpolation
- running `trackgen` / `accc` workflows
- adding raceline or shade layers
- editing generated `.ac` in AC3D/Blender and preserving alignment

## Core model (must be preserved)

- XML-generated track geometry is used by simulation/collision/AI.
- `Graphic/3d description` (`.ac` / `.acc`) is visual geometry.
- They align only because both use the same coordinates.

If generated road geometry is moved/rotated/scaled in AC3D/Blender, visuals and
physics drift apart.

## Coordinate and axis facts

### Origin meaning

- TORCS normalizes generated track geometry to a coordinate origin at the minimum
  corner of the generated track-geometry bounding box (`min x`, `min y`, `min z`).
- This is not start-line anchored and not terrain anchored.
- Barrier `width` is barrier metadata and does not expand this origin box.

### AC3D -> TORCS axis mapping

When loading AC3D geometry:

- `x_torcs = x_ac3d`
- `y_torcs = -z_ac3d`
- `z_torcs = y_ac3d`

Practical reading: AC3D `Y` up becomes TORCS `Z` up.

## v4 loader state/fallback behavior (important)

The loader is stateful and processes segments in order.

Do not assume one global fallback rule for all fields. Behavior is a mix of:

- segment value when present
- previous segment state (for many fields)
- Main Track/global defaults (for specific fields)

Key chains used frequently:

- main segment `surface`: carries over from previous segment; initial from `Main Track/surface`
- side widths: `start width` -> `width` -> `end width`, where omitted values cascade
- side/border/barrier style/surface often carry over from previous state unless reset
- `profil steps length` falls back to `Main Track/profil steps length`

Elevation precedence inside each segment:

- `z start` overrides side-specific starts
- `z end` overrides side-specific ends
- if `z end` missing and `grade` present, end center height is derived from grade
- banking can recompute left/right heights from center height and banking angle

Authoring rule: set key fields explicitly at transition segments (pit entry/start/end/exit,
first turn after pit exit) to avoid accidental carry-over.

## Practical pit defaults (validated starter setup)

Use this as a reliable baseline for road tracks:

- Put pits on straight helper segments only (`entry`, `start`, `end`, `exit` all straights).
- Typical default target is `20` pits.
- Common slot length is `15 m` (matches classic examples like e-track-2 style).
- Pit lane side width (without wall thickness) around `14.5 m` is a practical starter.
- Pit wall border width around `0.5 m` is a common value.

Recommended pit-segment pattern:

1. `pit_entry` (taper out)
2. `pit_main` (constant pit width)
3. `pit_exit` (taper in)

Surface/style recommendations from the validated manual example:

- `pit_entry` / `pit_exit`: side surface `asphalt`, border style `plan` (flat, no curb)
- `pit_main`: side surface `asphalt-pits`, border style `wall`
- non-pit sections: explicitly reset back to normal road-side surfaces (for example grass)

Why no curb in pit entry/exit:

- cars must cross between racing line and pit lane there
- curb edges in crossover sections create unnecessary visual/physical discontinuity

Pit count sizing rule of thumb:

- `nPits ~= pit_main_length / pit_length`
- example baseline: `pit_main = 300 m`, `pit_length = 15 m` -> `20` pits

## Canonical command workflow

Use `workdir` at the TORCS runtime tree when generating installed tracks.

### Baseline generation

```bash
trackgen -c <category> -n <name> -a
```

### Raceline generation

```bash
trackgen -c <category> -n <name> -r
```

### ACC merge (raceline only)

```bash
accc -g tracks/<category>/<name>/<name>.acc \
  -l0 tracks/<category>/<name>/<name>.ac \
  -l1 tracks/<category>/<name>/<name>-trk-raceline.ac \
  -d3 1000 -d2 500 -d1 300 -S 300 -es
```

After merge, set XML to load the merged model:

```xml
<attstr name="3d description" val="<name>.acc"/>
```

### ACC merge (with baked shadow)

```bash
accc -g tracks/<category>/<name>/<name>.acc \
  -l0 tracks/<category>/<name>/<name>.ac \
  -l1 tracks/<category>/<name>/<name>-shade.ac \
  -l2 tracks/<category>/<name>/<name>-trk-raceline.ac \
  -d3 1000 -d2 500 -d1 300 -S 300 -es
```

## Stripe dependency for `accc -g`

- `accc -g` may require external `stripe` in PATH.
- If missing, merge can fail (`stripe: command not found`).
- Windows binary installer workflow includes `stripe`.

Archived source downloads:

- https://web.archive.org/web/20131004102243/http://www.cs.sunysb.edu/~stripe/stripe.tar.gz
- https://web.archive.org/web/20131004102245/http://www.cs.sunysb.edu/~stripe/stripe.zip

## Validation checklist after edits

1. Run `trackgen -a` and verify closure deltas are near zero (`Delta X/Y/Z/Ang`).
2. Verify expected pit count in output (`pits = ...`).
3. Run raceline/shadow merge and confirm output `.acc` has non-zero size.
4. In-game: confirm visible road edges match drivable/collision edges.
5. Verify pits and cameras still align with visible geometry.

## Troubleshooting map

- Large closure deltas (`Delta X`, `Delta Y`, `Delta Z`, or `Delta Ang`):
  track loop mismatch (segment lengths/arcs/radii/elevation/banking transitions inconsistent).
- Pit boxes look mis-scaled: pit length/spacing mismatch (`Pits/length` vs pit-zone length).
- Crash or broken result after `accc -g`: check layer order (`-l1` vs `-l2`) and `stripe` availability.
- Missing textures: ensure texture names exist in track folder or global texture search paths.

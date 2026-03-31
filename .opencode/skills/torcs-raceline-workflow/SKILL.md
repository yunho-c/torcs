---
name: torcs-raceline-workflow
description: Generate and integrate TORCS raceline layers safely with trackgen/accc, including layer ordering and stripe dependency handling.
---

## When to load this skill

Load this skill whenever a task involves:

- adding or tuning raceline rendering for a track
- using `trackgen -r`
- merging `.ac` layers into final `.acc` with `accc -g`
- debugging raceline merge crashes or empty output files

## Preconditions

1. Track XML references a surface with `raceline name` set (for example `raceline.png`).
2. Raceline texture is available in the track folder or in global `data/textures`.
3. `3d description` points to the final merged output (`.acc`) for normal in-game use.

## Canonical raceline pipeline

Run in TORCS runtime root (where `tracks/...` resolves correctly).

```bash
# 1) Base geometry
trackgen -c <category> -n <name> -a

# 2) Raceline geometry layer
trackgen -c <category> -n <name> -r

# 3) Merge for in-game use
accc -g tracks/<category>/<name>/<name>.acc \
  -l0 tracks/<category>/<name>/<name>.ac \
  -l1 tracks/<category>/<name>/<name>-trk-raceline.ac \
  -d3 1000 -d2 500 -d1 300 -S 300 -es
```

If also using baked shadows:

```bash
accc -g tracks/<category>/<name>/<name>.acc \
  -l0 tracks/<category>/<name>/<name>.ac \
  -l1 tracks/<category>/<name>/<name>-shade.ac \
  -l2 tracks/<category>/<name>/<name>-trk-raceline.ac \
  -d3 1000 -d2 500 -d1 300 -S 300 -es
```

## Raceline shaping keys

Set these in `Main Track` as needed:

- `raceline widthscale`
- `raceline int`
- `raceline ext`

They are consumed by `trackgen -r` generation logic.

## Stripe dependency (`accc -g`)

- `accc -g` may call external `stripe`.
- If `stripe` is missing, merges can fail or produce broken output.

Quick check:

```bash
command -v stripe
```

Archived download links:

- https://web.archive.org/web/20131004102243/http://www.cs.sunysb.edu/~stripe/stripe.tar.gz
- https://web.archive.org/web/20131004102245/http://www.cs.sunysb.edu/~stripe/stripe.zip

## Verification checklist

1. `trackgen -r` creates `<name>-trk-raceline.ac` with non-zero size.
2. `accc -g` exits successfully.
3. Final `<name>.acc` exists and has non-zero size.
4. XML `Graphic/3d description` points to `<name>.acc`.
5. In-game raceline is visible and aligned with road.

## Common failure patterns

- Raceline file exists but not visible: wrong `3d description` target or missing `raceline name` in active surface.
- Crash/broken merged file: wrong layer order or missing `stripe`.
- Empty/invalid merged output: inspect `accc` log for `stripe` errors and rerun after install.

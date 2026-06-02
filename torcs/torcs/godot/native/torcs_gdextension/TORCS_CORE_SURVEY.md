# Retained TORCS Core Integration Survey

This survey identifies the first real TORCS code path to replace the current
placeholder bridge motion. It focuses on one human-controlled car on one track,
matching the Phase 0 charter.

## Recommendation

Do not link `src/libs/raceengineclient` into the first bridge harness. The race
engine initialization path includes `tgfclient`, racescreens, music, graphics
module loading, and UI hooks even in areas close to console mode.

Instead, build a smaller retained-core adapter that calls the track and sim
entry points directly:

```text
GfInit / SetDataDir / SetLocalDir / SetLibDir
TrackBuildv1
GfParmReadFile car/category XML
GfParmMergeHandles category + car + optional setup
SimInit
SimConfig
write tCarCtrl from TorcsBridgeInputState
SimUpdate at 0.002s
copy tCarElt into TorcsBridgeSnapshot
SimShutdown
TrackShutdown
GfParmReleaseHandle
```

This preserves the physics contract while avoiding graphics, sound, UI, and
dynamic robot loading in the first native milestone.

## Direct Entry Points

Track module:

- `src/modules/track/track.cpp`
  - `TrackBuildv1(char *trackfile)`
  - `TrackShutdown()`
- `src/modules/track/trackinc.h`
  - internal declarations for track builder and coordinate helpers.
- `src/modules/track/trackitf.cpp`
  - wraps direct functions in `tTrackItf`; useful reference, not required if
    linked directly.

Simulation module:

- `src/modules/simu/simuv2/sim.h`
  - `SimInit(int nbcars, tTrack *track, tdble fuelFactor, tdble damageFactor, tdble tireFactor)`
  - `SimConfig(tCarElt *carElt, RmInfo *info)`
  - `SimUpdate(tSituation *s, double deltaTime, int telemetry)`
  - `SimShutdown()`
- `src/modules/simu/simuv2/simuitf.cpp`
  - wraps direct functions in `tSimItf`; useful reference, not required if
    linked directly.

Runtime paths:

- `src/libs/tgf/tgf.cpp`
  - `GfInit()`
  - `SetDataDir(char*)`, `SetLocalDir(char*)`, `SetLibDir(char*)`
  - corresponding getters used by legacy setup code.

## Source Sets To Link

Minimal track source set, from existing Makefile/project:

```text
src/modules/track/track.cpp
src/modules/track/track3.cpp
src/modules/track/track4.cpp
src/modules/track/trackutil.cpp
```

Minimal simuv2 source set, from existing Makefile/project:

```text
src/modules/simu/simuv2/aero.cpp
src/modules/simu/simuv2/atmosphere.cpp
src/modules/simu/simuv2/axle.cpp
src/modules/simu/simuv2/brake.cpp
src/modules/simu/simuv2/car.cpp
src/modules/simu/simuv2/categories.cpp
src/modules/simu/simuv2/collide.cpp
src/modules/simu/simuv2/differential.cpp
src/modules/simu/simuv2/engine.cpp
src/modules/simu/simuv2/simu.cpp
src/modules/simu/simuv2/steer.cpp
src/modules/simu/simuv2/susp.cpp
src/modules/simu/simuv2/transmission.cpp
src/modules/simu/simuv2/wheel.cpp
```

The `simuitf.cpp` and `trackitf.cpp` module wrappers are optional if the bridge
calls direct functions.

Supporting libraries/source:

- `src/libs/tgf`: params, logging, path globals, module/hash/directory support.
- `src/libs/robottools/rttrack.cpp`: track coordinate and surface utilities used
  by simuv2 and starting-grid code.
- `src/modules/simu/simuv2/SOLID-2.0`: collision library required by simuv2.
- PLIB `sg`/`ul`: still required by interfaces and math calls such as
  `sgMakeCoordMat4`; this is not legacy graphics, but a math dependency to
  isolate later.
- `txml`: required by `tgf` parameter loading.

## Car Initialization Boundary

The first hard implementation step is replacing `ReInitCars()` with a one-car
adapter. Useful code locations:

- `src/libs/raceengineclient/raceinit.cpp`
  - car XML/category merge: around `ReInitCars()`.
  - starting grid positioning: `initStartingGrid()`.
  - track loading: `ReInitTrack()`.
- `src/interfaces/car.h`
  - `tCarElt`, `tCarCtrl`, public/private snapshot fields.
- `src/interfaces/raceman.h`
  - `tSituation`, `tRmInfo`, race state constants, fixed step constants.

For the first bridge adapter, do not load a robot module. Populate one
`tCarElt` manually:

- `index = 0`
- `_driverType = RM_DRV_HUMAN`
- `_carName = "car1-trb1"`
- `_category` from the car XML `category` field.
- `_carHandle` from category + car parameter merge.
- `_trkPos`, `_pos_X`, `_pos_Y`, `_pos_Z`, `_yaw` from a simplified copy of
  `initStartingGrid()` or an explicit start segment.
- `ctrl` from `TorcsBridgeInputState`.

Then create a minimal `tSituation` and `tRmInfo`:

- `s->_ncars = 1`
- `s->cars[0] = &car`
- `s->_raceState = 0` for free drive after initial setup.
- `s->_maxDammage` high enough to avoid early removal.
- `info.track = loadedTrack`
- `info.s = &s`
- `info.raceRules` factors all `1.0` except tire factor as chosen by charter.

Call order:

```text
SimInit(1, track, 1.0, 1.0, 0.0)
SimConfig(&car, &info)
loop:
  car.ctrl = mapped input
  situation.deltaTime = 0.002
  situation.currentTime += 0.002
  SimUpdate(&situation, 0.002, -1)
  snapshot car fields
```

## Snapshot Fields To Copy First

From `tCarElt`:

- `_pos_X`, `_pos_Y`, `_pos_Z`
- `_yaw`, `_roll`, `_pitch`
- `_speed_x`, `_speed_y`, `_speed_z`
- `_enginerpm`
- `_gear`
- `_fuel`
- `_dammage`
- `_skid[4]`
- `priv.collision`, `priv.simcollision`, `priv.collpos`
- wheel fields: `_wheelSpinVel(i)`, `_ride(i)`, `_brakeTemp(i)`,
  `_wheelSlipSide(i)`, `_wheelSlipAccel(i)`, `_wheelSeg(i)`

Keep TORCS memory ownership native-side and copy into `TorcsBridgeSnapshot`.

## Known Risks

- `TrackBuildv1()` and `TrackShutdown()` use static global track state, so the
  first bridge must remain one race at a time.
- `SimCarTable` is global in simuv2, so the first bridge must remain one race at
  a time.
- The retained core still needs PLIB math headers/libraries because `car.h` and
  simuv2 use `sg` types and functions.
- `GfParmReadFile()` path resolution depends on TORCS data/local roots. Set
  path globals before loading XML and prefer explicit paths during early
  debugging.
- `ReInitCars()` calls robot callbacks for setup merges. The first human-only
  adapter should skip driver setup handles until the base car/category merge is
  proven.

## Next Prototype Task

Keep iterating on the build-only `torcs_retained_core` CMake target until it
compiles cleanly on a machine with PLIB installed, then replace the
`TorcsRetainedAdapter` lifecycle skeleton with the direct-call sequence above.

The native CMake now includes a retained-core preflight. It reports missing
PLIB headers/libraries by default and fails early only when
`TORCS_BRIDGE_ENABLE_RETAINED_CORE=ON` is explicitly requested.

When the preflight passes, `torcs_retained_core` compiles
`TorcsRetainedAdapter` plus the retained TORCS source subset from `tgf`,
`robottools`, `track`, `simuv2`, and SOLID. The current adapter source is
intentionally a lifecycle placeholder; fill it with the direct-call sequence
above only after the retained-core target can compile against the required
PLIB/TORCS dependencies.

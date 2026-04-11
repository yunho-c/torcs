# AGENTS.md - TORCS (The Open Racing Car Simulator)

## Project Overview

TORCS is a GPL-licensed open-source racing car simulator written in C/C++.
The project root with all source code is in `torcs/torcs/` (double-nested).
Architecture is plugin-based: rendering, physics, track loading, and AI drivers
are dynamically loaded modules (shared libraries/DLLs).

Version: 1.3.9-test1. No CI/CD pipeline exists. No `.clang-format` or linting config.

## Repository Layout

```
torcs/torcs/
  src/interfaces/    # Public API headers (car.h, robot.h, track.h, simu.h, ...)
  src/libs/          # Shared libraries (tgf, tgfclient, txml, robottools, math, ...)
  src/modules/       # Runtime-loaded plugins (graphic/ssggraph, simu/simuv2, simu/simuv3, track, telemetry)
  src/drivers/       # AI robot drivers (berniw, bt, human, inferno, olethros, ...)
  src/tools/         # Standalone tools (accc, trackgen, nfs2ac, texmapper, ...)
  src/linux/         # Linux platform layer
  src/windows/       # Windows platform layer + pre-built dependencies
  data/              # Game data (cars, tracks)
  test/              # Unit tests (googletest) and trackgen regression tests
  export/            # Exported headers and libraries for module builds, never edit these files, edit the originals
  runtime/           # Windows runtime output (Release)
  runtimed/          # Windows runtime output (Debug)
  doc/               # Documentation, tutorials, man pages
```

## Build Commands

### Linux (GNU Autotools + Recursive Make)

```bash
cd torcs/torcs/
./configure              # --enable-debug for debug build, --help for all options
make                     # Build everything
make install             # Install binaries
make datainstall         # Install game data
make clean               # Clean build artifacts
make distclean           # Full clean including configure output
make doc                 # Generate Doxygen API docs
```

Compiler flags: `-Wall -fPIC -fno-strict-aliasing -O2`. Debug adds `-g -DDEBUG`.

### Windows (Visual Studio 2022)

- Solution file: `torcs/torcs/TORCS.sln`
- To populate torcs/torcs/export with updated headers, use `setup_win32.bat` or `setup_win32_debug.bat`
- Setup release: `setup_win32.bat` then `setup_win32-data-from-CVS.bat`
- Setup debug: `setup_win32_debug.bat` then `setup_win32-data-from-CVS_debug.bat`
- Open `TORCS.sln` and build `Win32-Release` or `Win32-Debug`
- x64 builds are supported; select `x64` platform in VS
- Runtime output: `runtime\wtorcs.exe` (Release), `runtimed\wtorcs.exe` (Debug)
- Configurations: Debug|Win32, Debug|x64, Release|Win32, Release|x64
- Platform toolset: v143. Target: 0 build warnings.
- Output directories: `runtime/` (Release), `runtimed/` (Debug).

**IMPORTANT - MSBuild from the command line:**

For detailed build and test commands, load the `build-torcs` skill.
For unit test implementation conventions (especially floating-point assertions
and robust `EXPECT_NEAR` usage), load the `torcs-test-writing` skill.
For v4 track XML authoring/tooling/alignment guidance, load the `torcs-track-authoring` skill.
For focused raceline generation and ACC merge workflows, load the `torcs-raceline-workflow` skill.

MSBuild is NOT in PATH. The full path is:
`C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe`

The Bash tool in OpenCode runs commands through `/usr/bin/bash`, even on Windows.
This causes problems with MSBuild because bash misinterprets `/p:` switches as
paths and mangles backslashes. **You MUST use `powershell -Command`** to invoke
MSBuild so that stdout/stderr is properly captured. Do NOT use `cmd /C` because
it swallows all output and you cannot verify build success or failure.

Use the `workdir` parameter to set the working directory to `torcs/torcs/`.
Use the exact command templates below (copy-paste, do not reformulate).
Use `timeout: 600000` for build commands.

**Build Release x64:**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Release /p:Platform=x64 2>&1"
```
workdir: `torcs/torcs/`

**Build Release Win32:**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Release /p:Platform=Win32 2>&1"
```
workdir: `torcs/torcs/`

**Build Debug x64:**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Debug /p:Platform=x64 2>&1"
```
workdir: `torcs/torcs/`

**Clean before build** (add `/t:Clean` as a separate step first):
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=Release /p:Platform=x64 2>&1"
```

**General pattern:**
```
powershell -Command "& '<full-path-to-MSBuild.exe>' <project-or-sln> /p:Configuration=<cfg> /p:Platform=<plat> 2>&1"
```
- The `& '...'` syntax invokes the executable with spaces in the path.
- The `2>&1` ensures stderr is also captured.
- Use `workdir` parameter instead of `cd`.
- Verify output ends with `0 Fehler` (0 errors). If output is empty or only
  shows a Windows version banner, the command failed silently.

### Running TORCS

```
torcs                          # Linux
runtime\wtorcs.exe             # Windows Release
runtimed\wtorcs.exe            # Windows Debug
```

Key CLI flags: `-s` disable multitexturing, `-r <config.xml>` run headless race,
`-d` run under gdb, `-g` run under Valgrind, `-k` keep modules loaded (Valgrind).

## Testing

### Unit Tests (Google Test 1.17.0)
- There is no `make test` target in the top-level Makefile
- GTest project: `test\libs\robottools_test\robottools_test.vcxproj` (Windows build)
- Run a single test: `robottools_test.exe --gtest_filter=Suite.TestName`
- Run a test group: `robottools_test.exe --gtest_filter=Suite.*`
- Trackgen regression data: `test/trackgen/generate.sh` (Linux shell script)

Built as standalone VS 2022 console executables (`robottools_test.vcxproj`).
C++ standard: C++17 for test projects. Tests link against `googletest.lib`,
`robottools.lib`, `tgf.lib`.

**IMPORTANT - Test binary output paths:** The test executable is output to
`x64\Release\robottools_test.exe` (or `Win32\Release\` etc.) relative to the
solution root (`torcs\torcs\`), NOT inside the test project subdirectory.

**Run all tests:** Call the binary directly from bash (do NOT use `cmd /C`,
which swallows stdout). Use `workdir` set to `torcs\torcs\`:
```
x64/Release/robottools_test.exe
```
workdir: `torcs/torcs/`

**Run a single test** (gtest filter):
```
x64/Release/robottools_test.exe --gtest_filter=RtTrackSideNormalGTest.StraightSegment_RightSideUsesRgtSideNormal
x64/Release/robottools_test.exe --gtest_filter=RtTrackSideNormalGCurveTest.*
```

### Trackgen Regression Tests

Located in `test/trackgen/`. Shell script `generate.sh` runs `trackgen` on all
tracks and compares output against reference files in `source/`.

### Manual Release Testing

Checklist in `doc/testing/release.txt` covers Valgrind, physics, graphics,
sound, input devices, multiplayer, all game modes.

## Code Style Guidelines

### File Headers

Every source file must have this header block followed by the GPL v2 license:
```c
/***************************************************************************

    file                 : filename.cpp
    created              : Date
    copyright            : (C) Year by Author Name
    email                : author@email

 ***************************************************************************/
```

### Naming Conventions

| Element                     | Convention              | Examples                                    |
|-----------------------------|-------------------------|---------------------------------------------|
| Public C API functions      | PrefixPascalCase        | `GfParmReadFile`, `RtTrackGetWidth`         |
| Internal/static functions   | camelCase               | `getFullName`, `addParam`, `initTrack`      |
| Classes                     | PascalCase              | `TrackDesc`, `MyCar`, `Pathfinder`          |
| Class methods               | camelCase               | `getCurrentPos`, `getSpeed`, `loadBehaviour`|
| Member variables            | camelCase (no prefix)   | `curCar`, `wheelbase`, `speedsqr`           |
| Typedef'd structs           | `t` prefix + PascalCase | `tCarElt`, `tTrack`, `tSituation`           |
| Function pointer typedefs   | `tf` prefix + PascalCase| `tfRbNewTrack`, `tfRbDrive`                 |
| Macros / constants          | ALL_CAPS                | `MAX_NAME_LEN`, `FRNT_RGT`                 |
| Section/attribute strings   | PREFIX_SECT/ATT/VAL_NAME| `ROB_SECT_ROBOTS`, `TRK_ATT_WIDTH`         |
| Template classes            | lowercase + `t` suffix  | `v3t<T>`, `v2t<T>`                          |
| Template typedefs           | lowercase concatenated  | `vec3f`, `vec3d`, `vec2f`                   |
| Global variables            | PascalCase              | `SimDeltaTime`, `SimCarTable`               |
| Core float type             | `tdble`                 | typedef for `float`                         |

### Formatting

- **Indentation:** Tabs (4-space equivalent) in core code.
- **Brace style:** K&R for functions (return type on own line, brace on next line):
  ```c
  void
  SimAeroConfig(tCar *car)
  {
      ...
  }
  ```
- **Control flow:** Opening brace on same line:
  ```c
  if (airSpeed > 10.0) {
      ...
  }
  for (i = 0; i < n; i++) {
      ...
  }
  ```
- **Header guards:** `#ifndef`/`#define`/`#endif` (not `#pragma once`).
  Preferred pattern: `_FILENAME_H_`.

### Includes

Order (no strict enforcement, follow existing file patterns):
1. Platform-specific (`#ifdef WIN32` blocks with `<windows.h>`)
2. C standard library (`<stdlib.h>`, `<stdio.h>`, `<math.h>`)
3. Project/framework headers (`<tgf.h>`, `<track.h>`, `<car.h>`)
4. Local module headers (`"sim.h"`, `"berniw.h"`)

System headers use `<>`, local headers use `""`.

### Error Handling

- Use TORCS logging macros: `GfOut` (info), `GfTrace` (debug), `GfError` (errors),
  `GfFatal` (fatal/abort).
- Return `0` for success, `-1` for error from C-style functions.
- Check `NULL` before dereferencing, use early returns.
- Use `FREEZ(ptr)` macro (free + nullify) instead of raw `free`.
- Memory: `malloc`/`calloc`/`free` for C-style code; `new`/`delete` for C++ classes.

### General Patterns

- No C++ namespaces used in the codebase.
- No STL in core code (custom linked lists via TAILQ macros); STL (`std::vector`,
  `std::map`, `std::string`) acceptable in newer additions.
- Modules export `extern "C"` entry points for dynamic loading.
- `void*` used for opaque parameter handles (XML config system).
- Doxygen comments use `/** @brief */`, `@param[in]`, `@param[out]`, `@return`,
  `@ingroup`, `@defgroup`.
- Warnings: Linux builds use `-Wall`. Windows suppresses C4244, C4305, C4996.
  Target is 0 build warnings.

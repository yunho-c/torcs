---
name: build-torcs
description: Build TORCS solution with MSBuild on Windows and run unit tests. Load this skill whenever you need to compile, clean, or test TORCS.
---

## MSBuild Invocation

MSBuild is NOT in PATH. The full path is:
`C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe`

**CRITICAL:** Use `powershell -Command` to invoke MSBuild. Do NOT use `cmd /C`
because it swallows all stdout/stderr and you cannot verify whether the build
succeeded or failed.

All MSBuild commands must use the Bash tool with `workdir` set to:
`torcs/torcs/`

Use `timeout: 600000` (10 minutes) for build commands since full rebuilds can
take a while.

## Build Templates

### Clean (any configuration)

```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=<CFG> /p:Platform=<PLAT> 2>&1"
```

### Build (any configuration)

```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=<CFG> /p:Platform=<PLAT> 2>&1"
```

Replace `<CFG>` with `Release` or `Debug`, and `<PLAT>` with `x64` or `Win32`.

### Common Combinations

**Release x64 (clean + build):**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=Release /p:Platform=x64 2>&1"
```
then:
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Release /p:Platform=x64 2>&1"
```

**Debug x64 (clean + build):**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=Debug /p:Platform=x64 2>&1"
```
then:
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Debug /p:Platform=x64 2>&1"
```

**Release Win32 (clean + build):**
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=Release /p:Platform=Win32 2>&1"
```
then:
```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Release /p:Platform=Win32 2>&1"
```

### Build a Single Project

```
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' <path-to-vcxproj> /p:Configuration=<CFG> /p:Platform=<PLAT> 2>&1"
```

## Running Unit Tests

Test binaries are output to `x64\Release\` (or `Win32\Release\`, `x64\Debug\`,
etc.) relative to the solution root, NOT inside the test project subdirectory.

Call test binaries directly from bash (do NOT use `cmd /C` or `powershell`).
Use `workdir` set to `torcs/torcs/`.

**Run all tests:**
```
x64/Release/robottools_test.exe
```

**Run a single test:**
```
x64/Release/robottools_test.exe --gtest_filter=SuiteName.TestName
```

**Run a test group:**
```
x64/Release/robottools_test.exe --gtest_filter=SuiteName.*
```

## Verifying Success

After every build, check the output for:
- Build succeeded: output ends with `0 Fehler` (0 errors) and a time
- Build failed: output contains `Fehler` count > 0, or specific error messages
- **Silent failure:** if output is empty or shows only the Windows version
  banner, the command did not execute properly. Retry with the correct
  `powershell -Command` invocation.

After every test run, check for:
- Tests passed: output contains `[  PASSED  ]` with the test count
- Tests failed: output contains `[  FAILED  ]` with failing test names

## Configurations

| Configuration   | Platform | Output directory | Executable           |
|-----------------|----------|------------------|----------------------|
| Release         | x64      | runtime/         | runtime/wtorcs.exe   |
| Debug           | x64      | runtimed/        | runtimed/wtorcs.exe  |
| Release         | Win32    | runtime/         | runtime/wtorcs.exe   |
| Debug           | Win32    | runtimed/        | runtimed/wtorcs.exe  |

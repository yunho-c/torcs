---
name: torcs-installer-for-windows-release
description: Build TORCS x64 release and prepare installer/windows/base from runtime plus doc, then generate the NSIS installer.
---

## When to load this skill

Load this skill whenever a task asks to prepare a Windows release installer payload
or rebuild the TORCS Windows installer from fresh runtime outputs.

This skill is specifically for:

- Release x64 build with MSBuild
- Refreshing `installer/windows/base`
- Keeping manual extras in base (`stripe.exe`, `trackeditor.bat`, `trackeditor/`)
- Building `torcs64.nsi`

## Required paths

- Solution root: `torcs/torcs/`
- Installer folder: `torcs/torcs/installer/windows/`
- Base payload: `torcs/torcs/installer/windows/base/`

## Canonical release workflow (in order)

### 1) Clean runtime and installer base payload

Use the helper script in `installer/windows/`.

```powershell
powershell -ExecutionPolicy Bypass -File .\clean_base_for_release.ps1
```

Behavior:

- Deletes and recreates `runtime/` for a truly clean release run
- Removes all generated payload currently in `base/`
- Preserves manual extras:
  - `stripe.exe`
  - `trackeditor.bat`
  - `trackeditor/`

Optional fast local mode (do not use for full clean release):

```powershell
powershell -ExecutionPolicy Bypass -File .\clean_base_for_release.ps1 -SkipRuntimeClean
```

Run with `workdir` set to `torcs/torcs/installer/windows/`.

### 2) Run setup scripts to repopulate runtime before build

```powershell
powershell -Command ".\setup_win32.bat"
```

then:

```powershell
powershell -Command ".\setup_win32-data-from-CVS.bat"
```

Run with `workdir` set to `torcs/torcs/`.

### 3) Build Release x64

MSBuild is not in PATH. Use the full executable path and PowerShell invocation.

Clean:

```powershell
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /t:Clean /p:Configuration=Release /p:Platform=x64 2>&1"
```

Build:

```powershell
powershell -Command "& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' TORCS.sln /p:Configuration=Release /p:Platform=x64 2>&1"
```

Run both commands with `workdir` set to `torcs/torcs/` and `timeout: 600000`.

### 4) Copy runtime and doc into installer base

Use the helper script in `installer/windows/`.

```powershell
powershell -ExecutionPolicy Bypass -File .\populate_base_from_runtime.ps1
```

Behavior:

- Copies everything from `runtime/` to `installer/windows/base/`
- Replaces `base/doc/` with `doc/`
- Keeps preserved manual extras untouched

Run with `workdir` set to `torcs/torcs/installer/windows/`.

### 5) Validate base payload coverage in torcs64.nsi

Run the checker to detect drift between `installer/windows/base` and `torcs64.nsi`:

```powershell
powershell -ExecutionPolicy Bypass -File .\validate_torcs64_payload.ps1
```

Behavior:

- Fails if a file exists in `base/` but is missing from `File` entries in `torcs64.nsi`
- Fails if a `File "base\..."` entry in `torcs64.nsi` no longer exists in `base/`
- Excludes `base/doc/` from coverage by default; use `-IncludeDoc` if needed
- Supports optional allowlist entries via `installer/windows/payload-ignore.txt`

Run with `workdir` set to `torcs/torcs/installer/windows/`.

### 6) Build installer

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" torcs64.nsi
```

Run with `workdir` set to `torcs/torcs/installer/windows/` and `timeout: 600000`.

Output:

- `torcs/torcs/installer/windows/torcs_<version>_setup.exe`

### 7) Verify installer artifact

Run this after NSIS completes to verify that output exists and has a fresh timestamp:

```powershell
$f = Get-Item "torcs_<version>_setup.exe"; Write-Output ("Path: " + $f.FullName); Write-Output ("SizeBytes: " + $f.Length); Write-Output ("LastWriteTime: " + $f.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))
```

Expected:

- File exists
- `SizeBytes` is greater than 0
- `LastWriteTime` matches the current build window

### 8) Optional checksum for release notes

If you need reproducible release metadata, generate a SHA256 hash:

```powershell
Get-FileHash "torcs_<version>_setup.exe" -Algorithm SHA256
```

## Validation checklist

After payload refresh, verify these files exist before running NSIS:

- `installer/windows/base/wtorcs.exe`
- `installer/windows/base/trackgen.exe`
- `installer/windows/base/doc/userman/how_to_drive.html`
- `installer/windows/base/stripe.exe`
- `installer/windows/base/trackeditor.bat`
- `powershell -ExecutionPolicy Bypass -File .\validate_torcs64_payload.ps1` passes

After NSIS build:

- Installer file exists
- Installer size is greater than 0
- Installer file timestamp is updated
- Optional: record SHA256 checksum for release notes
- Optional smoke test: `torcs_<version>_setup.exe /S`

## Notes

- This skill targets `torcs64.nsi` as the release installer script.
- Do not rename product display names for end users.
- Keep old installer scripts unless explicitly requested otherwise.

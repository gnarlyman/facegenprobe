# Building StormLog

`StormLog.dll` is a 32-bit OBSE plugin for Oblivion (xOBSE 22 SDK). It builds with
MSBuild against the Visual Studio v143 toolset. Win32 only — there is no x64 config.

## Prerequisites

- **Visual Studio 2022** (Community is fine) with the *Desktop development with C++*
  workload. The projects pin `PlatformToolset = v143` and
  `WindowsTargetPlatformVersion = 10.0`.
  - Local MSBuild: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
  - (The design spec references "VS 2026"; the actual working toolchain on this
    machine is VS 2022 + v143. Either works as long as v143 is installed.)
- The sibling checkouts under `D:\Modlists\_clones\`:
  - `xOBSE\` — provides the OBSE/common SDK and the Detours sources
  - `obse\` — the include tree the projects compile against (`obse\obse`, `obse\obse_common`)
  - `Blockhead\` — only needed if you ever regenerate the vcxproj from its template

## Dependency layout

The project files use relative paths off `$(SolutionDir)` (= `_clones\StormLog\`),
so the surrounding `_clones\` tree must be intact:

| Project reference | Resolves to | Provided by |
|---|---|---|
| `$(SolutionDir)\Detours` | `_clones\StormLog\Detours` | **junction** → `_clones\xOBSE\obse\Detours` |
| `$(SolutionDir)\..\common` | `_clones\common` | junction → `_clones\xOBSE\common` |
| `$(SolutionDir)\..\obse\obse` | `_clones\obse\obse` | `_clones\obse\` checkout |
| `$(SolutionDir)\..\obse` | `_clones\obse` | `_clones\obse\` checkout |
| `$(SolutionDir)\..\common\Release\common.lib` | `_clones\common\Release\common.lib` | built from `common.vcxproj` (Release link only) |

Forced include on every TU: `obse_common/obse_prefix.h`.
Key preprocessor defs: `OBLIVION`, `OBLIVION_VERSION=0x010201A0`, `WIN32`, `_USRDLL`.

### Pre-flight check

Run before a first build (PowerShell):

```powershell
Test-Path D:\Modlists\_clones\StormLog\Detours\detours.h     # must be True
Test-Path D:\Modlists\_clones\obse\obse\CommandTable.h        # must be True
Test-Path D:\Modlists\_clones\common\common.vcxproj           # must be True
```

If the `Detours` junction is missing, recreate it:

```powershell
New-Item -ItemType Junction `
  -Path "D:\Modlists\_clones\StormLog\Detours" `
  -Target "D:\Modlists\_clones\xOBSE\obse\Detours"
```

## The solution

`StormLog.sln` contains three projects, all `Debug|Win32` / `Release|Win32`:

| Project | Output | Type | Purpose |
|---|---|---|---|
| `StormLog.vcxproj` | `StormLog.dll` | DynamicLibrary | the plugin |
| `test\StormLogTest.vcxproj` | `StormLogTest.exe` | Application | offline unit tests (no game) |
| `probe\StormLogProbe.vcxproj` | `StormLogProbe.dll` | DynamicLibrary | L3 hook-address validation probe |

`StormLog.dll` exports `OBSEPlugin_Query` / `OBSEPlugin_Load` via `Exports.def`.
`BuildInfo.h` is maintained by hand (the `BuildIncrement.jse` pre-build step is a
no-op).

## Build

Open a **Developer PowerShell for VS 2022** (puts `MSBuild` on `PATH`), or call
MSBuild by full path. From `D:\Modlists\_clones\StormLog\`:

```powershell
# One-time / after cleaning: the Release link needs common.lib.
MSBuild "D:\Modlists\_clones\common\common.vcxproj" /p:Configuration=Release /p:Platform=Win32

# The plugin itself.
MSBuild StormLog.sln /t:StormLog /p:Configuration=Release /p:Platform=Win32 /m
```

- `Release\common.lib` is committed/present already in this checkout, so the first
  command is only needed after a clean or if it goes missing. The `Debug` config
  links the common sources directly and does not need it.
- Build everything (plugin + tests + probe): drop `/t:StormLog`.
- Debug build: `/p:Configuration=Debug`.

Outputs land in `$(SolutionDir)$(Configuration)\`:

- Release plugin → `D:\Modlists\_clones\StormLog\Release\StormLog.dll`
- Debug plugin → `D:\Modlists\_clones\StormLog\Debug\StormLog.dll`

(The `StormLogTest.exe` output goes under `test\Release\` /`test\Debug\`.)

## Verify

```powershell
# Unit tests — runs without Oblivion.
MSBuild StormLog.sln /t:StormLogTest /p:Configuration=Release /p:Platform=Win32 /m
& "D:\Modlists\_clones\StormLog\test\Release\StormLogTest.exe"
```

The probe (`StormLogProbe.dll`) is the in-game hook-address gate: deploy it like
the plugin, play a short session, and confirm it prints one line per L3a/L3b fire
before trusting the real hooks. See the design spec, "Testing", step 1.

## Deploy (Mod Organizer 2)

The plugin loads as an OBSE plugin. Create/refresh an MO2 mod named `StormLog`:

```
<MO2 mods>\StormLog\
  OBSE\Plugins\StormLog.dll      <- from Release\StormLog.dll
  OBSE\Plugins\StormLog.ini      <- from repo root StormLog.ini
```

Enable it in the test profile (the MOO-only Reborn test profile per the spec).
Per-session CSVs are written to `Data\OBSE\Plugins\StormLog\YYYY-MM-DD_HHMMSS.csv`.
Tunables live in `StormLog.ini` (`[Logging]` / `[Detection]` / `[Hooks]`); missing
keys fall back to the `Config.cpp` defaults.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Cannot open include file: 'detours.h'` | `StormLog\Detours` junction missing — recreate it (above). |
| `Cannot open include file: 'obse_common/...'` or `obse/...` | `_clones\obse\` checkout missing/moved. |
| `LNK1181: cannot open input file 'common.lib'` (Release) | Build `common.vcxproj` Release first (see Build step 1). |
| `error MSB8020 ... v143 build tools` | v143 toolset not installed — add it via the VS Installer. |
| Wrong platform | There is no x64 config; always pass `/p:Platform=Win32`. |
| Plugin doesn't load in-game | Confirm it's in `OBSE\Plugins\` and that xOBSE itself is installed/loading. |

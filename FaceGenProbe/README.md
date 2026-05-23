# FaceGenProbe

OBSE plugin that eliminates the FaceGen storm bug for creature-mounted NPCs in Oblivion.

## The Bug

NPCs mounted on creatures (e.g., Imperial Legion riders) trigger an infinite FaceGen reprocessing loop — the Filler (~50–100 calls/sec) and associated FaceGen chain run continuously, causing severe performance degradation. The task never completes and stays in the BSTaskManagerThread queue.

## Fix (Build 39)

Three hooks intercept the storm at different levels:

| Hook | Address | What it does |
|---|---|---|
| `TESNPC_FaceGenFiller` | 0x005221C0 | Zeroes race data (+0xE8) for mounted NPCs, forcing the default race FaceGen path |
| `FUN_0043b990` | 0x0043B990 | Blocks BSTask creation for creature mounts (vtable+0xF4 check) |
| `FUN_0043eb80` | 0x0043EB80 | 2-call gate — allows init + FaceGen setup, blocks all subsequent storm calls |

The 2-call gate at `FUN_0043eb80` is the key fix. Mounted NPCs get their first two calls (setup), then all subsequent calls return immediately. The storm still fires (~100/sec) but each call is ~3 pointer dereferences instead of the full FaceGen chain.

Storm self-resolves after ~20k–30k iterations (~200–300 seconds) when the task finally completes.

## Build

Requires:
- **Detours**: `D:\Modlists\_clones\StormLog\Detours\detours.lib` and headers
- **xOBSE common**: `D:\Modlists\_clones\common\Release\common.lib`
- **OBSE headers**: `D:\Modlists\_clones\obse\obse\`
- Visual Studio 2022, Win32 Release

```powershell
# From the FaceGenProbe directory:
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" FaceGenProbe.vcxproj /p:Configuration=Release /p:Platform=Win32 /m /nologo
```

Or use `build.bat`.

## Install

Copy `Release\FaceGenProbe.dll` to `Data\OBSE\Plugins\`. Log writes to `facegenprobe_realtime.log` beside the DLL.

## Files

| File | Purpose |
|---|---|
| `FaceGenProbe.cpp` | Main source — all three hooks |
| `Main.cpp` | OBSE plugin entry point |
| `FaceGenProbeExports.def` | DLL exports |
| `FaceGenProbe.vcxproj` | MSBuild project |

## History

Started May 2026 as a diagnostic probe. Evolved through 39 builds — dead-end hooks on PerFrameProcessor, FUN_00529530, FUN_0052c870, HelperPopulator (crashes), mid-function NOP (crashes), crash probes, stack scans — before the `FUN_0043eb80` 2-call gate was confirmed working in Build 38.

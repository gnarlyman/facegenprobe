# FaceGenProbe

Minimal diagnostic probe for investigating the **producer side** of FaceGen storms.

## Purpose

This probe targets the functions most likely responsible for releasing or falling back on FaceGen data, which appears to be the root trigger for the retry storms on certain NPCs (especially the vanilla Imperial Legion riders `000700C0–CD`).

### Current Targets (from Ghidra decompilation)

- `FUN_00519d20` → `FaceGen_ReleaseFace0Face1`
  - Reference-counted release of FaceGen data.
- `FUN_00521e40` → `FaceGen_FallbackPopulator` / `FaceGen_DefaultGetter`
  - Fallback logic that runs when normal FaceGen lookup fails.

## Why This Probe Exists

The previous L3 hooks (`QueuedHead_Run` and especially `BSTaskThread_Runnable`) were too aggressive and crashed the game on boot because they hooked functions that can be called on partially constructed objects.

This probe is intentionally much more focused and safer.

## Building

This probe follows the same build environment as the rest of the StormLog project.

1. It needs the Detours junction already present in the StormLog folder.
2. It links against `common.lib` (build `xOBSE/common` in Release first if necessary).
3. Recommended: Add it as a new project in `StormLog.sln` (Win32, same include paths as `StormLogProbe`).

Alternatively, you can build it with a minimal vcxproj modeled after `StormLog/probe/StormLogProbe.vcxproj`.

## Usage

1. Build `FaceGenProbe.dll`.
2. Create a MO2 mod folder:
   ```
   mods/FaceGenProbe/
       OBSE/Plugins/FaceGenProbe.dll
   ```
3. **Disable** the normal `StormLog` mod.
4. Enable `FaceGenProbe`.
5. Launch the game.

The probe will write to:
```
Stock Game/Data/OBSE/Plugins/FaceGenProbe.log
```

## What to Look For

- High call counts to `FaceGen_ReleaseFace0Face1` on storming NPCs.
- `FaceGen_FallbackPopulator` being hit repeatedly.
- Correlation between these calls and the L1 storms you're already capturing with the main StormLog.

This data should help identify whether the storm is caused by aggressive FaceGen data eviction or broken fallback/retry logic.

---

Built as a clean, focused diagnostic tool during the Reborn FaceGen storm investigation (May 2026).
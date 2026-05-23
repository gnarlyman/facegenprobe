# StormLog Build & Development Rules (Windows + Grok Build)

This project is a 32-bit OBSE plugin for Oblivion (xOBSE). It must be built with the Visual Studio 2022 v143 toolset (Win32 only).

## Critical: Grok Build Shell on Windows

Grok Build runs its `run_terminal_command` tool inside bash (Git Bash / MSYS2) by default. This causes severe path mangling on any argument starting with `/` (e.g. MSBuild `/t:`, `/p:`, `/nologo`).

**Always follow these rules when working in this repo:**

1. **Preferred build commands**
   - Use the provided batch launchers (they escape the bash context automatically):
     - `build-stormlog.bat` — builds + deploys StormLog.dll to the Reborn MO2 mod
     - `FaceGenProbe\Run-Build.bat` — builds + deploys FaceGenProbe.dll
     - `FaceGenProbe\build.bat` — direct MSBuild for the probe vcxproj
   - These are the safest way to build from inside Grok.

2. **When you must run MSBuild / cl / link directly**
   - NEVER invoke them with raw `/` switches from the agent.
   - ALWAYS wrap through cmd.exe:
     ```powershell
     cmd.exe /c "cd /d \"D:\Modlists\_clones\StormLog\" && \"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe\" StormLog.sln /t:StormLog /p:Configuration=Release /p:Platform=Win32 /m /nologo"
     ```
   - For PowerShell scripts (.ps1), always use:
     ```powershell
     powershell -ExecutionPolicy Bypass -File "path\to\script.ps1"
     ```

3. **Environment for launching Grok itself (user-side)**
   - Launch `grok` from a **native PowerShell** or Windows Terminal session (not from Git Bash).
   - Before starting grok, set:
     ```powershell
     $env:SHELL = "powershell.exe"
     $env:MSYS_NO_PATHCONV = "1"
     $env:MSYS2_ARG_CONV_EXCL = "*"
     grok --cwd D:\Modlists\_clones\StormLog
     ```
   - This combination prevents most mangling at the source.

4. **Build documentation**
   - Primary source of truth: `docs/BUILD.md`
   - FaceGenProbe specific: `FaceGenProbe/README.md`

## Build Targets

- `StormLog.sln` contains:
  - `StormLog` → StormLog.dll (the real plugin)
  - `StormLogTest` → unit tests (run after build)
  - `StormLogProbe` → L3 hook validation probe
  - `FaceGenProbe` → FaceGen storm diagnostic probe

- Common first step for Release builds (if common.lib is stale):
  Build `D:\Modlists\_clones\xOBSE\common\common.vcxproj` (Release | Win32) first.

## Testing

After building:
- Run `test\Release\StormLogTest.exe` to execute unit tests.
- For in-game validation use the probe DLLs via the MO2 mods `StormLog` / `FaceGenProbe` / `StormLogProbe`.

## DO NOT

- Run raw `MSBuild.exe ... /t:Foo /p:Bar` without a cmd.exe wrapper.
- Double-click .ps1 files or run them directly in cmd without `powershell -ExecutionPolicy Bypass`.
- Assume the current shell context is safe for forward-slash flags.

Follow the existing .bat files and the wrapping pattern above. This keeps builds reliable inside Grok Build on Windows.

@echo off
:: Run this file by double-clicking it or from any command prompt.
:: It will try to find Developer PowerShell and run the build script.

setlocal

set "SCRIPT_DIR=%~dp0"
set "PS1_SCRIPT=%SCRIPT_DIR%build-facegenprobe.ps1"

echo ===============================================
echo FaceGenProbe Build Launcher
echo ===============================================

if not exist "%PS1_SCRIPT%" (
    echo ERROR: build-facegenprobe.ps1 not found next to this .bat
    pause
    exit /b 1
)

:: Try to find Visual Studio 2022 Developer PowerShell
set "VSDEVPS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1"

if not exist "%VSDEVPS%" (
    set "VSDEVPS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\Launch-VsDevShell.ps1"
)

if not exist "%VSDEVPS%" (
    set "VSDEVPS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\Launch-VsDevShell.ps1"
)

if not exist "%VSDEVPS%" (
    echo ERROR: Could not find Launch-VsDevShell.ps1
    echo Please run this from a "Developer PowerShell for VS 2022" instead.
    pause
    exit /b 1
)

echo Launching Developer PowerShell and running the build...
echo.

powershell -NoExit -NoProfile -ExecutionPolicy Bypass -Command ^
    "& { & '%VSDEVPS%' -SkipAutomaticLocation; & '%PS1_SCRIPT%' }"

endlocal
pause
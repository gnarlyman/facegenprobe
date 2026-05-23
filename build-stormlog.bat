@echo off
setlocal

echo ===============================================
echo Building StormLog - Release Win32
echo ===============================================

set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set "SOLUTION=D:\Modlists\_clones\StormLog\StormLog.sln"
set "COMMON_PROJ=D:\Modlists\_clones\xOBSE\common\common.vcxproj"

echo.
echo [1/3] Ensuring common.lib exists (Release Win32)...
if not exist "D:\Modlists\_clones\xOBSE\common\Release\common.lib" (
    echo        Building common.lib...
    "%MSBUILD%" "%COMMON_PROJ%" /p:Configuration=Release /p:Platform=Win32 /m /nologo
)
if not exist "D:\Modlists\_clones\xOBSE\common\Release\common.lib" (
    echo ERROR: common.lib is missing and could not be built (v142 tools required)
    exit /b 1
)
echo        Using existing common.lib - OK (common project uses v142; this machine is v143-only)

echo.
echo [2/3] Building StormLog.dll (Release Win32)...
"%MSBUILD%" "%SOLUTION%" /t:StormLog /p:Configuration=Release /p:Platform=Win32 /m /nologo
if errorlevel 1 (
    echo ERROR: Failed to build StormLog
    exit /b 1
)

echo.
echo [3/3] Copying StormLog.dll to Mod Organizer mod folder...
copy /Y "D:\Modlists\_clones\StormLog\Release\StormLog.dll" "D:\Modlists\Reborn\mods\StormLog\OBSE\Plugins\StormLog.dll"
if errorlevel 1 (
    echo ERROR: Failed to copy DLL
    exit /b 1
)

echo.
echo ===============================================
echo BUILD SUCCESSFUL
echo StormLog.dll has been built and deployed.
echo ===============================================

endlocal
@echo off
setlocal

set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set "PROJECT=D:\Modlists\_clones\StormLog\FaceGenProbe\FaceGenProbe.vcxproj"

echo Building FaceGenProbe (Release | Win32)...

"%MSBUILD%" "%PROJECT%" /p:Configuration=Release /p:Platform=Win32 /m /nologo

if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Build succeeded.
echo DLL should be in Release\FaceGenProbe.dll

endlocal
pause
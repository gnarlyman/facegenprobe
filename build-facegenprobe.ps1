# Simple native PowerShell build script for FaceGenProbe
# Run this from a Developer PowerShell for VS 2022

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host "Building FaceGenProbe (Release | Win32)..." -ForegroundColor Cyan

$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
$project = "FaceGenProbe.vcxproj"

& $msbuild $project -p:Configuration=Release -p:Platform=Win32 -m -nologo

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed." -ForegroundColor Red
    exit 1
}

Write-Host "Build succeeded." -ForegroundColor Green

# Optional: Copy to mod folder
$source = "Release\FaceGenProbe.dll"
$dest   = "D:\Modlists\Reborn\mods\FaceGenProbe\OBSE\Plugins\FaceGenProbe.dll"

if (Test-Path $source) {
    Copy-Item $source $dest -Force
    Write-Host "Deployed to mod folder." -ForegroundColor Green
} else {
    Write-Host "DLL not found at $source" -ForegroundColor Yellow
}
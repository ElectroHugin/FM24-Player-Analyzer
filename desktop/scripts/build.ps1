# Builds the desktop app. Usage: .\build.ps1 [-Preset msvc-release] [-Test]
param(
    [string]$Preset = "msvc-release",
    [switch]$Test
)

$ErrorActionPreference = "Stop"
$desktopDir = Split-Path -Parent $PSScriptRoot
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $vcvars)) { throw "vcvars64.bat nicht gefunden: $vcvars" }

$commands = "cmake --preset $Preset && cmake --build --preset $Preset"
if ($Test) { $commands += " && ctest --preset $Preset" }

cmd /c "`"$vcvars`" >nul 2>&1 && cd /d `"$desktopDir`" && $commands"
exit $LASTEXITCODE

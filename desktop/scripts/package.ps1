# Baut die Release-Version, staged sie mit windeployqt und erzeugt den
# Inno-Setup-Installer. Usage: .\package.ps1 [-Preset msvc-release] [-SkipBuild]
param(
    [string]$Preset = "msvc-release",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$desktopDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $desktopDir "build\$Preset"
$exePath = Join-Path $buildDir "src\app\fmplayeranalyzer.exe"
$stagingDir = Join-Path $desktopDir "installer\staging"
$qtBin = "C:\Qt\6.8.3\msvc2022_64\bin"

# --- 1. Build ---
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }
}
if (-not (Test-Path $exePath)) { throw "App-Binary nicht gefunden: $exePath" }

# --- 2. Version aus Version.cpp lesen (single source of truth) ---
$versionSource = Get-Content (Join-Path $desktopDir "src\core\Version.cpp") -Raw
$version = "0.0.0"
if ($versionSource -match 'QStringLiteral\("(\d+\.\d+\.\d+)"\)') { $version = $Matches[1] }
Write-Host "Paketiere Version $version" -ForegroundColor Cyan

# --- 3. Staging mit windeployqt ---
if (Test-Path $stagingDir) { Remove-Item $stagingDir -Recurse -Force }
New-Item -ItemType Directory -Force $stagingDir | Out-Null
Copy-Item $exePath $stagingDir
$windeployqt = Join-Path $qtBin "windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt nicht gefunden: $windeployqt" }
& $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    (Join-Path $stagingDir "fmplayeranalyzer.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt fehlgeschlagen." }

# --- 4. Installer bauen (Inno Setup 6) ---
$isccCandidates = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)
$iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    Write-Warning "Inno Setup 6 (ISCC.exe) nicht gefunden - Staging liegt unter installer\staging."
    Write-Host "Portable Nutzung: den Inhalt von installer\staging einfach kopieren." -ForegroundColor Yellow
    exit 0
}
& $iscc "/DMyAppVersion=$version" (Join-Path $desktopDir "installer\setup.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC fehlgeschlagen." }
Write-Host "Installer liegt unter installer\output\" -ForegroundColor Green

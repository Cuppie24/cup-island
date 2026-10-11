# Builds Cup Island on Windows via CMake (see CMakePresets.json) using the
# CMake that ships with Visual Studio, so no separate install is needed.
#
# Usage: .\build.ps1 [-Run] [-Test]

param(
    [switch]$Run,
    [switch]$Test
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$cmake = Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Neither cmake nor vswhere.exe found - is Visual Studio installed?" }
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstall) { throw "No Visual Studio install with the C++ (VC.Tools.x86.x64) component was found." }
    $cmake = "$vsInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-not (Test-Path $cmake)) { throw "CMake not found at $cmake - install the 'C++ CMake tools for Windows' VS component." }
}
$ctest = Join-Path (Split-Path $cmake) "ctest.exe"

& $cmake --preset windows
if ($LASTEXITCODE -ne 0) { throw "Configure failed (exit $LASTEXITCODE)" }
& $cmake --build --preset windows
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

if ($Test) {
    & $ctest --preset windows
    if ($LASTEXITCODE -ne 0) { throw "Tests failed (exit $LASTEXITCODE)" }
}

$exePath = Join-Path $PSScriptRoot "build\windows\Release\CupIsland.exe"
Write-Host "Build succeeded: $exePath"
if ($Run) { & $exePath }

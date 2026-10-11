# Builds Cup Island on Windows via CMake (see CMakePresets.json).
# Enters the Visual Studio developer environment first, which puts the MSVC
# compiler, CMake and Ninja (all shipped with Visual Studio) on PATH.
#
# Usage: .\build.ps1 [-Run] [-Test] [-Debug]

param(
    [switch]$Run,
    [switch]$Test,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstall) { throw "No Visual Studio install with the 'Desktop development with C++' workload was found." }

    Import-Module "$vsInstall\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vsInstall -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    Set-Location $PSScriptRoot
}
foreach ($tool in "cmake", "ninja") {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found - install the 'C++ CMake tools for Windows' Visual Studio component."
    }
}

$preset = if ($Debug) { "windows-debug" } else { "windows" }

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { throw "Configure failed (exit $LASTEXITCODE)" }
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

if ($Test) {
    ctest --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Tests failed (exit $LASTEXITCODE)" }
}

$exePath = Join-Path $PSScriptRoot "build\$preset\CupIsland.exe"
Write-Host "Build succeeded: $exePath"
if ($Run) { & $exePath }

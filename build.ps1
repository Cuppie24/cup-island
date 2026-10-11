# Builds Cup Island on Windows via CMake (see CMakePresets.json).
# Enters the Visual Studio developer environment first, which puts the MSVC
# compiler, CMake and Ninja (all shipped with Visual Studio) on PATH.
#
# Usage: .\build.ps1 [-Run] [-Test] [-Debug] [-Installer]
#   -Installer  also builds dist\CupIsland-Setup-<version>.exe (Release only;
#               needs Inno Setup 6.3+, see installer\cup-island.iss)

param(
    [switch]$Run,
    [switch]$Test,
    [switch]$Debug,
    [switch]$Installer
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

if ($Installer -and $Debug) { throw "-Installer packages the Release build; drop -Debug." }

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

if ($Installer) {
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if (-not $iscc) {
        $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                  "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $iscc) { throw "Inno Setup 6 not found - install it from https://jrsoftware.org/isdl.php (or: winget install JRSoftware.InnoSetup)." }
    & $iscc /Qp "installer\cup-island.iss"
    if ($LASTEXITCODE -ne 0) { throw "Installer build failed (exit $LASTEXITCODE)" }
    Write-Host "Installer: $(Get-ChildItem dist\CupIsland-Setup-*.exe | Sort-Object LastWriteTime | Select-Object -Last 1)"
}
if ($Run) { & $exePath }

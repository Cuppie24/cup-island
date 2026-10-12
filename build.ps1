# Builds Cup Island on Windows via CMake (see CMakePresets.json).
# Enters the Visual Studio developer environment first, which puts the MSVC
# compiler, CMake and Ninja (all shipped with Visual Studio) on PATH.
#
# Usage: .\build.ps1 [-Run] [-Test] [-Debug] [-Installer] [-Version X.Y.Z]
#   -Installer  also builds dist\CupIsland-Setup-<version>.exe (Release only;
#               needs Inno Setup 6.3+, see installer\cup-island.iss)
#   -Version    the release's version; only the Release workflow passes it
#               (decision 036). Without it the version is git's: a dev build.

param(
    [switch]$Run,
    [switch]$Test,
    [switch]$Debug,
    [switch]$Installer,
    [string]$Version
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# Only the 64-bit compiler will do: "Developer PowerShell for VS" from the
# Start menu puts the 32-bit one (Host*\x86\cl.exe) on PATH by default.
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl -or $cl.Source -notmatch '\\x64\\cl\.exe$') {
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

# The version (decision 036): the Release workflow's, else git's description,
# e.g. 0.3.0-5-gabc1234 (five commits after v0.3.0): a dev build.
if (-not $Version) {
    $described = & {
        $ErrorActionPreference = "Continue" # git's "no tags" message must not stop the build
        try { git describe --tags --match "v[0-9]*" --dirty 2>$null } catch { $null }
    }
    $Version = if ($described) { $described -replace '^v', '' } else { "0.0.0-dev" }
}
Write-Host "Version: $Version"

# A build folder configured with the 32-bit compiler keeps using it: start it fresh.
$cache = "build\$preset\CMakeCache.txt"
$fresh = @()
if ((Test-Path $cache) -and (Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:.*[\\/]x86[\\/]cl\.exe' -Quiet)) {
    Write-Host "build\$preset was set up for 32-bit; reconfiguring for x64."
    $fresh = @("--fresh")
}

# The linker can't overwrite a running exe, and Release builds run in the tray
# with no console. Only stops this checkout's builds, never an installed copy.
Get-Process CupIsland -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path.StartsWith("$PSScriptRoot\build\", [StringComparison]::OrdinalIgnoreCase) } |
    Stop-Process -Force

cmake --preset $preset @fresh "-DCUP_VERSION=$Version"
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
        # Machine-wide or per-user (winget's default) installs, any version; newest first.
        $iscc = @("${env:ProgramFiles(x86)}", "$env:ProgramFiles", "$env:LOCALAPPDATA\Programs") |
            ForEach-Object { Get-ChildItem -Path $_ -Filter "Inno Setup *" -Directory -ErrorAction SilentlyContinue } |
            Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName "ISCC.exe" } |
            Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $iscc) { throw "Inno Setup not found. Install it (winget install JRSoftware.InnoSetup, or https://jrsoftware.org/isdl.php), then open a new terminal and re-run." }
    & $iscc /Qp "/DAppVersion=$Version" "installer\cup-island.iss"
    if ($LASTEXITCODE -ne 0) { throw "Installer build failed (exit $LASTEXITCODE)" }
    Write-Host "Installer: $(Get-ChildItem dist\CupIsland-Setup-*.exe | Sort-Object LastWriteTime | Select-Object -Last 1)"
}
if ($Run) { & $exePath }

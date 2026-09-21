# Builds the Cup Island animation prototype (main.cpp) directly with cl.exe.
# No .vcxproj/.sln - this is deliberately a minimal single-file prototype,
# not the eventual WinUI 3/MSIX project structure.
#
# Usage: .\build.ps1 [-Run]

param(
    [switch]$Run
)

$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }

$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) { throw "No Visual Studio install with the C++ (VC.Tools.x86.x64) component was found." }

$msvcDir = Get-ChildItem "$vsInstall\VC\Tools\MSVC" | Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcDir) { throw "No MSVC toolset found under $vsInstall\VC\Tools\MSVC" }
$msvcRoot = $msvcDir.FullName

$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdkVersionDir = Get-ChildItem "$sdkRoot\Include" | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdkVersionDir) { throw "No Windows 10/11 SDK found under $sdkRoot\Include" }
$sdkVersion = $sdkVersionDir.Name

$clExe = "$msvcRoot\bin\Hostx64\x64\cl.exe"
if (-not (Test-Path $clExe)) { throw "cl.exe not found at $clExe" }

Write-Host "MSVC:  $msvcRoot"
Write-Host "SDK:   $sdkVersion"

$includeDirs = @(
    "$msvcRoot\include",
    "$sdkRoot\Include\$sdkVersion\um",
    "$sdkRoot\Include\$sdkVersion\shared",
    "$sdkRoot\Include\$sdkVersion\winrt",
    "$sdkRoot\Include\$sdkVersion\cppwinrt",
    "$sdkRoot\Include\$sdkVersion\ucrt"
)

$libDirs = @(
    "$msvcRoot\lib\x64",
    "$sdkRoot\Lib\$sdkVersion\um\x64",
    "$sdkRoot\Lib\$sdkVersion\ucrt\x64"
)

$outDir = Join-Path $PSScriptRoot "bin"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exePath = Join-Path $outDir "CupIsland.exe"

$clArgs = @("/nologo", "/EHsc", "/std:c++20", "/O2", "/W3", "/DUNICODE", "/D_UNICODE")
$clArgs += ($includeDirs | ForEach-Object { "/I$_" })
$clArgs += (Join-Path $PSScriptRoot "main.cpp")
$clArgs += "/Fe:$exePath"
$clArgs += "/Fo:$outDir\"
$clArgs += "/link"
$clArgs += ($libDirs | ForEach-Object { "/LIBPATH:$_" })
$clArgs += @("WindowsApp.lib", "CoreMessaging.lib", "User32.lib", "dcomp.lib", "/SUBSYSTEM:CONSOLE")

Write-Host "Building $exePath ..."
& $clExe @clArgs
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

Write-Host "Build succeeded: $exePath"

if ($Run) {
    & $exePath
}

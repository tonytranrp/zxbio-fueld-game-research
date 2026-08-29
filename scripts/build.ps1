#Requires -Version 5.1
<#
.SYNOPSIS
    Configures and builds Fuel Farm (the "dev" preset -- Ninja, Bevy bridge off,
    fast path) and runs the test suite. Assumes prerequisites are already
    installed; run setup.ps1 first on a machine that doesn't have them yet.

.DESCRIPTION
    Equivalent to running, from a Visual Studio Developer Command Prompt:
        cmake --preset dev
        cmake --build --preset dev
        ctest --preset dev
    This script finds Visual Studio itself (via vswhere) and sets up that
    environment automatically, so it works from a plain PowerShell or
    double-clicked .bat -- see README.md's Build section for why that
    environment is required at all (Ninja doesn't auto-detect MSVC the way
    the Visual Studio generator does).

.PARAMETER Preset
    Which CMake preset to build: dev (default), dev-full, or release.
    See CMakePresets.json / README.md's Build section.

.PARAMETER SkipTests
    Configure and build only; skip running ctest.
#>
[CmdletBinding()]
param(
    [ValidateSet('dev', 'dev-full', 'release')]
    [string]$Preset = 'dev',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Failure {
    param([string]$Message)
    Write-Host ""
    Write-Host "FAILED: $Message" -ForegroundColor Red
}

# ------------------------------------------------------------------------------
# Locate Visual Studio and load its developer environment into THIS process.
# ------------------------------------------------------------------------------
function Import-VisualStudioEnvironment {
    Write-Step "Locating Visual Studio (C++ build tools)..."

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (-not (Test-Path $vswhere)) {
        Write-Failure "vswhere.exe not found -- Visual Studio (or VS Build Tools) with the 'Desktop development with C++' workload doesn't appear to be installed. Run scripts\setup.ps1 first."
        exit 1
    }

    $vsInstallPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsInstallPath) {
        Write-Failure "No Visual Studio installation with the C++ (VC.Tools.x86.x64) component was found. Run scripts\setup.ps1 first, or install the 'Desktop development with C++' workload manually."
        exit 1
    }

    $vcvarsall = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Failure "Found Visual Studio at '$vsInstallPath' but vcvarsall.bat is missing from it."
        exit 1
    }

    Write-Host "  Using: $vsInstallPath"

    # Run vcvarsall in a throwaway cmd.exe, dump the resulting environment, and
    # import every variable it set/changed into THIS PowerShell process -- the
    # standard trick for reusing a .bat-only environment setup from PowerShell.
    $envDump = cmd.exe /c "`"$vcvarsall`" x64 >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0 -or -not $envDump) {
        Write-Failure "vcvarsall.bat x64 did not run successfully."
        exit 1
    }
    foreach ($line in $envDump) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

Import-VisualStudioEnvironment

# ------------------------------------------------------------------------------
# Configure, build, test.
# ------------------------------------------------------------------------------
Push-Location $RepoRoot
try {
    Write-Step "Configuring (cmake --preset $Preset)..."
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { Write-Failure "CMake configure failed."; exit 1 }

    Write-Step "Building (cmake --build --preset $Preset)..."
    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { Write-Failure "Build failed -- see the compiler errors above."; exit 1 }

    if (-not $SkipTests) {
        Write-Step "Running tests (ctest --preset dev)..."
        # All presets share one binary layout for tests; the dev test preset works
        # against any of them since it only names the build tree, not the config.
        ctest --test-dir "$RepoRoot\Build\$Preset" --output-on-failure
        if ($LASTEXITCODE -ne 0) { Write-Failure "Some tests failed -- see above."; exit 1 }
    }

    $exeDir = "$RepoRoot\Build\$Preset\bin"
    Write-Host ""
    Write-Host "Build succeeded." -ForegroundColor Green
    Write-Host "Run the game: $exeDir\BiofuelGame.exe"
}
finally {
    Pop-Location
}

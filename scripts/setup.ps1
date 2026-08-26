#Requires -Version 5.1
<#
.SYNOPSIS
    One-time machine setup for building Fuel Farm: installs every tool the
    build needs (Visual Studio C++ Build Tools, CMake, Ninja, Rust, Git, and
    optionally sccache), then configures, builds, and tests the game.

.DESCRIPTION
    Meant for a brand-new Windows machine (e.g. a school computer) that has
    nothing installed yet. Safe to re-run -- every step only installs what's
    missing (winget skips already-installed packages; rustup skips an
    already-installed toolchain), so re-running after a partial failure just
    picks up where it left off.

    Needs administrator rights (Visual Studio's installer requires them) --
    this script elevates itself automatically and asks Windows for permission
    (UAC prompt) if it isn't already running elevated.

    Needs winget (Windows Package Manager), which ships with Windows 11 and
    current Windows 10. If it's missing, install "App Installer" from the
    Microsoft Store first, then re-run this script.

.PARAMETER SkipBuild
    Install prerequisites only; don't build the game afterward. Useful if you
    want to review what got installed before kicking off a build, or if you
    plan to open the folder in Visual Studio instead.
#>
[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Info {
    param([string]$Message)
    Write-Host "    $Message" -ForegroundColor DarkGray
}

function Write-Failure {
    param([string]$Message)
    Write-Host ""
    Write-Host "FAILED: $Message" -ForegroundColor Red
}

function Wait-ForEnterThenExit {
    param([int]$Code = 1)
    Write-Host ""
    Read-Host "Press Enter to close"
    exit $Code
}

# ------------------------------------------------------------------------------
# Self-elevate. Visual Studio's installer requires administrator rights, and
# doing this up front avoids a confusing failure partway through.
# ------------------------------------------------------------------------------
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)) {
    Write-Host "Administrator rights are required (Visual Studio's installer needs them)." -ForegroundColor Yellow
    Write-Host "Requesting elevation -- accept the prompt Windows shows you." -ForegroundColor Yellow
    try {
        Start-Process powershell -Verb RunAs -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`""
            $(if ($SkipBuild) { '-SkipBuild' })
        )
    } catch {
        Write-Failure "Elevation was declined or failed. This script can't continue without administrator rights."
        Write-Info "If you don't have administrator rights on this computer at all, ask whoever manages it to run this script for you, or to install manually: Visual Studio Build Tools (Desktop development with C++ workload), CMake, Ninja, and Rust (rustup)."
        Wait-ForEnterThenExit
    }
    exit 0
}

Write-Host "Fuel Farm build environment setup" -ForegroundColor Green
Write-Host "This installs Visual Studio C++ Build Tools, CMake, Ninja, Rust, Git, and sccache," -ForegroundColor Green
Write-Host "then builds the game. It can take a while on the first run (Visual Studio alone is" -ForegroundColor Green
Write-Host "a multi-GB download) -- this is normal, let it run." -ForegroundColor Green

# ------------------------------------------------------------------------------
# winget must exist; everything else is installed through it.
# ------------------------------------------------------------------------------
Write-Step "Checking for winget (Windows Package Manager)..."
$winget = Get-Command winget -ErrorAction SilentlyContinue
if (-not $winget) {
    Write-Failure "winget was not found."
    Write-Info "Install 'App Installer' from the Microsoft Store (search 'App Installer'), then re-run this script."
    Wait-ForEnterThenExit
}
Write-Info "Found: $($winget.Source)"

# ------------------------------------------------------------------------------
# Install packages. Each call is independent -- one failing doesn't stop the
# others, so a flaky download doesn't waste everything already installed.
# Failures are collected and reported together at the end.
# ------------------------------------------------------------------------------
$failures = New-Object System.Collections.Generic.List[string]

function Install-WingetPackage {
    param(
        [Parameter(Mandatory)][string]$Id,
        [Parameter(Mandatory)][string]$DisplayName,
        [string]$Override
    )
    Write-Step "Installing $DisplayName..."
    $wingetArgs = @('install', '--id', $Id, '-e', '--silent',
        '--accept-package-agreements', '--accept-source-agreements')
    if ($Override) {
        $wingetArgs += @('--override', $Override)
    }
    & winget @wingetArgs
    # winget's own exit code is unreliable for "already installed" / "no
    # newer version" cases across winget versions, so don't gate on it --
    # the real check is whether the tool works, done in the verify pass below.
    if ($LASTEXITCODE -ne 0) {
        Write-Info "winget exited with code $LASTEXITCODE (may just mean it was already installed -- verified below)."
    }
}

Install-WingetPackage -Id 'Git.Git' -DisplayName 'Git'
Install-WingetPackage -Id 'Kitware.CMake' -DisplayName 'CMake'
Install-WingetPackage -Id 'Ninja-build.Ninja' -DisplayName 'Ninja'
Install-WingetPackage -Id 'Rustlang.Rustup' -DisplayName 'Rust (rustup)'
Install-WingetPackage -Id 'Mozilla.sccache' -DisplayName 'sccache (optional compiler cache, makes rebuilds much faster)'

# Desktop development with C++ workload only -- no IDE, keeps this small and
# fast for a machine that just needs to build the game, not edit it in VS.
Install-WingetPackage -Id 'Microsoft.VisualStudio.BuildTools' -DisplayName 'Visual Studio C++ Build Tools (this is the big one -- please be patient)' `
    -Override '--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'

# ------------------------------------------------------------------------------
# Refresh this process's PATH from the registry so newly-installed tools are
# runnable without closing and reopening the terminal.
# ------------------------------------------------------------------------------
Write-Step "Refreshing environment..."
$machinePath = [System.Environment]::GetEnvironmentVariable('Path', 'Machine')
$userPath = [System.Environment]::GetEnvironmentVariable('Path', 'User')
$env:Path = "$machinePath;$userPath"

# ------------------------------------------------------------------------------
# rustup installs the manager, not a toolchain by itself -- make sure the
# exact toolchain Corrosion (the Rust/CMake bridge) expects is present and
# default.
# ------------------------------------------------------------------------------
Write-Step "Setting up the Rust toolchain (stable-x86_64-pc-windows-msvc)..."
$rustup = Get-Command rustup -ErrorAction SilentlyContinue
if ($rustup) {
    & rustup toolchain install stable-x86_64-pc-windows-msvc
    & rustup default stable-x86_64-pc-windows-msvc
    # rustup's own installer adds ~/.cargo/bin to User PATH, but this process
    # only picked that up if it existed *before* rustup just added it.
    $userPath = [System.Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = "$machinePath;$userPath"
} else {
    $failures.Add('rustup is not on PATH after installing it -- Rust toolchain setup was skipped.')
}

# ------------------------------------------------------------------------------
# Verify everything the build actually needs is now available. This is the
# real pass/fail check, not individual winget exit codes.
# ------------------------------------------------------------------------------
Write-Step "Verifying installed tools..."
$requiredCommands = @(
    @{ Name = 'git'; Label = 'Git' }
    @{ Name = 'cmake'; Label = 'CMake' }
    @{ Name = 'ninja'; Label = 'Ninja' }
    @{ Name = 'rustc'; Label = 'Rust compiler' }
    @{ Name = 'cargo'; Label = 'Cargo' }
)
foreach ($cmd in $requiredCommands) {
    $found = Get-Command $cmd.Name -ErrorAction SilentlyContinue
    if ($found) {
        Write-Info "$($cmd.Label): OK ($($found.Source))"
    } else {
        Write-Info "$($cmd.Label): NOT FOUND"
        $failures.Add("$($cmd.Label) ('$($cmd.Name)') is still not on PATH. Close this window, open a new PowerShell or Command Prompt, and re-run this script -- some installs only take effect in a new session.")
    }
}
# sccache is optional -- report but don't fail on it.
if (Get-Command sccache -ErrorAction SilentlyContinue) {
    Write-Info "sccache: OK (optional, speeds up rebuilds)"
} else {
    Write-Info "sccache: not found (optional -- the build still works without it, just slower on a from-scratch rebuild)"
}

if ($failures.Count -gt 0) {
    Write-Failure "Setup did not fully succeed:"
    foreach ($f in $failures) { Write-Host "  - $f" -ForegroundColor Red }
    Wait-ForEnterThenExit
}

Write-Host ""
Write-Host "All prerequisites installed." -ForegroundColor Green

if ($SkipBuild) {
    Wait-ForEnterThenExit -Code 0
}

# ------------------------------------------------------------------------------
# Build the game using the same fast preset documented in README.md.
# ------------------------------------------------------------------------------
& (Join-Path $PSScriptRoot 'build.ps1')
$buildExitCode = $LASTEXITCODE
Wait-ForEnterThenExit -Code $buildExitCode

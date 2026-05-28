#!/usr/bin/env pwsh
# run_all.ps1 — Full test suite runner for Windows.
# Runs Catch2 unit tests via ctest, then LAN cross-process scenarios.
#
# Usage: .\tests\scripts\run_all.ps1 [-BuildDir build] [-LinBin ""]
param(
    [string]$BuildDir  = "build",
    [string]$LinBin    = "",       # path to Linux binary (WSL), optional
    [int]   $MaxPlies  = 300,
    [int]   $Timeout   = 120
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildPath = Join-Path $Root $BuildDir
$BinPath   = Join-Path $BuildPath "bin\chess3d.exe"
$ScriptsDir = $PSScriptRoot

$Failures = @()

# ── 1. Build ──────────────────────────────────────────────────────────────────
Write-Host "`n=== BUILD ===" -ForegroundColor Cyan
cmake --build $BuildPath
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# ── 2. Catch2 unit tests ──────────────────────────────────────────────────────
Write-Host "`n=== CATCH2 TESTS ===" -ForegroundColor Cyan
ctest --test-dir $BuildPath --output-on-failure --label-exclude "slow"
if ($LASTEXITCODE -ne 0) { $Failures += "catch2-fast" }

# ── 3. Slow tests (perft etc) in separate pass ───────────────────────────────
Write-Host "`n=== CATCH2 SLOW TESTS ===" -ForegroundColor Cyan
ctest --test-dir $BuildPath --output-on-failure -L "slow"
if ($LASTEXITCODE -ne 0) { $Failures += "catch2-slow" }

# ── 4. Headless smoke test ────────────────────────────────────────────────────
Write-Host "`n=== HEADLESS SMOKE ===" -ForegroundColor Cyan
$output = & $BinPath --auto --max-plies 200 --print-result 2>&1
Write-Host $output
if (-not ($output | Select-String -Pattern "^RESULT\s+")) {
    Write-Warning "Headless smoke: no RESULT line found"
    $Failures += "headless-smoke"
}

# ── 5. LAN cross-process scenarios ───────────────────────────────────────────
Write-Host "`n=== LAN SCENARIOS ===" -ForegroundColor Cyan
$lanArgs = @(
    "$ScriptsDir\run_all_lan.py",
    "--win-bin", $BinPath,
    "--max-plies", $MaxPlies,
    "--timeout", $Timeout
)
if ($LinBin -ne "") { $lanArgs += @("--lin-bin", $LinBin) }
python @lanArgs
if ($LASTEXITCODE -ne 0) { $Failures += "lan-scenarios" }

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host "`n===================="
if ($Failures.Count -gt 0) {
    Write-Host "FAILED: $($Failures -join ', ')" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL TESTS PASSED" -ForegroundColor Green
    exit 0
}

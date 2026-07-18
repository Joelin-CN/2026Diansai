#Requires -Version 5.1
<#
.SYNOPSIS
    One-command host verification for the ICM42688 MSPM0G3507 driver port.
.DESCRIPTION
    Builds and runs three host-compiled GCC test programs in sequence:
      1. test_icm42688      (platform-independent core HAL)
      2. test_ahrs          (Mahony six-axis AHRS)
      3. test_mspm0_adapter (MSPM0 DriverLib adapter over fake DriverLib)
    Any compile or test failure exits immediately with a non-zero code.
    On full success the final line of output is exactly:
        ICM42688 host tests: PASS
.NOTES
    Run from the ICM42688 module root:
        powershell -ExecutionPolicy Bypass -File .\temp\tests\run_tests.ps1
#>

$ErrorActionPreference = 'Stop'

# Resolve module root from the script location so it can be invoked from anywhere.
$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path   # .../temp/tests
$moduleRoot  = Split-Path -Parent (Split-Path -Parent $scriptDir) # .../ICM42688
Set-Location -LiteralPath $moduleRoot

Write-Host '==> ICM42688 host test suite' -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Build + run helper
# ---------------------------------------------------------------------------

function Invoke-BuildAndRun {
    param(
        [string]$Name,
        [string[]]$BuildArgs,
        [string]$ExePath
    )

    Write-Host ""
    Write-Host "-- Building $Name" -ForegroundColor Yellow
    & gcc @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED: $Name (gcc exit $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }

    Write-Host "-- Running $Name" -ForegroundColor Yellow
    & $ExePath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "TEST FAILED: $Name (exit $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }
}

# ---------------------------------------------------------------------------
# Test 1: platform-independent ICM42688 core HAL
# ---------------------------------------------------------------------------
Invoke-BuildAndRun -Name 'test_icm42688' `
    -BuildArgs @(
        '-std=c99','-Wall','-Wextra','-Werror',
        '-Iinc',
        'temp/tests/test_icm42688.c',
        'src/icm42688_hal.c',
        '-lm',
        '-o','temp/tests/test_icm42688.exe'
    ) `
    -ExePath 'temp/tests/test_icm42688.exe'

# ---------------------------------------------------------------------------
# Test 2: six-axis Mahony AHRS
# ---------------------------------------------------------------------------
Invoke-BuildAndRun -Name 'test_ahrs' `
    -BuildArgs @(
        '-std=c99','-Wall','-Wextra','-Werror',
        '-Iinc',
        'temp/tests/test_ahrs.c',
        'src/ahrs_hal.c',
        '-lm',
        '-o','temp/tests/test_ahrs.exe'
    ) `
    -ExePath 'temp/tests/test_ahrs.exe'

# ---------------------------------------------------------------------------
# Test 3: MSPM0 DriverLib adapter (with fake DriverLib)
# ---------------------------------------------------------------------------
Invoke-BuildAndRun -Name 'test_mspm0_adapter' `
    -BuildArgs @(
        '-std=c99','-Wall','-Wextra','-Werror',
        '-Itemp/tests/fakes',
        '-Iinc',
        'temp/tests/test_mspm0_adapter.c',
        'src/icm42688_mspm0.c',
        'src/icm42688_hal.c',
        'src/ahrs_hal.c',
        '-lm',
        '-o','temp/tests/test_mspm0_adapter.exe'
    ) `
    -ExePath 'temp/tests/test_mspm0_adapter.exe'

# ---------------------------------------------------------------------------
# Success
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host 'ICM42688 host tests: PASS' -ForegroundColor Green
exit 0

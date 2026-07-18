$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $PSScriptRoot "build"

if (Test-Path -LiteralPath $build) {
    Remove-Item -Recurse -Force -LiteralPath $build
}
New-Item -ItemType Directory -Path $build | Out-Null

function Invoke-TestBuild {
    param(
        [string]$Name,
        [string[]]$Arguments
    )

    $exe = Join-Path $build "$Name.exe"
    & gcc @Arguments -o $exe
    if ($LASTEXITCODE -ne 0) { throw "$Name compile failed" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$Name failed" }
}

# Test: platform_time (pure conversion functions)
Invoke-TestBuild -Name "test_platform_time" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I$root\inc",
    "$PSScriptRoot\test_platform_time.c",
    "$root\src\platform_time.c"
)

# Test: ICM42688 HAL (temperature + accel + gyro)
Invoke-TestBuild -Name "test_icm42688" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I$root\modules\ICM42688\inc",
    "$PSScriptRoot\test_icm42688.c",
    "$root\modules\ICM42688\src\icm42688_hal.c"
)

Write-Host "Host tests: PASS"

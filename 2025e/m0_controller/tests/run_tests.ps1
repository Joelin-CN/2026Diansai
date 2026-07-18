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

# Test: MCP23017 driver with fake I2C
Invoke-TestBuild -Name "test_mcp23017" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-I$root\modules\MCP23017\inc",
    "-I$PSScriptRoot\fakes",
    "$PSScriptRoot\test_mcp23017.c"
)

# Test: Motion Control with fake encoder and motor
Invoke-TestBuild -Name "test_motion_control" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-I$root\modules\Motion Control\inc",
    "$PSScriptRoot\test_motion_control.c",
    "$root\modules\Motion Control\src\motion_control.c",
    "$root\modules\Motion Control\src\motion_feedback.c",
    "$root\modules\Motion Control\src\motion_feedforward.c",
    "$root\modules\Motion Control\src\motion_kinematics.c"
)

Write-Host "Host tests: PASS"

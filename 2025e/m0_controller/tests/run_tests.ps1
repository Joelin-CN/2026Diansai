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

# Test: Target Adapters (encoder, motor, sensor)
Invoke-TestBuild -Name "test_target_adapters" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-I$root\inc",
    "-I$root\modules\Motion Control\inc",
    "-I$root\modules\Sens-Decision\inc",
    "-I$root\modules\ICM42688\inc",
    "-I$root\modules\MCP23017\inc",
    "$PSScriptRoot\test_target_adapters.c",
    "$root\src\encoder_hw_bridge.c",
    "$root\src\encoder_adapter.c",
    "$root\src\motor_adapter.c",
    "$root\src\sensor_adapter.c"
)

# Test: Square Path (geometry, Pure Pursuit, corrections, lap counting)
Invoke-TestBuild -Name "test_square_path" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-I$root\inc",
    "-I$root\modules\Sens-Decision\inc",
    "$PSScriptRoot\test_square_path.c",
    "$root\src\square_path.c",
    "$root\modules\Sens-Decision\src\trajectory_generate.c",
    "$root\modules\Sens-Decision\src\utils.c"
)

Write-Host "Host tests: PASS"

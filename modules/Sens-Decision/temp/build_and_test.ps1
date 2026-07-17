param(
    [ValidateSet("None", "Empty", "Incomplete")]
    [string]$ProtocolProbe = "None"
)

$ErrorActionPreference = "Stop"

$moduleRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $PSScriptRoot "build"
$includeDir = Join-Path $moduleRoot "inc"
$sources = @(
    (Join-Path $PSScriptRoot "test_main.c"),
    (Join-Path $moduleRoot "src\config.c"),
    (Join-Path $moduleRoot "src\interface.c"),
    (Join-Path $moduleRoot "src\preprocess.c"),
    (Join-Path $moduleRoot "src\utils.c"),
    (Join-Path $moduleRoot "src\EKF.c"),
    (Join-Path $moduleRoot "src\state_evaluate.c"),
    (Join-Path $moduleRoot "src\perception.c"),
    (Join-Path $moduleRoot "src\behavior_planner.c"),
    (Join-Path $moduleRoot "src\trajectory_generate.c")
)
$objects = @(
    (Join-Path $buildDir "test_main.o"),
    (Join-Path $buildDir "config.o"),
    (Join-Path $buildDir "interface.o"),
    (Join-Path $buildDir "preprocess.o"),
    (Join-Path $buildDir "utils.o"),
    (Join-Path $buildDir "EKF.o"),
    (Join-Path $buildDir "state_evaluate.o"),
    (Join-Path $buildDir "perception.o"),
    (Join-Path $buildDir "behavior_planner.o"),
    (Join-Path $buildDir "trajectory_generate.o")
)
$executable = Join-Path $buildDir "sens_decision_tests.exe"

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "gcc was not found in PATH"
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

for ($index = 0; $index -lt $sources.Count; $index++) {
    & gcc -std=c99 -Wall -Wextra -Werror -pedantic "-I$includeDir" -c $sources[$index] -o $objects[$index]
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& gcc @objects -o $executable -lm
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $moduleRoot
try {
    $testOutput = @(& $executable 2>&1)
    $testExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($ProtocolProbe -eq "Empty") {
    $testOutput = @()
    $testExitCode = 0
} elseif ($ProtocolProbe -eq "Incomplete") {
    $testOutput = @("[Sens-Decision] info: incomplete output")
    $testExitCode = 0
}

if ($testExitCode -ne 0) {
    foreach ($line in $testOutput) {
        [Console]::Out.WriteLine([string]$line)
    }
    exit $testExitCode
}

$expectedOutput = @(
    "[Sens-Decision] info: test default_config passed",
    "[Sens-Decision] info: test config_validation passed",
    "[Sens-Decision] info: test assertion_guards passed",
    "[Sens-Decision] info: test utils passed",
    "[Sens-Decision] debug: debug message",
    "[Sens-Decision] info: info message",
    "[Sens-Decision] warning: warning message continued",
    "[Sens-Decision] error: error message",
    "[Sens-Decision] info: test logging passed",
    "[Sens-Decision] info: test sensor_hal_validation passed",
    "[Sens-Decision] info: test sensor_api_validation passed",
    "[Sens-Decision] info: test sensor_lifecycle passed",
    "[Sens-Decision] info: test sensor_init_reverse_rollback passed",
    "[Sens-Decision] info: test sensor_init_mixed_state_rollback passed",
    "[Sens-Decision] info: test release_resets_sensor_history passed",
    "[Sens-Decision] info: test encoder_conversion passed",
    "[Sens-Decision] info: test encoder_time_and_wrap passed",
    "[Sens-Decision] info: test active_sensor_config_snapshot passed",
    "[Sens-Decision] info: test imu_conversion_filter passed",
    "[Sens-Decision] info: test ir_mapping passed",
    "[Sens-Decision] info: test preprocess_partial_failure passed",
    "[Sens-Decision] info: test vtable_frame_metadata_coherence passed",
    "[Sens-Decision] info: test ekf_first_frame_baseline passed",
    "[Sens-Decision] info: test ekf_straight_travel passed",
    "[Sens-Decision] info: test ekf_rotation_in_place passed",
    "[Sens-Decision] info: test ekf_gyro_correction passed",
    "[Sens-Decision] info: test ekf_covariance_properties passed",
    "[Sens-Decision] info: test ekf_timestamp_rollback passed",
    "[Sens-Decision] info: test ekf_excessive_dt passed",
    "[Sens-Decision] info: test ekf_nan_observation passed",
    "[Sens-Decision] info: test ekf_repeated_failures_clear_valid passed",
    "[Sens-Decision] info: test perception_center_masks passed",
    "[Sens-Decision] info: test perception_lateral_signs passed",
    "[Sens-Decision] info: test perception_intersection_event passed",
    "[Sens-Decision] info: test perception_curve_entry_event passed",
    "[Sens-Decision] info: test perception_line_lost_event passed",
    "[Sens-Decision] info: test perception_loss_count_reset passed",
    "[Sens-Decision] info: test perception_timestamp_rollback passed",
    "[Sens-Decision] info: test perception_null_input passed",
    "[Sens-Decision] info: test perception_heading_derivative passed",
    "[Sens-Decision] warning: behavior changed from IDLE to LINE_FOLLOW",
    "[Sens-Decision] info: test behavior_idle_start_gating passed",
    "[Sens-Decision] warning: behavior changed from IDLE to LINE_FOLLOW",
    "[Sens-Decision] warning: behavior changed from LINE_FOLLOW to APPROACH_CURVE",
    "[Sens-Decision] info: test behavior_line_to_approach_curve passed",
    "[Sens-Decision] warning: behavior changed from APPROACH_CURVE to CURVE",
    "[Sens-Decision] info: test behavior_approach_to_curve passed",
    "[Sens-Decision] warning: behavior changed from CURVE to LINE_FOLLOW",
    "[Sens-Decision] info: test behavior_curve_exit passed",
    "[Sens-Decision] warning: behavior changed from LINE_FOLLOW to LINE_LOST_DEGRADED",
    "[Sens-Decision] warning: behavior changed from LINE_LOST_DEGRADED to LINE_FOLLOW",
    "[Sens-Decision] info: test behavior_short_loss_recovery passed",
    "[Sens-Decision] warning: behavior changed from LINE_FOLLOW to LINE_LOST_DEGRADED",
    "[Sens-Decision] warning: behavior changed from LINE_LOST_DEGRADED to STOPPED",
    "[Sens-Decision] info: test behavior_persistent_loss passed",
    "[Sens-Decision] warning: behavior changed from LINE_FOLLOW to STOPPED",
    "[Sens-Decision] info: test behavior_immediate_stop passed",
    "[Sens-Decision] warning: behavior changed from LINE_FOLLOW to LINE_LOST_DEGRADED",
    "[Sens-Decision] warning: behavior changed from LINE_LOST_DEGRADED to FAULT",
    "[Sens-Decision] info: test behavior_fault_on_failures passed",
    "[Sens-Decision] info: test behavior_reset_reject_unhealthy passed",
    "[Sens-Decision] warning: behavior changed from FAULT to IDLE",
    "[Sens-Decision] info: test behavior_reset_to_idle passed",
    "[Sens-Decision] info: test behavior_no_transition_log passed",
    "[Sens-Decision] info: test trajectory_empty_path passed",
    "[Sens-Decision] info: test trajectory_nan_path passed",
    "[Sens-Decision] info: test trajectory_straight_path_progress passed",
    "[Sens-Decision] info: test trajectory_curvature_speed_limit passed",
    "[Sens-Decision] info: test trajectory_accel_constraint passed",
    "[Sens-Decision] info: test trajectory_decel_constraint passed",
    "[Sens-Decision] info: test trajectory_jerk_constraint passed",
    "[Sens-Decision] info: test trajectory_angular_velocity passed",
    "[Sens-Decision] info: test trajectory_stopped_state passed",
    "[Sens-Decision] info: test trajectory_fault_state passed",
    "[Sens-Decision] info: test trajectory_stopped_decel_limit passed",
    "[Sens-Decision] info: test trajectory_fault_decel_limit passed",
    "[Sens-Decision] info: test trajectory_alpha_uses_vehicle_omega passed",
    "[Sens-Decision] info: test trajectory_zero_length_segment passed",
    "[Sens-Decision] info: test trajectory_backward_segment passed",
    "[Sens-Decision] info: test trajectory_line_lost_frozen_target passed",
    "[Sens-Decision] warning: behavior changed from IDLE to STOPPED",
    "[Sens-Decision] info: test complete_driving_sequence passed",
    "[Sens-Decision] info: test imu_failure_cascade passed",
    "[Sens-Decision] info: test summary: passed=65, failed=0"
)

if ($testOutput.Count -ne $expectedOutput.Count) {
    [Console]::Error.WriteLine(
        "invalid test output count: expected $($expectedOutput.Count), got $($testOutput.Count)")
    exit 1
}
for ($index = 0; $index -lt $expectedOutput.Count; $index++) {
    $text = [string]$testOutput[$index]
    if ($text -cne $expectedOutput[$index]) {
        [Console]::Error.WriteLine(
            "invalid test output at line $($index + 1): expected '$($expectedOutput[$index])', got '$text'")
        exit 1
    }
    [Console]::Out.WriteLine($text)
}
exit $testExitCode

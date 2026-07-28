# MSPM0 Controller Modules Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the existing PD line follower with a tested 500 Hz Motion Control loop and a 50 Hz hybrid square-path/IR Sens-Decision loop on the MSPM0G3507 robot.

**Architecture:** Reusable ICM42688, MCP23017, Motion Control, and Sens-Decision sources live under `modules/`. Target-only adapters in `src/` bind them to the four TB6612 motors, four software quadrature encoders, SPI1 IMU, and I2C0 line-sensor expander. Pure Pursuit on the known counter-clockwise 1 m square supplies nominal motion, while IR lateral and heading errors correct angular velocity to prevent dead-reckoning drift.

**Tech Stack:** C99, TI MSPM0 DriverLib, TI SysConfig, FreeRTOS, ARMCLANG/Keil AC6 target build, MinGW GCC host tests, PowerShell 5.1.

## Global Constraints

- MCU is TI MSPM0G3507, LQFP-48, running at 32 MHz with 128 KiB Flash and 32 KiB SRAM.
- Keep `CONTROL_TIMER` on TIMG0 as a 2 ms/500 Hz FreeRTOS task-notification source.
- Run Motion Control at 500 Hz and Sens-Decision exactly every ten fast cycles at 50 Hz with `dt = 0.020f`.
- Model M1/M2 as the logical left side and M3/M4 as the logical right side.
- Preserve the existing software quadrature decoder; wrap it rather than replacing it with timer encoder mode.
- Track the outer edge of a 1.00 m by 1.00 m square counter-clockwise; support a target of 1 through 5 laps.
- Use Pure Pursuit as nominal path feedforward and bounded IR lateral/heading feedback as angular correction.
- Do not migrate or compile OLED, `example_usage.c`, `temp/` firmware inputs, `motor/`, or `servo/`.
- All peripheral polling has finite timeout behavior and all mandatory initialization failures leave all four motors stopped.
- Use raw ICM42688 LSB data at the Sens-Decision HAL boundary; Sens-Decision owns scale and bias conversion.
- Use conda `base` for any Python command. No Python command is required by this plan's normal test path.
- Do not modify files under `pictures/`.
- Do not commit unless the user explicitly requests a commit during execution.
- The installed target tool paths are currently unavailable. Never claim a target build passed without fresh EIDE/ARMCLANG output.

## File Map

**Migrated reusable modules**

- `modules/ICM42688/inc/*.h`, `modules/ICM42688/src/*.c`: IMU register driver, raw samples, AHRS, MSPM0 SPI adapter.
- `modules/MCP23017/inc/mcp23017.h`, `modules/MCP23017/src/mcp23017.c`: status-preserving I2C GPIO-expander access.
- `modules/Motion Control/inc/*.h`, `modules/Motion Control/src/{motion_control,motion_feedback,motion_feedforward,motion_kinematics}.c`: 500 Hz wheel controller.
- `modules/Sens-Decision/inc/*.h`, `modules/Sens-Decision/src/*.c`: sensor preprocessing, EKF, perception, planner, and nominal trajectory generation.

**New target glue**

- `inc/platform_time.h`, `src/platform_time.c`: monotonic 32-bit and 64-bit microsecond time APIs.
- `inc/encoder_hw_bridge.h`, `src/encoder_hw_bridge.c`: isolates the low-level `encoder.h` enum namespace.
- `inc/encoder_adapter.h`, `src/encoder_adapter.c`: exposes `EncoderInterface_t`.
- `inc/motor_adapter.h`, `src/motor_adapter.c`: exposes `MotorInterface_t`.
- `inc/sensor_adapter.h`, `src/sensor_adapter.c`: exposes `sensor_hal_t` and preserves read failures.
- `inc/square_path.h`, `src/square_path.c`: static CCW square path, hybrid IR correction, and guarded lap counter.
- `inc/control_app.h`, `src/control_app.c`: owns module instances, initialization, 50 Hz pipeline, and 500 Hz update.
- `src/main.c`: hardware-safe startup, FreeRTOS task, and TIMG0 ISR only.

**Host tests**

- `tests/fakes/ti_msp_dl_config.h`: DriverLib fake contract used by target adapter tests.
- `tests/test_platform_time.c`
- `tests/test_mcp23017.c`
- `tests/test_motion_control.c`
- `tests/test_target_adapters.c`
- `tests/test_square_path.c`
- `tests/test_control_app.c`
- `tests/run_tests.ps1`: deterministic GCC build/run entry point.

**Generated/build metadata**

- `NewProject1.syscfg`: add a dedicated 1 MHz free-running timebase without changing TIMG0.
- `.eide/eide.yml`: restore project metadata and list intended source/include directories plus `portasm.c`.
- `.eide/files.options.yml`: restore matching per-file configuration if required by EIDE.
- `.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/settings.json`: restore local EIDE tasks while keeping machine-specific paths easy to edit.

---

### Task 1: Establish the Migrated Source Baseline and Host Test Runner

**Files:**
- Create: `modules/ICM42688/inc/ahrs_hal.h`
- Create: `modules/ICM42688/inc/icm42688_hal.h`
- Create: `modules/ICM42688/inc/icm42688_mspm0.h`
- Create: `modules/ICM42688/src/ahrs_hal.c`
- Create: `modules/ICM42688/src/icm42688_hal.c`
- Create: `modules/ICM42688/src/icm42688_mspm0.c`
- Create: `modules/MCP23017/inc/mcp23017.h`
- Create: `modules/MCP23017/src/mcp23017.c`
- Create: `modules/Motion Control/inc/*.h`
- Create: `modules/Motion Control/src/motion_control.c`
- Create: `modules/Motion Control/src/motion_feedback.c`
- Create: `modules/Motion Control/src/motion_feedforward.c`
- Create: `modules/Motion Control/src/motion_kinematics.c`
- Create: `modules/Sens-Decision/inc/*.h`
- Create: `modules/Sens-Decision/src/*.c`
- Create: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: source files under `E:/B306/2026/电赛/modules`.
- Produces: a target-local, self-contained copy of the four included modules and one host-test command.

- [ ] **Step 1: Add only production module sources with `apply_patch`**

Read each source file listed in this task from `E:/B306/2026/电赛/modules`, then add the same content under the target-local `modules/` path with `apply_patch`. Preserve the source text exactly at this baseline step. Do not add `example_usage.c`, `temp/`, OLED, `motor/`, or `servo/`.

Expected: only `inc/` and production `src/` files exist under the four target modules, and a content diff against the source paths is empty before target-specific edits.

- [ ] **Step 2: Create the test runner skeleton**

Create `tests/run_tests.ps1` with strict process handling:

```powershell
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

Write-Host "Host tests: PASS"
```

- [ ] **Step 3: Run portable module baselines**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File "E:\B306\2026\电赛\modules\ICM42688\temp\tests\run_tests.ps1"
powershell -ExecutionPolicy Bypass -File "E:\B306\2026\电赛\modules\Sens-Decision\temp\build_and_test.ps1"
gcc -std=c99 -Wall -Wextra -Werror -pedantic -fsyntax-only -I".\modules\Motion Control\inc" ".\modules\Motion Control\src\motion_kinematics.c" ".\modules\Motion Control\src\motion_feedforward.c" ".\modules\Motion Control\src\motion_feedback.c" ".\modules\Motion Control\src\motion_control.c"
```

Expected: ICM reports `ICM42688 host tests: PASS`, Sens-Decision reports `passed=65, failed=0`, and GCC syntax checking exits 0.

- [ ] **Step 4: Record a review checkpoint**

Review `git diff -- modules tests/run_tests.ps1` and confirm copied sources match the source modules exactly before target-specific changes begin.

---

### Task 2: Add a Monotonic Timebase and Complete ICM Raw Samples

**Files:**
- Create: `inc/platform_time.h`
- Create: `src/platform_time.c`
- Create: `tests/test_platform_time.c`
- Modify: `modules/ICM42688/inc/icm42688_hal.h`
- Modify: `modules/ICM42688/src/icm42688_hal.c`
- Modify: `modules/ICM42688/src/icm42688_mspm0.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: dedicated 1 MHz timer count after Task 7 generates `ICM42688_TIMER_INST`.
- Produces: `void PlatformTime_Init(void)`, `uint32_t PlatformTime_GetUs32(void)`, `uint64_t PlatformTime_GetUs64(void)`, and `icm42688_data_t.temperature_raw`.

- [ ] **Step 1: Write failing wrap and timer-direction tests**

Create `tests/test_platform_time.c` that injects fake down-counter samples and asserts:

```c
assert(PlatformTime_UpCountFromDownCount(UINT32_MAX) == 0U);
assert(PlatformTime_UpCountFromDownCount(UINT32_MAX - 25U) == 25U);
assert(PlatformTime_Extend32(0xFFFFFFF0U) == UINT64_C(0xFFFFFFF0));
assert(PlatformTime_Extend32(0x00000010U) == UINT64_C(0x100000010));
assert(PlatformTime_Extend32(0x00000020U) == UINT64_C(0x100000020));
```

Declare the tested pure helpers in `platform_time.h` so they are testable without DriverLib.

- [ ] **Step 2: Run the test and verify it fails**

Add a GCC invocation for `test_platform_time.c` and `src/platform_time.c` to `tests/run_tests.ps1`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: compile failure because `platform_time.h` and its functions do not exist.

- [ ] **Step 3: Implement the pure monotonic extension and target wrapper**

Use these exact public signatures:

```c
void PlatformTime_Init(void);
uint32_t PlatformTime_UpCountFromDownCount(uint32_t down_count);
uint64_t PlatformTime_Extend32(uint32_t now32);
uint32_t PlatformTime_GetUs32(void);
uint64_t PlatformTime_GetUs64(void);
```

Implement down-count inversion as `UINT32_MAX - down_count`. Implement wrap extension by retaining the previous 32-bit sample and a 64-bit high word; when `now32 < previous32`, add `UINT64_C(1) << 32`. Protect the shared extension state with a short critical section in target code. `PlatformTime_Init()` starts the dedicated timer and resets extension state.

- [ ] **Step 4: Run the platform-time test**

Run `powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"`.

Expected: platform-time test passes, including one 32-bit wrap.

- [ ] **Step 5: Write a failing ICM temperature test**

Extend the migrated ICM host test so a 14-byte burst beginning at temperature registers produces:

```c
assert(data.temperature_raw == expected_temperature_raw);
assert(data.acc_raw.x == expected_accel_x);
assert(data.gyro_raw.z == expected_gyro_z);
```

Expected before implementation: compile failure because `temperature_raw` is absent.

- [ ] **Step 6: Read temperature and expose raw samples without changing units**

Add this field to `icm42688_data_t`:

```c
int16_t temperature_raw;
```

Change `icm42688_read()` to burst-read temperature plus accelerometer and gyro bytes from `TEMP_DATA1`, decode all signed big-endian fields, and keep the existing `acc_g` and calibrated `gyro_dps` outputs for AHRS users. The Sens-Decision adapter will consume only `temperature_raw`, `acc_raw`, and `gyro_raw`.

- [ ] **Step 7: Bind AHRS to the platform clock**

Replace the adapter's direct `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)` callback with:

```c
static uint32_t timer_get_time_us(void)
{
    return PlatformTime_GetUs32();
}
```

Do not let `icm42688_mspm0.c` configure or start TIMG0.

- [ ] **Step 8: Run ICM and platform-time tests**

Run the target-local test runner and the original ICM suite.

Expected: all tests pass; a raw temperature sample is available; the AHRS callback sees increasing microseconds.

---

### Task 3: Make MCP23017 Failures Observable and Recoverable

**Files:**
- Modify: `modules/MCP23017/inc/mcp23017.h`
- Modify: `modules/MCP23017/src/mcp23017.c`
- Create: `tests/test_mcp23017.c`
- Create: `tests/fakes/ti_msp_dl_config.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `I2C0_INST` DriverLib operations.
- Produces: `mcp23017_status_t MCP23017_Init(void)` and `mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs)`.

- [ ] **Step 1: Write failing driver tests with scripted fake I2C states**

Cover these exact cases:

```c
assert(MCP23017_ReadInputs(NULL) == MCP23017_STATUS_INVALID_ARGUMENT);
assert(MCP23017_Init() == MCP23017_STATUS_OK);
assert(MCP23017_ReadInputs(&mask) == MCP23017_STATUS_OK);
assert(mask == UINT16_C(0x0A55));
assert(run_with_tx_busy_forever() == MCP23017_STATUS_TIMEOUT);
assert(run_with_nack() == MCP23017_STATUS_IO_ERROR);
```

The fake header supplies only the exact `DL_I2C_*` symbols used by the driver and records TX bytes, target address, transfer sizes, and timeout loops.

- [ ] **Step 2: Run the test and verify failure**

Run `tests/run_tests.ps1`.

Expected: compile failure because `mcp23017_status_t` and status constants do not exist.

- [ ] **Step 3: Implement a status enum and bounded transactions**

Use:

```c
typedef enum {
    MCP23017_STATUS_OK = 0,
    MCP23017_STATUS_INVALID_ARGUMENT,
    MCP23017_STATUS_TIMEOUT,
    MCP23017_STATUS_IO_ERROR
} mcp23017_status_t;
```

Every wait exits after `MCP23017_I2C_TIMEOUT` iterations. Clear/reinitialize transfer state after a timeout or controller error using the DriverLib operation supported by the selected SDK. Preserve the two-byte GPIOA/GPIOB result only on success.

- [ ] **Step 4: Run the MCP tests**

Expected: all success, invalid-argument, timeout, and I/O-error tests pass.

- [ ] **Step 5: Update existing line-sensor compatibility**

Modify `src/line_sensor.c` so its legacy API returns zero unless:

```c
MCP23017_ReadInputs(&mask) == MCP23017_STATUS_OK
```

This compatibility API remains available, but the new sensor adapter must call MCP23017 directly so it can preserve failure status.

---

### Task 4: Test and Correct Motion Control Semantics

**Files:**
- Create: `tests/test_motion_control.c`
- Modify: `modules/Motion Control/inc/motion_control.h`
- Modify: `modules/Motion Control/src/motion_control.c`
- Modify: `modules/Motion Control/src/motion_feedback.c`
- Modify: `modules/Motion Control/inc/motion_config.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: four cumulative encoder callbacks and signed differential PWM callbacks.
- Produces: safe stop, forward, reverse, turn-in-place, saturation, restart, and callback-validation behavior at 500 Hz.

- [ ] **Step 1: Write failing behavior tests**

Use fake encoder and motor vtables and assert:

```c
assert(!MotionControl_Init(&ctrl, &missing_encoder_callback, &motor));
assert(!MotionControl_Init(&ctrl, &encoder, &missing_motor_callback));
assert(MotionControl_Init(&ctrl, &encoder, &motor));

MotionControl_Start(&ctrl);
MotionControl_SetVelocityCommand(&ctrl, 0.0f, 0.0f);
run_cycles(&ctrl, 100U);
assert(last_left_pwm == 0 && last_right_pwm == 0);

MotionControl_SetVelocityCommand(&ctrl, -0.20f, 0.0f);
run_cycles(&ctrl, 100U);
assert(last_left_pwm < 0 && last_right_pwm < 0);

MotionControl_EmergencyStop(&ctrl);
assert(stop_call_count == 1U);
```

Also instantiate two controllers and prove their smoothing histories are independent.

- [ ] **Step 2: Run and observe the negative-speed failure**

Expected: reverse-command test fails because the current linear clamp converts negative/zero input to positive `MIN_SPEED`.

- [ ] **Step 3: Implement symmetric deadband and instance-owned smoothing**

Remove function-static smoothing state. Use `ctrl->smoothed_v` as previous state. Apply minimum speed only when the command magnitude is nonzero:

```c
if (fabsf(v_limited) < MIN_SPEED) {
    v_limited = 0.0f;
}
v_limited = clamp_float(v_limited, -MAX_SPEED, MAX_SPEED);
```

Validate every callback used by `MotionControl_Init()`. Reset command, smoothing, PI integrals, feedforward history, and estimator history in Start/Stop transitions.

- [ ] **Step 4: Correct PI anti-windup bounds**

Store the integral contribution in output units or set accumulator bounds to `feedback_limit / ki` when `ki > 0`. Add a test that holds a saturated error for 2 seconds, removes the error, and verifies output returns inside the non-saturated range rather than remaining wound up.

- [ ] **Step 5: Use measured-count semantics in configuration comments**

Rename ambiguous documentation around `ENCODER_PPR`: it means quadrature transition counts per complete wheel revolution after gearing. Do not multiply by the currently unused `GEAR_RATIO` unless a test and measured hardware convention require it.

- [ ] **Step 6: Run strict Motion Control tests**

Compile with `-std=c99 -Wall -Wextra -Werror -pedantic` and run.

Expected: callback validation, zero, reverse, turning, saturation, anti-windup, stop, restart, and two-instance tests all pass.

---

### Task 5: Add Encoder, Motor, and Sensor Adapters

**Files:**
- Modify: `inc/encoder.h`
- Modify: `src/encoder.c`
- Create: `inc/encoder_hw_bridge.h`
- Create: `src/encoder_hw_bridge.c`
- Create: `inc/encoder_adapter.h`
- Create: `src/encoder_adapter.c`
- Create: `inc/motor_adapter.h`
- Create: `src/motor_adapter.c`
- Create: `inc/sensor_adapter.h`
- Create: `src/sensor_adapter.c`
- Create: `tests/test_target_adapters.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `Encoder_GetCount`, `Encoder_ResetCount`, `Motor_SetSpeed`, `Motor_Stop`, `Motor_Init`, `icm42688_read`, and `MCP23017_ReadInputs`.
- Produces: `EncoderInterface_t *EncoderAdapter_GetInterface(void)`, `MotorInterface_t *MotorAdapter_GetInterface(void)`, and `const sensor_hal_t *SensorAdapter_GetHal(void)`.

- [ ] **Step 1: Write failing mapping and failure-propagation tests**

Assert explicit mappings:

```c
assert(read_motion_encoder(ENCODER_LEFT_FRONT)  == fake_count[0]);
assert(read_motion_encoder(ENCODER_LEFT_REAR)   == fake_count[1]);
assert(read_motion_encoder(ENCODER_RIGHT_FRONT) == fake_count[2]);
assert(read_motion_encoder(ENCODER_RIGHT_REAR)  == fake_count[3]);

motor->setDifferentialPWM(321, -456);
assert(last_m1 == 321 && last_m2 == 321);
assert(last_m3 == -456 && last_m4 == -456);

assert(sensor_hal->read_ir_mask(&mask) == SD_ERR_READ);
assert(sensor_hal->read_imu_raw(&imu) == SD_OK);
assert(imu.temperature == fake_icm.temperature_raw);
```

- [ ] **Step 2: Resolve the encoder enum collision**

Rename only the low-level terminal enumerator from `ENCODER_COUNT` to `ENCODER_ID_COUNT` in `inc/encoder.h` and `src/encoder.c`. Keep `ENCODER_M1` through `ENCODER_M4` stable.

- [ ] **Step 3: Isolate low-level encoder headers**

`encoder_hw_bridge.h` includes only `<stdint.h>` and exports:

```c
int32_t EncoderHwBridge_GetCount(uint8_t physical_id);
void EncoderHwBridge_ResetCount(uint8_t physical_id);
```

`encoder_hw_bridge.c` includes `encoder.h`, checks `physical_id < ENCODER_ID_COUNT`, and protects reset operations from GPIO ISR races with a minimal interrupt critical section.

- [ ] **Step 4: Implement explicit Motion Control adapters**

`encoder_adapter.c` includes `motion_feedback.h` and the bridge header, uses a `switch` for all four IDs, and returns a module-owned non-const `EncoderInterface_t` because current Motion Control stores mutable pointers.

`motor_adapter.c` returns:

```c
static MotorInterface_t g_motor_interface = {
    .setDifferentialPWM = Motor_SetSpeed,
    .stop = Motor_Stop,
    .init = Motor_Init,
};
```

- [ ] **Step 5: Implement the status-preserving Sensor HAL**

Use exact callback signatures:

```c
static sd_status_t ReadEncoder(uint8_t index, int32_t *count);
static sd_status_t ReadImu(imu_raw_data_t *data);
static sd_status_t ReadIr(uint16_t *active_mask);
```

`ReadImu` copies raw accel, raw gyro, and raw temperature. It does not copy `acc_g` or `gyro_dps`. `ReadIr` converts MCP timeout to `SD_ERR_TIMEOUT`, other driver failures to `SD_ERR_READ`, masks to `0x0FFF`, and applies active polarity exactly once.

- [ ] **Step 6: Run adapter tests**

Expected: enum collision is gone; four encoder mappings, left/right motor fan-out, IMU raw units, IR polarity, null arguments, and failure propagation all pass.

---

### Task 6: Implement 50 Hz Hybrid Square Tracking and Lap Counting

**Files:**
- Create: `inc/square_path.h`
- Create: `src/square_path.c`
- Create: `tests/test_square_path.c`
- Modify: `modules/Sens-Decision/src/trajectory_generate.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `trajectory_point_t`, `perception_result_t`, current path index/progress, and target lap count.
- Produces: a static CCW 1 m square path, corrected `(v, omega)`, and a guarded `lap_complete`/`target_reached` state.

- [ ] **Step 1: Write failing square geometry tests**

Define the coordinate frame as A=`(0,0)`, B=`(1,0)`, D=`(1,1)`, C=`(0,1)`, with CCW traversal `A -> C -> D -> B -> A`. Generate points at no more than 20 mm spacing and assert:

```c
assert(SquarePath_GetPointCount() >= 200U);
assert_point_near(point_at_a, 0.0f, 0.0f);
assert_all_points_inside_bounds(0.0f, 1.0f);
assert_path_is_closed());
assert_path_direction_is_counter_clockwise());
```

The exact A/B/C/D assignment follows the contest figure: AB and CD are opposite sides, AC and BD are opposite sides.

- [ ] **Step 2: Write failing Pure Pursuit geometry tests**

The existing trajectory module does not yet calculate Pure Pursuit curvature from the vehicle pose. Add tests that place the vehicle on a straight segment and before a corner:

```c
assert_near(generate_at(0.0f, 0.50f, -M_PI_2).omega, 0.0f, 1e-3f);
assert(generate_before_corner().omega > 0.0f);
assert(fabsf(generate_with_target_on_left().curvature) > 0.0f);
```

For lookahead target `(x_t, y_t)`, vehicle pose `(x, y, theta)`, and lookahead distance `L`, transform the target into vehicle coordinates and expect:

```text
y_local   = -sin(theta) * (x_t - x) + cos(theta) * (y_t - y)
curvature = 2 * y_local / (L * L)
omega     = v * curvature
```

This calculation replaces the current behavior of copying `target_point->curvature` into the command.

- [ ] **Step 3: Write failing hybrid correction tests**

Use:

```c
omega = SquarePath_CorrectOmega(nominal_omega, lateral_error,
                                heading_error, &config);
```

Assert zero error preserves nominal omega, opposite lateral errors produce opposite corrections, heading correction has configured sign, and output is clamped to `max_omega_radps`.

- [ ] **Step 4: Write failing guarded lap tests**

Simulate path progress crossing the start reference. Assert no lap is counted before leaving the start guard, no repeated count occurs while remaining near the line, one full monotonic wrap counts exactly one lap, and target laps outside `1..5` are rejected.

- [ ] **Step 5: Implement square path and hybrid configuration**

Expose:

```c
typedef struct {
    float lateral_gain;
    float heading_gain;
    float max_omega_radps;
    uint8_t target_laps;
} square_path_config_t;

typedef struct {
    uint8_t completed_laps;
    bool left_start_guard;
    bool target_reached;
} lap_counter_t;

const path_point_t *SquarePath_GetPoints(void);
size_t SquarePath_GetPointCount(void);
float SquarePath_CorrectOmega(float nominal_omega, float lateral_error,
                              float heading_error,
                              const square_path_config_t *config);
bool SquarePath_UpdateLap(lap_counter_t *counter, size_t nearest_index,
                          size_t path_count);
```

Keep gains in one application configuration object; do not hide tuneable constants in `main.c`.

- [ ] **Step 6: Implement real Pure Pursuit and harden preconditions**

Compute curvature from the selected lookahead point and current vehicle pose using the formula in Step 2. Keep path-point curvature only as metadata for speed limiting and behavior input. Add tests and checks for uninitialized generator, missing path, path count below 2, non-finite vehicle state, and `dt <= 0`. Return `SD_ERR_NOT_INITIALIZED`, `SD_ERR_INVALID_ARGUMENT`, or `SD_ERR_NUMERIC` rather than dividing by invalid `dt`.

- [ ] **Step 7: Run square-path and original Sens-Decision tests**

Expected: geometry, correction, lap guard, target-lap, and trajectory-precondition tests pass; the original 65 Sens-Decision tests remain green.

---

### Task 7: Restore a Reproducible EIDE/SysConfig Project Definition

**Files:**
- Modify: `NewProject1.syscfg`
- Create: `.eide/eide.yml`
- Create: `.eide/files.options.yml`
- Create: `.vscode/tasks.json`
- Create: `.vscode/launch.json`
- Create: `.vscode/settings.json`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: one matched MSPM0 SDK/SysConfig installation selected by the user at build time.
- Produces: `ICM42688_TIMER_INST`, all intended source/include lists, FreeRTOS `portasm.c`, and EIDE build/upload tasks.

- [ ] **Step 1: Restore metadata from the matching template, then make paths project-relative**

Use `controller/mspm0_2025e_template_v1/.eide` and `.vscode` as the structural baseline. Remove ignore rules that prevent project metadata from being tracked. Keep installation roots in `.vscode/settings.json` rather than duplicating absolute paths across files.

- [ ] **Step 2: Add production sources and includes explicitly**

Add these module source directories to EIDE and exclude `example_usage.c`, OLED, and all `temp/` paths. Add all four module `inc/` paths plus `inc/` and `Debug/`.

Add FreeRTOS:

```text
tasks.c
list.c
queue.c
portable/GCC/ARM_CM0/port.c
portable/GCC/ARM_CM0/portasm.c
portable/MemMang/heap_4.c
```

The map must contain `PendSV_Handler`, `SVC_Handler`, and `vStartFirstTask` from `portasm.o`.

- [ ] **Step 3: Add a dedicated SysConfig timer**

In `NewProject1.syscfg`, add a timer instance named `ICM42688_TIMER`, assigned to a free 32-bit-capable timer such as TIMG12, configured from 32 MHz BUSCLK with a divide/prescale combination yielding exactly 1 MHz, maximum period, automatic start disabled, and no interrupt. `PlatformTime_Init()` owns start timing.

Do not edit `Debug/ti_msp_dl_config.h` or `.c` manually.

- [ ] **Step 4: Select one SDK/SysConfig version pair**

Prefer the repository SDK `controller/documents/sdk` version `2.10.00.04` with SysConfig `1.26.2`, or install and consistently use the older `2.05.01.00`/`1.24.1` pair. Do not generate with one pair and compile against the other.

- [ ] **Step 5: Generate SysConfig and inspect output when tools are available**

With the repository SDK selected, install SysConfig 1.26.2 at `C:\ti\sysconfig_1.26.2` and run:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" --product "E:\B306\2026\电赛\controller\documents\sdk\.metadata\product.json" --device MSPM0G3507 --package "LQFP-48(PT)" --script ".\NewProject1.syscfg" --output ".\Debug" --compiler keil
```

Expected: generated header contains `ICM42688_TIMER_INST`; existing motor, encoder, I2C0, SPI1, DMA, UART, and `CONTROL_TIMER_INST` mappings remain unchanged.

- [ ] **Step 6: Build or record the environmental blocker accurately**

Run EIDE Rebuild. Expected success artifact is `build/Debug/NewProject1.axf`. If ARMCLANG/SysConfig remains unavailable, record that exact blocker and continue only with host-tested work; do not label the firmware build as passing.

---

### Task 8: Integrate the Application Pipeline and FreeRTOS Scheduling

**Files:**
- Create: `inc/control_app.h`
- Create: `src/control_app.c`
- Create: `tests/test_control_app.c`
- Modify: `src/main.c`
- Modify: `inc/FreeRTOSConfig.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: platform time, all three adapters, square path, Motion Control, and Sens-Decision APIs.
- Produces: `bool ControlApp_Init(uint8_t target_laps)`, `void ControlApp_RunFastCycle(void)`, and `void ControlApp_EmergencyStop(void)`.

- [ ] **Step 1: Write failing scheduler-divider tests**

Fake all module update calls and assert:

```c
for (unsigned i = 0; i < 100U; ++i) {
    ControlApp_RunFastCycle();
}
assert(motion_update_calls == 100U);
assert(decision_update_calls == 10U);
assert(last_decision_dt == 0.020f);
```

Also assert the decision update precedes Motion Control on every tenth cycle.

- [ ] **Step 2: Write failing startup/fault tests**

Inject failures from MCP initialization, ICM identity, sensor HAL initialization, trajectory path setup, and Motion Control initialization. Every case must call the motor stop path and return `false`.

- [ ] **Step 3: Implement application-owned static state**

`control_app.c` owns static-lifetime configuration, path pointers, module instances, sensor frames, and outputs. This satisfies APIs that retain configuration/path pointers.

Initialization order is:

```text
Motor_Init -> Motor_Stop -> Encoder_Init -> MCP23017_Init and read
-> PlatformTime_Init -> ICM bind/init -> configure raw scales/bias
-> Sensor HAL configure/init -> Sens-Decision objects/path
-> Motion Control init/start
```

Do not call `icm42688_calibrate_gyro()` and then forward uncorrected raw gyro as if calibrated. Instead, collect stationary raw gyro samples during startup and write their converted mean into `g_sens_decision_config.imu.gyro_bias_radps[]`.

- [ ] **Step 4: Implement the 50 Hz pipeline**

On every tenth cycle:

```c
status = preprocess_update(PlatformTime_GetUs64(), &frame);
status = state_evaluator_update(&state_evaluator, &frame);
status = perception_update(&perception, &frame.ir, frame.timestamp_us,
                           &perception_result);
status = behavior_planner_update(&planner, &behavior_input, &behavior_output);
status = trajectory_generate(&trajectory_generator, &state_evaluator.state,
                             &behavior_output, 0.020f, &trajectory);
corrected_omega = SquarePath_CorrectOmega(trajectory.omega,
                                          perception_result.lateral_error,
                                          perception_result.heading_error,
                                          &square_config);
MotionControl_SetVelocityCommand(&motion, trajectory.v, corrected_omega);
```

Use actual field names from `sensor_frame_t` and `state_evaluator_t`; compile the host integration test to catch signature drift. Maintain `BEHAVIOR_CMD_START` until localization and line validity allow the planner to enter running state; do not send it only once at boot.

- [ ] **Step 5: Handle invalid data and lap completion**

Invalid IR data enters line-lost/degraded behavior, not zero-error behavior. Mandatory IMU/encoder failures increment a critical-failure count. Reaching target laps commands stop and transitions Motion Control to idle. Repeated critical failures call `MotionControl_EmergencyStop()`.

- [ ] **Step 6: Rewrite `main.c` as the RTOS shell**

Keep only safe startup, task creation, ISR notification, and fatal hooks. Set `CONTROL_TASK_STACK_WORDS` initially to `512U`. The control task starts TIMG0, blocks on notifications, calls `ControlApp_RunFastCycle()`, and periodically records `uxTaskGetStackHighWaterMark(NULL)` without synchronous logging inside the 2 ms path.

- [ ] **Step 7: Run host integration tests**

Expected: exact 500/50 Hz call counts, initialization ordering, persistent start command, IR correction, lap stop, and every injected failure's motor-stop assertion pass.

---

### Task 9: Run Full Verification and Stage Hardware Bring-Up

**Files:**
- Modify: `WIRING_AND_SYSCONFIG.md`
- Modify: `docs/superpowers/specs/2026-07-18-modules-migration-design.md` only if implementation facts require correction
- Modify: `docs/superpowers/plans/2026-07-18-modules-migration.md` checkbox state during execution

**Interfaces:**
- Consumes: completed firmware and test suite.
- Produces: reproducible evidence for host behavior, target build status, pin mapping, measured physical parameters, timing, and safe vehicle operation.

- [ ] **Step 1: Run all host suites from clean build directories**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
powershell -ExecutionPolicy Bypass -File ".\modules\ICM42688\temp\tests\run_tests.ps1"
powershell -ExecutionPolicy Bypass -File ".\modules\Sens-Decision\temp\build_and_test.ps1"
```

Expected: all target-local tests pass, ICM reports PASS, and Sens-Decision reports 65/65 or a larger updated passing count.

- [ ] **Step 2: Run target rebuild when the matched toolchain exists**

Use EIDE Rebuild and inspect the map. Confirm intended module objects and `portasm.o` are present; OLED, `example_usage.o`, and temp tests are absent. Record Flash/RAM usage and ensure stack/heap fit inside 32 KiB SRAM.

- [ ] **Step 3: Verify sensors with motors disabled**

Confirm MCP input masks, timeout behavior with the device disconnected, ICM `WHO_AM_I`, stationary acceleration axis, raw gyro bias, temperature plausibility, monotonic timestamps across timer wrap simulation, and 50 Hz sensor-frame timestamps.

- [ ] **Step 4: Verify motor and encoder mapping off the ground**

At low PWM, command each side and direction separately. Confirm M1/M2 left, M3/M4 right, forward encoder sign, and no cross-channel count. Measure and document transition counts per complete wheel revolution, wheel radius, and effective track width.

- [ ] **Step 5: Tune wheel control before enabling path tracking**

Use conservative speed and log target/actual wheel speed outside the real-time task. Tune feedforward first, then PI. Verify zero command, reverse, emergency stop, saturation recovery, and no sustained oscillation.

- [ ] **Step 6: Measure real-time resource margins**

Measure worst-case fast cycle, worst-case combined 50 Hz cycle, missed TIMG0 notifications, and task stack high-water mark. Acceptance requires no persistent 2 ms deadline misses. Reduce stack from 512 words only after measured margin is documented.

- [ ] **Step 7: Test the contest square incrementally**

Run in this order:

```text
one straight segment at low speed
one corner at low speed
one complete counter-clockwise lap
two laps
configured 1..5 lap stop behavior
line-loss and sensor-disconnect safe-stop tests
```

Confirm the vehicle projection never fully leaves the 1.8 cm line. Tune Pure Pursuit lookahead and speed limits first, then IR lateral/heading gains. Do not raise speed until one-lap behavior and stop conditions are repeatable.

- [ ] **Step 8: Update hardware documentation**

Document the generated timebase peripheral, measured wheel values, encoder counts/revolution, per-channel direction signs, ICM axis orientation/bias, selected SDK/SysConfig versions, target lap selection mechanism, control gains, measured execution times, and final task stack margin in `WIRING_AND_SYSCONFIG.md`.

- [ ] **Step 9: Review the final diff**

Run `git status --short` and `git diff --check`. Review only migration-owned files, leave unrelated worktree changes untouched, and report any remaining target-tool or hardware-only verification gaps explicitly.

## Plan Self-Review

- Spec coverage: module scope, OLED exclusion, 500/50 Hz scheduling, four-motor differential mapping, encoder wrapping, ICM/MCP hardware, error propagation, square path, IR correction, lap count, build metadata, FreeRTOS port, timing, stack, and staged hardware tests all map to tasks above.
- Placeholder scan: every implementation step specifies concrete files, behavior, commands, and expected results.
- Type consistency: Motion Control adapters return mutable interface pointers; Sensor HAL returns a const pointer copied by `sensors_configure_hal`; raw IMU fields remain `int16_t`; application timestamps are `uint64_t`; AHRS timestamps are increasing `uint32_t` microseconds.
- Residual external dependency: target compilation and hardware acceptance cannot be completed until a matched MSPM0 SDK/SysConfig plus ARMCLANG/EIDE installation is available and the physical robot is connected.

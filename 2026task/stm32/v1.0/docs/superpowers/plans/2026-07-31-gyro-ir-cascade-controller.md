# Gyro and IR Cascade Controller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace MotionControl feedforward with encoder-only wheel PI and add a low-speed, forward-only IR-P plus gyro-PI cascade controller to playground tracking, with deterministic IR-PD degradation.

**Architecture:** MotionControl remains the shared wheel-speed execution layer and keeps the existing `(v, omega)` path for non-playground callers. A small app-specific `playground_controller` unit owns testable cascade math and gyro-health transitions; `playground_track.c` owns hardware sampling, scheduling, segment selection, and state-machine integration.

**Tech Stack:** C11, CMake 3.22+, native host tests with CTest, ARM GCC (`arm-none-eabi-gcc`), STM32F407 HAL, FreeRTOS, ICM42688 over SPI, 8-channel UART IR array.

## Global Constraints

- The first hardware milestone is stable tracking at approximately 0.25-0.30 m/s; do not optimize maximum speed in this change.
- Enable gyro-assisted cascade control only in `playground_track.c`; preserve `(v, omega)` semantics for `track_control_app.c` and `control_app.c`.
- Remove `motion_feedforward.c`, `motion_feedforward.h`, `Feedforward_t`, `MotionControl_SetFeedforward()`, and every `FF_*` parameter without a compatibility shim.
- Normal playground control is IR proportional outer loop, gyro angular-rate PI inner loop, and encoder wheel PI.
- Normal playground control may command a wheel forward or stopped, never reverse.
- A gyro sample older than 30 ms or three consecutive invalid 100 Hz reads enters IR-PD degradation.
- Ten consecutive valid, fresh 100 Hz gyro reads restore cascade mode.
- Do not modify historical files under `logs/` or original inputs under `pictures/`.
- Do not manually edit generated files under `cmake-build-debug/`.
- Preserve unrelated and pre-existing worktree changes; stage only files belonging to each task.
- Use ASCII in newly created source and test files.
- Update README, changelog, and operational tuning documents only after user hardware acceptance.

## File Map

### Create

- `Core/Inc/app/playground_controller.h`: app-specific cascade inputs, configuration, state, outputs, and pure update API.
- `Core/Src/app/playground_controller.c`: IR-P, gyro PI, forward-only target allocation, anti-windup, and degrade/recover transitions.
- `tests/CMakeLists.txt`: native test targets independent of STM32 HAL.
- `tests/test_motion_feedback.c`: deterministic PID and state-estimator tests.
- `tests/test_motion_control.c`: command-mode and pure wheel-PI tests with fake encoder/motor interfaces.
- `tests/test_perception.c`: detected-mask and line-validity tests.
- `tests/test_playground_controller.c`: cascade, saturation, degradation, recovery, and reset tests.
- `tests/test_icm42688_hal.c`: communication failure and initialization-state tests.

### Modify

- `CMakeLists.txt`: remove the feedforward source and add the playground controller source; host tests remain a standalone native CMake project under `tests/`.
- `modules/MotionControl/inc/motion_feedback.h`: add conditional-integration PID API and explicit estimator `dt`.
- `modules/MotionControl/src/motion_feedback.c`: implement conditional integration and `dt`-based speed calculation.
- `modules/MotionControl/inc/motion_control.h`: remove feedforward types/API, add command mode and direct wheel targets, and accept update `dt`.
- `modules/MotionControl/src/motion_control.c`: pure wheel PI, direct wheel target mode, zero-target reset, and final saturation.
- `modules/MotionControl/inc/motion_config.h`: remove feedforward parameters and correct wheel-PI limits/comments.
- `modules/MotionControl/src/motion_kinematics.c`: restore signed differential kinematics and leave the playground forward-only rule in playground target allocation.
- `modules/Sens-Decision/src/perception.c`: build the detected-channel mask from black-strength threshold crossings.
- `modules/IR-tracker/inc/ir_uart_sensor.h`: expose fresh-frame sequence metadata.
- `modules/IR-tracker/src/ir_uart_sensor.c`: increment sequence only after successful frame parsing.
- `Core/Inc/app/sensor_adapter.h`: expose a calibrated 100 Hz gyro read helper.
- `Core/Src/app/sensor_adapter.c`: reject reused IR frames as fresh and provide calibrated gyro Z reads.
- `modules/ICM42688/inc/icm42688_hal.h`: make communication callbacks return success/failure.
- `modules/ICM42688/src/icm42688_hal.c`: reject reads before initialization and propagate communication failures.
- `modules/ICM42688/src/icm42688_stm32.c`: convert HAL SPI results into callback status.
- `Core/Src/app/playground_track.c`: integrate the pure controller, reorder 50/100 Hz work, and implement hardware-mode degradation.
- `Core/Inc/app/playground_track.h`: document actual control layers and degraded startup behavior.
- `Core/Src/app/track_control_app.c`: pass the actual 0.010 s MotionControl update interval.
- `Core/Src/app/control_app.c`: pass the actual 0.002 s MotionControl update interval.
- `Core/Inc/app/motor_static_friction_test.h`: rename exported API to generic minimum-startup-PWM terminology if the uncommitted diagnostic remains present.
- `Core/Src/app/motor_static_friction_test.c`: remove deleted `FF_K_STATIC` claims while retaining the raw motor diagnostic.
- `Core/Src/app/motor_interactive_test.c`: update diagnostic callback names for the retained minimum-startup-PWM test.
- `Core/Src/freertos.c`: update diagnostic mode labels and leave the intended test mode selection explicit.

### Delete

- `modules/MotionControl/inc/motion_feedforward.h`
- `modules/MotionControl/src/motion_feedforward.c`

---

### Task 1: Native Test Harness, Conditional PI, and Correct Estimator Timing

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_motion_feedback.c`
- Modify: `modules/MotionControl/inc/motion_feedback.h`
- Modify: `modules/MotionControl/src/motion_feedback.c`

**Interfaces:**
- Produces: `float PID_UpdateConditional(PID_t *pid, float setpoint, float measurement, float dt, bool allow_integral)`.
- Produces: `void StateEst_Init(StateEstimator_t *est, EncoderInterface_t *encoder, float wheel_radius, float encoder_ppr)`.
- Produces: `void StateEst_Update(StateEstimator_t *est, float dt)`.
- Preserves: `PID_Update()` as the always-integrating wrapper for callers that do not require actuator-aware saturation handling.

- [ ] **Step 1: Create a minimal native CTest project**

Create `tests/CMakeLists.txt` with a standalone native project. Do not include the ARM toolchain or top-level STM32 target:

```cmake
cmake_minimum_required(VERSION 3.22)
project(v1_controller_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

add_executable(test_motion_feedback
    test_motion_feedback.c
    ../modules/MotionControl/src/motion_feedback.c
)
target_include_directories(test_motion_feedback PRIVATE
    ../modules/MotionControl/inc
)
target_link_libraries(test_motion_feedback PRIVATE m)
add_test(NAME motion_feedback COMMAND test_motion_feedback)
```

- [ ] **Step 2: Write failing timing and conditional-integration tests**

Create `tests/test_motion_feedback.c` with fake cumulative encoder counts. Cover these exact cases:

```c
#include "motion_feedback.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int32_t counts[ENCODER_COUNT];

static int32_t fake_get_count(EncoderId_t id) { return counts[id]; }
static void fake_reset_count(EncoderId_t id) { counts[id] = 0; }
static void assert_near(float actual, float expected, float tolerance) {
    assert(fabsf(actual - expected) <= tolerance);
}

static void test_state_estimator_uses_supplied_dt(void) {
    EncoderInterface_t encoder = {fake_get_count, fake_reset_count};
    StateEstimator_t est;
    const float radius = 0.033f;
    const float ppr = 60000.0f;
    const float circumference = 2.0f * 3.14159265f * radius;

    StateEst_Init(&est, &encoder, radius, ppr);
    counts[ENCODER_LEFT] = 600;
    counts[ENCODER_RIGHT] = 600;
    StateEst_Update(&est, 0.010f);
    assert_near(est.wheel_speed[ENCODER_LEFT], circumference, 1e-5f);

    StateEst_Reset(&est);
    counts[ENCODER_LEFT] = 120;
    counts[ENCODER_RIGHT] = 120;
    StateEst_Update(&est, 0.002f);
    assert_near(est.wheel_speed[ENCODER_LEFT], circumference, 1e-5f);
}

static void test_invalid_dt_does_not_consume_counts(void) {
    EncoderInterface_t encoder = {fake_get_count, fake_reset_count};
    StateEstimator_t est;
    StateEst_Init(&est, &encoder, 0.033f, 60000.0f);
    counts[ENCODER_LEFT] = 100;
    StateEst_Update(&est, 0.0f);
    assert(est.prev_count[ENCODER_LEFT] == 0);
}

static void test_conditional_update_freezes_integral(void) {
    PID_t pid;
    PID_Init(&pid, 1.0f, 2.0f, 0.0f, -10.0f, 10.0f);
    (void)PID_UpdateConditional(&pid, 2.0f, 0.0f, 0.1f, false);
    assert_near(pid.integral, 0.0f, 1e-6f);
    (void)PID_UpdateConditional(&pid, 2.0f, 0.0f, 0.1f, true);
    assert_near(pid.integral, 0.2f, 1e-6f);
}

int main(void) {
    test_state_estimator_uses_supplied_dt();
    test_invalid_dt_does_not_consume_counts();
    test_conditional_update_freezes_integral();
    puts("motion_feedback tests passed");
    return 0;
}
```

- [ ] **Step 3: Run the tests and verify the API mismatch fails**

Run:

```powershell
cmake -S "tests" -B "build/host-tests" -G Ninja
cmake --build "build/host-tests" --target test_motion_feedback
```

Expected: compilation fails because `PID_UpdateConditional`, the four-argument `StateEst_Init`, and `StateEst_Update(est, dt)` do not exist.

- [ ] **Step 4: Implement explicit timing and conditional integration**

In `motion_feedback.h`, remove `StateEstimator_t.update_freq`, change the state-estimator signatures to the interfaces above, and declare `PID_UpdateConditional`.

In `motion_feedback.c`, make `PID_Update()` delegate to the new API:

```c
float PID_Update(PID_t *pid, float setpoint, float measurement, float dt) {
    return PID_UpdateConditional(pid, setpoint, measurement, dt, true);
}

float PID_UpdateConditional(PID_t *pid, float setpoint, float measurement,
                            float dt, bool allow_integral) {
    const float error = setpoint - measurement;
    const float p_term = pid->kp * error;
    float d_term = 0.0f;

    if (allow_integral && dt > 0.0f) {
        pid->integral = clamp(pid->integral + error * dt,
                              pid->integral_min, pid->integral_max);
    }
    if (dt > 1e-6f) {
        d_term = pid->kd * (error - pid->prev_error) / dt;
    }
    pid->prev_error = error;
    return clamp(p_term + pid->ki * pid->integral + d_term,
                 pid->output_min, pid->output_max);
}
```

Make `StateEst_Update()` return immediately for `dt <= 0.0f`, before reading or consuming counts. For a valid interval, calculate raw speed with division by `dt`:

```c
float speed = ((float)delta_count / est->encoder_ppr)
            * wheel_circumference / dt;
```

- [ ] **Step 5: Run the native tests**

Run:

```powershell
cmake --build "build/host-tests" --target test_motion_feedback
ctest --test-dir "build/host-tests" -R motion_feedback --output-on-failure
```

Expected: build succeeds and `motion_feedback` passes.

- [ ] **Step 6: Commit the timing foundation**

Run:

```powershell
git add -- "2026task/stm32/v1.0/tests/CMakeLists.txt" "2026task/stm32/v1.0/tests/test_motion_feedback.c" "2026task/stm32/v1.0/modules/MotionControl/inc/motion_feedback.h" "2026task/stm32/v1.0/modules/MotionControl/src/motion_feedback.c"
git commit -m "fix: use explicit motion control timing"
```

Expected: one commit containing only the test harness and timing/PID foundation.

---

### Task 2: Remove Feedforward and Add Direct Wheel Targets

**Files:**
- Create: `tests/test_motion_control.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `modules/MotionControl/inc/motion_control.h`
- Modify: `modules/MotionControl/src/motion_control.c`
- Modify: `modules/MotionControl/inc/motion_config.h`
- Modify: `modules/MotionControl/src/motion_kinematics.c`
- Modify: `Core/Src/app/track_control_app.c`
- Modify: `Core/Src/app/control_app.c`
- Modify: `CMakeLists.txt`
- Delete: `modules/MotionControl/inc/motion_feedforward.h`
- Delete: `modules/MotionControl/src/motion_feedforward.c`

**Interfaces:**
- Consumes: `StateEst_Update(StateEstimator_t *est, float dt)` and `PID_UpdateConditional(...)` from Task 1.
- Produces: `void MotionControl_Update(MotionControl_t *ctrl, float dt)`.
- Produces: `void MotionControl_SetWheelSpeedTargets(MotionControl_t *ctrl, float left_mps, float right_mps)`.
- Produces: `MotionCommandMode_t` with `MOTION_COMMAND_VELOCITY` and `MOTION_COMMAND_WHEEL_SPEED`.
- Preserves: `MotionControl_SetVelocityCommand(MotionControl_t *, float, float)` and its meaning for non-playground callers.

- [ ] **Step 1: Add a fake-interface MotionControl test target**

Append this target to `tests/CMakeLists.txt`:

```cmake
add_executable(test_motion_control
    test_motion_control.c
    ../modules/MotionControl/src/motion_control.c
    ../modules/MotionControl/src/motion_feedback.c
    ../modules/MotionControl/src/motion_kinematics.c
)
target_include_directories(test_motion_control PRIVATE
    ../modules/MotionControl/inc
)
target_link_libraries(test_motion_control PRIVATE m)
add_test(NAME motion_control COMMAND test_motion_control)
```

Create `tests/test_motion_control.c` with fake encoders and a fake motor callback. Assert these behaviors:

```c
static void test_direct_targets_select_mode(void) {
    MotionControl_t ctrl = make_started_controller();
    MotionControl_SetWheelSpeedTargets(&ctrl, 0.2f, 0.3f);
    assert(ctrl.command_mode == MOTION_COMMAND_WHEEL_SPEED);
    assert_near(ctrl.wheel_left.target_velocity, 0.2f, 1e-6f);
    assert_near(ctrl.wheel_right.target_velocity, 0.3f, 1e-6f);
}

static void test_velocity_command_restores_velocity_mode(void) {
    MotionControl_t ctrl = make_started_controller();
    MotionControl_SetWheelSpeedTargets(&ctrl, 0.2f, 0.3f);
    MotionControl_SetVelocityCommand(&ctrl, 0.25f, 0.0f);
    assert(ctrl.command_mode == MOTION_COMMAND_VELOCITY);
}

static void test_zero_direct_target_resets_integral_and_pwm(void) {
    MotionControl_t ctrl = make_started_controller();
    ctrl.wheel_left.pid.integral = 1.0f;
    MotionControl_SetWheelSpeedTargets(&ctrl, 0.0f, 0.2f);
    MotionControl_Update(&ctrl, 0.010f);
    assert_near(ctrl.wheel_left.pid.integral, 0.0f, 1e-6f);
    assert(last_left_pwm == 0);
}
```

The helper `make_started_controller()` must initialize fake `EncoderInterface_t` and `MotorInterface_t`, call `MotionControl_Init`, then call `MotionControl_Start`. Keep all fixture state in this test file.

- [ ] **Step 2: Verify the direct-target tests fail to compile**

Run:

```powershell
cmake -S "tests" -B "build/host-tests" -G Ninja
cmake --build "build/host-tests" --target test_motion_control
```

Expected: compilation fails because the command mode, direct-target API, and `MotionControl_Update(ctrl, dt)` do not exist.

- [ ] **Step 3: Remove feedforward from types and wheel updates**

In `motion_control.h`:

- Remove `#include "motion_feedforward.h"`.
- Remove `Feedforward_t ff` from `WheelController_t`.
- Remove feedforward fields from the unused `MotionControlConfig_t`; delete the whole type if it remains unused after a repository search.
- Delete the `MotionControl_SetFeedforward()` declaration.
- Add command mode and direct target storage:

```c
typedef enum {
    MOTION_COMMAND_VELOCITY = 0,
    MOTION_COMMAND_WHEEL_SPEED
} MotionCommandMode_t;

typedef struct {
    float left_mps;
    float right_mps;
} WheelSpeedCommand_t;
```

Add `MotionCommandMode_t command_mode` and `WheelSpeedCommand_t wheel_cmd` to `MotionControl_t`.

In `motion_control.c`, initialize each wheel with PI only and calculate PWM only from feedback. For zero targets, reset and return zero. Pass a `forward_only` flag into the wheel update so direct playground targets use `[0, 100]` while the existing velocity path retains signed `[-100, 100]` behavior:

```c
static int16_t WheelController_Update(WheelController_t *wheel,
                                      float target_velocity,
                                      float actual_velocity,
                                      float dt,
                                      bool forward_only) {
    wheel->target_velocity = target_velocity;
    wheel->actual_velocity = actual_velocity;

    if (target_velocity == 0.0f) {
        PID_Reset(&wheel->pid);
        wheel->pwm_output = 0;
        return 0;
    }

    const float output_min = forward_only ? 0.0f : (float)PWM_MIN;
    const float output_max = (float)PWM_MAX;
    float unsaturated = wheel->pid.kp * (target_velocity - actual_velocity)
                      + wheel->pid.ki * wheel->pid.integral;
    float error = target_velocity - actual_velocity;
    bool allow_integral = !((unsaturated >= output_max && error > 0.0f) ||
                            (unsaturated <= output_min && error < 0.0f));
    float pwm = PID_UpdateConditional(&wheel->pid, target_velocity,
                                      actual_velocity, dt, allow_integral);
    wheel->pwm_output = (int16_t)clamp_float(pwm, output_min, output_max);
    return wheel->pwm_output;
}
```

Use direct targets without smoothing or inverse kinematics when `command_mode == MOTION_COMMAND_WHEEL_SPEED`; retain the existing smoothed `(v, omega)` flow otherwise. Validate `dt > 0.0f` at the start of `MotionControl_Update`.

- [ ] **Step 4: Implement command-mode setters and call-site timing**

Implement direct target clamping and mode selection:

```c
void MotionControl_SetWheelSpeedTargets(MotionControl_t *ctrl,
                                        float left_mps,
                                        float right_mps) {
    if (ctrl == NULL) return;
    ctrl->wheel_cmd.left_mps = clamp_float(left_mps, 0.0f, MAX_SPEED);
    ctrl->wheel_cmd.right_mps = clamp_float(right_mps, 0.0f, MAX_SPEED);
    ctrl->command_mode = MOTION_COMMAND_WHEEL_SPEED;
}
```

Set `command_mode = MOTION_COMMAND_VELOCITY` in `MotionControl_SetVelocityCommand`.

Update callers exactly as follows:

```c
/* playground_track.c is migrated in Task 5 */
MotionControl_Update(&g_motion_control, 0.010f); /* track_control_app.c */
MotionControl_Update(&g_motion_control, 0.002f); /* control_app.c */
```

- [ ] **Step 5: Delete feedforward and build references**

Delete both feedforward files with `apply_patch`. Remove `modules/MotionControl/src/motion_feedforward.c` from top-level `CMakeLists.txt`. Delete the complete feedforward configuration section from `motion_config.h`. Keep the generic PI bounds signed because non-playground callers retain signed commands:

```c
#define SPEED_OUTPUT_MAX 100.0f
#define SPEED_OUTPUT_MIN (-100.0f)
```

In `motion_kinematics.c`, remove the current per-wheel negative-target clamp so generic inverse kinematics remains signed. The direct playground path bypasses inverse kinematics and enforces nonnegative targets in `playground_controller.c`.

Update MotionControl comments so they describe pure PI. Do not rewrite historical logs.

- [ ] **Step 6: Run host tests and prove feedforward references are gone from active code**

Run:

```powershell
cmake --build "build/host-tests" --target test_motion_feedback test_motion_control
ctest --test-dir "build/host-tests" --output-on-failure
rg -n "motion_feedforward|MotionControl_SetFeedforward|Feedforward_t|Feedforward_|FF_K_|FF_STATIC_DEADZONE" "modules" "Core" "CMakeLists.txt"
```

Expected: both tests pass. The `rg` command returns no active-code matches; references in historical docs are not part of this check.

- [ ] **Step 7: Commit pure wheel feedback**

Stage only the files listed in this task, review `git diff --cached`, and commit:

```powershell
git commit -m "refactor: replace motor feedforward with wheel PI"
```

---

### Task 3: Make IR and Gyro Samples Trustworthy

**Files:**
- Create: `tests/test_perception.c`
- Create: `tests/test_icm42688_hal.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `modules/Sens-Decision/src/perception.c`
- Modify: `modules/IR-tracker/inc/ir_uart_sensor.h`
- Modify: `modules/IR-tracker/src/ir_uart_sensor.c`
- Modify: `modules/ICM42688/inc/icm42688_hal.h`
- Modify: `modules/ICM42688/src/icm42688_hal.c`
- Modify: `modules/ICM42688/src/icm42688_stm32.c`
- Modify: `Core/Inc/app/sensor_adapter.h`
- Modify: `Core/Src/app/sensor_adapter.c`

**Interfaces:**
- Produces: `uint32_t IrUartSensor_GetFrameSequence(void)`, incremented only after successful parse.
- Produces: `bool SensorAdapter_ReadGyroZ(float *gyro_z_radps, uint64_t *sample_time_us)`.
- Changes: ICM communication callbacks return `bool`; `false` means the transaction failed.
- Preserves: `sensor_hal_t` and `sensor_read()` public interfaces.

- [ ] **Step 1: Add a failing perception mask test**

Create `tests/test_perception.c`. Define the required `g_sens_decision_config` fixture with white references, threshold, weights, and heading filter. Feed channel values where only channels 1 and 6 cross the black-strength threshold:

```c
static void test_active_mask_contains_detected_black_channels(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir = {0};
    perception_init(&perception);

    for (size_t i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        ir.values[i] = 1000.0f;
        g_sens_decision_config.perception.white_reference[i] = 1000.0f;
    }
    g_sens_decision_config.perception.black_strength_threshold = 100.0f;
    ir.values[1] = 800.0f;
    ir.values[6] = 700.0f;

    assert(perception_update(&perception, &ir, 20000U, &result) == SD_OK);
    assert(result.line_valid);
    assert(result.active_mask == ((1U << 1) | (1U << 6)));
}
```

Add a second test with no threshold crossing; expect `line_valid == false` and `active_mask == 0`.

Add the target to `tests/CMakeLists.txt`, compiling `perception.c` and linking `m`.

- [ ] **Step 2: Run the perception test and observe the copied-mask failure**

Run:

```powershell
cmake --build "build/host-tests" --target test_perception
ctest --test-dir "build/host-tests" -R perception --output-on-failure
```

Expected: the detected-mask assertion fails because current code copies `ir_data->active_mask`.

- [ ] **Step 3: Build the detected mask in perception**

Remove `result->active_mask = ir_data->active_mask`. In the threshold branch, set the detected bit:

```c
if (strength > g_sens_decision_config.perception.black_strength_threshold) {
    result->active_mask |= (uint16_t)(1U << i);
    ++active_count;
    weighted_sum += weight * strength;
    strength_sum += strength;
}
```

Run the perception test again. Expected: PASS.

- [ ] **Step 4: Write failing ICM communication tests**

Create `tests/test_icm42688_hal.c` with injected callbacks backed by a fake register array and a `comm_ok` flag. Assert:

```c
static void test_read_rejects_uninitialized_sensor(void) {
    icm42688_data_t data;
    bind_fake_comm();
    assert(icm42688_read(&data) == ICM42688_STATUS_NOT_READY);
}

static void test_read_propagates_burst_failure(void) {
    icm42688_data_t data;
    bind_and_initialize_fake_sensor();
    comm_ok = false;
    assert(icm42688_read(&data) == ICM42688_STATUS_IO_ERROR);
}
```

Add `ICM42688_STATUS_IO_ERROR` to `icm42688_status_t` immediately after `ICM42688_STATUS_BAD_ID`; use it for failed communication transactions.

Add the test target compiling `icm42688_hal.c`.

- [ ] **Step 5: Run ICM tests and verify failures**

Run:

```powershell
cmake --build "build/host-tests" --target test_icm42688_hal
ctest --test-dir "build/host-tests" -R icm42688_hal --output-on-failure
```

Expected: build fails until callback signatures return status, or tests fail because `icm42688_read()` accepts an uninitialized sensor and ignores burst failure.

- [ ] **Step 6: Propagate SPI transaction status through ICM42688**

Change `icm42688_comm_t` to:

```c
typedef struct {
    bool (*init)(void);
    bool (*read_reg)(uint8_t reg, uint8_t *value);
    bool (*write_reg)(uint8_t reg, uint8_t value);
    bool (*read_regs)(uint8_t reg, uint8_t *data, uint8_t len);
} icm42688_comm_t;
```

Update `icm42688_hal.c` so initialization fails on any callback failure and `icm42688_read()` first checks `initialized`, then checks `read_regs()` before decoding data.

Update STM32 callbacks to return `HAL_OK` status and always deassert chip select before returning:

```c
static bool _read_regs(uint8_t reg, uint8_t *data, uint8_t len) {
    uint8_t tx_reg = (uint8_t)(reg | 0x80U);
    HAL_StatusTypeDef tx_status;
    HAL_StatusTypeDef rx_status = HAL_ERROR;
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    tx_status = HAL_SPI_Transmit(&hspi2, &tx_reg, 1U, 10U);
    if (tx_status == HAL_OK) {
        rx_status = HAL_SPI_Receive(&hspi2, data, len, 10U);
    }
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    return tx_status == HAL_OK && rx_status == HAL_OK;
}
```

Apply the same rule to single-register read/write and initialization callbacks.

- [ ] **Step 7: Add true IR freshness metadata**

In the IR driver, add a static `uint32_t g_frame_sequence`. Reset it in `IrUartSensor_Reset()` and increment it only when `IrUartSensor_Process()` successfully validates and copies a new frame. Expose:

```c
uint32_t IrUartSensor_GetFrameSequence(void) {
    return g_frame_sequence;
}
```

In `sensor_adapter.c`, retain `s_last_ir_sequence` and `s_last_ir_time_us`. `read_ir()` must call `IrUartSensor_Process()`, and only report `SD_OK` as a new sample when the sequence changed. Record `PlatformTime_GetUs64()` at that point for diagnostics; unchanged sequences return `SD_ERR_READ` and cannot be restamped as new samples.

Do not assign a detected-line mask in the adapter. Set the hardware-availability mask to the eight valid channel bits:

```c
*active_mask = (uint16_t)((1U << SD_IR_CHANNEL_COUNT) - 1U);
```

Perception remains solely responsible for the detected black-channel mask.

- [ ] **Step 8: Add the calibrated 100 Hz gyro helper**

Implement this adapter API:

```c
bool SensorAdapter_ReadGyroZ(float *gyro_z_radps, uint64_t *sample_time_us) {
    imu_data_t imu;
    uint64_t now;
    sensor_t *sensor;
    if (gyro_z_radps == NULL || sample_time_us == NULL) return false;
    sensor = sensor_get(SENSOR_ID_IMU);
    now = PlatformTime_GetUs64();
    if (sensor_read(sensor, &imu, now) != SD_OK || !isfinite(imu.gyro_radps[2])) {
        return false;
    }
    *gyro_z_radps = imu.gyro_radps[2];
    *sample_time_us = imu.timestamp_us;
    return true;
}
```

Include `<math.h>` and `platform_time.h`. This reuses the existing scale, bias, filter, and coordinate path and avoids duplicate conversion in `playground_track.c`.

- [ ] **Step 9: Run all sensor tests**

Run:

```powershell
cmake --build "build/host-tests"
ctest --test-dir "build/host-tests" --output-on-failure
```

Expected: all host tests pass, including perception mask and ICM failure propagation.

- [ ] **Step 10: Commit trustworthy sensor samples**

Stage only Task 3 files, inspect the cached diff, and commit:

```powershell
git commit -m "fix: validate tracking sensor samples"
```

---

### Task 4: Implement the Pure Playground Cascade Controller

**Files:**
- Create: `Core/Inc/app/playground_controller.h`
- Create: `Core/Src/app/playground_controller.c`
- Create: `tests/test_playground_controller.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: no hardware APIs; only scalar inputs and timestamps.
- Produces: `bool PlaygroundController_Init(PlaygroundController_t *ctrl, const PlaygroundControllerConfig_t *cfg, bool gyro_available_at_start)`.
- Produces: `void PlaygroundController_Reset(PlaygroundController_t *ctrl, bool gyro_available)`.
- Produces: `void PlaygroundController_SetOuterLoop(PlaygroundController_t *ctrl, const PlaygroundOuterCommand_t *outer)`.
- Produces: `void PlaygroundController_Update100Hz(PlaygroundController_t *ctrl, const PlaygroundGyroSample_t *sample, PlaygroundControllerOutput_t *output)`.
- Produces: direct wheel targets in cascade mode and `(v, omega)` fallback output in degraded mode.

- [ ] **Step 1: Define the public pure-controller contract in a failing test**

Create `tests/test_playground_controller.c` using the following public concepts:

```c
typedef enum {
    PG_CONTROL_GYRO_CASCADE = 0,
    PG_CONTROL_IR_PD_DEGRADED
} PlaygroundControlMode_t;

typedef struct {
    float angular_kp;
    float angular_ki;
    float delta_v_max;
    uint32_t gyro_stale_us;
    uint8_t invalid_limit;
    uint8_t recovery_valid_count;
    float max_wheel_speed;
} PlaygroundControllerConfig_t;

typedef struct {
    float base_speed;
    float omega_target;
    float fallback_omega;
    bool line_valid;
} PlaygroundOuterCommand_t;

typedef struct {
    bool gyro_valid;
    float gyro_z_radps;
    uint64_t gyro_timestamp_us;
    uint64_t now_us;
} PlaygroundGyroSample_t;

typedef struct {
    PlaygroundControlMode_t mode;
    float left_target_mps;
    float right_target_mps;
    float fallback_v_mps;
    float fallback_omega_radps;
} PlaygroundControllerOutput_t;
```

Add tests for these exact requirements:

- Zero error produces equal wheel targets.
- Positive angular-rate error produces `right_target > left_target` under the project kinematic convention.
- Large error never produces a negative wheel target.
- If scaling is required, neither target exceeds `max_wheel_speed`.
- Saturation freezes integral when error pushes farther into saturation.
- Three invalid reads enter degraded mode.
- A timestamp older than 30 ms enters degraded mode immediately.
- Nine valid recovery reads remain degraded; the tenth restores cascade.
- Entering and leaving degraded mode resets angular integral.
- Zero base speed returns zero wheel targets and resets angular integral.

- [ ] **Step 2: Add the test target and verify it fails**

Append:

```cmake
add_executable(test_playground_controller
    test_playground_controller.c
    ../Core/Src/app/playground_controller.c
)
target_include_directories(test_playground_controller PRIVATE
    ../Core/Inc/app
)
target_link_libraries(test_playground_controller PRIVATE m)
add_test(NAME playground_controller COMMAND test_playground_controller)
```

Run:

```powershell
cmake -S "tests" -B "build/host-tests" -G Ninja
cmake --build "build/host-tests" --target test_playground_controller
```

Expected: fails because the header and implementation do not exist.

- [ ] **Step 3: Implement initialization, reset, and outer-command storage**

Define `PlaygroundController_t` with configuration, current outer command, mode, angular integral, invalid count, and valid recovery count. Initialization validates pointers and positive limits, copies configuration, starts in cascade only when `gyro_available_at_start` is true, and otherwise starts degraded.

`PlaygroundController_Reset(ctrl, gyro_available)` sets integral and counters to zero and selects cascade or degraded mode from the supplied current gyro availability. This makes stop/restart behavior explicit rather than preserving stale health state.

- [ ] **Step 4: Implement health transitions before control math**

In `PlaygroundController_Update100Hz`, calculate freshness without unsigned underflow:

```c
bool fresh = sample->gyro_valid
          && sample->now_us >= sample->gyro_timestamp_us
          && (sample->now_us - sample->gyro_timestamp_us) <= ctrl->cfg.gyro_stale_us;
```

In cascade mode, increment `invalid_count` for invalid samples and degrade when it reaches `invalid_limit`; a stale sample degrades immediately. In degraded mode, count only consecutive fresh samples and recover at `recovery_valid_count`. Any invalid sample resets the recovery count.

Every mode transition sets `angular_integral = 0.0f`.

- [ ] **Step 5: Implement PI, conditional integration, and wheel allocation**

Calculate the unsaturated PI output, then limit it by all three constraints:

```c
float dynamic_limit = 2.0f * outer.base_speed;
float limit = fminf(ctrl->cfg.delta_v_max, dynamic_limit);
float error = outer.omega_target - sample->gyro_z_radps;
float unsaturated = ctrl->cfg.angular_kp * error
                  + ctrl->cfg.angular_ki * ctrl->angular_integral;
float delta_v = clampf(unsaturated, -limit, limit);
bool drives_outward = (unsaturated > limit && error > 0.0f)
                   || (unsaturated < -limit && error < 0.0f);
if (!drives_outward) {
    ctrl->angular_integral += error * 0.010f;
}
```

Recalculate `delta_v` after accepted integration, clamp it again, derive targets, clamp tiny negative roundoff to zero, then uniformly scale both if their maximum exceeds `max_wheel_speed`.

In degraded mode, return `fallback_v_mps = base_speed` and the already segment-clamped `fallback_omega_radps`; leave direct wheel targets zero so the caller cannot accidentally mix command modes.

- [ ] **Step 6: Run the pure-controller tests**

Run:

```powershell
cmake --build "build/host-tests" --target test_playground_controller
ctest --test-dir "build/host-tests" -R playground_controller --output-on-failure
```

Expected: all cascade, saturation, transition, and reset tests pass.

- [ ] **Step 7: Commit the pure controller**

Stage the four Task 4 files and commit:

```powershell
git commit -m "feat: add playground cascade controller"
```

---

### Task 5: Integrate Cascade Control into Playground Scheduling

**Files:**
- Modify: `Core/Src/app/playground_track.c`
- Modify: `Core/Inc/app/playground_track.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `SensorAdapter_ReadGyroZ`, fresh IR reads through preprocessing, `PlaygroundController_*`, and `MotionControl_SetWheelSpeedTargets`.
- Produces: 50 Hz outer commands followed by 100 Hz cascade output and then 100 Hz MotionControl output.
- Preserves: public `PlaygroundTrack_*` API and competition state names.

- [ ] **Step 1: Add controller state and low-speed configuration**

Add a static `PlaygroundController_t g_controller` and controller configuration to `playground_track.c`. Keep segment-specific IR-P gains and fallback IR-PD gains separate in `pg_config_t`; do not reuse a field with two meanings.

Use initial normal-mode speeds:

```c
g_cfg.v_straight = 0.25f;
g_cfg.v_curve = 0.25f;
g_cfg.v_approach = 0.125f;
```

Use explicit constants for `gyro_stale_us = 30000U`, `invalid_limit = 3U`, and `recovery_valid_count = 10U`. Preserve current outer-loop proportional gains as conservative starting values; set normal-path derivative use to zero while retaining existing derivative gains for fallback.

- [ ] **Step 2: Track whether IMU startup succeeded**

Replace the current implicit `goto skip_imu` behavior with a `bool g_gyro_available` initialized false. Set it true only after ICM initialization, gyro calibration, scale-factor retrieval, and bias retrieval all succeed. If any step fails:

- Print one warning that IR-PD degraded mode will be used.
- Continue initializing IR, perception, and MotionControl.
- Initialize `PlaygroundController` in degraded mode.

Do not let a failed ICM initialization be read as valid later; Task 3 makes `icm42688_read()` reject uninitialized use.

- [ ] **Step 3: Separate 50 Hz IR work from 100 Hz gyro work**

Refactor `pg_decide_50hz()` so it only:

- Reads/processes IR and updates perception.
- Sets `line_valid = status == SD_OK && g_res.line_valid`.
- Updates line-valid/lost counters.
- Updates distance and competition state.
- Selects `base_speed`, normal `omega_target`, fallback `omega`, and current limits.
- Calls `PlaygroundController_SetOuterLoop()`.

Remove every direct `MotionControl_SetVelocityCommand()` call from active run states in `pg_state_machine()`. Idle, stopped, and fault states instead set a zero outer command and reset/stop through the existing state transitions; only `pg_control_100hz()` publishes a motor command while running.

Normal target:

```c
omega_target = pg_clamp(-kp_normal * g_res.lateral_error,
                        -omega_max, omega_max);
```

Fallback target:

```c
fallback_omega = pg_clamp(-(kp_fallback * g_res.lateral_error
                          + kd_fallback * g_res.heading_error),
                          -omega_max, omega_max);
```

When the line is invalid, do not calculate a new target from zeroed perception values. Preserve the previous outer command until the configured line-loss fault threshold stops the vehicle.

- [ ] **Step 4: Add the 100 Hz gyro/control function**

Add `pg_control_100hz()`:

```c
static void pg_control_100hz(void) {
    PlaygroundGyroSample_t gyro = {0};
    PlaygroundControllerOutput_t output;
    gyro.now_us = PlatformTime_GetUs64();
    gyro.gyro_valid = g_gyro_available
        && SensorAdapter_ReadGyroZ(&gyro.gyro_z_radps, &gyro.gyro_timestamp_us);

    PlaygroundController_Update100Hz(&g_controller, &gyro, &output);
    if (output.mode == PG_CONTROL_GYRO_CASCADE) {
        MotionControl_SetWheelSpeedTargets(&g_mc,
                                           output.left_target_mps,
                                           output.right_target_mps);
    } else {
        MotionControl_SetVelocityCommand(&g_mc,
                                         output.fallback_v_mps,
                                         output.fallback_omega_radps);
    }
    MotionControl_Update(&g_mc, 0.010f);
}
```

Throttle diagnostics to mode transitions rather than every failed sample. Include mode, reason, gyro rate, target rate, left target, and right target in periodic debug output.

- [ ] **Step 5: Reorder the scheduler**

Make shared cycles execute IR outer-loop work before gyro and wheel control:

```c
void PlaygroundTrack_RunFastCycle(void) {
    Encoder_Poll();
    if ((g_cyc % 10U) == 0U) {
        pg_decide_50hz();
    }
    if ((g_cyc % 5U) == 0U) {
        pg_control_100hz();
    }
    g_cyc = (g_cyc + 1U) % 10U;
}
```

Ensure stop/fault states publish zero targets and reset the pure controller before stopping MotionControl.

- [ ] **Step 6: Correct transverse-line and line-loss use**

Keep transverse detection based on `g_res.active_mask`, which Task 3 now builds from actual threshold crossings. Keep `pg_count_active_channels()` limited to `SD_IR_CHANNEL_COUNT`.

Use the corrected line validity expression everywhere. A successful function return with `g_res.line_valid == false` increments `g_line_lost_count`, not `g_line_valid_count`.

- [ ] **Step 7: Add source and update header comments**

Add `Core/Src/app/playground_controller.c` to top-level `CMakeLists.txt`. Update `playground_track.h` to document:

- 50 Hz IR-P normal outer loop.
- 100 Hz gyro PI and wheel PI.
- IR-PD degraded mode after gyro failure.
- Actual software gyro sampling rate of 100 Hz, not FIFO consumption.

- [ ] **Step 8: Run host regression tests and ARM compile**

Run:

```powershell
cmake --build "build/host-tests"
ctest --test-dir "build/host-tests" --output-on-failure
cmake --preset Debug
cmake --build --preset Debug --clean-first
```

Expected: all host tests pass; ARM firmware compiles and links without feedforward symbols or signature mismatches.

If CMake regeneration changes tracked generated files under `cmake-build-debug`, do not stage those files. The preset build writes to `build/Debug`.

- [ ] **Step 9: Commit playground integration**

Stage only `playground_track.c`, `playground_track.h`, and the intended top-level `CMakeLists.txt` hunks. Because `CMakeLists.txt` and `playground_track.c` were dirty before this work, inspect each staged hunk and retain all user edits without staging unrelated portions.

Commit:

```powershell
git commit -m "feat: use gyro cascade for playground tracking"
```

---

### Task 6: Align Diagnostics, Verify the Complete Change, and Prepare Hardware Validation

**Files:**
- Modify: `Core/Inc/app/motor_static_friction_test.h`
- Modify: `Core/Src/app/motor_static_friction_test.c`
- Modify: `Core/Src/app/motor_interactive_test.c`
- Modify: `Core/Src/freertos.c`
- Modify: `CMakeLists.txt`
- Create: `docs/VALIDATION_AFTER_GYRO_IR_CASCADE_2026-07-31.md`

**Interfaces:**
- Consumes: completed controller and sensor implementation.
- Produces: a compile-tested firmware tree and an executable bench/vehicle validation checklist.
- Does not update: README, CHANGELOG, or final tuning guides before user hardware acceptance.

- [ ] **Step 1: Rename the uncommitted static-friction diagnostic semantically**

If the uncommitted diagnostic files are still present, retain their raw PWM/encoder functionality but remove all claims that they calibrate `FF_K_STATIC`. Prefer names such as:

```c
void MotorMinimumPwmTest_Init(void);
void MotorMinimumPwmTest_Run(void);
void MotorMinimumPwmTest_OnRxByte(uint8_t byte);
```

Update its banner to `Minimum Startup PWM Test`, update callback forwarding in `motor_interactive_test.c`, and update the FreeRTOS mode label. Do not delete the user's diagnostic work.

If the user has removed those untracked files before this task begins, skip this step and do not recreate them.

- [ ] **Step 2: Search for active feedforward and obsolete API references**

Run:

```powershell
rg -n "motion_feedforward|MotionControl_SetFeedforward|Feedforward_t|Feedforward_|FF_K_|FF_STATIC_DEADZONE" "modules" "Core" "CMakeLists.txt"
rg -n "MotionControl_Update\s*\([^,\)]*\)" "Core" "modules"
```

Expected: both commands return no matches. Historical docs and logs are intentionally excluded.

- [ ] **Step 3: Run all automated verification from clean outputs**

Run:

```powershell
cmake -S "tests" -B "build/host-tests" -G Ninja
cmake --build "build/host-tests" --clean-first
ctest --test-dir "build/host-tests" --output-on-failure
cmake --preset Debug
cmake --build --preset Debug --clean-first
git diff --check
```

Expected:

- All host tests pass.
- ARM Debug firmware compiles and links.
- `git diff --check` reports no whitespace errors.
- Generated files under `cmake-build-debug` remain unstaged.

- [ ] **Step 4: Write the hardware validation checklist**

Create `docs/VALIDATION_AFTER_GYRO_IR_CASCADE_2026-07-31.md` with these explicit gates:

```markdown
# Gyro and IR Cascade Validation - 2026-07-31

## P0 Bench Gates

- [ ] Keep the vehicle stationary during the one-second gyro calibration.
- [ ] Confirm stationary `gyro_z_radps` remains near zero without monotonic drift.
- [ ] Rotate the vehicle counter-clockwise by hand and confirm `gyro_z_radps > 0`.
- [ ] Move the black line from left to right and record `lateral_error`, `active_mask`, and `line_valid`.
- [ ] Raise the drive wheels and confirm both wheel targets and PWM values remain non-negative.
- [ ] Disconnect or simulate failed gyro reads; confirm IR-PD mode after no more than 30 ms.
- [ ] Restore gyro reads; confirm cascade mode returns after 10 valid samples without a large PWM step.

## P1 Low-Speed Ground Gates

- [ ] Start at 0.25 m/s on a straight and confirm no sustained left-right oscillation.
- [ ] Enter each curve at no more than 0.30 m/s and confirm neither wheel remains abnormally stopped.
- [ ] Confirm `delta_v` and PWM do not remain saturated through an entire curve.
- [ ] Trigger gyro degradation and confirm low-speed IR-PD tracking remains controllable.
- [ ] Remove the line and confirm the configured line-loss fault stops the vehicle.

## Tuning Order

1. Wheel PI with steering disabled.
2. Gyro angular-rate PI with a bounded rate target.
3. IR proportional outer loop.
4. Segment speeds and angular limits.

## Acceptance Record

Record the tested commit, wheel PI gains, gyro PI gains, IR gains, straight speed,
curve speed, observed failure mode, and pass/fail result before changing one layer.
```

- [ ] **Step 5: Perform final code review before claiming completion**

Inspect:

```powershell
git status --short
git diff --stat
git diff -- "2026task/stm32/v1.0/modules" "2026task/stm32/v1.0/Core" "2026task/stm32/v1.0/tests" "2026task/stm32/v1.0/CMakeLists.txt"
```

Review specifically for:

- Positive-feedback gyro sign errors.
- Mixed command modes in one cycle.
- Unsigned timestamp underflow.
- Integral state surviving stop or mode transition.
- Reused IR data being treated as fresh.
- User work accidentally removed from already-dirty files.

- [ ] **Step 6: Commit diagnostics and validation material**

Stage only intended diagnostic renames and the new validation checklist. Commit:

```powershell
git commit -m "docs: add gyro cascade validation checklist"
```

- [ ] **Step 7: Hand off for user hardware acceptance**

Report automated test and ARM build evidence, then ask the user to execute the P0 and P1 checklist. Do not update README, CHANGELOG, parameter traceability, or tuning guides until the user reports hardware results, as required by the project workflow.

---

## Final Verification Matrix

| Requirement | Automated Evidence | Hardware Evidence |
|---|---|---|
| Feedforward fully removed | Active-code `rg` returns no matches; ARM link passes | None |
| Non-playground semantics preserved | MotionControl command-mode host tests; both callers compile | Existing modes smoke test when convenient |
| Correct estimator timing | 100 Hz and 500 Hz host tests | Encoder speed sanity check |
| Real IR active mask | Perception host tests | Move line across all eight channels |
| Gyro failures are observable | ICM HAL host tests | Disconnect/simulate SPI failure |
| Three invalid samples degrade | Pure-controller host test | Observe mode within 30 ms |
| Ten valid samples recover | Pure-controller host test | Observe smooth recovery |
| Wheels never reverse | Allocation and MotionControl tests | Raised-wheel target/PWM check |
| Low-speed stability | Not fully simulatable | P1 straight and curve gates |

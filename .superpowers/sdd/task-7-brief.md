### Task 7: Closed-Loop Coordinator

**Files:**
- Create: `App/Inc/balance_loop.h`
- Create: `App/Src/balance_loop.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: all earlier configs, supervisor events, target selection, protocol-decoded camera samples, and monotonic time.
- Produces: at most one `BalanceActuatorCommand` for each accepted fresh frame, a fault state on timeout or bad data, and a read-only `BalanceTelemetry` snapshot for the later UART adapter.

- [ ] **Step 1: Add failing fresh-frame, timeout, and reset integration tests**

Add all existing source files plus `../App/Src/balance_loop.c` to the host target. Add:

```c
#include "balance_loop.h"

static BalanceLoopConfig loop_config(void)
{
    const BalanceLoopConfig config = {
        .observer = { .alpha = 1.0f, .beta = 0.2f,
                      .min_dt_s = 0.010f, .max_dt_s = 0.100f },
        .controller = { .kp = 1.0f, .kv = 0.0f, .ki = 0.0f,
                        .integral_zone_cm = 1.0f, .integral_limit = 1.0f,
                        .output_limit = 4.0f },
        .actuator = { .control_sign = 1.0f, .position_limit = 4.0f,
                      .max_delta_per_frame = 1.0f, .speed = 20.0f,
                      .acceleration = 50.0f },
        .measurement = { .min_position_cm = -11.5f, .max_position_cm = 11.5f,
                         .max_jump_cm = 4.0f, .timeout_ms = 100U },
        .target_rate_cm_s = 2.0f,
        .ball_end_zone_cm = 11.0f,
        .saturation_frame_limit = 3U,
        .saturation_error_min_cm = 1.0f,
    };
    return config;
}

static void prepare_loop(BalanceLoop *loop)
{
    balance_loop_confirm_manual_zero(loop);
    CHECK_TRUE(balance_loop_complete_open_loop(loop, true));
}

static void test_loop_emits_one_command_per_fresh_frame(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement first = { .sequence = 1U, .rx_timestamp_ms = 1000U,
                                       .valid = true, .position_cm = 1.0f };
    const BalanceMeasurement second = { .sequence = 2U, .rx_timestamp_ms = 1030U,
                                        .valid = true, .position_cm = 1.0f };
    const BalanceMeasurement third = { .sequence = 3U, .rx_timestamp_ms = 1060U,
                                       .valid = true, .position_cm = 1.0f };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &first, &command));
    CHECK_TRUE(!balance_loop_start(&loop));
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &second, &command));
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &third, &command));
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &third, &command));
    CHECK_NEAR(command.position, -1.0f, 0.0001f);
}

static void test_loop_faults_on_camera_timeout_and_clears_control_state(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement first = { .sequence = 1U, .rx_timestamp_ms = 1000U,
                                       .valid = true, .position_cm = 0.0f };
    const BalanceMeasurement second = { .sequence = 2U, .rx_timestamp_ms = 1030U,
                                        .valid = true, .position_cm = 0.0f };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    (void)balance_loop_process_measurement(&loop, &first, &command);
    (void)balance_loop_process_measurement(&loop, &second, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    balance_loop_poll(&loop, 1131U);
    CHECK_TRUE(loop.supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT);
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
}

static void test_loop_faults_before_commanding_near_tube_end(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement sample = { .sequence = 1U, .rx_timestamp_ms = 1000U,
                                        .valid = true, .position_cm = 11.1f };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &sample, &command));
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_BALL_END_ZONE);
}

static void test_loop_faults_after_sustained_output_saturation(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceLoopConfig config = loop_config();
    config.controller.kp = 30.0f;
    const BalanceMeasurement samples[] = {
        { .sequence = 1U, .rx_timestamp_ms = 1000U, .valid = true, .position_cm = -2.0f },
        { .sequence = 2U, .rx_timestamp_ms = 1030U, .valid = true, .position_cm = -2.0f },
        { .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true, .position_cm = -2.0f },
        { .sequence = 4U, .rx_timestamp_ms = 1090U, .valid = true, .position_cm = -2.0f },
        { .sequence = 5U, .rx_timestamp_ms = 1120U, .valid = true, .position_cm = -2.0f },
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    (void)balance_loop_process_measurement(&loop, &samples[0], &command);
    (void)balance_loop_process_measurement(&loop, &samples[1], &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &samples[2], &command));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &samples[3], &command));
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &samples[4], &command));
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_OUTPUT_SATURATION);
}

static void test_loop_exposes_last_control_telemetry(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceTelemetry telemetry;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement samples[] = {
        { .sequence = 1U, .rx_timestamp_ms = 1000U, .valid = true, .position_cm = 1.0f },
        { .sequence = 2U, .rx_timestamp_ms = 1030U, .valid = true, .position_cm = 1.0f },
        { .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true, .position_cm = 1.0f },
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    (void)balance_loop_process_measurement(&loop, &samples[0], &command);
    (void)balance_loop_process_measurement(&loop, &samples[1], &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &samples[2], &command));
    balance_loop_get_telemetry(&loop, &telemetry);
    CHECK_TRUE(telemetry.valid);
    CHECK_TRUE(telemetry.timestamp_ms == 1060U);
    CHECK_NEAR(telemetry.raw_position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(telemetry.estimate.position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(telemetry.control.error_cm, -1.0f, 0.0001f);
    CHECK_NEAR(telemetry.command.position, -1.0f, 0.0001f);
    CHECK_TRUE(telemetry.state == BALANCE_STATE_CLOSED_LOOP);
}

static void test_loop_stop_clears_control_state_and_requires_fresh_camera_data(void)
{
    BalanceLoop loop;
    const BalanceLoopConfig config = loop_config();
    balance_loop_init(&loop, &config);
    loop.supervisor.state = BALANCE_STATE_CLOSED_LOOP;
    loop.controller.integral = 0.75f;
    loop.camera_ready = true;
    balance_loop_stop(&loop);
    CHECK_TRUE(loop.supervisor.state == BALANCE_STATE_READY);
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
    CHECK_TRUE(!loop.camera_ready);
    CHECK_TRUE(!balance_loop_start(&loop));
}
```

Call all six loop tests from `main`.

- [ ] **Step 2: Run tests and confirm the loop API is missing**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_loop.h` does not exist.

- [ ] **Step 3: Implement the coordinator**

Create `App/Inc/balance_loop.h`:

```c
#ifndef BALANCE_LOOP_H
#define BALANCE_LOOP_H

#include "balance_actuator.h"
#include "balance_controller.h"
#include "balance_measurement.h"
#include "balance_observer.h"
#include "balance_supervisor.h"
#include "balance_target.h"

typedef struct {
    BalanceObserverConfig observer;
    BalanceControllerConfig controller;
    BalanceActuatorConfig actuator;
    BalanceMeasurementConfig measurement;
    float target_rate_cm_s;
    float ball_end_zone_cm;
    uint16_t saturation_frame_limit;
    float saturation_error_min_cm;
} BalanceLoopConfig;

typedef struct {
    bool valid;
    uint32_t timestamp_ms;
    float raw_position_cm;
    float target_cm;
    BalanceEstimate estimate;
    BalanceControlOutput control;
    BalanceActuatorCommand command;
    BalanceState state;
    BalanceFault fault;
} BalanceTelemetry;

typedef struct {
    BalanceLoopConfig config;
    BalanceObserver observer;
    BalanceController controller;
    BalanceActuator actuator;
    BalanceMeasurementGuard measurement;
    BalanceSupervisor supervisor;
    BalanceTarget target;
    uint16_t saturation_frames;
    bool camera_ready;
    BalanceTelemetry telemetry;
} BalanceLoop;

void balance_loop_init(BalanceLoop *loop, const BalanceLoopConfig *config);
void balance_loop_confirm_manual_zero(BalanceLoop *loop);
bool balance_loop_complete_open_loop(BalanceLoop *loop, bool approved);
bool balance_loop_start(BalanceLoop *loop);
void balance_loop_stop(BalanceLoop *loop);
bool balance_loop_select_target(BalanceLoop *loop, float target_cm);
bool balance_loop_process_measurement(BalanceLoop *loop,
                                      const BalanceMeasurement *measurement,
                                      BalanceActuatorCommand *command);
void balance_loop_poll(BalanceLoop *loop, uint32_t now_ms);
void balance_loop_raise_motor_fault(BalanceLoop *loop);
void balance_loop_emergency_stop(BalanceLoop *loop);
void balance_loop_acknowledge_fault(BalanceLoop *loop);
void balance_loop_get_telemetry(const BalanceLoop *loop, BalanceTelemetry *telemetry);

#endif
```

Implement `App/Src/balance_loop.c`:

```c
#include "balance_loop.h"

static float absf(float value) { return value < 0.0f ? -value : value; }

static void reset_control_state(BalanceLoop *loop)
{
    balance_observer_reset(&loop->observer);
    balance_controller_reset(&loop->controller);
    balance_actuator_reset(&loop->actuator);
    balance_target_reset(&loop->target);
    loop->saturation_frames = 0U;
    loop->camera_ready = false;
    loop->telemetry.valid = false;
}

void balance_loop_init(BalanceLoop *loop, const BalanceLoopConfig *config)
{
    loop->config = *config;
    balance_observer_init(&loop->observer, &config->observer);
    balance_controller_init(&loop->controller, &config->controller);
    balance_actuator_init(&loop->actuator, &config->actuator);
    balance_measurement_guard_init(&loop->measurement, &config->measurement);
    balance_supervisor_init(&loop->supervisor);
    balance_target_init(&loop->target, config->target_rate_cm_s);
    loop->saturation_frames = 0U;
    loop->camera_ready = false;
    loop->telemetry.valid = false;
}

void balance_loop_confirm_manual_zero(BalanceLoop *loop)
{
    reset_control_state(loop);
    balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    balance_supervisor_confirm_manual_zero(&loop->supervisor);
}

bool balance_loop_complete_open_loop(BalanceLoop *loop, bool approved)
{
    return balance_supervisor_complete_open_loop(&loop->supervisor, approved);
}

bool balance_loop_start(BalanceLoop *loop)
{
    if (!loop->camera_ready) return false;
    return balance_supervisor_start_closed_loop(&loop->supervisor);
}

void balance_loop_stop(BalanceLoop *loop)
{
    balance_supervisor_stop(&loop->supervisor);
    if (loop->supervisor.state == BALANCE_STATE_READY) {
        reset_control_state(loop);
        balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    }
}

bool balance_loop_select_target(BalanceLoop *loop, float target_cm)
{
    if (loop->supervisor.state != BALANCE_STATE_READY
        && loop->supervisor.state != BALANCE_STATE_CLOSED_LOOP) return false;
    if (!balance_target_select(&loop->target, target_cm)) return false;
    balance_controller_reset(&loop->controller);
    return true;
}

bool balance_loop_process_measurement(BalanceLoop *loop,
                                      const BalanceMeasurement *measurement,
                                      BalanceActuatorCommand *command)
{
    if (loop->supervisor.state != BALANCE_STATE_READY
        && loop->supervisor.state != BALANCE_STATE_CLOSED_LOOP) return false;
    const BalanceMeasurementResult accepted =
        balance_measurement_accept(&loop->measurement, measurement);
    if (accepted != BALANCE_MEASUREMENT_ACCEPTED) {
        if (accepted == BALANCE_MEASUREMENT_OUT_OF_RANGE
            || accepted == BALANCE_MEASUREMENT_JUMP) {
            balance_supervisor_raise_fault(&loop->supervisor, BALANCE_FAULT_CAMERA_DATA);
            reset_control_state(loop);
        }
        return false;
    }
    if (absf(measurement->position_cm) >= loop->config.ball_end_zone_cm) {
        balance_supervisor_raise_fault(&loop->supervisor, BALANCE_FAULT_BALL_END_ZONE);
        reset_control_state(loop);
        return false;
    }

    BalanceEstimate estimate;
    const uint32_t previous_timestamp_ms = loop->observer.last_timestamp_ms;
    const BalanceObserverResult observer_result =
        balance_observer_update(&loop->observer, measurement->rx_timestamp_ms,
                                measurement->position_cm, &estimate);
    if (observer_result != BALANCE_OBSERVER_UPDATED) return false;
    loop->camera_ready = true;
    if (loop->supervisor.state == BALANCE_STATE_READY) return false;

    const float dt_s = (float)(measurement->rx_timestamp_ms
                     - previous_timestamp_ms) * 0.001f;
    const float target_cm = balance_target_step(&loop->target, dt_s);
    const BalanceControlOutput control =
        balance_controller_step(&loop->controller, target_cm, &estimate, dt_s, true);
    *command = balance_actuator_limit(&loop->actuator, control.limited);
    if ((control.saturated || command->position_limited)
        && absf(control.error_cm) >= loop->config.saturation_error_min_cm) {
        loop->saturation_frames++;
    } else {
        loop->saturation_frames = 0U;
    }
    if (loop->saturation_frames >= loop->config.saturation_frame_limit) {
        balance_supervisor_raise_fault(&loop->supervisor,
                                       BALANCE_FAULT_OUTPUT_SATURATION);
        reset_control_state(loop);
        return false;
    }
    loop->telemetry.valid = true;
    loop->telemetry.timestamp_ms = measurement->rx_timestamp_ms;
    loop->telemetry.raw_position_cm = measurement->position_cm;
    loop->telemetry.target_cm = target_cm;
    loop->telemetry.estimate = estimate;
    loop->telemetry.control = control;
    loop->telemetry.command = *command;
    loop->telemetry.state = loop->supervisor.state;
    loop->telemetry.fault = loop->supervisor.fault;
    return true;
}

void balance_loop_poll(BalanceLoop *loop, uint32_t now_ms)
{
    if (loop->supervisor.state == BALANCE_STATE_CLOSED_LOOP
        && balance_measurement_timed_out(&loop->measurement, now_ms)) {
        balance_supervisor_raise_fault(&loop->supervisor, BALANCE_FAULT_CAMERA_TIMEOUT);
        reset_control_state(loop);
    }
}

void balance_loop_raise_motor_fault(BalanceLoop *loop)
{
    balance_supervisor_raise_fault(&loop->supervisor, BALANCE_FAULT_MOTOR_COMMUNICATION);
    reset_control_state(loop);
}

void balance_loop_emergency_stop(BalanceLoop *loop)
{
    balance_supervisor_raise_fault(&loop->supervisor, BALANCE_FAULT_EMERGENCY_STOP);
    reset_control_state(loop);
}

void balance_loop_acknowledge_fault(BalanceLoop *loop)
{
    reset_control_state(loop);
    balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    balance_supervisor_acknowledge_fault(&loop->supervisor);
}

void balance_loop_get_telemetry(const BalanceLoop *loop, BalanceTelemetry *telemetry)
{
    *telemetry = loop->telemetry;
}
```

- [ ] **Step 4: Run all pipeline tests and inspect warnings**

Run:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Expected: build has no warnings because `-Werror` is active, and CTest reports `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the coordinator**

```powershell
git add App/Inc/balance_loop.h App/Src/balance_loop.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: compose static balance control loop"
```


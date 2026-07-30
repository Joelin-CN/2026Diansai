# Balance Ball Control Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and host-test the protocol-independent static balance-ball control core for `0 cm`, `+5 cm`, and `-5 cm` targets, then compile the same C11 sources into the STM32F103 firmware.

**Architecture:** New camera frames enter a measurement guard and an alpha-beta observer. A ramped target feeds a position/velocity feedback controller with conditional integral action, then an actuator limiter converts the result into a bounded relative stepper target. A supervisor and loop coordinator enforce manual zeroing, open-loop approval, fresh-frame-only updates, timeout faults, and deterministic reset behavior.

**Tech Stack:** C11, CMake 3.22+, native MinGW GCC host tests, CTest, STM32Cube HAL build with ARM GCC.

## Global Constraints

- The MCU is STM32F103C8T6 at `72 MHz`; all core code must remain suitable for its memory and floating-point limitations.
- Camera position is a signed `float` in `cm`, nominally `35 Hz`, with static noise below `±0.1 cm`.
- Run observer, integral, and control updates once per newly accepted camera frame; never recalculate an old frame.
- Use measured receive-time delta, not a fixed `1/35 s` timestep.
- Controller output is a relative stepper-driver position unit, not a claimed rod angle.
- Manual level confirmation defines software zero on every boot; no persisted zero and no automatic recovery after MCU reset.
- Keep IMU feed-forward disabled and out of this implementation plan.
- Do not invent camera or stepper wire formats. UART/DMA adapters, CRC, byte order, and driver units require a second plan after both protocols are provided.
- Do not modify generated STM32 files outside their `USER CODE` sections. This plan does not need to modify generated files.
- Do not modify anything under `pictures/`.

## Scope Boundary

This plan produces a tested control library and proves it cross-compiles in the firmware. It intentionally does not start UART DMA reception, encode a motor command, read a physical confirmation button, or energize the motor. Those operations depend on protocols and pin assignments that are not available yet.

## File Map

- `App/Inc/balance_observer.h`, `App/Src/balance_observer.c`: alpha-beta position and velocity estimation.
- `App/Inc/balance_target.h`, `App/Src/balance_target.c`: bounded-rate target selection for `0 cm` and `±5 cm`.
- `App/Inc/balance_controller.h`, `App/Src/balance_controller.c`: `Kp/Kv/Ki` feedback and anti-windup.
- `App/Inc/balance_actuator.h`, `App/Src/balance_actuator.c`: direction correction, absolute limit, slew limit, and driver motion limits.
- `App/Inc/balance_measurement.h`, `App/Src/balance_measurement.c`: generic camera-sample validation, sequencing, and timeout detection.
- `App/Inc/balance_supervisor.h`, `App/Src/balance_supervisor.c`: startup, manual-zero, open-loop approval, ready, closed-loop, and fault transitions.
- `App/Inc/balance_loop.h`, `App/Src/balance_loop.c`: protocol-independent composition of the preceding modules.
- `tests/CMakeLists.txt`, `tests/test_balance.c`: native unit and pipeline tests.
- `CMakeLists.txt`: compile all `App` sources into the STM32 target.

---

### Task 1: Alpha-Beta Ball-State Observer

**Files:**
- Create: `App/Inc/balance_observer.h`
- Create: `App/Src/balance_observer.c`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_balance.c`

**Interfaces:**
- Consumes: signed camera position in `cm` and STM32-style receive timestamp in `uint32_t` milliseconds.
- Produces: `BalanceEstimate`; `balance_observer_init`, `balance_observer_reset`, and `balance_observer_update`.

- [ ] **Step 1: Create the host test harness and failing observer tests**

Create `tests/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(balance_ball_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

add_executable(test_balance
    test_balance.c
    ../App/Src/balance_observer.c
)
target_include_directories(test_balance PRIVATE ../App/Inc)
target_compile_options(test_balance PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_balance PRIVATE m)
add_test(NAME balance_core COMMAND test_balance)
```

Create `tests/test_balance.c` with a minimal test runner and these initial tests:

```c
#include <math.h>
#include <stdio.h>
#include "balance_observer.h"

static int failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

#define CHECK_NEAR(actual, expected, tolerance) \
    CHECK_TRUE(fabsf((actual) - (expected)) <= (tolerance))

static void test_observer_initializes_from_first_sample(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 0.75f,
        .beta = 0.20f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    CHECK_TRUE(balance_observer_update(&observer, 1000U, 2.5f, &estimate)
               == BALANCE_OBSERVER_RESET);
    CHECK_NEAR(estimate.position_cm, 2.5f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 0.0f, 0.0001f);
}

static void test_observer_uses_actual_frame_interval(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 1.0f,
        .beta = 0.5f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    balance_observer_update(&observer, 1000U, 0.0f, &estimate);
    CHECK_TRUE(balance_observer_update(&observer, 1040U, 1.0f, &estimate)
               == BALANCE_OBSERVER_UPDATED);
    CHECK_NEAR(estimate.position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 12.5f, 0.001f);
}

static void test_observer_rejects_duplicate_and_resets_after_gap(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 0.75f,
        .beta = 0.20f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    balance_observer_update(&observer, 1000U, 0.0f, &estimate);
    CHECK_TRUE(balance_observer_update(&observer, 1005U, 1.0f, &estimate)
               == BALANCE_OBSERVER_REJECTED);
    CHECK_TRUE(balance_observer_update(&observer, 1200U, 4.0f, &estimate)
               == BALANCE_OBSERVER_RESET);
    CHECK_NEAR(estimate.position_cm, 4.0f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 0.0f, 0.0001f);
}

int main(void)
{
    test_observer_initializes_from_first_sample();
    test_observer_uses_actual_frame_interval();
    test_observer_rejects_duplicate_and_resets_after_gap();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Run the tests and verify the observer API is missing**

Run:

```powershell
cmake -S tests -B build/host-tests -G "MinGW Makefiles"
cmake --build build/host-tests
```

Expected: build fails because `balance_observer.h` does not exist.

- [ ] **Step 3: Implement the observer**

Create `App/Inc/balance_observer.h`:

```c
#ifndef BALANCE_OBSERVER_H
#define BALANCE_OBSERVER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float alpha;
    float beta;
    float min_dt_s;
    float max_dt_s;
} BalanceObserverConfig;

typedef struct {
    float position_cm;
    float velocity_cm_s;
} BalanceEstimate;

typedef enum {
    BALANCE_OBSERVER_REJECTED = 0,
    BALANCE_OBSERVER_RESET,
    BALANCE_OBSERVER_UPDATED,
} BalanceObserverResult;

typedef struct {
    BalanceObserverConfig config;
    BalanceEstimate estimate;
    uint32_t last_timestamp_ms;
    bool initialized;
} BalanceObserver;

void balance_observer_init(BalanceObserver *observer,
                           const BalanceObserverConfig *config);
void balance_observer_reset(BalanceObserver *observer);
BalanceObserverResult balance_observer_update(BalanceObserver *observer,
                                              uint32_t timestamp_ms,
                                              float position_cm,
                                              BalanceEstimate *estimate);

#endif
```

Create `App/Src/balance_observer.c`:

```c
#include "balance_observer.h"

void balance_observer_init(BalanceObserver *observer,
                           const BalanceObserverConfig *config)
{
    observer->config = *config;
    balance_observer_reset(observer);
}

void balance_observer_reset(BalanceObserver *observer)
{
    observer->estimate.position_cm = 0.0f;
    observer->estimate.velocity_cm_s = 0.0f;
    observer->last_timestamp_ms = 0U;
    observer->initialized = false;
}

BalanceObserverResult balance_observer_update(BalanceObserver *observer,
                                              uint32_t timestamp_ms,
                                              float position_cm,
                                              BalanceEstimate *estimate)
{
    if (!observer->initialized) {
        observer->estimate.position_cm = position_cm;
        observer->estimate.velocity_cm_s = 0.0f;
        observer->last_timestamp_ms = timestamp_ms;
        observer->initialized = true;
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_RESET;
    }

    const float dt_s = (float)(timestamp_ms - observer->last_timestamp_ms) * 0.001f;
    if (dt_s < observer->config.min_dt_s) {
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_REJECTED;
    }
    if (dt_s > observer->config.max_dt_s) {
        observer->estimate.position_cm = position_cm;
        observer->estimate.velocity_cm_s = 0.0f;
        observer->last_timestamp_ms = timestamp_ms;
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_RESET;
    }

    const float predicted_position = observer->estimate.position_cm
                                   + observer->estimate.velocity_cm_s * dt_s;
    const float residual = position_cm - predicted_position;
    observer->estimate.position_cm = predicted_position
                                  + observer->config.alpha * residual;
    observer->estimate.velocity_cm_s += observer->config.beta * residual / dt_s;
    observer->last_timestamp_ms = timestamp_ms;
    *estimate = observer->estimate;
    return BALANCE_OBSERVER_UPDATED;
}
```

- [ ] **Step 4: Build and run the observer tests**

Run:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the observer**

```powershell
git add App/Inc/balance_observer.h App/Src/balance_observer.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: add balance ball state observer"
```

### Task 2: Ramped Position Target

**Files:**
- Create: `App/Inc/balance_target.h`
- Create: `App/Src/balance_target.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: requested target in `cm` and frame `dt` in seconds.
- Produces: `balance_target_select` and `balance_target_step`; only `-5.0f`, `0.0f`, and `+5.0f` selections are accepted.

- [ ] **Step 1: Add failing ramp and target-validation tests**

Add `../App/Src/balance_target.c` to `test_balance` in `tests/CMakeLists.txt`. Add the include and tests to `tests/test_balance.c`:

```c
#include "balance_target.h"

static void test_target_rejects_non_competition_position(void)
{
    BalanceTarget target;
    balance_target_init(&target, 2.0f);
    CHECK_TRUE(!balance_target_select(&target, 3.0f));
    CHECK_NEAR(target.requested_cm, 0.0f, 0.0001f);
}

static void test_target_ramps_without_overshoot(void)
{
    BalanceTarget target;
    balance_target_init(&target, 2.0f);
    CHECK_TRUE(balance_target_select(&target, 5.0f));
    CHECK_NEAR(balance_target_step(&target, 0.5f), 1.0f, 0.0001f);
    CHECK_NEAR(balance_target_step(&target, 2.0f), 5.0f, 0.0001f);
    CHECK_NEAR(balance_target_step(&target, 0.5f), 5.0f, 0.0001f);
}
```

Call both tests from `main`.

- [ ] **Step 2: Run the tests and verify they fail for the missing target API**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_target.h` does not exist.

- [ ] **Step 3: Implement target validation and slew**

Create `App/Inc/balance_target.h`:

```c
#ifndef BALANCE_TARGET_H
#define BALANCE_TARGET_H

#include <stdbool.h>

typedef struct {
    float requested_cm;
    float ramped_cm;
    float max_rate_cm_s;
} BalanceTarget;

void balance_target_init(BalanceTarget *target, float max_rate_cm_s);
bool balance_target_select(BalanceTarget *target, float requested_cm);
float balance_target_step(BalanceTarget *target, float dt_s);
void balance_target_reset(BalanceTarget *target);

#endif
```

Create `App/Src/balance_target.c`:

```c
#include "balance_target.h"

static float absf(float value) { return value < 0.0f ? -value : value; }

void balance_target_init(BalanceTarget *target, float max_rate_cm_s)
{
    target->max_rate_cm_s = max_rate_cm_s;
    balance_target_reset(target);
}

void balance_target_reset(BalanceTarget *target)
{
    target->requested_cm = 0.0f;
    target->ramped_cm = 0.0f;
}

bool balance_target_select(BalanceTarget *target, float requested_cm)
{
    if (requested_cm != -5.0f && requested_cm != 0.0f && requested_cm != 5.0f) {
        return false;
    }
    target->requested_cm = requested_cm;
    return true;
}

float balance_target_step(BalanceTarget *target, float dt_s)
{
    const float delta = target->requested_cm - target->ramped_cm;
    const float max_delta = target->max_rate_cm_s * dt_s;
    if (absf(delta) <= max_delta) {
        target->ramped_cm = target->requested_cm;
    } else {
        target->ramped_cm += delta > 0.0f ? max_delta : -max_delta;
    }
    return target->ramped_cm;
}
```

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the target generator**

```powershell
git add App/Inc/balance_target.h App/Src/balance_target.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: add ramped balance targets"
```

### Task 3: Position and Velocity Feedback Controller

**Files:**
- Create: `App/Inc/balance_controller.h`
- Create: `App/Src/balance_controller.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: ramped target, `BalanceEstimate`, frame `dt`, and an integral-enable flag.
- Produces: `BalanceControlOutput` containing error, raw output, limited output, and saturation status.

- [ ] **Step 1: Add failing feedback, damping, and anti-windup tests**

Add `../App/Src/balance_controller.c` to the host target. Add:

```c
#include "balance_controller.h"

static BalanceControllerConfig controller_config(void)
{
    const BalanceControllerConfig config = {
        .kp = 2.0f,
        .kv = 0.5f,
        .ki = 1.0f,
        .integral_zone_cm = 1.0f,
        .integral_limit = 2.0f,
        .output_limit = 10.0f,
    };
    return config;
}

static void test_controller_uses_velocity_as_damping(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    const BalanceControllerConfig config = controller_config();
    const BalanceEstimate estimate = { .position_cm = 1.0f, .velocity_cm_s = 2.0f };
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 0.03f, false);
    CHECK_NEAR(output.error_cm, -1.0f, 0.0001f);
    CHECK_NEAR(output.limited, -3.0f, 0.0001f);
}

static void test_controller_integrates_only_inside_zone(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    const BalanceControllerConfig config = controller_config();
    BalanceEstimate estimate = { .position_cm = -0.5f, .velocity_cm_s = 0.0f };
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_NEAR(controller.integral, 0.5f, 0.0001f);
    estimate.position_cm = -2.0f;
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_NEAR(controller.integral, 0.5f, 0.0001f);
    (void)output;
}

static void test_controller_does_not_wind_up_into_saturation(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    BalanceControllerConfig config = controller_config();
    const BalanceEstimate estimate = { .position_cm = -0.5f, .velocity_cm_s = 0.0f };
    config.kp = 30.0f;
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_TRUE(output.saturated);
    CHECK_NEAR(controller.integral, 0.0f, 0.0001f);
    CHECK_NEAR(output.limited, 10.0f, 0.0001f);
}
```

Call the tests from `main`.

- [ ] **Step 2: Run the tests and confirm the controller API is missing**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_controller.h` does not exist.

- [ ] **Step 3: Implement feedback and conditional integration**

Create `App/Inc/balance_controller.h`:

```c
#ifndef BALANCE_CONTROLLER_H
#define BALANCE_CONTROLLER_H

#include <stdbool.h>
#include "balance_observer.h"

typedef struct {
    float kp;
    float kv;
    float ki;
    float integral_zone_cm;
    float integral_limit;
    float output_limit;
} BalanceControllerConfig;

typedef struct {
    BalanceControllerConfig config;
    float integral;
} BalanceController;

typedef struct {
    float error_cm;
    float raw;
    float limited;
    bool saturated;
} BalanceControlOutput;

void balance_controller_init(BalanceController *controller,
                             const BalanceControllerConfig *config);
void balance_controller_reset(BalanceController *controller);
BalanceControlOutput balance_controller_step(BalanceController *controller,
                                             float target_cm,
                                             const BalanceEstimate *estimate,
                                             float dt_s,
                                             bool allow_integral);

#endif
```

Implement `App/Src/balance_controller.c`:

```c
#include "balance_controller.h"

static float clampf(float value, float limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

static float absf(float value) { return value < 0.0f ? -value : value; }

void balance_controller_init(BalanceController *controller,
                             const BalanceControllerConfig *config)
{
    controller->config = *config;
    balance_controller_reset(controller);
}

void balance_controller_reset(BalanceController *controller)
{
    controller->integral = 0.0f;
}

BalanceControlOutput balance_controller_step(BalanceController *controller,
                                             float target_cm,
                                             const BalanceEstimate *estimate,
                                             float dt_s,
                                             bool allow_integral)
{
    BalanceControlOutput output;
    output.error_cm = target_cm - estimate->position_cm;
    const float base = controller->config.kp * output.error_cm
                     - controller->config.kv * estimate->velocity_cm_s;
    float candidate_integral = controller->integral;
    if (allow_integral && absf(output.error_cm) <= controller->config.integral_zone_cm) {
        candidate_integral = clampf(controller->integral + output.error_cm * dt_s,
                                    controller->config.integral_limit);
    }

    const float candidate_raw = base + controller->config.ki * candidate_integral;
    const bool pushes_positive_limit = candidate_raw > controller->config.output_limit
                                    && output.error_cm > 0.0f;
    const bool pushes_negative_limit = candidate_raw < -controller->config.output_limit
                                    && output.error_cm < 0.0f;
    if (!pushes_positive_limit && !pushes_negative_limit) {
        controller->integral = candidate_integral;
    }

    output.raw = base + controller->config.ki * controller->integral;
    output.limited = clampf(output.raw, controller->config.output_limit);
    output.saturated = output.raw != output.limited;
    return output;
}
```

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the controller**

```powershell
git add App/Inc/balance_controller.h App/Src/balance_controller.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: add balance state feedback controller"
```

### Task 4: Relative Stepper Command Limiter

**Files:**
- Create: `App/Inc/balance_actuator.h`
- Create: `App/Src/balance_actuator.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: controller output in relative driver position units.
- Produces: `BalanceActuatorCommand` with target position, configured speed, configured acceleration, and limit flags.

- [ ] **Step 1: Add failing sign, absolute-limit, and slew-limit tests**

Add the source to host CMake and add:

```c
#include "balance_actuator.h"

static void test_actuator_applies_direction_absolute_and_slew_limits(void)
{
    BalanceActuator actuator;
    BalanceActuatorCommand command;
    const BalanceActuatorConfig config = {
        .control_sign = -1.0f,
        .position_limit = 4.0f,
        .max_delta_per_frame = 1.0f,
        .speed = 20.0f,
        .acceleration = 50.0f,
    };

    balance_actuator_init(&actuator, &config);
    command = balance_actuator_limit(&actuator, 10.0f);
    CHECK_NEAR(command.position, -1.0f, 0.0001f);
    CHECK_TRUE(command.position_limited);
    CHECK_TRUE(command.slew_limited);
    CHECK_NEAR(command.speed, 20.0f, 0.0001f);
    CHECK_NEAR(command.acceleration, 50.0f, 0.0001f);
    command = balance_actuator_limit(&actuator, 10.0f);
    CHECK_NEAR(command.position, -2.0f, 0.0001f);
}

static void test_actuator_reset_returns_command_state_to_zero(void)
{
    BalanceActuator actuator;
    const BalanceActuatorConfig config = {
        .control_sign = 1.0f, .position_limit = 4.0f,
        .max_delta_per_frame = 1.0f, .speed = 20.0f, .acceleration = 50.0f,
    };
    balance_actuator_init(&actuator, &config);
    (void)balance_actuator_limit(&actuator, 3.0f);
    balance_actuator_reset(&actuator);
    CHECK_NEAR(actuator.previous_position, 0.0f, 0.0001f);
}
```

- [ ] **Step 2: Run the tests and confirm the actuator API is missing**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_actuator.h` does not exist.

- [ ] **Step 3: Implement the actuator limiter**

Create `App/Inc/balance_actuator.h`:

```c
#ifndef BALANCE_ACTUATOR_H
#define BALANCE_ACTUATOR_H

#include <stdbool.h>

typedef struct {
    float control_sign;
    float position_limit;
    float max_delta_per_frame;
    float speed;
    float acceleration;
} BalanceActuatorConfig;

typedef struct {
    BalanceActuatorConfig config;
    float previous_position;
} BalanceActuator;

typedef struct {
    float position;
    float speed;
    float acceleration;
    bool position_limited;
    bool slew_limited;
} BalanceActuatorCommand;

void balance_actuator_init(BalanceActuator *actuator,
                           const BalanceActuatorConfig *config);
void balance_actuator_reset(BalanceActuator *actuator);
BalanceActuatorCommand balance_actuator_limit(BalanceActuator *actuator,
                                              float controller_output);

#endif
```

Implement `App/Src/balance_actuator.c`:

```c
#include "balance_actuator.h"

static float clampf(float value, float limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

void balance_actuator_init(BalanceActuator *actuator,
                           const BalanceActuatorConfig *config)
{
    actuator->config = *config;
    balance_actuator_reset(actuator);
}

void balance_actuator_reset(BalanceActuator *actuator)
{
    actuator->previous_position = 0.0f;
}

BalanceActuatorCommand balance_actuator_limit(BalanceActuator *actuator,
                                              float controller_output)
{
    BalanceActuatorCommand command = {0};
    const float signed_target = actuator->config.control_sign * controller_output;
    const float bounded_target = clampf(signed_target, actuator->config.position_limit);
    const float delta = bounded_target - actuator->previous_position;
    const float bounded_delta = clampf(delta, actuator->config.max_delta_per_frame);
    command.position = actuator->previous_position + bounded_delta;
    command.speed = actuator->config.speed;
    command.acceleration = actuator->config.acceleration;
    command.position_limited = signed_target != bounded_target;
    command.slew_limited = delta != bounded_delta;
    actuator->previous_position = command.position;
    return command;
}
```

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the limiter**

```powershell
git add App/Inc/balance_actuator.h App/Src/balance_actuator.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: limit balance actuator commands"
```

### Task 5: Generic Camera Measurement Guard

**Files:**
- Create: `App/Inc/balance_measurement.h`
- Create: `App/Src/balance_measurement.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: protocol-decoded `BalanceMeasurement` with sequence, validity, position, and STM32 receive timestamp.
- Produces: accepted/rejected classification and timeout status. It does not decode bytes.

- [ ] **Step 1: Add failing validity, sequencing, range, jump, and timeout tests**

Add the source to host CMake and add:

```c
#include "balance_measurement.h"

static void test_measurement_guard_rejects_invalid_duplicate_and_out_of_range(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = {
        .min_position_cm = -11.5f,
        .max_position_cm = 11.5f,
        .max_jump_cm = 4.0f,
        .timeout_ms = 100U,
    };
    balance_measurement_guard_init(&guard, &config);
    BalanceMeasurement sample = { .sequence = 1U, .rx_timestamp_ms = 1000U,
                                  .valid = true, .position_cm = 0.0f };
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_ACCEPTED);
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_DUPLICATE);
    sample.sequence = 0U;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_STALE);
    sample.sequence = 2U; sample.position_cm = 20.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_OUT_OF_RANGE);
    sample.sequence = 3U; sample.position_cm = 5.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_JUMP);
    sample.sequence = 4U; sample.valid = false; sample.position_cm = 0.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample) == BALANCE_MEASUREMENT_INVALID);
}

static void test_measurement_timeout_uses_last_accepted_frame(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = {
        .min_position_cm = -11.5f, .max_position_cm = 11.5f,
        .max_jump_cm = 4.0f, .timeout_ms = 100U,
    };
    const BalanceMeasurement sample = { .sequence = 1U, .rx_timestamp_ms = 1000U,
                                        .valid = true, .position_cm = 0.0f };
    balance_measurement_guard_init(&guard, &config);
    CHECK_TRUE(!balance_measurement_timed_out(&guard, 5000U));
    (void)balance_measurement_accept(&guard, &sample);
    CHECK_TRUE(!balance_measurement_timed_out(&guard, 1100U));
    CHECK_TRUE(balance_measurement_timed_out(&guard, 1101U));
}
```

- [ ] **Step 2: Run tests and confirm the measurement API is missing**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_measurement.h` does not exist.

- [ ] **Step 3: Implement protocol-independent sample validation**

Create `App/Inc/balance_measurement.h`:

```c
#ifndef BALANCE_MEASUREMENT_H
#define BALANCE_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t sequence;
    uint32_t rx_timestamp_ms;
    bool valid;
    float position_cm;
} BalanceMeasurement;

typedef struct {
    float min_position_cm;
    float max_position_cm;
    float max_jump_cm;
    uint32_t timeout_ms;
} BalanceMeasurementConfig;

typedef enum {
    BALANCE_MEASUREMENT_ACCEPTED = 0,
    BALANCE_MEASUREMENT_INVALID,
    BALANCE_MEASUREMENT_DUPLICATE,
    BALANCE_MEASUREMENT_STALE,
    BALANCE_MEASUREMENT_OUT_OF_RANGE,
    BALANCE_MEASUREMENT_JUMP,
} BalanceMeasurementResult;

typedef struct {
    BalanceMeasurementConfig config;
    BalanceMeasurement last;
    bool has_sample;
} BalanceMeasurementGuard;

void balance_measurement_guard_init(BalanceMeasurementGuard *guard,
                                    const BalanceMeasurementConfig *config);
BalanceMeasurementResult balance_measurement_accept(BalanceMeasurementGuard *guard,
                                                    const BalanceMeasurement *sample);
bool balance_measurement_timed_out(const BalanceMeasurementGuard *guard,
                                   uint32_t now_ms);

#endif
```

Implement `App/Src/balance_measurement.c`:

```c
#include "balance_measurement.h"
#include <math.h>

void balance_measurement_guard_init(BalanceMeasurementGuard *guard,
                                    const BalanceMeasurementConfig *config)
{
    guard->config = *config;
    guard->has_sample = false;
}

BalanceMeasurementResult balance_measurement_accept(BalanceMeasurementGuard *guard,
                                                    const BalanceMeasurement *sample)
{
    if (!sample->valid || !isfinite(sample->position_cm)) {
        return BALANCE_MEASUREMENT_INVALID;
    }
    if (guard->has_sample && sample->sequence == guard->last.sequence) {
        return BALANCE_MEASUREMENT_DUPLICATE;
    }
    if (guard->has_sample
        && (int32_t)(sample->sequence - guard->last.sequence) < 0) {
        return BALANCE_MEASUREMENT_STALE;
    }
    if (sample->position_cm < guard->config.min_position_cm
        || sample->position_cm > guard->config.max_position_cm) {
        return BALANCE_MEASUREMENT_OUT_OF_RANGE;
    }
    if (guard->has_sample
        && fabsf(sample->position_cm - guard->last.position_cm) > guard->config.max_jump_cm) {
        return BALANCE_MEASUREMENT_JUMP;
    }
    guard->last = *sample;
    guard->has_sample = true;
    return BALANCE_MEASUREMENT_ACCEPTED;
}

bool balance_measurement_timed_out(const BalanceMeasurementGuard *guard,
                                   uint32_t now_ms)
{
    return guard->has_sample
        && (uint32_t)(now_ms - guard->last.rx_timestamp_ms) > guard->config.timeout_ms;
}
```

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the measurement guard**

```powershell
git add App/Inc/balance_measurement.h App/Src/balance_measurement.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: validate balance camera measurements"
```

### Task 6: Safety Supervisor

**Files:**
- Create: `App/Inc/balance_supervisor.h`
- Create: `App/Src/balance_supervisor.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_balance.c`

**Interfaces:**
- Consumes: explicit operator and subsystem events.
- Produces: `BalanceState`, `BalanceFault`, and transition acceptance. No GPIO or HAL dependency.

- [ ] **Step 1: Add failing startup, approval, and fault-latching tests**

Add the source to host CMake and add:

```c
#include "balance_supervisor.h"

static void test_supervisor_requires_zero_and_open_loop_approval(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO);
    CHECK_TRUE(!balance_supervisor_start_closed_loop(&supervisor));
    balance_supervisor_confirm_manual_zero(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_OPEN_LOOP_CHECK);
    CHECK_TRUE(!balance_supervisor_complete_open_loop(&supervisor, false));
    CHECK_TRUE(balance_supervisor_complete_open_loop(&supervisor, true));
    CHECK_TRUE(supervisor.state == BALANCE_STATE_READY);
    CHECK_TRUE(balance_supervisor_start_closed_loop(&supervisor));
    CHECK_TRUE(supervisor.state == BALANCE_STATE_CLOSED_LOOP);
}

static void test_supervisor_fault_requires_new_manual_zero(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_CAMERA_TIMEOUT);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT);
    balance_supervisor_acknowledge_fault(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_NONE);
}
```

- [ ] **Step 2: Run tests and confirm the supervisor API is missing**

Run `cmake --build build/host-tests`.

Expected: compile fails because `balance_supervisor.h` does not exist.

- [ ] **Step 3: Implement explicit state transitions**

Create `App/Inc/balance_supervisor.h`:

```c
#ifndef BALANCE_SUPERVISOR_H
#define BALANCE_SUPERVISOR_H

#include <stdbool.h>

typedef enum {
    BALANCE_STATE_WAIT_MANUAL_ZERO = 0,
    BALANCE_STATE_OPEN_LOOP_CHECK,
    BALANCE_STATE_READY,
    BALANCE_STATE_CLOSED_LOOP,
    BALANCE_STATE_FAULT,
} BalanceState;

typedef enum {
    BALANCE_FAULT_NONE = 0,
    BALANCE_FAULT_CAMERA_TIMEOUT,
    BALANCE_FAULT_CAMERA_DATA,
    BALANCE_FAULT_MOTOR_COMMUNICATION,
    BALANCE_FAULT_OUTPUT_SATURATION,
    BALANCE_FAULT_BALL_END_ZONE,
    BALANCE_FAULT_EMERGENCY_STOP,
} BalanceFault;

typedef struct {
    BalanceState state;
    BalanceFault fault;
} BalanceSupervisor;

void balance_supervisor_init(BalanceSupervisor *supervisor);
void balance_supervisor_confirm_manual_zero(BalanceSupervisor *supervisor);
bool balance_supervisor_complete_open_loop(BalanceSupervisor *supervisor, bool approved);
bool balance_supervisor_start_closed_loop(BalanceSupervisor *supervisor);
void balance_supervisor_stop(BalanceSupervisor *supervisor);
void balance_supervisor_raise_fault(BalanceSupervisor *supervisor, BalanceFault fault);
void balance_supervisor_acknowledge_fault(BalanceSupervisor *supervisor);

#endif
```

Implement `App/Src/balance_supervisor.c`:

```c
#include "balance_supervisor.h"

void balance_supervisor_init(BalanceSupervisor *supervisor)
{
    supervisor->state = BALANCE_STATE_WAIT_MANUAL_ZERO;
    supervisor->fault = BALANCE_FAULT_NONE;
}

void balance_supervisor_confirm_manual_zero(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_WAIT_MANUAL_ZERO) {
        supervisor->state = BALANCE_STATE_OPEN_LOOP_CHECK;
    }
}

bool balance_supervisor_complete_open_loop(BalanceSupervisor *supervisor, bool approved)
{
    if (supervisor->state != BALANCE_STATE_OPEN_LOOP_CHECK || !approved) return false;
    supervisor->state = BALANCE_STATE_READY;
    return true;
}

bool balance_supervisor_start_closed_loop(BalanceSupervisor *supervisor)
{
    if (supervisor->state != BALANCE_STATE_READY) return false;
    supervisor->state = BALANCE_STATE_CLOSED_LOOP;
    return true;
}

void balance_supervisor_stop(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_CLOSED_LOOP) supervisor->state = BALANCE_STATE_READY;
}

void balance_supervisor_raise_fault(BalanceSupervisor *supervisor, BalanceFault fault)
{
    supervisor->fault = fault;
    supervisor->state = BALANCE_STATE_FAULT;
}

void balance_supervisor_acknowledge_fault(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_FAULT) balance_supervisor_init(supervisor);
}
```

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: `100% tests passed, 0 tests failed`.

- [ ] **Step 5: Commit the supervisor**

```powershell
git add App/Inc/balance_supervisor.h App/Src/balance_supervisor.c tests/CMakeLists.txt tests/test_balance.c
git commit -m "feat: add balance safety supervisor"
```

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

### Task 8: STM32 Cross-Compilation Integration

**Files:**
- Modify: `CMakeLists.txt`
- Verify: all `App/Inc/*.h`, `App/Src/*.c`

**Interfaces:**
- Consumes: all protocol-independent core modules from Tasks 1-7.
- Produces: an ARM firmware ELF containing the control core but not invoking it from `main.c`.

- [ ] **Step 1: Establish the failing firmware link check**

Run:

```powershell
cmake --preset Debug
cmake --build --preset Debug
Test-Path -LiteralPath "build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj"
```

Expected: firmware may build, then `Test-Path` prints `False` because the App sources are not compiled.

- [ ] **Step 2: Add the protocol-independent sources and includes to the target**

Replace the empty user sections in root `CMakeLists.txt` with:

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/balance_observer.c
    App/Src/balance_target.c
    App/Src/balance_controller.c
    App/Src/balance_actuator.c
    App/Src/balance_measurement.c
    App/Src/balance_supervisor.c
    App/Src/balance_loop.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Inc
)
```

- [ ] **Step 3: Run host tests and a clean ARM build**

Run:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
cmake --preset Debug
cmake --build --preset Debug --clean-first
arm-none-eabi-nm build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj | rg "balance_loop_init"
```

Expected:

- Host CTest: `100% tests passed, 0 tests failed`.
- ARM build exits `0` and produces `build/Debug/balanceBall.elf`.
- `nm` prints one `balance_loop_init` symbol from the ARM object file, proving the core was cross-compiled. The function may be absent from the final ELF because it is intentionally not called yet and linker garbage collection may remove it.

- [ ] **Step 4: Check final diff scope and generated-file safety**

Run:

```powershell
git diff --check
git status --short
git diff -- CMakeLists.txt App tests
```

Expected: no whitespace errors; intended changes are limited to `CMakeLists.txt`, `App/`, and `tests/`. Do not stage `build/`, `.superpowers/`, the photo, deleted PDFs, or unrelated `v1.0` work.

- [ ] **Step 5: Commit cross-compilation integration**

```powershell
git add CMakeLists.txt App tests
git commit -m "build: link balance control core"
```

## Completion Gate

Before claiming this plan implemented, run fresh:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
cmake --build --preset Debug --clean-first
git status --short
```

Report the exact CTest pass count, ARM build exit status, resulting ELF path, and any remaining unrelated worktree changes. Do not claim physical closed-loop operation: that requires the camera protocol, stepper protocol, hardware adapter plan, bench tests, and user acceptance.

## Required Follow-Up Inputs

The hardware-integration plan starts only after receiving both wire protocols. It must define exact frame bytes, checksum behavior, parser recovery, DMA framing method, driver units, safe stop command, status response, command refresh limit, and UART assignment. It will then cover `main.c` integration, UART callbacks, telemetry, manual-zero input, open-loop check commands, and hardware verification.

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


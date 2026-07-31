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


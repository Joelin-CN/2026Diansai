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


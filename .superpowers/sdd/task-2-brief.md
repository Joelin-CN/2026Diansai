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


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


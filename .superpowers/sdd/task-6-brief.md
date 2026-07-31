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


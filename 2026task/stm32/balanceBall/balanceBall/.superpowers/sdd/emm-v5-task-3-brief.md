### Task 3: Software-Zero Balance Motor Adapter

**Files:**
- Create: `App/Inc/balance_motor.h`
- Create: `App/Src/balance_motor.c`
- Create: `tests/test_balance_motor.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `BalanceActuatorCommand`, Task 1 encoders, and Task 2 position/ACK parsers.
- Produces: `BalanceMotor`, `BalanceMotorTransport`, `balance_motor_init()`, `balance_motor_request_zero()`, `balance_motor_submit()`, `balance_motor_stop()`, `balance_motor_disable()`, `balance_motor_on_response()`, `balance_motor_on_transport_error()`, and `balance_motor_process()`.

- [ ] **Step 1: Add failing adapter tests with a fake transport**

The fake transport captures the last frame and returns accepted, busy, or failed. Add tests with this configuration:

```c
const BalanceMotorConfig config = {
    .address = 0x01U,
    .pulses_per_position_unit = 100.0f,
    .max_consecutive_failures = 3U,
};
```

Required cases:

- `balance_motor_init()` emits no frame and starts without a valid zero.
- `balance_motor_submit()` rejects motion before a valid zero.
- `balance_motor_request_zero()` emits `{0x01, 0x36, 0x6B}`; only a valid response establishes zero.
- Zero `1000`, command position `+2.5` produces absolute target `1250`; position `-2.5` produces `750`.
- `NAN`, `INFINITY`, negative speed/acceleration, conversion overflow, and `zero + offset` overflow are rejected.
- While transport is busy, three position commands retain only the third frame.
- Stop and disable clear a pending position and are sent before normal motion.
- Three consecutive transport/protocol failures lock movement and invalidate zero.
- Clearing the fault does not restore zero; a new zero query is required.

Use a test such as:

```c
static void test_busy_transport_keeps_only_latest_target(void)
{
    BalanceActuatorCommand first = {.position = 1.0f, .speed = 20.0f, .acceleration = 50.0f};
    BalanceActuatorCommand latest = {.position = 3.0f, .speed = 20.0f, .acceleration = 50.0f};
    fake.busy = true;
    CHECK_TRUE(balance_motor_submit(&motor, &first) == BALANCE_MOTOR_QUEUED);
    CHECK_TRUE(balance_motor_submit(&motor, &latest) == BALANCE_MOTOR_QUEUED);
    fake.busy = false;
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 1U);
    CHECK_TRUE(fake.frame[6] == 0x00U && fake.frame[7] == 0x00U
               && fake.frame[8] == 0x05U && fake.frame[9] == 0x14U);
}
```

- [ ] **Step 2: Run adapter tests and verify failure**

Run `cmake --build build/host-tests --target test_balance_motor`.

Expected: compilation fails because `balance_motor.h` is absent.

- [ ] **Step 3: Implement the minimal adapter and transport boundary**

Use this transport contract:

```c
typedef enum {
    BALANCE_MOTOR_TX_ACCEPTED,
    BALANCE_MOTOR_TX_BUSY,
    BALANCE_MOTOR_TX_FAILED
} BalanceMotorTxResult;

typedef BalanceMotorTxResult (*BalanceMotorSendFn)(void *context,
                                                    const uint8_t *frame,
                                                    size_t length,
                                                    uint8_t expected_function,
                                                    size_t expected_length);

typedef struct {
    BalanceMotorSendFn send;
    void *context;
} BalanceMotorTransport;

typedef struct {
    uint8_t address;
    float pulses_per_position_unit;
    uint8_t max_consecutive_failures;
} BalanceMotorConfig;

typedef enum {
    BALANCE_MOTOR_OK,
    BALANCE_MOTOR_QUEUED,
    BALANCE_MOTOR_NOT_ZEROED,
    BALANCE_MOTOR_BUSY,
    BALANCE_MOTOR_INVALID_ARGUMENT,
    BALANCE_MOTOR_OVERFLOW,
    BALANCE_MOTOR_LOCKED,
    BALANCE_MOTOR_TRANSPORT_ERROR,
    BALANCE_MOTOR_PROTOCOL_ERROR
} BalanceMotorResult;

void balance_motor_init(BalanceMotor *motor,
                        const BalanceMotorConfig *config,
                        BalanceMotorTransport transport);
BalanceMotorResult balance_motor_request_zero(BalanceMotor *motor);
BalanceMotorResult balance_motor_submit(BalanceMotor *motor,
                                        const BalanceActuatorCommand *command);
BalanceMotorResult balance_motor_stop(BalanceMotor *motor);
BalanceMotorResult balance_motor_disable(BalanceMotor *motor);
void balance_motor_on_response(BalanceMotor *motor,
                               uint8_t expected_function,
                               const uint8_t *response,
                               size_t response_length);
void balance_motor_on_transport_error(BalanceMotor *motor);
void balance_motor_process(BalanceMotor *motor);
void balance_motor_clear_fault(BalanceMotor *motor);
bool balance_motor_has_zero(const BalanceMotor *motor);
```

Round finite non-negative speed and acceleration to nearest integer after range checks (`speed <= UINT16_MAX`, `acceleration <= UINT8_MAX`). Convert position using `roundf(position * pulses_per_position_unit)` after checking the scaled value fits `int32_t`. Check `zero_position + offset` before addition. Encode the resulting signed absolute target as direction plus magnitude without negating `INT32_MIN` in signed arithmetic.

Keep one pending normal target and one pending priority command. A newly queued target overwrites the previous target. Stop or disable clears the normal target. `balance_motor_on_response()` accepts the completed raw response and updates zero or ACK state; `balance_motor_on_transport_error()` increments the failure count. Lockout invalidates zero and clears pending work. `balance_motor_clear_fault()` clears lockout and counters but deliberately leaves zero invalid.

- [ ] **Step 4: Run adapter and regression tests**

Run:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Expected: `balance_core`, `emm_v5_protocol`, and `balance_motor` pass.

- [ ] **Step 5: Commit the adapter**

```powershell
git add App/Inc/balance_motor.h App/Src/balance_motor.c tests/test_balance_motor.c tests/CMakeLists.txt
git commit -m "feat: adapt balance commands to Emm V5"
```


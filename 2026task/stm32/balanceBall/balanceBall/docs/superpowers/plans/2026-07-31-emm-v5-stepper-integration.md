# Emm V5 Stepper Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the complete usable Emm V5 UART protocol from `../temp/SM.c` and `../temp/SM.h` into the balance-ball firmware, connect `BalanceActuatorCommand` through a software-zero-aware adapter, and provide non-blocking USART2 DMA transport without automatic motor motion at startup.

**Architecture:** A HAL-independent `emm_v5_protocol` module owns byte encoding and response parsing. A HAL-independent `balance_motor` module owns software zero, unit conversion, latest-target replacement, command priority, and communication-failure lockout through an injected raw-frame transport interface. `emm_v5_uart` owns USART2 DMA buffers and one in-flight request; `main.c` initializes and polls these objects and forwards HAL callbacks without sending startup commands.

**Tech Stack:** C11, STM32F1 HAL, USART2 RX/TX DMA, CMake/Ninja, native CTest tests, GNU Arm Embedded GCC.

## Global Constraints

- Do not modify or compile `../temp/SM.c` or `../temp/SM.h`; use them only as protocol reference input.
- Target one Emm V5 driver at address `0x01` on USART2 at the existing `115200 8N1` configuration.
- Do not write microstepping settings, enable the motor, home the mechanism, or emit a motion command automatically at boot.
- Establish software zero only after an explicit operator request and a valid real-time-position response.
- Use fixed-width integer types and reject invalid values, overflows, short frames, mismatched responses, and insufficient buffers.
- Keep protocol and balance-motor logic free of STM32 HAL dependencies so they run in host tests.
- Keep CubeMX-owned edits inside `USER CODE` regions.
- Position targets replace older unsent position targets; stop and disable commands take priority over pending position targets.
- Acceptance requires existing host tests, new host tests, and the ARM Debug build to pass.
- Preserve the reference-file SHA-256 hashes: `SM.c` = `634421E0D84592FA447A77BD1C8AF8DD15CD7BC2794F49AD9C7CAD600E2D3F8F`; `SM.h` = `B586B830C9A682DC6AC8DD811CA0C7DE4D8F5ACA1F4F2B2AD300C280311C92B1`.

## File Map

- `App/Inc/emm_v5_protocol.h`: protocol constants, fixed-width command parameters, parse results, and public encoding/parsing declarations.
- `App/Src/emm_v5_protocol.c`: bounded frame construction, big-endian helpers, ACK/position/status/PID parsing.
- `tests/test_emm_v5_protocol.c`: byte-exact protocol and malformed-response tests.
- `App/Inc/balance_motor.h`: software-zero adapter state, transport interface, configuration, and public lifecycle API.
- `App/Src/balance_motor.c`: command conversion, overflow checks, pending-target replacement, priority commands, and lockout policy.
- `tests/test_balance_motor.c`: fake-transport tests for software zero, conversion, queuing, priority, and faults.
- `App/Inc/emm_v5_uart.h`: USART DMA transaction state and callback-facing API.
- `App/Src/emm_v5_uart.c`: stable DMA buffers, one-request state machine, response timeout, and event handling.
- `tests/stubs/main.h`, `tests/stubs/stm32f1xx_hal.h`, `tests/test_emm_v5_uart.c`: host HAL fakes and DMA state-machine tests.
- `tests/CMakeLists.txt`: native test executables and CTest registration.
- `CMakeLists.txt`: firmware source registration.
- `Core/Src/main.c`: object initialization, non-blocking polling, and HAL callback forwarding in `USER CODE` regions.

---

### Task 1: Emm V5 Command Encoding

**Files:**
- Create: `App/Inc/emm_v5_protocol.h`
- Create: `App/Src/emm_v5_protocol.c`
- Create: `tests/test_emm_v5_protocol.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Only `<stdbool.h>`, `<stddef.h>`, and `<stdint.h>`.
- Produces: `EmmV5Result`, `EmmV5Frame`, `EmmV5PositionCommand`, and `emm_v5_encode_*()` functions used by Tasks 2-5.

- [ ] **Step 1: Add the failing command-frame tests**

Create a small test runner following `tests/test_balance.c`. Cover every command represented by the reference driver with byte-exact checks. The core expectations must include:

```c
static void test_position_command_is_big_endian(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = { .data = bytes, .capacity = sizeof(bytes) };
    const EmmV5PositionCommand command = {
        .direction = EMM_V5_DIRECTION_CCW,
        .speed_rpm = 0x1234U,
        .acceleration = 0x56U,
        .pulse_count = 0x789ABCDEUL,
        .absolute = true,
        .synchronized = false,
    };

    CHECK_TRUE(emm_v5_encode_position(0x01U, &command, &frame) == EMM_V5_OK);
    CHECK_BYTES(frame.data,
                ((uint8_t[]){0x01, 0xFD, 0x01, 0x12, 0x34, 0x56,
                             0x78, 0x9A, 0xBC, 0xDE, 0x01, 0x00, 0x6B}),
                13U);
}

static void test_small_output_buffer_is_rejected(void)
{
    uint8_t bytes[4] = {0};
    EmmV5Frame frame = { .data = bytes, .capacity = sizeof(bytes) };
    CHECK_TRUE(emm_v5_encode_stop(0x01U, false, &frame) == EMM_V5_BUFFER_TOO_SMALL);
    CHECK_TRUE(frame.length == 0U);
}
```

Also assert exact frames for enable `F3`, velocity `F6`, fast-position setup `F1`, fast move `FC`, sync trigger `00 FF 66 6B`, position query `36`, status query `3A`, microstep `84`, stop `FE`, set-zero `93`, home `9A`, abort-home `9C`, and PID query `21`. Test null pointers, address `0x00` where broadcast is not explicitly allowed, and invalid direction/mode values.

Add a dedicated `test_emm_v5_protocol` executable to `tests/CMakeLists.txt` with `-Wall -Wextra -Werror` and CTest name `emm_v5_protocol`.

- [ ] **Step 2: Run the protocol test and verify the RED state**

Run:

```powershell
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests --target test_emm_v5_protocol
```

Expected: compilation fails because `emm_v5_protocol.h` and its functions do not exist.

- [ ] **Step 3: Implement bounded command encoding**

Define the public shape exactly as follows, adding one encoder declaration per command listed above:

```c
#define EMM_V5_FRAME_END 0x6BU
#define EMM_V5_MAX_FRAME_SIZE 19U

typedef enum {
    EMM_V5_OK = 0,
    EMM_V5_INVALID_ARGUMENT,
    EMM_V5_BUFFER_TOO_SMALL,
    EMM_V5_INVALID_FRAME,
    EMM_V5_UNEXPECTED_RESPONSE,
    EMM_V5_DRIVER_ERROR
} EmmV5Result;

typedef enum {
    EMM_V5_DIRECTION_CW = 0,
    EMM_V5_DIRECTION_CCW = 1
} EmmV5Direction;

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
} EmmV5Frame;

typedef struct {
    EmmV5Direction direction;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t pulse_count;
    bool absolute;
    bool synchronized;
} EmmV5PositionCommand;
```

Use one private bounded builder and explicit big-endian writers. On any error set `frame->length = 0U`; never partially report a valid frame. Keep the exact frame formats from `../temp/SM.c`, except do not reproduce global buffers, HAL calls, delays, or mutable motor state.

- [ ] **Step 4: Run command encoding tests**

Run `cmake --build build/host-tests --target test_emm_v5_protocol; ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure`.

Expected: `emm_v5_protocol` passes and compiler warnings are zero.

- [ ] **Step 5: Commit command encoding**

```powershell
git add App/Inc/emm_v5_protocol.h App/Src/emm_v5_protocol.c tests/test_emm_v5_protocol.c tests/CMakeLists.txt
git commit -m "feat: encode Emm V5 motor commands"
```

### Task 2: Emm V5 Response Parsing

**Files:**
- Modify: `App/Inc/emm_v5_protocol.h`
- Modify: `App/Src/emm_v5_protocol.c`
- Modify: `tests/test_emm_v5_protocol.c`

**Interfaces:**
- Consumes: `EmmV5Result` and protocol constants from Task 1.
- Produces: `EmmV5Ack`, `EmmV5Pid`, `emm_v5_parse_ack()`, `emm_v5_parse_position()`, `emm_v5_parse_status()`, and `emm_v5_parse_pid()`.

- [ ] **Step 1: Add failing response-parser tests**

Add tests for ACK status `0x02`, `0x12`, `0x22`, `0x9F`, `0xE2`, and `0xEE`; signed position; status; and 15-byte Emm PID response. Include malformed length, wrong address, wrong function, and wrong `0x6B` trailer:

```c
static void test_position_response_preserves_sign(void)
{
    const uint8_t response[] = {0x01, 0x36, 0x01, 0x00, 0x00, 0x12, 0x34, 0x6B};
    int32_t position = 0;
    CHECK_TRUE(emm_v5_parse_position(0x01U, response, sizeof(response), &position)
               == EMM_V5_OK);
    CHECK_TRUE(position == -0x1234);
}

static void test_pid_response_rejects_x_series_length(void)
{
    uint8_t response[19] = {0x01, 0x21};
    EmmV5Pid pid;
    response[18] = 0x6B;
    CHECK_TRUE(emm_v5_parse_pid(0x01U, response, sizeof(response), &pid)
               == EMM_V5_INVALID_FRAME);
}
```

Define the sign mapping according to the Emm V5 response: sign byte `0x00` is non-negative and `0x01` is negative. Explicitly test negative magnitude `0x80000000`; reject it because it cannot be represented as a positive `int32_t` magnitude before negation.

- [ ] **Step 2: Run the parser tests and verify failure**

Run `cmake --build build/host-tests --target test_emm_v5_protocol`.

Expected: compilation fails for missing parser declarations or assertions fail for unimplemented parsing.

- [ ] **Step 3: Implement strict response parsing**

Add these public types:

```c
typedef enum {
    EMM_V5_ACK_COMPLETE = 0x02,
    EMM_V5_ACK_START = 0x12,
    EMM_V5_ACK_END = 0x22,
    EMM_V5_ACK_HOME_FAILED = 0x9F,
    EMM_V5_ACK_CONFLICT = 0xE2,
    EMM_V5_ACK_BAD_COMMAND = 0xEE
} EmmV5Ack;

typedef struct {
    int32_t kp;
    int32_t ki;
    int32_t kd;
} EmmV5Pid;
```

Every parser must validate pointer, exact length, expected address, expected function, and trailer before reading fields. ACK parser returns `EMM_V5_DRIVER_ERROR` for `E2`, `EE`, and `9F` while still exposing the decoded ACK; accepted progress codes are returned as `EMM_V5_OK`. Use unsigned shifts to assemble fields and `memcpy` into signed PID fields to avoid implementation-defined signed shifts.

- [ ] **Step 4: Run all protocol tests**

Run `cmake --build build/host-tests --target test_emm_v5_protocol; ctest --test-dir build/host-tests -R emm_v5_protocol --output-on-failure`.

Expected: all protocol tests pass.

- [ ] **Step 5: Commit response parsing**

```powershell
git add App/Inc/emm_v5_protocol.h App/Src/emm_v5_protocol.c tests/test_emm_v5_protocol.c
git commit -m "feat: parse Emm V5 motor responses"
```

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

### Task 4: USART2 DMA Transaction Layer

**Files:**
- Create: `App/Inc/emm_v5_uart.h`
- Create: `App/Src/emm_v5_uart.c`
- Create: `tests/stubs/main.h`
- Create: `tests/stubs/stm32f1xx_hal.h`
- Create: `tests/test_emm_v5_uart.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: STM32 HAL UART/DMA calls and the `BalanceMotorSendFn` signature from Task 3.
- Produces: `EmmV5Uart`, `emm_v5_uart_init()`, `emm_v5_uart_send()`, `emm_v5_uart_poll()`, `emm_v5_uart_take_result()`, `emm_v5_uart_on_tx_complete()`, `emm_v5_uart_on_rx_event()`, and `emm_v5_uart_on_error()`. The send function has the exact injected-transport signature `BalanceMotorTxResult emm_v5_uart_send(void *context, const uint8_t *frame, size_t length, uint8_t expected_function, size_t expected_length)` and casts `context` to `EmmV5Uart *` internally.

- [ ] **Step 1: Add failing host tests around fake HAL calls**

Provide just enough HAL declarations in `tests/stubs` for the transport source to compile. Fake `HAL_UART_Transmit_DMA`, `HAL_UARTEx_ReceiveToIdle_DMA`, and `HAL_UART_AbortReceive` and record arguments.

Register `test_emm_v5_uart` with `tests/stubs` before `../App/Inc` in `target_include_directories()`, so the host-only `main.h` and HAL declarations are selected while the firmware build continues using `Core/Inc/main.h` and the real HAL.

Test these transitions:

- Initialization is idle and starts no transfer.
- Accepted send copies caller bytes into internal TX storage before calling HAL.
- A second send while active returns `BALANCE_MOTOR_TX_BUSY`.
- RX DMA starts before TX DMA so a fast ACK cannot be missed.
- TX completion does not complete a response-bearing request.
- RX event completes only at the configured exact length and preserves bytes for application retrieval.
- Wrong/short receive becomes a protocol result rather than reading beyond received data.
- `(uint32_t)(now_ms - deadline_ms) < 0x80000000U` style deadline handling remains correct over tick wrap.
- UART error and HAL start failure produce terminal error results and return to idle after result retrieval.

Core assertion:

```c
static void test_send_copies_frame_and_arms_rx_first(void)
{
    uint8_t caller_frame[] = {0x01, 0x36, 0x6B};
    CHECK_TRUE(emm_v5_uart_send(&uart, caller_frame, sizeof(caller_frame),
                                0x36U, 8U) == BALANCE_MOTOR_TX_ACCEPTED);
    caller_frame[0] = 0xFFU;
    CHECK_TRUE(fake.calls[0] == FAKE_CALL_RECEIVE_TO_IDLE_DMA);
    CHECK_TRUE(fake.calls[1] == FAKE_CALL_TRANSMIT_DMA);
    CHECK_TRUE(fake.tx_data[0] == 0x01U);
}
```

- [ ] **Step 2: Run UART transport tests and verify failure**

Run `cmake --build build/host-tests --target test_emm_v5_uart`.

Expected: compilation fails because the UART transaction module does not exist.

- [ ] **Step 3: Implement one in-flight DMA transaction**

Use fixed internal arrays sized `EMM_V5_MAX_FRAME_SIZE`. `emm_v5_uart_send()` validates lengths, copies TX bytes, records expected function/response length/deadline, arms Receive-to-Idle DMA, then starts TX DMA. If TX start fails, abort RX and publish a HAL error result.

After starting Receive-to-Idle DMA, disable the RX DMA half-transfer interrupt with `__HAL_DMA_DISABLE_IT(uart->handle->hdmarx, DMA_IT_HT)`. Emm responses are short variable-length protocol frames; processing a half-buffer event as a terminal receive would create false short-frame errors.

Callbacks only set volatile event flags and received length. `emm_v5_uart_poll(now_ms)` consumes flags, validates terminal lengths, handles timeout, and stores a result object:

```c
typedef enum {
    EMM_V5_UART_IDLE,
    EMM_V5_UART_ACTIVE,
    EMM_V5_UART_COMPLETE,
    EMM_V5_UART_TIMEOUT,
    EMM_V5_UART_PROTOCOL_ERROR,
    EMM_V5_UART_HAL_ERROR
} EmmV5UartState;

typedef struct {
    EmmV5UartState state;
    const uint8_t *response;
    size_t response_length;
    uint8_t expected_function;
} EmmV5UartResult;
```

For response-bearing commands, remain active until RX completes or timeout occurs. Do not call `HAL_Delay`. `emm_v5_uart_take_result()` copies the result to the caller and resets the object to idle, at which point the adapter may submit its latest queued target.

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: four CTest tests pass with no warnings.

- [ ] **Step 5: Commit the UART transaction layer**

```powershell
git add App/Inc/emm_v5_uart.h App/Src/emm_v5_uart.c tests/stubs tests/test_emm_v5_uart.c tests/CMakeLists.txt
git commit -m "feat: add Emm V5 USART DMA transport"
```

### Task 5: Firmware Wiring and End-to-End Verification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Core/Src/main.c` within `USER CODE` regions only
- Modify: `tests/test_balance_motor.c`

**Interfaces:**
- Consumes: Tasks 1-4 and existing `huart2`, `HAL_GetTick()`, and `BalanceActuatorCommand`.
- Produces: Initialized global `EmmV5Uart g_emm_uart` and `BalanceMotor g_balance_motor`, callback forwarding, polling, and a firmware image containing the complete integration without startup motion.

- [ ] **Step 1: Add the failing integration-safety assertion**

Extend the adapter test to model initialization plus repeated processing without an operator zero request:

```c
static void test_startup_processing_emits_no_motor_command(void)
{
    BalanceMotor motor;
    FakeTransport fake = {0};
    balance_motor_init(&motor, &config, fake_transport(&fake));
    for (unsigned i = 0; i < 100U; ++i) {
        balance_motor_process(&motor);
    }
    CHECK_TRUE(fake.send_count == 0U);
    CHECK_TRUE(!balance_motor_has_zero(&motor));
}
```

Run `cmake --build build/host-tests --target test_balance_motor; ctest --test-dir build/host-tests -R balance_motor --output-on-failure`.

Expected: test passes before firmware wiring, establishing the no-startup-command invariant that wiring must preserve.

- [ ] **Step 2: Register firmware sources and initialize objects**

Add all three source files to `target_sources()` in the root `CMakeLists.txt`.

In `main.c` USER CODE sections:

```c
/* USER CODE BEGIN Includes */
#include "balance_motor.h"
#include "emm_v5_uart.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
EmmV5Uart g_emm_uart;
BalanceMotor g_balance_motor;
/* USER CODE END PV */
```

After `MX_USART3_UART_Init()`, initialize the UART transport and motor adapter with address `0x01`. Use a conservative placeholder conversion of `1.0f` pulse per actuator position unit only as a compile-time default; do not submit any command. The final mechanical conversion remains a configuration value to calibrate before real motion.

In the main loop, call `emm_v5_uart_poll(&g_emm_uart, HAL_GetTick())`, deliver a taken response/error to `balance_motor_on_response()` or `balance_motor_on_transport_error()`, then call `balance_motor_process()`. This processing remains inert until an external caller explicitly requests software zero or a command.

- [ ] **Step 3: Forward HAL callbacks only for USART2**

Add these functions in `main.c` `USER CODE BEGIN 4`, filtering by `huart->Instance == USART2`:

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) emm_v5_uart_on_tx_complete(&g_emm_uart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance == USART2) emm_v5_uart_on_rx_event(&g_emm_uart, size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) emm_v5_uart_on_error(&g_emm_uart);
}
```

Do not alter generated IRQ handlers; they already dispatch USART2 and DMA channels 6/7 through HAL.

- [ ] **Step 4: Run format, host, source-protection, and ARM verification**

Run:

```powershell
git diff --check
cmake -S tests -B build/host-tests -G Ninja
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
cmake --preset Debug
cmake --build --preset Debug --clean-first
$hashes = Get-FileHash -Algorithm SHA256 "../temp/SM.c", "../temp/SM.h"
if ($hashes[0].Hash -ne "634421E0D84592FA447A77BD1C8AF8DD15CD7BC2794F49AD9C7CAD600E2D3F8F" -or $hashes[1].Hash -ne "B586B830C9A682DC6AC8DD811CA0C7DE4D8F5ACA1F4F2B2AD300C280311C92B1") { throw "temp reference source changed" }
```

Expected:

- `git diff --check` has no output.
- Four host tests pass: `balance_core`, `emm_v5_protocol`, `balance_motor`, and `emm_v5_uart`.
- ARM build produces `balanceBall.elf` with no compile or link errors.
- The SHA-256 check completes without throwing, proving both untracked reference files still match the design-time baseline.

- [ ] **Step 5: Inspect the final integration for startup safety**

Run `git diff -- CMakeLists.txt Core/Src/main.c App tests` and confirm:

- no call to enable, home, set microstep, set zero, stop, or position submission occurs during initialization;
- no `HAL_Delay` exists in the new driver path;
- USART1 and USART3 callbacks are ignored by this driver;
- no `../temp` path was added to CMake or include directories.

- [ ] **Step 6: Commit firmware integration**

```powershell
git add CMakeLists.txt Core/Src/main.c tests/test_balance_motor.c
git commit -m "build: integrate Emm V5 motor driver"
```

## Final Review Checklist

- [ ] Every command available in the reference `SM` driver has a byte-exact encoder test.
- [ ] Emm ACK, signed position, status, and 15-byte PID responses have strict parser tests.
- [ ] Software zero can only become valid from an explicit query and valid response.
- [ ] Latest-target replacement and stop/disable priority are tested.
- [ ] DMA buffers remain valid for the full transfer and callbacks do no blocking work.
- [ ] Communication lockout invalidates zero and requires a new manual-zero cycle.
- [ ] Startup sends no driver frame.
- [ ] Reference files under `../temp` remain unchanged.
- [ ] All host tests and the ARM build pass from fresh configure/build commands.

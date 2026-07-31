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

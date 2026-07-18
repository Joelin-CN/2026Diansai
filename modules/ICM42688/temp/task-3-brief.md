### Task 3: MSPM0G3507 DriverLib Adapter

**Files:**
- Create: `inc/icm42688_mspm0.h`
- Create: `src/icm42688_mspm0.c`
- Create: `temp/tests/fakes/ti_msp_dl_config.h`
- Create: `temp/tests/test_mspm0_adapter.c`

**Interfaces:**
- Consumes: SysConfig symbols `SPI1_INST`, `ICM42688_PORT`, `ICM42688_CS_PIN`, `ICM42688_TIMER_INST`, `CPUCLK_FREQ` and DriverLib functions declared by `ti_msp_dl_config.h`.
- Produces: `icm42688_mspm0_comm`, `icm42688_mspm0_system`, `icm42688_mspm0_timer` and `icm42688_mspm0_bind(...)`.

- [ ] **Step 1: Write the failing adapter test and DriverLib fake**

`temp/tests/fakes/ti_msp_dl_config.h` defines host-compilable fake instances, GPIO/SPI/TIMG function declarations and `delay_cycles`. `test_mspm0_adapter.c` records every transmit, receive, and CS level change, and verifies:

```c
static void test_single_register_read_transaction(void)
{
    reset_fake_driverlib();
    fake_rx_byte = 0x47U;
    assert(icm42688_mspm0_comm.read_reg(0x75U) == 0x47U);
    assert(cs_events[0] == CS_LOW);
    assert(tx_bytes[0] == 0xF5U);
    assert(cs_events[1] == CS_HIGH);
}

static void test_timer_returns_sysconfig_counter(void)
{
    fake_timer_count = 123456U;
    assert(icm42688_mspm0_timer.get_time_us() == 123456U);
}
```

Burst read must verify only one pair of CS events. Write register verifies address bit 7 cleared. Init verifies CS pulled high.

- [ ] **Step 2: Run the adapter test to verify it fails**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc temp/tests/test_mspm0_adapter.c src/icm42688_mspm0.c -o temp/tests/test_mspm0_adapter.exe
```
Expected: FAIL because adapter files do not exist.

- [ ] **Step 3: Define the SysConfig contract and exported bindings**

`inc/icm42688_mspm0.h` includes core and AHRS headers, and declares:

```c
extern const icm42688_comm_t icm42688_mspm0_comm;
extern const icm42688_system_t icm42688_mspm0_system;
extern const ahrs_timer_t icm42688_mspm0_timer;

void icm42688_mspm0_bind(const icm42688_config_t *config);
```

`icm42688_mspm0_bind` only binds interfaces, does NOT call `SYSCFG_DL_init()`; the controller must have done system init first.

- [ ] **Step 4: Implement blocking SPI, GPIO, delay, and timer bindings**

`src/icm42688_mspm0.c` uses this transaction pattern:

```c
static uint8_t spi_exchange(uint8_t value)
{
    DL_SPI_transmitDataBlocking8(SPI1_INST, value);
    return DL_SPI_receiveDataBlocking8(SPI1_INST);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t value;
    DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
    (void)spi_exchange((uint8_t)(reg | 0x80U));
    value = spi_exchange(0xFFU);
    DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);
    return value;
}
```

Burst read loops `spi_exchange(0xFFU)` within one CS-low window. Millisecond delay calls `delay_cycles(CPUCLK_FREQ / 1000U)` per ms in a loop to avoid `ms * cycles` overflow. TIMG0 is configured and started by SysConfig, so timer `init/start` callbacks are no-ops; `get_time_us` returns `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)`.

- [ ] **Step 5: Run the adapter test**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc temp/tests/test_mspm0_adapter.c src/icm42688_mspm0.c -o temp/tests/test_mspm0_adapter.exe
& .\temp\tests\test_mspm0_adapter.exe
```
Expected: compile clean, program prints `test_mspm0_adapter: PASS`.

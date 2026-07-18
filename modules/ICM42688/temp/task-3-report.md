### Task 3 Report: MSPM0G3507 DriverLib Adapter

**Status:** DONE_WITH_CONCERNS

**Files created:**
- `inc/icm42688_mspm0.h` — adapter header; includes `ti_msp_dl_config.h` + core/AHRS HAL; declares `icm42688_mspm0_comm`, `icm42688_mspm0_system`, `icm42688_mspm0_timer`, `icm42688_mspm0_bind()`.
- `src/icm42688_mspm0.c` — implementation: blocking SPI exchange, CS-toggled read/write/burst, `delay_cycles`-based ms delay, TIMG0 counter-backed microsecond timer; `bind()` calls `icm42688_hal_init()` + `ahrs_hal_init()` only (no `SYSCFG_DL_init()`).
- `temp/tests/fakes/ti_msp_dl_config.h` — host-compilable fake SysConfig symbols (`SPI1_INST`, `ICM42688_PORT`, `ICM42688_CS_PIN`, `ICM42688_TIMER_INST`, `CPUCLK_FREQ`), empty `SPI_Regs`/`GPIO_Regs`/`GPTIMER_Regs` stubs, and declarations for the DriverLib functions used.
- `temp/tests/test_mspm0_adapter.c` — fake DriverLib implementations + 9 tests; prints `test_mspm0_adapter: PASS`.

**Tests:** 9/9 PASS.

```
  PASS  single register read transaction
  PASS  single register write transaction
  PASS  burst read single CS window
  PASS  burst read NULL safe
  PASS  comm_init pulls CS high
  PASS  timer returns SysConfig counter
  PASS  timer init/start are no-ops
  PASS  delay_ms uses cycles per ms
  PASS  delay_ms(0) is no-op
test_mspm0_adapter: PASS
```

**Implementation notes:**
- `read_reg`: CS low → `spi_exchange(reg | 0x80)` → `spi_exchange(0xFF)` → CS high.
- `write_reg`: CS low → `spi_exchange(reg & 0x7F)` → `spi_exchange(value)` → CS high (verified bit 7 cleared).
- `read_regs`: single CS-low window for the whole burst; NULL buffer / zero length are no-ops (no CS toggle).
- `delay_ms`: loops `delay_cycles(CPUCLK_FREQ / 1000)` per ms (avoids `ms * cycles` overflow); `delay_ms(0)` does not call `delay_cycles`.
- Timer `init`/`start` are no-ops (SysConfig owns TIMG0); `get_time_us` returns `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)` verbatim.
- `comm_init` pulls CS high (idle).

**Concern / deviation from brief:**
The brief's Step 5 compile command omits the core HAL sources, but `icm42688_mspm0_bind()` (in `src/icm42688_mspm0.c`) references `icm42688_hal_init()` and `ahrs_hal_init()`, so the linker requires `src/icm42688_hal.c` and `src/ahrs_hal.c` (the latter needs `-lm`). The actual build command used was:

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc `
    temp/tests/test_mspm0_adapter.c `
    src/icm42688_mspm0.c src/icm42688_hal.c src/ahrs_hal.c `
    -lm -o temp/tests/test_mspm0_adapter.exe
```

This matches the pattern used in Tasks 1–2 (which also link the core `.c` files they depend on). No `controller/` or `.syscfg` files were touched; no git commits made.

**SysConfig contract reminder (for README):** Users must create a TIMG0 instance named `ICM42688_TIMER` in SysConfig so that `ICM42688_TIMER_INST` is generated. `SPI1`, `ICM42688_PORT`/`ICM42688_CS_PIN`, and `CPUCLK_FREQ` are already present in the current `ti_msp_dl_config.h`.

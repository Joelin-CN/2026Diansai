### Task 3: Make MCP23017 Failures Observable and Recoverable

**Files:**
- Modify: `modules/MCP23017/inc/mcp23017.h`
- Modify: `modules/MCP23017/src/mcp23017.c`
- Create: `tests/test_mcp23017.c`
- Create: `tests/fakes/ti_msp_dl_config.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `I2C0_INST` DriverLib operations.
- Produces: `mcp23017_status_t MCP23017_Init(void)` and `mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs)`.

- [ ] **Step 1: Write failing driver tests with scripted fake I2C states**

Cover these exact cases:

```c
assert(MCP23017_ReadInputs(NULL) == MCP23017_STATUS_INVALID_ARGUMENT);
assert(MCP23017_Init() == MCP23017_STATUS_OK);
assert(MCP23017_ReadInputs(&mask) == MCP23017_STATUS_OK);
assert(mask == UINT16_C(0x0A55));
assert(run_with_tx_busy_forever() == MCP23017_STATUS_TIMEOUT);
assert(run_with_nack() == MCP23017_STATUS_IO_ERROR);
```

The fake header supplies only the exact `DL_I2C_*` symbols used by the driver and records TX bytes, target address, transfer sizes, and timeout loops.

- [ ] **Step 2: Run the test and verify failure**

Run `tests/run_tests.ps1`.

Expected: compile failure because `mcp23017_status_t` and status constants do not exist.

- [ ] **Step 3: Implement a status enum and bounded transactions**

Use:

```c
typedef enum {
    MCP23017_STATUS_OK = 0,
    MCP23017_STATUS_INVALID_ARGUMENT,
    MCP23017_STATUS_TIMEOUT,
    MCP23017_STATUS_IO_ERROR
} mcp23017_status_t;
```

Every wait exits after `MCP23017_I2C_TIMEOUT` iterations. Clear/reinitialize transfer state after a timeout or controller error using the DriverLib operation supported by the selected SDK. Preserve the two-byte GPIOA/GPIOB result only on success.

- [ ] **Step 4: Run the MCP tests**

Expected: all success, invalid-argument, timeout, and I/O-error tests pass.

- [ ] **Step 5: Update existing line-sensor compatibility**

Modify `src/line_sensor.c` so its legacy API returns zero unless:

```c
MCP23017_ReadInputs(&mask) == MCP23017_STATUS_OK
```

This compatibility API remains available, but the new sensor adapter must call MCP23017 directly so it can preserve failure status.

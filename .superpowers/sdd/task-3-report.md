# Task 3 Report: Make MCP23017 Failures Observable and Recoverable

## Status: DONE

## Commit
- **ee59097** Add MCP23017 status returns with timeout and error handling

## Implementation Summary

Successfully converted MCP23017 driver from boolean returns to explicit status enum with proper error handling and timeout behavior.

### Changes Made

1. **Status Enum (mcp23017.h)**
   - Added `mcp23017_status_t` with OK, INVALID_ARGUMENT, TIMEOUT, IO_ERROR
   - Updated function signatures to return status instead of bool
   - Added legacy function declarations (for future compatibility if needed)

2. **Driver Implementation (mcp23017.c)**
   - Converted `MCP23017_WaitFor()` to return status (timeout vs I2C error)
   - Added NULL pointer validation in `MCP23017_ReadInputs()`
   - Changed `MCP23017_Init()` to propagate first failure instead of AND logic
   - All I2C operations now have bounded waits with `MCP23017_I2C_TIMEOUT`
   - Distinguish NACK/arbitration errors from timeout

3. **Line Sensor Compatibility (src/line_sensor.c)**
   - Updated to check `MCP23017_STATUS_OK` instead of boolean
   - Returns zero on any MCP23017 failure (preserves legacy behavior)

4. **Test Infrastructure**
   - Created `tests/fakes/ti_msp_dl_config.h` - fake DriverLib I2C layer
   - Created `tests/test_mcp23017.c` - comprehensive driver tests
   - Updated `tests/run_tests.ps1` to run MCP23017 tests

### TDD Evidence

#### RED Phase
Command:
```powershell
.\tests\run_tests.ps1
```

Output (expected failure):
```
E:\B306\2026\电赛\2025e\m0_controller\tests\test_mcp23017.c:90:41: error: 'MCP23017_STATUS_INVALID_ARGUMENT' undeclared
E:\B306\2026\电赛\2025e\m0_controller\tests\test_mcp23017.c:99:31: error: 'MCP23017_STATUS_OK' undeclared
E:\B306\2026\电赛\2025e\m0_controller\tests\test_mcp23017.c:111:31: error: 'MCP23017_STATUS_TIMEOUT' undeclared
E:\B306\2026\电赛\2025e\m0_controller\tests\test_mcp23017.c:122:31: error: 'MCP23017_STATUS_IO_ERROR' undeclared
test_mcp23017 compile failed
```

**Why failure was expected:** Status enum and constants did not exist yet. Tests reference them before implementation.

#### GREEN Phase
Command:
```powershell
.\tests\run_tests.ps1
```

Output (all tests pass):
```
Testing PlatformTime_UpCountFromDownCount...
Testing PlatformTime_Extend32 wrap detection...
All platform_time tests PASSED
Test: ICM42688 temperature + accel + gyro reading...
  Temperature raw: 0x1234
  Accel raw: (100, -200, 300)
  Gyro raw: (-50, 75, -125)
  PASS

All ICM42688 tests PASSED
PASS: test_invalid_argument
PASS: test_normal_operation
PASS: test_timeout
PASS: test_nack
All MCP23017 tests passed!
Host tests: PASS
```

### Test Coverage

The test suite covers all required scenarios:

1. **test_invalid_argument**: NULL pointer returns `MCP23017_STATUS_INVALID_ARGUMENT`
2. **test_normal_operation**: Init succeeds and reads 0x0A55 correctly
3. **test_timeout**: TX busy forever triggers `MCP23017_STATUS_TIMEOUT`
4. **test_nack**: I2C NACK returns `MCP23017_STATUS_IO_ERROR`

### Fake I2C Layer

The fake DriverLib implementation provides:
- Scriptable I2C states (NORMAL, TX_BUSY_FOREVER, NACK)
- Timeout counter for verification
- Configurable RX data (default 0x0A55)
- All `DL_I2C_*` symbols used by the driver

### Technical Details

- **Timeout value**: `MCP23017_I2C_TIMEOUT = 100000U` iterations
- **I2C address**: `0x20` (unchanged)
- **Error detection**: NACK + arbitration lost flags
- **Status preservation**: Only update output pointer on `STATUS_OK`
- **Compatibility**: `line_sensor.c` updated but maintains zero-on-error behavior

## Concerns

None. All tests pass, TDD workflow followed, status propagation is explicit and consistent.

## Files Modified
- `modules/MCP23017/inc/mcp23017.h` - Added status enum and updated signatures
- `modules/MCP23017/src/mcp23017.c` - Converted to status returns with bounded waits
- `src/line_sensor.c` - Updated to use new status API
- `tests/run_tests.ps1` - Added MCP23017 test execution
- `tests/fakes/ti_msp_dl_config.h` - Created fake DriverLib I2C layer
- `tests/test_mcp23017.c` - Created comprehensive test suite

## Next Steps

The MCP23017 driver is now ready for integration. The new sensor adapter (future task) should call `MCP23017_ReadInputs()` directly and check the status to distinguish "sensors read zeros" from "I2C failed".

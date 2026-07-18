# Task 5 Report: Add Encoder, Motor, and Sensor Adapters

**Date:** 2026-07-18  
**Status:** DONE  
**Commit:** d31d0a6

## Summary

Successfully implemented the adapter layer that bridges target-specific hardware drivers to the portable Motion Control and Sens-Decision modules. Fixed the `ENCODER_COUNT` enum collision and created three adapter types with full TDD coverage.

## Implementation Details

### Step 1: Failing Tests (TDD RED Phase)

Created `tests/test_target_adapters.c` with comprehensive test coverage:
- Encoder mapping tests (logical → physical ID translation)
- Motor differential PWM fan-out tests
- Sensor HAL tests: IMU raw data, IR mask, encoder reading
- Failure propagation tests: NULL arguments, timeouts, IO errors

Initial test run: **FAILED** (adapters not yet implemented).

### Step 2: Encoder Enum Collision Fix

**Problem:** `ENCODER_COUNT` in `encoder.h` collides with Motion Control's `ENCODER_COUNT` in `motion_feedback.h:49`.

**Solution:** Renamed low-level sentinel from `ENCODER_COUNT` to `ENCODER_ID_COUNT`:
- Modified: `inc/encoder.h:11` 
- Updated all references in `src/encoder.c:5,6,50,60,65`

This preserves `ENCODER_M1` through `ENCODER_M4` stability while eliminating the collision.

### Step 3: Encoder Hardware Bridge

Created isolation layer to hide `encoder.h` dependencies:

**Files:**
- `inc/encoder_hw_bridge.h` - Clean interface using only `stdint.h`
- `src/encoder_hw_bridge.c` - ISR-safe implementation with critical sections

**Key features:**
- Physical ID validation (`< ENCODER_ID_COUNT`)
- Critical section protection for reset operations (guards against GPIO ISR races)
- Conditional compilation for host tests (MinGW detection)

### Step 4: Motion Control Adapters

#### Encoder Adapter (`encoder_adapter.h/c`)

Maps Motion Control logical IDs to physical hardware:
```c
ENCODER_LEFT_FRONT  (0) → M1 (physical 0)
ENCODER_LEFT_REAR   (1) → M2 (physical 1)
ENCODER_RIGHT_FRONT (2) → M3 (physical 2)
ENCODER_RIGHT_REAR  (3) → M4 (physical 3)
```

Returns non-const `EncoderInterface_t*` as required by Motion Control's current API.

#### Motor Adapter (`motor_adapter.h/c`)

Simple vtable forwarding to `motor.h` functions:
- `setDifferentialPWM` → `Motor_SetSpeed(left, right)`
- `stop` → `Motor_Stop()`
- `init` → `Motor_Init()`

The underlying `Motor_SetSpeed` already handles M1/M2=left, M3/M4=right fan-out.

### Step 5: Sensor HAL Adapter

Created `sensor_adapter.h/c` implementing `sensor_hal_t` interface for Sens-Decision:

#### Encoder Reading (`ReadEncoder`)
- Maps indices 0-3 directly to physical encoder IDs
- Validates index and NULL pointer
- Returns `SD_ERR_INVALID_ARGUMENT` for invalid input

#### IMU Reading (`ReadImu`)
- Calls `icm42688_read()` and extracts **only raw LSB data**:
  - `temperature_raw`, `acc_raw.{x,y,z}`, `gyro_raw.{x,y,z}`
  - Does NOT copy `acc_g` or `gyro_dps` (converted values)
- Returns `SD_ERR_READ` if ICM42688 fails
- Returns `SD_ERR_INVALID_ARGUMENT` for NULL pointer

#### IR Reading (`ReadIr`)
- Calls `MCP23017_ReadInputs()` and masks result to 12 bits (`& 0x0FFF`)
- Status preservation:
  - `MCP23017_STATUS_OK` → `SD_OK`
  - `MCP23017_STATUS_TIMEOUT` → `SD_ERR_TIMEOUT`
  - `MCP23017_STATUS_IO_ERROR` → `SD_ERR_READ`
  - `MCP23017_STATUS_INVALID_ARGUMENT` → `SD_ERR_INVALID_ARGUMENT`
- No polarity inversion (applied elsewhere in sensor pipeline)

### Step 6: Test Results (TDD GREEN Phase)

```
=== Target Adapter Tests ===

TEST: Encoder adapter logical-to-physical mapping
  PASS: All four encoder mappings verified
TEST: Motor adapter differential PWM fan-out
  PASS: Motor fan-out (M1/M2=left, M3/M4=right) verified
TEST: Sensor HAL IMU raw data passthrough
  PASS: IMU raw LSB values preserved
TEST: Sensor HAL IMU failure propagation
  PASS: IMU failure propagated as SD_ERR_READ
TEST: Sensor HAL IR mask with active polarity
  PASS: IR mask masked to 12 bits
TEST: Sensor HAL IR failure propagation
  PASS: IR failures propagated correctly
TEST: Sensor HAL encoder reading
  PASS: Encoder HAL validated

=== All Tests PASSED ===
```

All existing tests (platform_time, ICM42688, MCP23017, motion_control) continue to pass.

## TDD Evidence: RED → GREEN

### RED Phase
Initial test execution failed with compilation errors:
```
fatal error: encoder_adapter.h: No such file or directory
```

### GREEN Phase
After implementing all adapters:
- **7/7 adapter tests PASS**
- **All existing tests PASS** (no regressions)
- Compilation clean with `-std=c99 -Wall -Wextra -Werror -pedantic`

## Files Created

1. `inc/encoder_hw_bridge.h` - Hardware bridge interface (34 lines)
2. `src/encoder_hw_bridge.c` - Hardware bridge implementation (36 lines)
3. `inc/encoder_adapter.h` - Encoder adapter interface (24 lines)
4. `src/encoder_adapter.c` - Encoder adapter implementation (70 lines)
5. `inc/motor_adapter.h` - Motor adapter interface (24 lines)
6. `src/motor_adapter.c` - Motor adapter implementation (30 lines)
7. `inc/sensor_adapter.h` - Sensor HAL adapter interface (24 lines)
8. `src/sensor_adapter.c` - Sensor HAL adapter implementation (115 lines)
9. `tests/test_target_adapters.c` - Comprehensive adapter tests (369 lines)

## Files Modified

1. `inc/encoder.h` - Fixed enum collision (line 11)
2. `src/encoder.c` - Updated all ENCODER_COUNT refs (lines 5,6,50,60,65)
3. `tests/run_tests.ps1` - Added adapter test build (lines 68-83)

## Architecture Validation

The three-layer architecture is now complete:

```
┌─────────────────────────────────────────────────────────────┐
│ Portable Modules (Motion Control, Sens-Decision)           │
│  - Use abstract interface types (EncoderInterface_t, etc.) │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│ Adapter Layer (THIS TASK)                                   │
│  - encoder_adapter, motor_adapter, sensor_adapter           │
│  - Maps logical IDs ↔ physical IDs                          │
│  - Translates status codes                                  │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│ Low-Level Drivers (encoder.h, motor.h, icm42688, mcp23017) │
│  - Hardware-specific types and enums                        │
│  - Direct register access                                   │
└─────────────────────────────────────────────────────────────┘
```

## Verification Checklist

- [x] Enum collision resolved (`ENCODER_COUNT` → `ENCODER_ID_COUNT`)
- [x] Four encoder mappings tested and verified
- [x] Motor differential PWM fan-out tested (M1/M2=left, M3/M4=right)
- [x] IMU raw LSB data passthrough (no converted values)
- [x] IR mask limited to 12 bits
- [x] MCP23017 status preservation (timeout, IO error)
- [x] NULL argument handling for all HAL functions
- [x] ISR-safe encoder reset with critical sections
- [x] Host build compatibility (MinGW detection)
- [x] All tests pass with strict warnings (`-Werror -pedantic`)
- [x] No regressions in existing tests

## Concerns

None. All requirements met.

## Next Steps

Task 6 should integrate these adapters into the main application and verify end-to-end operation.

# Sensor Initialization Fix - Test Plan

**Date**: 2026-07-30  
**Purpose**: Verify that sensor initialization failures are properly detected and handled  

---

## Test Environment Setup

### Hardware Requirements
- STM32 development board
- 2x Motor encoders (TIM3, TIM4)
- 8-channel IR sensor array (USART2)
- ICM42688 IMU (SPI2)
- Serial terminal (115200 baud, UART5 for debug output)

### Software Requirements
- Firmware with initialization fix applied
- Serial terminal software (PuTTY, TeraTerm, or similar)

---

## Test Cases

### TC1: Normal Initialization (All Sensors Working)

**Objective**: Verify system operates normally when all sensors are functional

**Preconditions**:
- All sensors properly connected
- Power supply stable
- Correct configuration in `config.c`

**Test Steps**:
1. Power on the system
2. Observe serial output
3. Verify control loop starts
4. Observe vehicle operation

**Expected Results**:
```
[SensDecision] ===== Sensor Initialization Start =====
[SensDecision] HAL configuration: OK
[SensDecision] Config validation: OK
[SensDecision] Initializing sensor 0 (Encoder_Left)...
[SensDecision] Sensor 0 (Encoder_Left): OK
[SensDecision] Initializing sensor 1 (Encoder_Right)...
[SensDecision] Sensor 1 (Encoder_Right): OK
[SensDecision] Initializing sensor 2 (IMU)...
[SensDecision] Sensor 2 (IMU): OK
[SensDecision] Initializing sensor 3 (IR_Array)...
[SensDecision] Sensor 3 (IR_Array): OK
[SensDecision] ===== All sensors initialized successfully =====
[TrackControlApp] ========== Initialization complete! ==========
[TrackControlApp] Running — target laps: 3
```

**Pass Criteria**:
- ✅ All sensors report "OK"
- ✅ No error messages
- ✅ Control loop starts
- ✅ Vehicle responds to line detection

---

### TC2: Left Encoder Connection Failure

**Objective**: Verify system detects and reports left encoder failure

**Preconditions**:
- Disconnect left encoder (TIM3)
- All other sensors connected

**Test Steps**:
1. Power on the system
2. Observe serial output
3. Verify system does not enter control loop
4. Verify motor remains stopped

**Expected Results**:
```
[SensDecision] ===== Sensor Initialization Start =====
[SensDecision] HAL configuration: OK
[SensDecision] Config validation: OK
[SensDecision] Initializing sensor 0 (Encoder_Left)...
[ERROR] Sensor 0 (Encoder_Left) initialization failed: [error_type]
[SensDecision] Rolling back initialization...
[FATAL] Sensor initialization failed
[FATAL] System cannot operate without sensors
[FATAL] Please check:
  1. Encoder connections (TIM3=Left, TIM4=Right)
  ...
[FATAL] TrackControlApp_Init failed — motors halted
```

**Pass Criteria**:
- ✅ Error identifies "Encoder_Left"
- ✅ System halts before control loop
- ✅ Motors remain stopped
- ✅ Troubleshooting guide displayed

---

### TC3: Right Encoder Connection Failure

**Objective**: Verify system detects and reports right encoder failure

**Preconditions**:
- Disconnect right encoder (TIM4)
- All other sensors connected

**Test Steps**:
1. Power on the system
2. Observe serial output
3. Verify system does not enter control loop

**Expected Results**:
```
[SensDecision] Sensor 0 (Encoder_Left): OK
[SensDecision] Initializing sensor 1 (Encoder_Right)...
[ERROR] Sensor 1 (Encoder_Right) initialization failed: [error_type]
[SensDecision] Rolling back initialization...
[SensDecision] Released sensor 0 (Encoder_Left)
[FATAL] Sensor initialization failed
```

**Pass Criteria**:
- ✅ Error identifies "Encoder_Right"
- ✅ Rollback releases previously initialized sensors
- ✅ System halts safely

---

### TC4: IR Sensor Connection Failure

**Objective**: Verify system detects and reports IR sensor failure

**Preconditions**:
- Disconnect IR sensor UART (USART2)
- All other sensors connected

**Test Steps**:
1. Power on the system
2. Observe serial output
3. Verify system identifies IR sensor failure

**Expected Results**:
```
[SensDecision] Sensor 0 (Encoder_Left): OK
[SensDecision] Sensor 1 (Encoder_Right): OK
[SensDecision] Sensor 2 (IMU): OK
[SensDecision] Initializing sensor 3 (IR_Array)...
[ERROR] Sensor 3 (IR_Array) initialization failed: [error_type]
[SensDecision] Rolling back initialization...
[FATAL] Sensor initialization failed
[FATAL] Please check:
  2. IR sensor UART (USART2, 115200 baud)
  ...
```

**Pass Criteria**:
- ✅ Error identifies "IR_Array"
- ✅ Rollback releases encoders and IMU
- ✅ UART troubleshooting mentioned

---

### TC5: IMU Connection Failure

**Objective**: Verify system detects and reports IMU failure

**Preconditions**:
- Disconnect or disable IMU (SPI2)
- All other sensors connected

**Test Steps**:
1. Power on the system
2. Observe serial output during ICM42688 init
3. Check if system continues or halts

**Expected Results**:
```
[TrackControlApp] Step 5: ICM42688 bind and init...
[WARNING] icm42688_init failed (skipping for now)
[TrackControlApp] Step 8: Sensor HAL configure/init...
[SensDecision] Sensor 0 (Encoder_Left): OK
[SensDecision] Sensor 1 (Encoder_Right): OK
[SensDecision] Initializing sensor 2 (IMU)...
[ERROR] Sensor 2 (IMU) initialization failed: [error_type]
```

**Pass Criteria**:
- ✅ IMU HAL init warning shown
- ✅ Sensor-Decision IMU init fails
- ✅ System halts properly

**Note**: IMU failure handling depends on whether IMU is marked as critical. Current implementation treats it as critical.

---

### TC6: Configuration Validation Failure

**Objective**: Verify config validation catches invalid parameters

**Preconditions**:
- Modify `config.c` to have duplicate encoder indices:
  ```c
  g_sens_decision_config.vehicle.left_encoder_indices[0] = 0U;
  g_sens_decision_config.vehicle.right_encoder_indices[0] = 0U; // Duplicate!
  ```

**Test Steps**:
1. Rebuild and flash firmware
2. Power on the system
3. Observe serial output

**Expected Results**:
```
[SensDecision] ===== Sensor Initialization Start =====
[SensDecision] HAL configuration: OK
[ERROR] Config validation failed: Invalid argument
[ERROR] Check encoder indices and parameter ranges
[FATAL] Sensor initialization failed
```

**Pass Criteria**:
- ✅ Config validation fails before sensor init
- ✅ Error message indicates "Invalid argument"
- ✅ Troubleshooting guide mentions configuration

**Cleanup**: Revert `config.c` to correct values

---

### TC7: HAL Not Configured

**Objective**: Verify proper error when HAL interface is not set up

**Preconditions**:
- Comment out `sensors_configure_hal(hal)` call in `track_control_app.c`

**Test Steps**:
1. Rebuild and flash firmware
2. Power on the system
3. Observe serial output

**Expected Results**:
```
[SensDecision] ===== Sensor Initialization Start =====
[ERROR] HAL not configured - call sensors_configure_hal() first
[FATAL] sensors_configure_hal failed
[FATAL] Cannot initialize sensor HAL interface
```

**Pass Criteria**:
- ✅ Error identifies HAL not configured
- ✅ System halts before attempting sensor init

**Cleanup**: Restore `sensors_configure_hal(hal)` call

---

### TC8: Runtime Sensor Failure Detection

**Objective**: Verify health monitoring detects sensor failures during operation

**Preconditions**:
- System successfully initialized
- Control loop running

**Test Steps**:
1. Start system normally
2. Wait for control loop to start
3. Disconnect left encoder during operation
4. Wait 10 seconds
5. Observe serial output

**Expected Results**:
```
[TrackControlApp] Running — target laps: 3
... (normal operation) ...
[DEBUG] Failure #1: preprocess=1, ir_valid=1, imu_valid=1
[DEBUG] Failure #2: preprocess=1, ir_valid=1, imu_valid=1
... (continues) ...
[WARN] Left encoder not responding
[INFO] Run sensors_diagnostic_report() for detailed analysis
[ERROR] Critical failure - stopping motor
```

**Pass Criteria**:
- ✅ Health monitoring detects failure
- ✅ Warning message appears
- ✅ System enters safe stop after 50 failures (~1 second)

---

### TC9: Sensor Diagnostic Tool

**Objective**: Verify diagnostic tool provides accurate sensor status

**Preconditions**:
- Add call to `sensors_diagnostic_report()` in application code
- Rebuild firmware

**Test Steps**:
1. Call diagnostic function after successful init
2. Observe detailed output
3. Call diagnostic function after disconnecting a sensor
4. Compare outputs

**Expected Results (Normal)**:
```
========== SENSOR DIAGNOSTIC REPORT ==========
[OK] HAL configured
Config validation: [PASS]

Sensor Status:
  0. Encoder_Left: [OK] count=0
  1. Encoder_Right: [OK] count=0
  2. IMU: [OK] accel=(0.00, 0.00, 9.81) m/s^2
  3. IR_Array: [OK] mask=0x00FF, values=[800, 820, 850, 200, 190, 830, 840, 810]
==============================================
```

**Expected Results (After Disconnecting Encoder)**:
```
Sensor Status:
  0. Encoder_Left: [FAIL] Not initialized
  1. Encoder_Right: [OK] count=12345
  ...
```

**Pass Criteria**:
- ✅ All sensors show correct status
- ✅ Sensor data values are reasonable
- ✅ Failures are clearly indicated

---

## Regression Testing

### RT1: Verify Normal Operation Unchanged

**Objective**: Ensure fix does not break normal operation

**Test Steps**:
1. Run vehicle through complete lap sequence with all sensors working
2. Monitor for any unexpected errors or warnings
3. Verify lap counting works correctly
4. Check control performance unchanged

**Pass Criteria**:
- ✅ No new error messages during normal operation
- ✅ Vehicle completes target laps
- ✅ Control performance matches pre-fix baseline

---

### RT2: Verify Emergency Stop Still Functions

**Objective**: Ensure existing safety mechanisms still work

**Test Steps**:
1. Start vehicle normally
2. Manually trigger line loss (lift vehicle off track)
3. Verify emergency stop after 50 failures

**Pass Criteria**:
- ✅ Emergency stop triggers at correct threshold
- ✅ Motors stop safely
- ✅ Error message displayed

---

## Performance Testing

### PT1: Initialization Time

**Objective**: Measure initialization time with enhanced diagnostics

**Test Steps**:
1. Record timestamp at start of initialization
2. Record timestamp when control loop starts
3. Calculate total initialization time

**Expected**: < 3 seconds (including 1-2 second IR sensor warm-up)

**Pass Criteria**:
- ✅ Initialization time acceptable
- ✅ No significant slowdown from added logging

---

### PT2: Runtime Overhead

**Objective**: Verify health monitoring does not impact control performance

**Test Steps**:
1. Monitor stack usage with health monitoring
2. Check control loop timing stability
3. Verify no delays in control cycle

**Expected**: Negligible overhead (health check runs once per 10 seconds)

**Pass Criteria**:
- ✅ Control loop maintains 500 Hz
- ✅ Stack usage within limits
- ✅ No timing jitter introduced

---

## Test Execution Summary Template

| Test Case | Status | Notes | Tester | Date |
|-----------|--------|-------|--------|------|
| TC1: Normal Init | ⬜ Pass / ⬜ Fail | | | |
| TC2: Left Encoder Fail | ⬜ Pass / ⬜ Fail | | | |
| TC3: Right Encoder Fail | ⬜ Pass / ⬜ Fail | | | |
| TC4: IR Sensor Fail | ⬜ Pass / ⬜ Fail | | | |
| TC5: IMU Fail | ⬜ Pass / ⬜ Fail | | | |
| TC6: Config Validation | ⬜ Pass / ⬜ Fail | | | |
| TC7: HAL Not Configured | ⬜ Pass / ⬜ Fail | | | |
| TC8: Runtime Failure | ⬜ Pass / ⬜ Fail | | | |
| TC9: Diagnostic Tool | ⬜ Pass / ⬜ Fail | | | |
| RT1: Normal Operation | ⬜ Pass / ⬜ Fail | | | |
| RT2: Emergency Stop | ⬜ Pass / ⬜ Fail | | | |
| PT1: Init Time | ⬜ Pass / ⬜ Fail | | | |
| PT2: Runtime Overhead | ⬜ Pass / ⬜ Fail | | | |

---

## Known Issues / Limitations

1. **IMU as Optional Sensor**: Current implementation treats IMU as critical. Future enhancement could allow degraded mode without IMU.

2. **Retry Mechanism**: No automatic retry on transient failures. Future enhancement could add configurable retry attempts.

3. **Diagnostic Tool Access**: Currently requires code modification to call. Future enhancement could add UART command interface.

4. **Health Check Interval**: Fixed at 10 seconds. Could be made configurable if needed.

---

## Troubleshooting Guide for Testers

### Issue: All sensors report failure
- Check power supply voltage (should be stable 5V or 3.3V as designed)
- Verify HAL configuration is correct
- Check for firmware build errors

### Issue: Intermittent initialization failures
- Check for loose connections
- Verify adequate power supply current capacity
- Check for EMI interference near sensors

### Issue: Config validation always fails
- Review `config.c` for invalid parameters
- Check encoder index assignments (no duplicates)
- Verify wheel dimensions are positive and finite

### Issue: Diagnostic tool shows wrong data
- Verify sensor calibration
- Check for endianness issues in data parsing
- Confirm sensor communication protocol matches expected

---

## Test Completion Criteria

✅ All test cases (TC1-TC9) pass  
✅ Both regression tests (RT1-RT2) pass  
✅ Performance tests meet criteria  
✅ No critical or blocking issues found  
✅ Documentation updated with any findings  

---

## Sign-Off

**Tested By**: ___________________________  
**Date**: ___________________________  
**Overall Result**: ⬜ PASS / ⬜ FAIL  
**Comments**:
```
[Space for tester comments and observations]
```

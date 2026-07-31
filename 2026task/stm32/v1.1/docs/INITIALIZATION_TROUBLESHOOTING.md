# Sensor Initialization Quick Reference Guide

**Purpose**: Fast troubleshooting guide for sensor initialization failures  
**Date**: 2026-07-30

---

## Error Message Quick Lookup

### Error: "HAL not configured"
```
[ERROR] HAL not configured - call sensors_configure_hal() first
```
**Cause**: Sensor HAL interface not set up  
**Fix**: Ensure `sensors_configure_hal()` is called before `sensors_init_all()`  
**Code Location**: `track_control_app.c:168-171`

---

### Error: "Config validation failed: Invalid argument"
```
[ERROR] Config validation failed: Invalid argument
[ERROR] Check encoder indices and parameter ranges
```
**Possible Causes**:
1. Duplicate encoder indices
2. Invalid encoder index (>= SD_ENCODER_COUNT)
3. Zero or negative wheel radius
4. Zero pulses per revolution
5. Invalid encoder direction (not -1 or +1)

**Fix**: Check `modules/Sens-Decision/src/config.c`:
```c
// Line 136-139: Verify encoder indices are unique
g_sens_decision_config.vehicle.left_encoder_indices[0] = 0U;
g_sens_decision_config.vehicle.right_encoder_indices[0] = 1U;

// Line 164: Wheel radius must be positive
encoder->wheel_radius_m = 0.033f;  // 33mm

// Line 192: PPR must be non-zero
encoder->pulses_per_revolution = 60000U;

// Line 193: Direction must be -1 or +1
encoder->direction = encoder_directions[index];  // {1, -1}
```

---

### Error: "Sensor 0 (Encoder_Left) initialization failed"
```
[ERROR] Sensor 0 (Encoder_Left) initialization failed: Invalid data
```
**Possible Causes**:
1. TIM3 not configured or enabled
2. Encoder not connected to TIM3
3. Encoder power supply issue
4. Wrong encoder polarity

**Hardware Check**:
- TIM3 CH1/CH2 pins connected to encoder A/B phases
- Encoder VCC = 5V (or 3.3V depending on encoder model)
- Encoder GND connected
- Encoder shaft can rotate freely

**Software Check**:
```c
// Verify encoder adapter returns valid counts
int32_t count;
if (g_sensor_hal->read_encoder_count(0, &count) == SD_OK) {
    printf("Left encoder count: %ld\n", count);
}
```

---

### Error: "Sensor 1 (Encoder_Right) initialization failed"
```
[ERROR] Sensor 1 (Encoder_Right) initialization failed: Invalid data
```
**Possible Causes**: Same as left encoder but for TIM4

**Hardware Check**:
- TIM4 CH1/CH2 pins connected to encoder A/B phases
- Power and ground connected
- Check for loose connections

---

### Error: "Sensor 2 (IMU) initialization failed"
```
[ERROR] Sensor 2 (IMU) initialization failed: Not initialized
```
**Possible Causes**:
1. ICM42688 not responding on SPI2
2. WHO_AM_I register mismatch
3. SPI clock/polarity misconfiguration
4. CS pin not working
5. IMU not powered

**Hardware Check**:
- SPI2 SCK/MISO/MOSI/CS pins connected
- IMU VCC = 3.3V
- IMU GND connected
- Check SPI signal integrity with oscilloscope

**Software Check**:
```c
// Step 5 in track_control_app.c shows ICM init result
if (icm42688_init() != ICM42688_STATUS_OK) {
    printf("[WARNING] icm42688_init failed (skipping for now)\n");
    goto skip_icm42688;
}
```

**Note**: Current firmware allows IMU init warning in Step 5 but requires success in Step 8. This may need adjustment if IMU is optional.

---

### Error: "Sensor 3 (IR_Array) initialization failed"
```
[ERROR] Sensor 3 (IR_Array) initialization failed: Not initialized
```
**Possible Causes**:
1. IR sensor not responding on USART2
2. Wrong baud rate (should be 115200)
3. UART RX/TX pins swapped
4. IR sensor not powered or in wrong mode
5. Sensor still warming up (needs ~2 seconds)

**Hardware Check**:
- USART2 RX connected to IR sensor TX
- USART2 TX connected to IR sensor RX
- IR sensor VCC = 5V
- IR sensor GND connected
- Check with oscilloscope: sensor should transmit at 115200 baud

**Software Check**:
```c
// Verify IR sensor initialized and requested analog mode
IrUartSensor_Init();
IrUartSensor_RequestAnalogMode();
osDelay(2000);  // Wait for warm-up
```

**Common Issue**: IR sensor takes 1-2 seconds to warm up after power-on. Ensure adequate delay.

---

## Diagnostic Commands

### Run Full Diagnostic Report
Add to your code temporarily:
```c
#include "interface.h"

void debug_sensors(void) {
    sensors_diagnostic_report();
}

// Call after initialization or in debug loop
debug_sensors();
```

### Check Individual Sensor
```c
sensor_t *sensor = sensor_get(SENSOR_ID_ENCODER_LEFT);
if (sensor->initialized) {
    encoder_data_t data;
    if (sensor_read(sensor, &data, 0) == SD_OK) {
        printf("Encoder Left: count=%ld, speed=%.3f m/s\n", 
               data.count, data.speed_mps);
    }
}
```

### Test Encoder Manually
```c
// Rotate wheel manually and check count change
int32_t count1, count2;
g_sensor_hal->read_encoder_count(0, &count1);
printf("Rotate left wheel NOW\n");
osDelay(2000);
g_sensor_hal->read_encoder_count(0, &count2);
printf("Count changed: %ld -> %ld (diff=%ld)\n", 
       count1, count2, count2 - count1);
```

---

## Hardware Checklist

### Power Supply
- [ ] Main power supply provides adequate current (>2A recommended)
- [ ] Voltage stable (not dropping under load)
- [ ] Separate analog/digital grounds if available
- [ ] Decoupling capacitors near ICs

### Encoder Connections
- [ ] Left encoder → TIM3 CH1/CH2
- [ ] Right encoder → TIM4 CH1/CH2
- [ ] Encoder VCC = 5V (or 3.3V depending on model)
- [ ] Encoder GND connected
- [ ] No loose connectors
- [ ] Encoder cables not crossing high-current motor wires

### IR Sensor Connections
- [ ] IR TX → MCU USART2 RX
- [ ] IR RX → MCU USART2 TX
- [ ] IR VCC = 5V
- [ ] IR GND connected
- [ ] Sensor positioned at correct height (2-3mm above ground)
- [ ] Sensor array aligned with vehicle centerline

### IMU Connections
- [ ] IMU SCK → MCU SPI2 SCK
- [ ] IMU MISO → MCU SPI2 MISO
- [ ] IMU MOSI → MCU SPI2 MOSI
- [ ] IMU CS → MCU GPIO CS pin
- [ ] IMU VCC = 3.3V
- [ ] IMU GND connected
- [ ] IMU mounted securely (vibration affects readings)

---

## Software Checklist

### Configuration (`config.c`)
- [ ] Encoder indices: left=0, right=1 (no duplicates)
- [ ] Wheel radius = 0.033m (33mm) - verify with actual wheel
- [ ] Pulses per revolution = 60000 (500 PPR encoder × 4× × 30:1 gearbox)
- [ ] Encoder directions = {1, -1} (verify with motor test)
- [ ] Wheel track = 0.115m (115mm between wheels)
- [ ] IR weights array matches physical sensor layout

### HAL Configuration
- [ ] TIM3 configured in Encoder Mode
- [ ] TIM4 configured in Encoder Mode
- [ ] USART2 configured: 115200 baud, 8N1
- [ ] SPI2 configured: appropriate speed for ICM42688
- [ ] GPIO pins configured correctly

### Initialization Sequence
```c
// Correct order in track_control_app.c:
1. Motor_Init() + Motor_Stop()          // Step 1
2. sensors_release_all()                 // Step 1b
3. Memory clear                          // Step 1c
4. Validate target laps                  // Step 1d
5. Encoder_Init()                        // Step 2
6. IrUartSensor_Init()                   // Step 3
7. sd_config_reset_defaults()            // Step 4
8. icm42688 init and calibration         // Steps 5-7
9. sensors_configure_hal()               // Step 8
10. sensors_init_all()                   // Step 8 (CRITICAL)
```

---

## Common Initialization Patterns

### Pattern 1: All Sensors Fail Immediately
**Symptom**: Every sensor reports "Not initialized"  
**Cause**: HAL not configured  
**Fix**: Check `sensors_configure_hal()` is called and returns SD_OK

### Pattern 2: Encoders OK, IR Fails
**Symptom**: Encoders initialize fine, IR fails  
**Cause**: UART communication issue or warm-up time insufficient  
**Fix**: 
1. Check UART connections and baud rate
2. Increase delay after `IrUartSensor_Init()` to 2-3 seconds
3. Verify IR sensor is in analog mode

### Pattern 3: Initialization Succeeds, Runtime Failures
**Symptom**: Init OK, but preprocess_update() keeps failing  
**Cause**: Sensor disconnected or failing during operation  
**Fix**: 
1. Check for loose connections (vibration)
2. Monitor health warnings (appear every 10 seconds)
3. Run diagnostic tool to identify which sensor

### Pattern 4: Config Validation Always Fails
**Symptom**: Never gets past config validation  
**Cause**: Parameter out of range in config.c  
**Fix**: 
1. Check for NaN or Inf values
2. Verify encoder indices are 0 and 1
3. Check all positive values are > 0

---

## Emergency Recovery Procedures

### If Vehicle Won't Initialize At All
1. **Comment out IMU init** (if IMU is optional):
   ```c
   // Temporarily skip IMU
   goto skip_icm42688;
   ```
2. **Test encoders in isolation**:
   - Run encoder test program
   - Manually rotate wheels and check counts
3. **Test IR sensor in isolation**:
   - Run IR sensor test program
   - Verify data stream on UART
4. **Check power supply**:
   - Measure VCC under load
   - Look for voltage drops

### If Initialization Hangs
**Symptom**: System stuck, no output  
**Possible Locations**:
1. Waiting for IR sensor warm-up
2. SPI communication deadlock (IMU)
3. Infinite loop in HAL code

**Debug**:
- Add printf() at each init step
- Use debugger to find where it hangs
- Check for peripheral clock enable

---

## Performance Optimization

### Reduce Initialization Time
Current: ~3 seconds (mostly IR warm-up)

**Options**:
1. Reduce IR warm-up delay (risky - may fail)
2. Initialize sensors in parallel (complex)
3. Pre-warm IR sensor before main init

### Reduce Diagnostic Overhead
Current health check: Every 10 seconds (negligible overhead)

**If needed**:
```c
// Increase interval to 30 seconds
if (++g_health_check_counter >= 1500U) {  // 30s at 50Hz
```

---

## Support Resources

### File Locations
- Sensor configuration: `modules/Sens-Decision/src/config.c`
- Sensor interface: `modules/Sens-Decision/src/interface.c`
- Application init: `Core/Src/app/track_control_app.c`
- FreeRTOS task: `Core/Src/freertos.c`

### Key Functions
- `sensors_init_all()` - Initialize all sensors
- `sensors_diagnostic_report()` - Run diagnostic test
- `sensor_get(id)` - Get sensor by ID
- `sensor_read()` - Read sensor data

### Debug Output
All debug output goes to UART5 at 115200 baud. Use serial terminal to monitor.

---

## Contact / Escalation

If initialization continues to fail after following this guide:

1. **Capture serial output** - Full boot log
2. **Document hardware setup** - Photos of connections
3. **List all attempts** - What you've tried
4. **Check hardware with multimeter** - Verify voltages
5. **Try known-good hardware** - Swap components if available

**Note**: Most initialization failures are hardware issues (loose connections, wrong voltage, damaged components) rather than software bugs.

### Task 5: Add Encoder, Motor, and Sensor Adapters

**Files:**
- Modify: `inc/encoder.h`
- Modify: `src/encoder.c`
- Create: `inc/encoder_hw_bridge.h`
- Create: `src/encoder_hw_bridge.c`
- Create: `inc/encoder_adapter.h`
- Create: `src/encoder_adapter.c`
- Create: `inc/motor_adapter.h`
- Create: `src/motor_adapter.c`
- Create: `inc/sensor_adapter.h`
- Create: `src/sensor_adapter.c`
- Create: `tests/test_target_adapters.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `Encoder_GetCount`, `Encoder_ResetCount`, `Motor_SetSpeed`, `Motor_Stop`, `Motor_Init`, `icm42688_read`, and `MCP23017_ReadInputs`.
- Produces: `EncoderInterface_t *EncoderAdapter_GetInterface(void)`, `MotorInterface_t *MotorAdapter_GetInterface(void)`, and `const sensor_hal_t *SensorAdapter_GetHal(void)`.

- [ ] **Step 1: Write failing mapping and failure-propagation tests**

Assert explicit mappings:

```c
assert(read_motion_encoder(ENCODER_LEFT_FRONT)  == fake_count[0]);
assert(read_motion_encoder(ENCODER_LEFT_REAR)   == fake_count[1]);
assert(read_motion_encoder(ENCODER_RIGHT_FRONT) == fake_count[2]);
assert(read_motion_encoder(ENCODER_RIGHT_REAR)  == fake_count[3]);

motor->setDifferentialPWM(321, -456);
assert(last_m1 == 321 && last_m2 == 321);
assert(last_m3 == -456 && last_m4 == -456);

assert(sensor_hal->read_ir_mask(&mask) == SD_ERR_READ);
assert(sensor_hal->read_imu_raw(&imu) == SD_OK);
assert(imu.temperature == fake_icm.temperature_raw);
```

- [ ] **Step 2: Resolve the encoder enum collision**

Rename only the low-level terminal enumerator from `ENCODER_COUNT` to `ENCODER_ID_COUNT` in `inc/encoder.h` and `src/encoder.c`. Keep `ENCODER_M1` through `ENCODER_M4` stable.

- [ ] **Step 3: Isolate low-level encoder headers**

`encoder_hw_bridge.h` includes only `<stdint.h>` and exports:

```c
int32_t EncoderHwBridge_GetCount(uint8_t physical_id);
void EncoderHwBridge_ResetCount(uint8_t physical_id);
```

`encoder_hw_bridge.c` includes `encoder.h`, checks `physical_id < ENCODER_ID_COUNT`, and protects reset operations from GPIO ISR races with a minimal interrupt critical section.

- [ ] **Step 4: Implement explicit Motion Control adapters**

`encoder_adapter.c` includes `motion_feedback.h` and the bridge header, uses a `switch` for all four IDs, and returns a module-owned non-const `EncoderInterface_t` because current Motion Control stores mutable pointers.

`motor_adapter.c` returns:

```c
static MotorInterface_t g_motor_interface = {
    .setDifferentialPWM = Motor_SetSpeed,
    .stop = Motor_Stop,
    .init = Motor_Init,
};
```

- [ ] **Step 5: Implement the status-preserving Sensor HAL**

Use exact callback signatures:

```c
static sd_status_t ReadEncoder(uint8_t index, int32_t *count);
static sd_status_t ReadImu(imu_raw_data_t *data);
static sd_status_t ReadIr(uint16_t *active_mask);
```

`ReadImu` copies raw accel, raw gyro, and raw temperature. It does not copy `acc_g` or `gyro_dps`. `ReadIr` converts MCP timeout to `SD_ERR_TIMEOUT`, other driver failures to `SD_ERR_READ`, masks to `0x0FFF`, and applies active polarity exactly once.

- [ ] **Step 6: Run adapter tests**

Expected: enum collision is gone; four encoder mappings, left/right motor fan-out, IMU raw units, IR polarity, null arguments, and failure propagation all pass.

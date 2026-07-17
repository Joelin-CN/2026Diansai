# Sens-Decision Module

Sensor fusion and decision-making module for autonomous line-following vehicles.

## Architecture

Five-layer data flow from sensors through trajectory generation:

1. **Preprocessing**: Sensor data conversion, filtering, and frame assembly
2. **State Evaluation**: Extended Kalman Filter for vehicle localization (x, y, θ, v, ω)
3. **Perception**: IR array processing for line tracking, curve detection, and road event classification
4. **Behavior Planning**: FSM for driving modes (IDLE, LINE_FOLLOW, CURVE, LINE_LOST_DEGRADED, STOPPED, FAULT)
5. **Trajectory Generation**: Speed and steering commands with kinematic and dynamic constraints

Hardware sensors interface through a HAL (Hardware Abstraction Layer) with virtual function tables for encoder, IMU, and IR array.

## Public API Call Order

```c
// 1. Configuration
sd_config_reset_defaults();
sd_config_validate(&g_sens_decision_config);

// 2. HAL setup
sensor_hal_t hal = { read_encoder_fn, read_imu_fn, read_ir_fn };
sensors_configure_hal(&hal);

// 3. Sensor initialization
sensors_init_all();

// 4. Per-frame loop (e.g., 10 ms interval)
while (running) {
    sensor_frame_t frame;
    perception_result_t perception;
    behavior_output_t behavior;
    trajectory_point_t trajectory;
    
    // Preprocess sensor data
    preprocess_update(timestamp_us, &frame);
    
    // Update vehicle state estimate
    state_evaluator_update(&evaluator, &frame);
    
    // Detect road features
    perception_update(&perception_state, &frame.ir, timestamp_us, &perception);
    
    // Plan driving behavior
    behavior_input_t input = { &vehicle_state, &perception, command, path_curvature };
    behavior_planner_update(&planner, &input, &behavior);
    
    // Generate trajectory command
    trajectory_generate(&generator, &vehicle_state, &behavior, dt, &trajectory);
}
```

## Units

- **Distance**: meters (m)
- **Time**: seconds (s), timestamps in microseconds (μs)
- **Angle**: radians (rad)
- **Velocity**: meters per second (m/s)
- **Angular velocity**: radians per second (rad/s)
- **Acceleration**: meters per second squared (m/s²)

## Digital IR Semantics

- **Mask format**: 12-bit active channel mask (bit 0 = leftmost sensor, bit 11 = rightmost)
- **Values**: Each channel is 0.0f (no line) or 1.0f (line detected)
- Preprocessing maps hardware 16-bit mask to 12-bit by masking bits 12-15

## VTable Contract

All sensors implement four operations:

- `init`: Initialize sensor, establish baseline, return SD_OK or error
- `read`: Read sensor data at given timestamp, return SD_OK or error
- `write`: Write configuration (read-only sensors return SD_ERR_UNSUPPORTED)
- `release`: Release resources, reset state

Read-only sensors (encoders, IMU, IR) return `SD_ERR_UNSUPPORTED` on write attempts.

## Hardware Adapter Boundary

Current implementation uses virtual HAL for testing. Real hardware adapters:

- Map HAL callback pointers to platform-specific drivers (GPIO, I2C, SPI, etc.)
- Handle hardware timing, interrupts, and DMA as needed
- Return raw counts/LSB values; sensors handle unit conversion internally
- Report errors via `sd_status_t` codes (SD_ERR_HW_FAULT, SD_ERR_TIMEOUT, etc.)

## Test Command

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\build_and_test.ps1
```

## Generated Files

All build artifacts (`.o`, `.obj`, `.exe`, `.log`, `.d`) and test code (`.c`, `.ps1`) reside under `temp/`.

The `temp/.gitignore` excludes build output while retaining test source for version control.

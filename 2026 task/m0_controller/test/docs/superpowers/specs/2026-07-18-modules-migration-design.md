# MSPM0 Controller Modules Migration Design

## 1. Goal

Migrate the reusable control logic from `E:/B306/2026/电赛/modules` into
`E:/B306/2026/电赛/2025e/m0_controller/modules`, and integrate it with the
actual MSPM0G3507 hardware configuration.

The new architecture fully replaces the PD line-following logic currently in
`src/main.c`. Existing low-level motor, encoder, and line-sensor code remains in
the repository so its tested hardware behavior can be reused where appropriate.

OLED migration is explicitly out of scope. The source OLED driver is SPI-based,
while the target hardware uses I2C0, and the display is not required for the
control pipeline.

## 2. Target Hardware

- MCU: TI MSPM0G3507, Arm Cortex-M0+, LQFP-48
- CPU clock: 32 MHz
- Flash: 128 KiB
- SRAM: 32 KiB
- RTOS: FreeRTOS
- Fast control rate: 500 Hz, driven by a 2 ms TIMG0 interrupt
- Decision rate: 50 Hz, executed every ten fast-control cycles
- Drive model: logical two-wheel differential drive
- Physical motors: M1 and M2 form the left side; M3 and M4 form the right side
- Encoders: four software-decoded quadrature encoders
- Line sensors: 12 digital channels through MCP23017 on I2C0
- IMU: ICM-42688-P on SPI1
- Competition path: counter-clockwise travel around the outer edge of a
  1.00 m by 1.00 m square, with a 1.8 cm nominal black line
- Configurable lap target: 1 through 5 laps

### 2.1 Pin Mapping

| Function | Pin/peripheral |
| --- | --- |
| M1 PWM | PA21 / TIMA0 CCP0 |
| M2 PWM | PA22 / TIMA0 CCP1 |
| M3 PWM | PA24 / TIMA1 CCP1 |
| M4 PWM | PA25 / TIMA0 CCP3 |
| M1 direction | PB6, PB7 |
| M2 direction | PB8, PA7 |
| M3 direction | PA15, PB9 |
| M4 direction | PB19, PB24 |
| Encoder 1 | PA12, PA13 |
| Encoder 2 | PA2, PA26 |
| Encoder 3 | PA27, PA28 |
| Encoder 4 | PA31, PB18 |
| I2C0 | PA0 SDA, PA1 SCL |
| ICM42688 SPI1 | PA18 PICO, PA16 POCI, PA17 SCLK, PB20 CS |
| Debug UART0 | PA10 TX, PA11 RX, 115200 baud |

## 3. Migration Scope

### 3.1 Included Modules

- `ICM42688`: portable IMU driver, MSPM0 adapter, and Mahony AHRS
- `MCP23017`: I2C GPIO expander driver
- `Motion Control`: kinematics, wheel-speed feedback, feedforward, PI control,
  command limiting, and controller state machine
- `Sens-Decision`: preprocessing, EKF state estimation, line perception,
  behavior planning, and trajectory generation

### 3.2 Excluded Content

- `oled`: excluded because the existing driver is SPI-based and the target OLED
  is connected by I2C
- `motor`: documentation only, no implementation to migrate
- `servo`: vendor documents only, no implementation to migrate
- Module examples and temporary host-test build outputs are not compiled into
  the target firmware

## 4. Directory Layout

The target layout preserves each reusable module as an independent unit:

```text
m0_controller/
|-- inc/
|   |-- encoder.h
|   |-- line_sensor.h
|   |-- motor.h
|   |-- motor_adapter.h
|   `-- encoder_adapter.h
|-- src/
|   |-- main.c
|   |-- encoder.c
|   |-- line_sensor.c
|   |-- motor.c
|   |-- motor_adapter.c
|   `-- encoder_adapter.c
`-- modules/
    |-- ICM42688/
    |   |-- inc/
    |   `-- src/
    |-- MCP23017/
    |   |-- inc/
    |   `-- src/
    |-- Motion Control/
    |   |-- inc/
    |   `-- src/
    `-- Sens-Decision/
        |-- inc/
        `-- src/
```

Project-specific glue code belongs in `src/` and `inc/`. Reusable module code
remains under `modules/` and should not gain dependencies on application files.

## 5. Architecture

The system uses two control rates to fit the 32 MHz Cortex-M0+ CPU budget.

### 5.1 Fast Loop: 500 Hz

The TIMG0 ISR notifies the FreeRTOS control task every 2 ms. On every wake-up,
the task performs:

1. Read encoder counts through `EncoderInterface_t`.
2. Estimate wheel speed.
3. Smooth and limit the latest `(v, omega)` command.
4. Run differential-drive inverse kinematics.
5. Run wheel feedforward and PI feedback.
6. Write left and right motor output through `MotorInterface_t`.

Only `MotionControl_Update()` runs on every 2 ms cycle.

### 5.2 Decision Loop: 50 Hz

Every tenth fast-loop iteration, the same control task performs one
Sens-Decision update with `dt = 0.020f`:

1. Read encoder, IMU, and IR sensor inputs through `sensor_hal_t`.
2. Assemble a timestamped sensor frame.
3. Update the EKF state estimator.
4. Update line perception and road-event classification.
5. Update the behavior-planner state machine.
6. Generate a new trajectory point.
7. Send the generated `(v, omega)` command to Motion Control.

Motion Control holds and tracks the most recent command between decision-loop
updates. This split prevents EKF, AHRS, and trajectory math from running at
500 Hz on a processor without an FPU.

### 5.3 Hybrid Square-Path Tracking

The competition path is known globally, but encoder and gyro dead reckoning can
drift over multiple laps. The velocity command therefore combines two sources:

1. Pure Pursuit on a static counter-clockwise 1 m square path provides nominal
   speed, corner lookahead, curvature, and lap progress.
2. The 12-channel IR array provides real-time lateral and heading feedback that
   corrects the nominal angular velocity.

The corrected command is:

```text
v_cmd     = trajectory.v, limited by behavior state
omega_cmd = trajectory.omega
            + k_lateral * perception.lateral_error
            + k_heading * perception.heading_error
```

The correction is clamped to the configured angular-velocity limit. When the
line is invalid, the behavior planner owns degraded search or stop behavior;
invalid IR data must not be interpreted as zero tracking error.

Lap counting uses monotonic progress around the square path and a guarded
start-line crossing. The vehicle stops after the configured target of 1 to 5
completed laps. The start/finish reference is the specified start position on
segment AB, with counter-clockwise travel.

## 6. Hardware Adapter Layer

### 6.1 Motor Adapter

`motor_adapter.c` implements the Motion Control `MotorInterface_t` using the
SysConfig-generated DriverLib symbols.

The differential output mapping is:

```text
left command  -> M1 and M2
right command -> M3 and M4
```

Each command is clamped to the motor driver's accepted range. Direction GPIOs
select forward or reverse operation, while the corresponding timer compare
register sets PWM duty. A zero command sets PWM to zero and applies the existing
project's selected stop mode consistently to all four motors.

The adapter owns the logical-to-physical mapping. Motion Control must not know
about individual PWM channels or TB6612 direction pins.

Motor polarity correction remains configurable per motor so wiring differences
can be corrected without changing controller algorithms.

### 6.2 Encoder Adapter

`encoder_adapter.c` implements `EncoderInterface_t` by wrapping the existing
software quadrature decoder in `encoder.c`.

The fixed mapping is:

| Motion Control ID | Physical encoder |
| --- | --- |
| Left front | Encoder 1 |
| Left rear | Encoder 2 |
| Right front | Encoder 3 |
| Right rear | Encoder 4 |

The adapter exposes per-channel count and reset operations. Encoder polarity
correction is performed in the adapter or configuration, not in the kinematics
module.

### 6.3 Sensor HAL

Sens-Decision receives hardware data only through `sensor_hal_t` callbacks:

- Encoder callback: encoder adapter
- IMU callback: `icm42688_read()` with units converted to the Sens-Decision
  interface contract
- IR callback: `MCP23017_ReadInputs()` or the retained `LineSensor_ReadMask()`

The callback layer must define unit and sign conventions explicitly:

- Distance: metres
- Time: seconds
- Angle: radians
- Angular velocity: radians per second
- Acceleration: metres per second squared where required
- Encoder counts: signed cumulative counts, positive for forward wheel motion

## 7. Module Integration

### 7.1 MCP23017

The driver uses I2C0 at address `0x20`, matching the target hardware. It is the
first module to migrate because the current line-sensor code already references
its API.

Initialization configures both ports as inputs. Reads return a 16-bit mask, of
which the configured 12 line-sensor bits are consumed by perception.

All polling loops must have bounded timeouts. An I2C failure marks the IR sample
invalid rather than blocking the 500 Hz control task indefinitely.

### 7.2 ICM42688

The existing MSPM0 adapter already matches SPI1 and PB20 chip-select wiring.
The core driver and AHRS code remain platform-independent.

Startup sequence:

1. Bind the MSPM0 communication and timing callbacks.
2. Reset and initialize the sensor.
3. Verify `WHO_AM_I`.
4. Configure accelerometer and gyro ranges and output rates.
5. Calibrate gyro bias while the vehicle is stationary.
6. Initialize AHRS state.

The adapter currently expects a microsecond time source. TIMG0 is already the
500 Hz scheduler source, so implementation must first verify whether the
existing counter can provide an unambiguous monotonic timestamp. If not, a free
timer instance must be added in SysConfig. The fast-loop interrupt configuration
must not be weakened to satisfy the IMU adapter.

IMU initialization failure prevents entry into the running state. Runtime read
failures invalidate that sample and allow the behavior planner to enter its
degraded or fault policy.

### 7.3 Motion Control

The reusable Motion Control implementation is migrated without its example
program. Its hardware interfaces bind to the motor and encoder adapters.

Parameters requiring physical measurement include:

- Wheel radius
- Effective wheelbase
- Encoder counts per wheel revolution, including gearbox ratio and quadrature
  counting convention
- Encoder sign for each wheel
- Maximum usable PWM
- Maximum wheel speed and acceleration
- PI gains
- Feedforward static-friction, viscous-friction, and acceleration coefficients

The left-front and left-rear wheel estimates are combined into one logical left
wheel measurement; the right pair is combined similarly. The implementation
should reject an obviously invalid encoder channel instead of allowing one bad
count to dominate the side estimate.

### 7.4 Sens-Decision

The reusable pipeline is migrated without temporary test-build files. It runs at
50 Hz and produces the latest target linear and angular velocity.

Parameters requiring target-hardware validation include:

- Twelve IR sensor positions and active polarity
- IMU axis orientation and yaw-rate sign
- Encoder scale and wheel geometry
- EKF process and observation noise
- Perception thresholds for curve, intersection, and line loss
- Behavior speed limits
- Trajectory lookahead, acceleration, and jerk limits
- IR lateral and heading correction gains and angular correction limit
- Start-line crossing guard and target lap count from 1 through 5

The default configuration is treated as a safe starting point, not as a final
tuning set.

## 8. FreeRTOS Integration

The existing timer-to-task notification structure is retained. The ISR performs
no sensor I/O, floating-point math, logging, or control calculations.

Pseudo-code for the task is:

```c
for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (++decision_divider >= 10U) {
        decision_divider = 0U;
        run_sens_decision_50hz(0.020f);
        MotionControl_SetVelocityCommand(&motion, target_v, target_omega);
    }

    MotionControl_Update(&motion);
}
```

The decision update runs before the fast update on its scheduled iteration so
the new command can be applied immediately.

The control-task stack must be increased from the existing small value and then
measured with FreeRTOS stack high-water-mark instrumentation. A provisional
starting allocation of 512 words is acceptable, but the final value is based on
measurement rather than assumption.

## 9. Main Initialization Order

The application initializes in this order:

1. Run `SYSCFG_DL_init()`.
2. Force all motor outputs to a safe stopped state.
3. Initialize encoder state and GPIO interrupts.
4. Initialize MCP23017 and verify a bounded input read.
5. Bind and initialize ICM42688.
6. Calibrate gyro bias while motors remain disabled.
7. Initialize motor and encoder adapters.
8. Reset and validate Sens-Decision configuration.
9. Configure and initialize the sensor HAL.
10. Initialize state evaluator, perception, planner, and trajectory generator.
11. Initialize Motion Control and leave it idle/stopped.
12. Create the FreeRTOS control task.
13. Enable the 500 Hz timer interrupt at the correct point in startup.
14. Start the scheduler.
15. Enter the running control state only after all mandatory initialization
    steps succeed.

Any mandatory initialization failure leaves the motors stopped and reports a
fault through UART0 where available.

## 10. Build Integration

The EIDE project must compile the following target sources:

```text
modules/MCP23017/src/mcp23017.c
modules/ICM42688/src/icm42688_hal.c
modules/ICM42688/src/ahrs_hal.c
modules/ICM42688/src/icm42688_mspm0.c
modules/Motion Control/src/motion_control.c
modules/Motion Control/src/motion_feedback.c
modules/Motion Control/src/motion_feedforward.c
modules/Motion Control/src/motion_kinematics.c
modules/Sens-Decision/src/config.c
modules/Sens-Decision/src/interface.c
modules/Sens-Decision/src/preprocess.c
modules/Sens-Decision/src/EKF.c
modules/Sens-Decision/src/perception.c
modules/Sens-Decision/src/behavior_planner.c
modules/Sens-Decision/src/state_evaluate.c
modules/Sens-Decision/src/trajectory_generate.c
modules/Sens-Decision/src/utils.c
src/motor_adapter.c
src/encoder_adapter.c
```

`example_usage.c`, module `temp/` directories, OLED sources, documentation-only
directories, and host-test executables are excluded from firmware compilation.

Required include paths are each module's `inc/` directory plus the existing
project and SysConfig-generated include paths. The toolchain must provide the
software floating-point and math-library functions used by AHRS, EKF, and
trajectory generation.

## 11. Error Handling and Safety

- Motors are stopped before peripheral initialization begins.
- Motor output remains disabled after task, heap, stack, sensor, or controller
  initialization failure.
- Hardware polling operations use finite timeouts.
- A single invalid sensor frame is propagated as invalid data, not silently
  converted into a valid zero measurement.
- Repeated invalid data drives planner degradation or fault transitions.
- Commands are clamped at the trajectory, wheel-speed, and PWM boundaries.
- Task overruns are counted. Repeated 2 ms deadline misses produce a diagnostic
  and can trigger a safe stop.
- Logging is rate-limited and must not block the control task.

## 12. Verification Strategy

Migration is performed incrementally, with verification after each stage.

### Stage 1: MCP23017

- Compile the driver in the target project.
- Verify initialization and changing 12-bit sensor masks.
- Verify timeout behavior with the device disconnected.

### Stage 2: ICM42688

- Compile core, adapter, and AHRS.
- Verify `WHO_AM_I` and stationary acceleration.
- Verify calibrated stationary gyro drift and axis signs.
- Verify the timestamp remains monotonic across timer wrap.

### Stage 3: Motor Adapter

- Test each side independently at low PWM.
- Verify all four directions and stop behavior.
- Confirm logical left/right commands map to the correct motors.

### Stage 4: Encoder Adapter

- Rotate each wheel separately and verify only the intended channel changes.
- Verify forward count signs.
- Measure counts per complete wheel revolution.

### Stage 5: Motion Control

- Run wheels off the ground with limited commands.
- Compare target and measured wheel speed over UART.
- Tune feedforward and PI before floor tests.
- Verify stop, emergency stop, saturation, and state transitions.

### Stage 6: Sens-Decision at 50 Hz

- Exercise module host tests before target integration when available.
- Verify sensor-frame validity and units.
- Verify EKF estimates remain finite and bounded.
- Verify line perception with recorded sensor patterns.
- Verify planner transitions for straight line, curve, line loss, stop, and
  fault inputs.

### Stage 7: Integrated Vehicle

- Straight-line low-speed test.
- Curve test with conservative speed limits.
- Line-loss and sensor-failure tests.
- Increase speed only after wheel control and planner transitions are stable.
- Measure task stack high-water mark, 2 ms execution time, 20 ms decision-loop
  execution time, and missed deadlines.

## 13. Acceptance Criteria

The migration is accepted when:

- All included modules compile in the m0_controller project without module
  example sources.
- OLED code is not compiled or required.
- The 500 Hz Motion Control loop runs without persistent deadline misses.
- Sens-Decision runs at 50 Hz with the correct 20 ms timestep.
- All four motor and encoder channels have verified direction and mapping.
- MCP23017 and ICM42688 failures do not leave motors running uncontrolled.
- The vehicle can follow a line at conservative speed using the new pipeline.
- Emergency and fault paths stop all four motors.
- Physical and controller tuning parameters are documented with their measured
  values before high-speed testing.

## 14. Implementation Sequence

1. Migrate and verify MCP23017.
2. Migrate and verify ICM42688 and its time source.
3. Add and verify the motor adapter.
4. Add and verify the encoder adapter.
5. Migrate and integrate Motion Control.
6. Migrate and integrate Sens-Decision at 50 Hz.
7. Rewrite application initialization and the FreeRTOS control task.
8. Perform integrated safety, timing, and vehicle tests.

OLED migration is not part of this sequence.

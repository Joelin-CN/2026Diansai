# Gyro and IR Cascade Controller Design

## 1. Purpose

This change replaces feedforward-assisted wheel control with feedback-only control and introduces gyro-assisted cascade tracking for the playground application.

The first hardware milestone is stable low-speed tracking at approximately 0.25-0.30 m/s. Maximum-speed operation and unrelated controller refactoring are outside this scope.

## 2. Scope

### 2.1 Global MotionControl changes

- Remove the feedforward subsystem from all MotionControl users.
- Remove `motion_feedforward.c`, `motion_feedforward.h`, `Feedforward_t`, `MotionControl_SetFeedforward()`, and all `FF_*` configuration parameters.
- Retain independent encoder-feedback PI controllers for the left and right wheels.
- Retain the existing `(v, omega)` command path for applications other than playground tracking.
- Add an explicit left/right wheel-speed target command path for playground cascade control.
- Correct wheel-speed estimation so it uses the actual estimator update interval instead of assuming a fixed 500 Hz update rate.

No compatibility shim is required because this repository has no callers of `MotionControl_SetFeedforward()`.

### 2.2 Playground-only cascade changes

The gyro-assisted cascade controller is enabled only in `playground_track.c`. The command semantics of `track_control_app.c` and `control_app.c` remain unchanged.

### 2.3 Out of scope

- Migrating every application to gyro-assisted cascade control.
- Adding a new general-purpose cascade-control module.
- Increasing competition speed before low-speed stability is verified.
- Rewriting historical development logs.
- Unrelated cleanup of Sens-Decision or motor-driver architecture.

## 3. Control Architecture

### 3.1 Normal gyro-assisted path

The normal path has three feedback layers:

```text
50 Hz IR outer loop
IR lateral_error
    -> proportional control and segment-specific limit
    -> omega_target

100 Hz gyro inner loop
omega_target - gyro_z_radps
    -> angular-rate PI
    -> delta_v

100 Hz wheel-speed layer
v_left_target  = v_base - delta_v / 2
v_right_target = v_base + delta_v / 2
    -> independent encoder PI controllers
    -> PWM left and right
```

The IR outer loop is proportional-only in the first version:

```text
omega_target = clamp(-lateral_kp * lateral_error,
                     -omega_max,
                     omega_max)
```

The gyro inner loop is PI in the first version. Derivative gain is zero to avoid amplifying gyro noise:

```text
omega_error = omega_target - gyro_z_radps
delta_v = angular_kp * omega_error
        + angular_ki * integral(omega_error)
```

The existing IR-derived `heading_error` is not used in the normal gyro-assisted path.

### 3.2 Forward-only wheel constraint

Playground tracking permits each wheel to move forward or stop, but not reverse. The angular controller output is therefore dynamically limited:

```text
delta_v_limit = 2 * v_base
delta_v = clamp(delta_v, -delta_v_limit, delta_v_limit)

v_left_target  = v_base - delta_v / 2
v_right_target = v_base + delta_v / 2
```

For a non-negative `v_base`, both wheel targets remain in `[0, 2 * v_base]`. If either target exceeds the global speed limit, both targets are scaled by the same factor so their ratio and steering intent are preserved.

The forward-only constraint belongs primarily in playground target generation. The motor driver retains final defensive clamping, but duplicate behavioral clamping should not be spread across kinematics, MotionControl, and the motor application layer.

### 3.3 Wheel-speed control

Each wheel uses encoder PI feedback only:

```text
pwm = speed_kp * speed_error
    + speed_ki * integral(speed_error)
```

The wheel PI output range is `[0, 100]` for the forward-only playground target path. A zero wheel target clears that wheel controller's integral and produces zero PWM. This prevents residual integral output from driving a stopped wheel.

Other applications retain their existing `(v, omega)` command path and its documented direction behavior. Removing feedforward does not otherwise change those applications' command semantics.

## 4. Module Boundaries and APIs

### 4.1 PlaygroundTrack responsibilities

`playground_track.c` owns:

- The 50 Hz IR proportional outer loop.
- The 100 Hz gyro angular-rate PI loop.
- Gyro validity, freshness, degradation, and recovery state.
- Segment-specific base speed, lateral gain, and angular-rate limit.
- Conversion of `v_base` and `delta_v` into left and right wheel-speed targets.
- Competition state transitions, line-loss behavior, and stopping behavior.

### 4.2 MotionControl responsibilities

MotionControl owns:

- Encoder-based wheel-speed estimation.
- Independent left and right wheel PI state.
- PWM calculation and motor output.
- Command smoothing where applicable to the existing `(v, omega)` path.
- Explicit switching between velocity-command mode and direct wheel-target mode.

MotionControl does not include ICM42688 headers and does not perform SPI reads. This preserves the existing hardware-interface boundary and keeps MotionControl testable without sensor hardware.

### 4.3 New command API

The minimal new API is:

```c
void MotionControl_SetWheelSpeedTargets(MotionControl_t *ctrl,
                                        float left_mps,
                                        float right_mps);
```

Calling this function selects direct wheel-target mode. Calling the existing `MotionControl_SetVelocityCommand()` selects `(v, omega)` mode. A single update must never combine values from both modes.

Starting and stopping MotionControl resets wheel PI state and direct-target state.

## 5. Timing and Scheduling

Nominal control rates are:

| Layer | Rate | Responsibility |
|---|---:|---|
| Fast cycle | 500 Hz | Encoder hardware polling and scheduling |
| Wheel-speed control | 100 Hz | Speed estimation, wheel PI, motor output |
| Gyro inner loop | 100 Hz | Fresh gyro read and angular-rate PI |
| IR outer loop | 50 Hz | IR perception and `omega_target` update |

On cycles shared by the 50 Hz and 100 Hz layers, the intended order is:

1. Acquire and process the newest IR sample.
2. Update `omega_target`.
3. Acquire the gyro sample.
4. Update the angular-rate PI and wheel targets.
5. Run MotionControl wheel PI and motor output.

This avoids the current extra cycle of delay caused by running MotionControl before publishing the new outer-loop command.

Wheel-speed estimation must use the actual interval between state-estimator updates. The current playground path calls `StateEst_Update()` at 100 Hz while configuring it as 500 Hz, which can inflate estimated speed by approximately five times. The implementation must accept an explicit `dt` or derive it from timestamps so other MotionControl callers with different rates remain correct.

## 6. Sensor Data and Validity

### 6.1 Gyroscope

The controller consumes calibrated vehicle-frame Z-axis angular rate:

- Unit: rad/s.
- Positive direction: counter-clockwise when viewed from above.
- Bias removed using stationary startup calibration.
- Vehicle coordinate transform applied.
- Low-pass filtering applied once in the sensor path.

The gyro must be sampled by the 100 Hz inner-loop path. The current 50 Hz `preprocess_update()` result is insufficient as the sole inner-loop source.

An ICM42688 hardware ODR of 1000 Hz does not imply 1000 Hz software sampling. The current implementation reads current output registers and does not consume a FIFO. Documentation and implementation must describe the actual 100 Hz software read rate.

Before vehicle tests, rotating the vehicle counter-clockwise must produce positive `gyro_z_radps`. A sign mismatch is a blocking fault because it creates positive feedback.

### 6.2 IR array

IR validity requires both successful processing and an actually detected line:

```text
line_valid = (status == SD_OK) && perception_result.line_valid
```

`perception_result.active_mask` must represent channels whose measured black strength exceeds the configured detection threshold. It must not copy the hardware availability mask, which is currently always set to all channels and causes transverse-line detection to report eight active channels regardless of the surface.

The sensor path must retain the timestamp of the last successfully parsed IR frame. Reusing old analog values must not make them appear newly sampled.

### 6.3 Freshness

Gyro and IR freshness are tracked independently. Failure of one sensor must not make a valid sample from the other sensor appear invalid.

At minimum, each path records its last successful sample time. The gyro-assisted path degrades when the gyro sample age exceeds 30 ms or three consecutive 100 Hz gyro reads are invalid. Both thresholds are configuration constants and are included in controller telemetry.

## 7. Gyro Failure Degradation and Recovery

### 7.1 Degraded IR-PD path

If gyro initialization fails, or gyro data becomes invalid or stale, playground tracking degrades to the existing IR-PD steering law:

```text
omega_fallback = -(kp * lateral_error + kd * heading_error)
```

The fallback command uses the existing `(v, omega)` MotionControl path and current segment limits. It is a deliberate degraded mode, not the normal cascade path.

If IMU initialization fails, playground initialization may complete, but the application starts in degraded IR-PD mode and prints a clear warning.

### 7.2 Transition to degraded mode

On transition from gyro-assisted mode to IR-PD mode:

- Reset the angular-rate PI integral.
- Preserve the current segment base speed.
- Clamp fallback angular velocity to the current segment limit.
- Record and throttle a diagnostic message describing the failure reason.

### 7.3 Recovery

The controller returns to gyro-assisted mode only after ten consecutive valid, fresh gyro samples. On recovery:

- Reset the angular-rate PI integral.
- Initialize `omega_target` from the current IR outer-loop result.
- Apply normal angular and wheel-target limits on the first recovered cycle.
- Emit one recovery diagnostic.

This hysteresis prevents rapid switching between control modes.

## 8. State, Saturation, and Reset Rules

### 8.1 Angular-rate PI anti-windup

The angular-rate integral must not grow further in a direction that is already saturated by:

- The configured angular controller output limit.
- The dynamic `2 * v_base` forward-only limit.
- The global wheel-speed limit.

Conditional integration is sufficient for the first implementation: integrate when output is unsaturated, or when the current error would drive a saturated output back toward the valid range.

### 8.2 Wheel PI anti-windup

Wheel PI uses the same conditional-integration rule against its final PWM limits. It must account for the actual command range rather than integrating and then relying on later negative-PWM clamps.

### 8.3 Reset conditions

Reset the angular-rate PI when:

- Starting or stopping playground control.
- Entering a fault or line-loss stop.
- Entering or leaving degraded IR-PD mode.
- The base-speed command is zero.

Reset both wheel PI controllers when:

- MotionControl starts or stops.
- An emergency stop occurs.
- Their corresponding target is explicitly zero.

Changing track segment does not automatically reset the angular PI. If a new segment reduces an output limit, clamp existing integral contribution to the new limit.

## 9. Configuration and Initial Tuning

The first version adds explicit playground cascade parameters for:

- Straight, curve, and approach IR proportional gains.
- Straight, curve, and approach `omega_target` limits.
- Angular-rate `Kp` and `Ki`.
- Angular controller `delta_v` limit, in addition to the dynamic forward-only limit.
- Gyro stale timeout and recovery sample count.
- Low-speed initial straight and curve speeds.

Initial tuning order is fixed:

1. Tune left and right wheel PI response without steering control.
2. Hold an angular-rate target and tune gyro PI.
3. Enable the IR proportional outer loop and tune lateral gain.
4. Tune segment speeds and angular limits.

Only one layer is adjusted at a time so oscillation can be attributed to the correct loop.

## 10. Static-Friction Diagnostic

The existing uncommitted static-friction test does not depend on feedforward code; it directly applies PWM and observes encoders. It may remain as a generic minimum-startup-PWM motor diagnostic, but its names, messages, and documentation must not claim that it calibrates the deleted `FF_K_STATIC` parameter.

## 11. Verification Strategy

### 11.1 Host-side controller tests

Add focused tests for logic that can run without STM32 hardware:

- IR proportional output sign and limit.
- Angular PI normal response and conditional integration.
- Dynamic `delta_v` limit keeps both targets non-negative.
- Uniform target scaling preserves the steering ratio.
- Zero target clears wheel PI state and produces zero output.
- Gyro stale transition enters degraded IR-PD mode.
- Ten consecutive valid gyro samples recover normal mode.
- Degradation and recovery reset angular PI state.
- Line loss uses `perception_result.line_valid`.
- Active-channel mask reflects threshold detections.
- Wheel-speed conversion remains correct at both 100 Hz and 500 Hz update intervals.

The repository currently lacks a host test target. The implementation plan must introduce only the smallest test harness needed for deterministic controller and perception tests.

### 11.2 ARM build verification

- Regenerate the CMake build after removing the feedforward source entry.
- Perform a clean ARM Debug compile and link.
- Search non-generated source for `motion_feedforward`, `Feedforward_`, `Feedforward_t`, `MotionControl_SetFeedforward`, `FF_K_`, and `FF_STATIC_DEADZONE`.
- Do not manually edit tracked generated files under `cmake-build-debug`.

### 11.3 Bench verification

- Confirm stationary calibrated gyro Z is near zero.
- Confirm counter-clockwise vehicle rotation yields positive gyro Z.
- Move a line under the IR array and confirm lateral-error and steering signs.
- Raise the wheels and verify wheel targets, measured speeds, and PWM remain non-negative.
- Simulate gyro timeout and verify IR-PD degradation.
- Restore gyro data and verify recovery without a large PWM step.

### 11.4 Low-speed vehicle acceptance

- Begin around 0.25 m/s and keep curve speed at or below 0.30 m/s.
- No sustained left-right oscillation on straights.
- No sustained controller saturation or abnormal single-wheel stopping in curves.
- The vehicle continues low-speed tracking in degraded IR-PD mode.
- A real line loss enters the configured fault stop.
- Speed is increased only after these checks pass.

## 12. Documentation Policy

Implementation-facing comments and the new design specification are updated with the code. Per the project collaboration rules, README, changelog, tuning guides, and final operational documentation are synchronized after user hardware acceptance.

Historical logs remain unchanged. The existing untracked `docs/CASCADE_PID_DESIGN_2026-07-31.md` is not overwritten by this specification.

## 13. Acceptance Criteria

The implementation is accepted when:

- Feedforward code, public API, configuration, and build references are removed.
- Non-playground applications retain their existing `(v, omega)` command behavior using pure wheel PI.
- Playground normal mode uses IR-P, gyro angular-rate PI, and encoder wheel PI.
- Playground never commands a negative wheel speed in normal cascade mode.
- Gyro failure degrades to IR-PD and valid-data hysteresis restores cascade mode.
- IR line validity and transverse detection use actual perception results.
- Wheel-speed estimation uses the actual update interval.
- Host logic tests and a clean ARM build pass.
- Bench checks pass before the first low-speed ground test.
- Low-speed tracking meets the stability goals in Section 11.4.

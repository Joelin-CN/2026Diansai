### Task 4: Test and Correct Motion Control Semantics

**Files:**
- Create: `tests/test_motion_control.c`
- Modify: `modules/Motion Control/inc/motion_control.h`
- Modify: `modules/Motion Control/src/motion_control.c`
- Modify: `modules/Motion Control/src/motion_feedback.c`
- Modify: `modules/Motion Control/inc/motion_config.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: four cumulative encoder callbacks and signed differential PWM callbacks.
- Produces: safe stop, forward, reverse, turn-in-place, saturation, restart, and callback-validation behavior at 500 Hz.

- [ ] **Step 1: Write failing behavior tests**

Use fake encoder and motor vtables and assert:

```c
assert(!MotionControl_Init(&ctrl, &missing_encoder_callback, &motor));
assert(!MotionControl_Init(&ctrl, &encoder, &missing_motor_callback));
assert(MotionControl_Init(&ctrl, &encoder, &motor));

MotionControl_Start(&ctrl);
MotionControl_SetVelocityCommand(&ctrl, 0.0f, 0.0f);
run_cycles(&ctrl, 100U);
assert(last_left_pwm == 0 && last_right_pwm == 0);

MotionControl_SetVelocityCommand(&ctrl, -0.20f, 0.0f);
run_cycles(&ctrl, 100U);
assert(last_left_pwm < 0 && last_right_pwm < 0);

MotionControl_EmergencyStop(&ctrl);
assert(stop_call_count == 1U);
```

Also instantiate two controllers and prove their smoothing histories are independent.

- [ ] **Step 2: Run and observe the negative-speed failure**

Expected: reverse-command test fails because the current linear clamp converts negative/zero input to positive `MIN_SPEED`.

- [ ] **Step 3: Implement symmetric deadband and instance-owned smoothing**

Remove function-static smoothing state. Use `ctrl->smoothed_v` as previous state. Apply minimum speed only when the command magnitude is nonzero:

```c
if (fabsf(v_limited) < MIN_SPEED) {
    v_limited = 0.0f;
}
v_limited = clamp_float(v_limited, -MAX_SPEED, MAX_SPEED);
```

Validate every callback used by `MotionControl_Init()`. Reset command, smoothing, PI integrals, feedforward history, and estimator history in Start/Stop transitions.

- [ ] **Step 4: Correct PI anti-windup bounds**

Store the integral contribution in output units or set accumulator bounds to `feedback_limit / ki` when `ki > 0`. Add a test that holds a saturated error for 2 seconds, removes the error, and verifies output returns inside the non-saturated range rather than remaining wound up.

- [ ] **Step 5: Use measured-count semantics in configuration comments**

Rename ambiguous documentation around `ENCODER_PPR`: it means quadrature transition counts per complete wheel revolution after gearing. Do not multiply by the currently unused `GEAR_RATIO` unless a test and measured hardware convention require it.

- [ ] **Step 6: Run strict Motion Control tests**

Compile with `-std=c99 -Wall -Wextra -Werror -pedantic` and run.

Expected: callback validation, zero, reverse, turning, saturation, anti-windup, stop, restart, and two-instance tests all pass.

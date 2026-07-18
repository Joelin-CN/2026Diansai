### Task 8: Integrate the Application Pipeline and FreeRTOS Scheduling

**Files:**
- Create: `inc/control_app.h`
- Create: `src/control_app.c`
- Create: `tests/test_control_app.c`
- Modify: `src/main.c`
- Modify: `inc/FreeRTOSConfig.h`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: platform time, all three adapters, square path, Motion Control, and Sens-Decision APIs.
- Produces: `bool ControlApp_Init(uint8_t target_laps)`, `void ControlApp_RunFastCycle(void)`, and `void ControlApp_EmergencyStop(void)`.

- [ ] **Step 1: Write failing scheduler-divider tests**

Fake all module update calls and assert:

```c
for (unsigned i = 0; i < 100U; ++i) {
    ControlApp_RunFastCycle();
}
assert(motion_update_calls == 100U);
assert(decision_update_calls == 10U);
assert(last_decision_dt == 0.020f);
```

Also assert the decision update precedes Motion Control on every tenth cycle.

- [ ] **Step 2: Write failing startup/fault tests**

Inject failures from MCP initialization, ICM identity, sensor HAL initialization, trajectory path setup, and Motion Control initialization. Every case must call the motor stop path and return `false`.

- [ ] **Step 3: Implement application-owned static state**

`control_app.c` owns static-lifetime configuration, path pointers, module instances, sensor frames, and outputs. This satisfies APIs that retain configuration/path pointers.

Initialization order is:

```text
Motor_Init -> Motor_Stop -> Encoder_Init -> MCP23017_Init and read
-> PlatformTime_Init -> ICM bind/init -> configure raw scales/bias
-> Sensor HAL configure/init -> Sens-Decision objects/path
-> Motion Control init/start
```

Do not call `icm42688_calibrate_gyro()` and then forward uncorrected raw gyro as if calibrated. Instead, collect stationary raw gyro samples during startup and write their converted mean into `g_sens_decision_config.imu.gyro_bias_radps[]`.

- [ ] **Step 4: Implement the 50 Hz pipeline**

On every tenth cycle:

```c
status = preprocess_update(PlatformTime_GetUs64(), &frame);
status = state_evaluator_update(&state_evaluator, &frame);
status = perception_update(&perception, &frame.ir, frame.timestamp_us,
                           &perception_result);
status = behavior_planner_update(&planner, &behavior_input, &behavior_output);
status = trajectory_generate(&trajectory_generator, &state_evaluator.state,
                             &behavior_output, 0.020f, &trajectory);
corrected_omega = SquarePath_CorrectOmega(trajectory.omega,
                                          perception_result.lateral_error,
                                          perception_result.heading_error,
                                          &square_config);
MotionControl_SetVelocityCommand(&motion, trajectory.v, corrected_omega);
```

Use actual field names from `sensor_frame_t` and `state_evaluator_t`; compile the host integration test to catch signature drift. Maintain `BEHAVIOR_CMD_START` until localization and line validity allow the planner to enter running state; do not send it only once at boot.

- [ ] **Step 5: Handle invalid data and lap completion**

Invalid IR data enters line-lost/degraded behavior, not zero-error behavior. Mandatory IMU/encoder failures increment a critical-failure count. Reaching target laps commands stop and transitions Motion Control to idle. Repeated critical failures call `MotionControl_EmergencyStop()`.

- [ ] **Step 6: Rewrite `main.c` as the RTOS shell**

Keep only safe startup, task creation, ISR notification, and fatal hooks. Set `CONTROL_TASK_STACK_WORDS` initially to `512U`. The control task starts TIMG0, blocks on notifications, calls `ControlApp_RunFastCycle()`, and periodically records `uxTaskGetStackHighWaterMark(NULL)` without synchronous logging inside the 2 ms path.

- [ ] **Step 7: Run host integration tests**

Expected: exact 500/50 Hz call counts, initialization ordering, persistent start command, IR correction, lap stop, and every injected failure's motor-stop assertion pass.

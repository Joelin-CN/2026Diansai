# Sens-Decision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and host-test the complete Sens-Decision pipeline from virtual raw sensor data through VTable sensors, EKF, perception, FSM, and trajectory generation.

**Architecture:** Production C99 code remains in `inc/` and `src/`; all test sources, scripts, objects, executables, and logs live under `temp/`. A static sensor registry reads a virtual or future real HAL, preprocessing emits one timestamped frame, and each following layer consumes only the previous layer's public structures.

**Tech Stack:** C99, standard C library, `math.h`, MinGW GCC on Windows PowerShell, no dynamic allocation, no external test framework.

## Global Constraints

- Modify only `modules/Sens-Decision` plus this plan/spec; do not adapt or modify current MSPM0, ICM42688, MCP23017, encoder, or Motion Control modules.
- Every sensor exposes `init`, `read`, `write`, and `release` through a function-pointer VTable.
- Current sensors are read-only; `write` returns `SD_ERR_UNSUPPORTED`.
- External timestamps are monotonic `uint64_t` microseconds and one acquisition frame uses one timestamp.
- The vehicle model is four-wheel differential drive; EKF state is `[x, y, theta, v, omega]` with a full `5 x 5` covariance.
- IR input is a 12-bit digital mask; normalized channel values are only `0.0f` or `1.0f`.
- Use C99 and static storage only; do not call `malloc`, `calloc`, `realloc`, or `free`.
- Every runtime message matches `[Sens-Decision] <level>: <message>` and uses standard `warning` spelling.
- Keep test source, build scripts, object files, executables, logs, and other generated files in `modules/Sens-Decision/temp/`.
- Do not create Git commits unless the user explicitly requests them.

## File Map

- `inc/config.h`, `src/config.c`: status codes, constants, all tunable configuration, default initialization, validation.
- `inc/utils.h`, `src/utils.c`: levelled logging, finite-value checks, clamping, angle normalization.
- `inc/interface.h`, `src/interface.c`: HAL contract, sensor VTable, sensor data types, static registry, lifecycle, raw-to-physical conversion.
- `inc/preprocess.h`, `src/preprocess.c`: acquire one coherent sensor frame and preserve per-input validity.
- `inc/EKF.h`, `src/EKF.c`: five-state EKF prediction/update and covariance mathematics.
- `inc/state_evaluate.h`, `src/state_evaluate.c`: differential-drive observations and `VehicleState` lifecycle.
- `inc/perception.h`, `src/perception.c`: digital line-array errors and road events.
- `inc/behavior_planner.h`, `src/behavior_planner.c`: behavior FSM, degradation, stopping, fault, reset.
- `inc/trajectory_generate.h`, `src/trajectory_generate.c`: path lookup, curvature limits, jerk-limited discrete speed output.
- `temp/test_main.c`: assertion harness, virtual HAL, unit scenarios, end-to-end scenario.
- `temp/build_and_test.ps1`: compile all production and test C sources and execute the test binary.
- `temp/.gitignore`: ignore generated artifacts while retaining test source and script.
- `READEME.md`: correct filename/content and document public data flow, test command, and hardware-adapter boundary.

---

### Task 1: Test Harness, Status, Configuration, and Logging

**Files:**
- Modify: `modules/Sens-Decision/inc/config.h`
- Create: `modules/Sens-Decision/src/config.c`
- Modify: `modules/Sens-Decision/inc/utils.h`
- Modify: `modules/Sens-Decision/src/utils.c`
- Create: `modules/Sens-Decision/temp/test_main.c`
- Create: `modules/Sens-Decision/temp/build_and_test.ps1`
- Create: `modules/Sens-Decision/temp/.gitignore`

**Interfaces:**
- Produces: `sd_status_t`, `sens_decision_config_t`, `g_sens_decision_config`, `sd_config_reset_defaults()`, `sd_config_validate()`, `sd_normalize_angle()`, `sd_clampf()`, and `SD_LOG_*` macros.
- Test API: local `TEST_ASSERT_TRUE`, `TEST_ASSERT_EQ_INT`, `TEST_ASSERT_NEAR`, and `run_test()` in `temp/test_main.c`.

- [ ] **Step 1: Create a failing configuration and utility test**

Add a minimal harness in `temp/test_main.c` and tests that require default wheel radius/track/PPR to be positive, invalid wheel track to return `SD_ERR_INVALID_ARGUMENT`, angle normalization to map `3.5f` into `[-pi, pi]`, and `sd_clampf(2, 0, 1)` to return `1`:

```c
static int g_passed;
static int g_failed;
#define TEST_ASSERT_TRUE(x) do { if (!(x)) return false; } while (0)
#define TEST_ASSERT_EQ_INT(e, a) TEST_ASSERT_TRUE((e) == (a))
#define TEST_ASSERT_NEAR(e, a, t) TEST_ASSERT_TRUE(fabsf((e) - (a)) <= (t))

static bool test_config_and_utils(void) {
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sd_config_validate(&g_sens_decision_config));
    TEST_ASSERT_TRUE(g_sens_decision_config.vehicle.wheel_track_m > 0.0f);
    TEST_ASSERT_NEAR(1.0f, sd_clampf(2.0f, 0.0f, 1.0f), 1e-6f);
    TEST_ASSERT_TRUE(fabsf(sd_normalize_angle(3.5f)) <= SD_PI);
    g_sens_decision_config.vehicle.wheel_track_m = 0.0f;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));
    return true;
}
```

- [ ] **Step 2: Add a test-only PowerShell build script and verify RED**

`temp/build_and_test.ps1` must create `temp/build`, find GCC via `Get-Command gcc`, compile with `-std=c99 -Wall -Wextra -Werror -pedantic -Iinc`, link `-lm`, place all `.o` and `sens_decision_tests.exe` below `temp/build`, and run the executable. Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\build_and_test.ps1
```

Expected: compile fails because `sd_config_reset_defaults`, status/config types, and utility functions are not defined.

- [ ] **Step 3: Implement minimal public configuration and utilities**

Define explicit nested config structures for vehicle, four encoders, IMU, IR perception, EKF, behavior, and trajectory. Defaults must include four encoder directions, positive radius/PPR/track, symmetric IR weights `{-11,-9,-7,-5,-3,-1,1,3,5,7,9,11}`, positive diagonal noise values, and ordered `dt_min_s < dt_max_s`. Validation rejects null pointers, non-finite values, non-positive geometry/PPR/noise, filter alpha outside `[0,1]`, zero frame thresholds, and invalid trajectory limits.

Replace the mismatched print VTable with:

```c
typedef enum { SD_LOG_LEVEL_DEBUG, SD_LOG_LEVEL_INFO,
               SD_LOG_LEVEL_WARNING, SD_LOG_LEVEL_ERROR } sd_log_level_t;
void sd_log(sd_log_level_t level, const char *format, ...);
#define SD_LOG_INFO(...) sd_log(SD_LOG_LEVEL_INFO, __VA_ARGS__)
#define SD_LOG_WARNING(...) sd_log(SD_LOG_LEVEL_WARNING, __VA_ARGS__)
#define SD_LOG_ERROR(...) sd_log(SD_LOG_LEVEL_ERROR, __VA_ARGS__)
```

`sd_log()` prints exactly one prefix per line. Utility functions reject or safely handle non-finite values.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run the script. Expected: `test config_and_utils passed` and summary reports zero failures, both with the standard prefix.

### Task 2: Static Sensor VTable and Coherent Preprocessing

**Files:**
- Modify: `modules/Sens-Decision/inc/interface.h`
- Create: `modules/Sens-Decision/src/interface.c`
- Modify: `modules/Sens-Decision/inc/preprocess.h`
- Modify: `modules/Sens-Decision/src/preprocess.c`
- Modify: `modules/Sens-Decision/temp/test_main.c`

**Interfaces:**
- Consumes: global validated configuration and `sd_status_t`.
- Produces: `sensor_hal_t`, `sensor_t`, `sensor_vtable_t`, `sensor_id_t`, `encoder_data_t`, `imu_data_t`, `ir_array_data_t`, `sensors_configure_hal()`, `sensors_init_all()`, `sensor_get()`, `sensor_read()`, `sensor_write()`, `sensors_release_all()`, `sensor_frame_t`, and `preprocess_update(uint64_t, sensor_frame_t *)`.

- [ ] **Step 1: Add failing virtual-HAL lifecycle/conversion tests**

Create static virtual raw values and callbacks. Assert: reading before init fails; four encoder first reads establish baselines; after `100000 us`, count deltas produce expected `direction * delta * 2*pi*radius/PPR/dt`; IMU scale/bias/filter produce finite SI values; IR mask `0x805` maps channels 0, 2, and 11 to `1.0f`; `write` is unsupported; repeated release succeeds; a HAL failure marks only that sub-input invalid.

- [ ] **Step 2: Run tests to verify RED**

Run `powershell -ExecutionPolicy Bypass -File .\temp\build_and_test.ps1`.

Expected: compile fails on missing sensor/preprocess interfaces.

- [ ] **Step 3: Implement static sensor objects and VTables**

Use this fixed registry order:

```c
typedef enum {
    SENSOR_ID_ENCODER_LEFT_FRONT,
    SENSOR_ID_ENCODER_LEFT_REAR,
    SENSOR_ID_ENCODER_RIGHT_FRONT,
    SENSOR_ID_ENCODER_RIGHT_REAR,
    SENSOR_ID_IMU,
    SENSOR_ID_IR_ARRAY,
    SENSOR_ID_COUNT
} sensor_id_t;
```

Each private object stores only its configuration pointer and state required by conversion/filtering. Wrapper functions validate object, VTable, output, and initialization. Batch initialization rolls back in reverse order. `preprocess_update()` zero-initializes output, sends the same timestamp to all six objects, sets four encoder validity flags plus IMU/IR validity, and returns the first non-OK status while retaining valid results.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: lifecycle, conversion, IR mapping, and partial-failure tests pass with no sanitizer-independent undefined data use.

### Task 3: Full Five-State EKF and Differential State Evaluation

**Files:**
- Modify: `modules/Sens-Decision/inc/EKF.h`
- Modify: `modules/Sens-Decision/src/EKF.c`
- Modify: `modules/Sens-Decision/inc/state_evaluate.h`
- Modify: `modules/Sens-Decision/src/state_evaluate.c`
- Modify: `modules/Sens-Decision/temp/test_main.c`

**Interfaces:**
- Consumes: `sensor_frame_t`, vehicle geometry, EKF Q/R and timestamp limits.
- Produces: `ekf_t`, `ekf_init()`, `ekf_predict()`, `ekf_update()`, `vehicle_state_t`, `state_evaluator_t`, `state_evaluator_init()`, and `state_evaluator_update()`.

- [ ] **Step 1: Add failing EKF tests**

Test first-frame baseline, 1 m/s straight travel for 1 second (`x` near 1, `y/theta` near 0), equal/opposite wheel speeds (near-zero `v`, nonzero `omega`), gyro correction, symmetric finite covariance with non-negative diagonal, timestamp rollback, excessive `dt`, NaN observation, and repeated failures clearing `localization_valid`.

- [ ] **Step 2: Run tests to verify RED**

Expected: missing EKF/state evaluator symbols.

- [ ] **Step 3: Implement prediction and 3-observation update**

Use fixed-size stack arrays. Prediction uses:

```text
F[0,2] = -v*sin(theta)*dt  F[0,3] = cos(theta)*dt
F[1,2] =  v*cos(theta)*dt  F[1,3] = sin(theta)*dt
F[2,4] = dt
```

Observation rows select `v`, `omega`, and `omega`. Invert the `3 x 3` innovation covariance with determinant checking. Update covariance with Joseph form `(I-KH)P(I-KH)^T + KRK^T`, then explicitly symmetrize `P`. State evaluation averages left/right wheel pairs, calculates differential observations, tracks consecutive valid/failure frames, and never predicts using an invalid `dt`.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: all numerical and error-path tests pass within declared tolerances; no NaN reaches `vehicle_state_t`.

### Task 4: Digital IR Perception

**Files:**
- Modify: `modules/Sens-Decision/inc/perception.h`
- Modify: `modules/Sens-Decision/src/perception.c`
- Modify: `modules/Sens-Decision/temp/test_main.c`

**Interfaces:**
- Consumes: valid `ir_array_data_t`, timestamp, perception thresholds and configured weights.
- Produces: `road_event_t`, `perception_result_t`, `perception_t`, `perception_init()`, and `perception_update()`.

- [ ] **Step 1: Add failing perception scenarios**

Assert masks `0x060`/`0x0C0` produce near-center results according to configured weights, left-only and right-only masks have opposite signs, wide active masks trigger intersection, rapidly moving masks trigger curve entry, zero masks increment loss count, and a valid mask resets it. Include timestamp rollback and invalid-input tests.

- [ ] **Step 2: Run tests to verify RED**

Expected: perception symbols are missing.

- [ ] **Step 3: Implement deterministic event priority and filtering**

Calculate lateral error as active-weight average divided by maximum absolute configured weight. Calculate derivative using real `dt`, low-pass it into heading error, and use event priority `LINE_LOST > INTERSECTION > CURVE_ENTRY > NONE` so one frame has one unambiguous event. Do not retain road events across frames.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: all mask, event, reset, and time validation tests pass.

### Task 5: Behavior FSM and Safety Degradation

**Files:**
- Modify: `modules/Sens-Decision/inc/behavior_planner.h`
- Modify: `modules/Sens-Decision/src/behavior_planner.c`
- Modify: `modules/Sens-Decision/temp/test_main.c`

**Interfaces:**
- Consumes: `vehicle_state_t`, `perception_result_t`, start/stop/reset commands, path curvature, behavior thresholds.
- Produces: `behavior_state_t`, `behavior_command_t`, `behavior_input_t`, `behavior_output_t`, `behavior_planner_init()`, and `behavior_planner_update()`.

- [ ] **Step 1: Add failing transition-table tests**

Cover IDLE start gating, line follow, approach/curve entry and stable curve exit, short loss and recovery to saved state, persistent loss to STOPPED, immediate stop command, consecutive critical failures to FAULT, reset rejection while inputs remain invalid, and successful reset to IDLE when healthy.

- [ ] **Step 2: Run tests to verify RED**

Expected: missing behavior types/functions.

- [ ] **Step 3: Implement a single explicit FSM update**

Evaluate stop first, then fault, reset, line-loss safety, and normal road transitions. Output state-specific speed limits and the last valid steering direction during degradation. Log only actual transitions, for example:

```c
SD_LOG_WARNING("behavior changed from %s to %s", old_name, new_name);
```

Use bounds-checked state-name lookup and saturating frame counters.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: complete transition table passes and repeated same-state updates do not emit transition logs.

### Task 6: Path Tracking and Jerk-Limited Trajectory Generation

**Files:**
- Modify: `modules/Sens-Decision/inc/trajectory_generate.h`
- Modify: `modules/Sens-Decision/src/trajectory_generate.c`
- Modify: `modules/Sens-Decision/temp/test_main.c`

**Interfaces:**
- Consumes: path array/count, `vehicle_state_t`, `behavior_output_t`, trajectory limits, and `dt`.
- Produces: `path_point_t`, `trajectory_point_t`, `trajectory_generator_t`, `trajectory_generator_init()`, `trajectory_set_path()`, and `trajectory_generate()`.

- [ ] **Step 1: Add failing straight/arc/safety tests**

Use explicit finite arrays for a straight path and quarter-circle path. Assert target index never moves backward during forward travel, curvature reduces speed, `|a| <= max_accel/decel`, `|delta_a/dt| <= max_jerk`, `omega = v * curvature`, empty/NaN paths fail, and STOPPED/FAULT always return finite zero speed and angular speed.

- [ ] **Step 2: Run tests to verify RED**

Expected: trajectory symbols are missing.

- [ ] **Step 3: Implement bounded search and discrete S-curve update**

Search from the previous nearest index through a configured forward window, walk accumulated path distance to the lookahead target, and compute curvature speed limit with a finite epsilon-protected formula. Update desired acceleration by at most `max_jerk * dt`, clamp acceleration/deceleration, integrate speed without overshooting target, and derive angular acceleration from consecutive angular speeds. Degraded mode keeps the last valid target while commanding a decreasing speed; STOPPED/FAULT bypass planning with a zero safe point.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: straight, arc, constraints, invalid path, degraded, and safety outputs pass.

### Task 7: End-to-End Virtual Scenarios and Documentation

**Files:**
- Modify: `modules/Sens-Decision/temp/test_main.c`
- Modify: `modules/Sens-Decision/temp/build_and_test.ps1`
- Delete: `modules/Sens-Decision/src/test_main.c`
- Delete: `modules/Sens-Decision/READEME.md`
- Create: `modules/Sens-Decision/README.md`

**Interfaces:**
- Consumes: every public interface created in Tasks 1-6.
- Produces: one host command that compiles and exercises the whole module, plus accurate module documentation.

- [ ] **Step 1: Add a failing complete driving scenario**

Drive virtual raw counts, gyro, and masks through this sequence at fixed 10 ms timestamps: initialize, establish encoder baselines, centered straight line, rightward curve, short no-line interval, line recovery, long no-line interval, stop. Assert every stage's expected perception event/FSM state, finite EKF state/covariance, monotonic path progress, and final zero-speed trajectory. Add a second scenario where IMU HAL reads fail until FAULT.

- [ ] **Step 2: Run the complete suite before final integration changes**

Expected: new end-to-end assertions expose any missing initialization or cross-layer contract errors.

- [ ] **Step 3: Fix only integration defects and finalize build hygiene**

Keep scenario orchestration in `temp/test_main.c`; do not add a production coordinator absent from the approved design. Make the script compile an explicit production source list so accidental C files are not silently linked. `temp/.gitignore` must retain `.c` and `.ps1` while ignoring `build/`, `*.o`, `*.obj`, `*.exe`, `*.log`, and dependency files.

- [ ] **Step 4: Replace the misspelled README with final usage documentation**

Document architecture, public call order, units, digital IR semantics, VTable contract, lack of current hardware adaptation, and exact command:

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\build_and_test.ps1
```

State that all generated files remain below `temp/`.

- [ ] **Step 5: Run final verification**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\build_and_test.ps1
git diff --check -- modules/Sens-Decision docs/superpowers/specs/2026-07-17-sens-decision-design.md docs/superpowers/plans/2026-07-17-sens-decision-implementation.md
rg -n "printf|puts|fprintf" modules/Sens-Decision --glob "!temp/build/**"
rg -n "malloc|calloc|realloc|free" modules/Sens-Decision/inc modules/Sens-Decision/src
```

Expected: test process exits 0 and ends with `[Sens-Decision] info: test summary: passed=N, failed=0`; `git diff --check` has no output; print search finds only the centralized logger/test capture code and every visible line uses the required prefix; allocation search has no matches.

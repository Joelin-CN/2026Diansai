### Task 6: Implement 50 Hz Hybrid Square Tracking and Lap Counting

**Files:**
- Create: `inc/square_path.h`
- Create: `src/square_path.c`
- Create: `tests/test_square_path.c`
- Modify: `modules/Sens-Decision/src/trajectory_generate.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: `trajectory_point_t`, `perception_result_t`, current path index/progress, and target lap count.
- Produces: a static CCW 1 m square path, corrected `(v, omega)`, and a guarded `lap_complete`/`target_reached` state.

- [ ] **Step 1: Write failing square geometry tests**

Define the coordinate frame as A=`(0,0)`, B=`(1,0)`, D=`(1,1)`, C=`(0,1)`, with CCW traversal `A -> C -> D -> B -> A`. Generate points at no more than 20 mm spacing and assert:

```c
assert(SquarePath_GetPointCount() >= 200U);
assert_point_near(point_at_a, 0.0f, 0.0f);
assert_all_points_inside_bounds(0.0f, 1.0f);
assert_path_is_closed());
assert_path_direction_is_counter_clockwise());
```

The exact A/B/C/D assignment follows the contest figure: AB and CD are opposite sides, AC and BD are opposite sides.

- [ ] **Step 2: Write failing Pure Pursuit geometry tests**

The existing trajectory module does not yet calculate Pure Pursuit curvature from the vehicle pose. Add tests that place the vehicle on a straight segment and before a corner:

```c
assert_near(generate_at(0.0f, 0.50f, -M_PI_2).omega, 0.0f, 1e-3f);
assert(generate_before_corner().omega > 0.0f);
assert(fabsf(generate_with_target_on_left().curvature) > 0.0f);
```

For lookahead target `(x_t, y_t)`, vehicle pose `(x, y, theta)`, and lookahead distance `L`, transform the target into vehicle coordinates and expect:

```text
y_local   = -sin(theta) * (x_t - x) + cos(theta) * (y_t - y)
curvature = 2 * y_local / (L * L)
omega     = v * curvature
```

This calculation replaces the current behavior of copying `target_point->curvature` into the command.

- [ ] **Step 3: Write failing hybrid correction tests**

Use:

```c
omega = SquarePath_CorrectOmega(nominal_omega, lateral_error,
                                heading_error, &config);
```

Assert zero error preserves nominal omega, opposite lateral errors produce opposite corrections, heading correction has configured sign, and output is clamped to `max_omega_radps`.

- [ ] **Step 4: Write failing guarded lap tests**

Simulate path progress crossing the start reference. Assert no lap is counted before leaving the start guard, no repeated count occurs while remaining near the line, one full monotonic wrap counts exactly one lap, and target laps outside `1..5` are rejected.

- [ ] **Step 5: Implement square path and hybrid configuration**

Expose:

```c
typedef struct {
    float lateral_gain;
    float heading_gain;
    float max_omega_radps;
    uint8_t target_laps;
} square_path_config_t;

typedef struct {
    uint8_t completed_laps;
    bool left_start_guard;
    bool target_reached;
} lap_counter_t;

const path_point_t *SquarePath_GetPoints(void);
size_t SquarePath_GetPointCount(void);
float SquarePath_CorrectOmega(float nominal_omega, float lateral_error,
                              float heading_error,
                              const square_path_config_t *config);
bool SquarePath_UpdateLap(lap_counter_t *counter, size_t nearest_index,
                          size_t path_count);
```

Keep gains in one application configuration object; do not hide tuneable constants in `main.c`.

- [ ] **Step 6: Implement real Pure Pursuit and harden preconditions**

Compute curvature from the selected lookahead point and current vehicle pose using the formula in Step 2. Keep path-point curvature only as metadata for speed limiting and behavior input. Add tests and checks for uninitialized generator, missing path, path count below 2, non-finite vehicle state, and `dt <= 0`. Return `SD_ERR_NOT_INITIALIZED`, `SD_ERR_INVALID_ARGUMENT`, or `SD_ERR_NUMERIC` rather than dividing by invalid `dt`.

- [ ] **Step 7: Run square-path and original Sens-Decision tests**

Expected: geometry, correction, lap guard, target-lap, and trajectory-precondition tests pass; the original 65 Sens-Decision tests remain green.

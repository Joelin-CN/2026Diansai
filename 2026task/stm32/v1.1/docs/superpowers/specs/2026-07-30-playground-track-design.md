# Playground Track (操场型循迹) — Design Spec

**Date**: 2026-07-30
**Status**: Approved for implementation
**Version**: 1.0

---

## 1. Overview

Implementation of segment-aware adaptive line-following for two competition tasks on a
standard oval playground track (操场型赛道).

**Hardware**: STM32F407VGT6 · 8-channel IR array (USART2, 115200) · TB6612 differential
drive · dual encoders (TIM3 left, TIM4 right)

| Task | Requirement | Key constraint |
|---|---|---|
| **Task 2** | Clockwise one lap A→A, ≤20 s, stop ≤2 cm from A | Precise stop at transverse start/stop line |
| **Task 4** | A→B straight, ≤8 s, steel-ball pendulum deviation ≤1 cm | Minimal longitudinal and lateral acceleration |

---

## 2. Track Geometry

```
  A ─────────────── B
  │                 │
  │  (left curve)   │  (right curve, R=0.5 m)
  │                 │
  D ─────────────── C

Clockwise order: A → B → C → D → A
```

| Segment | Arc length | Cumulative from A |
|---|---|---|
| A → B (straight) | 1.500 m | 0.000 – 1.500 m |
| B → C (right curve, R=0.5 m) | π×0.5 = 1.571 m | 1.500 – 3.071 m |
| C → D (straight) | 1.500 m | 3.071 – 4.571 m |
| D → A (left curve, R=0.5 m) | π×0.5 = 1.571 m | 4.571 – 6.142 m |

**A-line (启停线)**: thick black transverse line at A, perpendicular to travel direction.
Activates ≥6 of the 8 IR channels simultaneously when crossed.

---

## 3. Architecture

### 3.1 File Changes

| File | Action |
|---|---|
| `Core/Inc/app/playground_track.h` | **Create** — public API |
| `Core/Src/app/playground_track.c` | **Create** — implementation |
| `Core/Src/freertos.c` | **Modify** — add `TEST_MODE_PLAYGROUND_TRACK` branch |
| `CMakeLists.txt` | **Modify** — add `playground_track.c` to source list |

**Unchanged** (reused as-is): `perception.c`, all of `modules/MotionControl/`,
`ir_calibration.c`, all hardware drivers.

**Preserved but inactive**: `track_control_app.c` (Pure Pursuit mode, reachable by
switching the define back).

### 3.2 Control Frequency Layers (identical to existing architecture)

```
500 Hz  Encoder_Poll()
100 Hz  MotionControl_Update()
 50 Hz  preprocess_update()          — fills sensor_frame (IR, encoder, IMU)
        perception_update()          — lateral_error (cm), heading_error, active_mask
        PlaygroundTrack_Decide()     — state machine → MotionControl_SetVelocityCommand()
```

**Removed vs track_control_app.c**: `state_evaluator_update()` (EKF),
`behavior_planner_update()`, `trajectory_generate()`. No EKF → no drift across restarts.

### 3.3 Distance Tracking

Cumulative arc-length from A, integrated every 50 Hz cycle (dt = 0.020 s):

```c
float vl, vr;
MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
g_dist_m += (vl + vr) * 0.5f * 0.020f;
```

Uses encoder-derived wheel speeds directly from MotionControl. Accurate for one lap;
does not drift because no EKF accumulation.

### 3.4 freertos.c Change

```c
// Replace active define:
#define TEST_MODE_PLAYGROUND_TRACK   /* Task 2 / Task 4 competition mode */
// #define TEST_MODE_IR_CALIBRATION
// #define TEST_MODE_TRACK_CONTROL

// New #elif branch in StartDefaultTask():
#elif defined(TEST_MODE_PLAYGROUND_TRACK)
    #include "playground_track.h"
    if (!PlaygroundTrack_Init(PLAYGROUND_TASK_LAP)) {   /* or TASK_AB_STRAIGHT */
        for (;;) osDelay(1000);
    }
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
    for (;;) {
        PlaygroundTrack_RunFastCycle();
        osDelay(2);
    }
```

---

## 4. Public API

```c
/* playground_track.h */

typedef enum {
    PLAYGROUND_TASK_LAP,          /* Task 2: full lap A→A              */
    PLAYGROUND_TASK_AB_STRAIGHT,  /* Task 4: A→B, pendulum-safe mode   */
} playground_task_t;

bool  PlaygroundTrack_Init(playground_task_t task);
void  PlaygroundTrack_RunFastCycle(void);  /* call at 500 Hz            */
bool  PlaygroundTrack_IsComplete(void);
float PlaygroundTrack_GetDistance(void);   /* debug: cumulative metres  */
```

---

## 5. State Machines

### 5.1 Task 2 — Full Lap (PLAYGROUND_TASK_LAP)

Internal states (`pt_state_t`):

```
PT_IDLE
  │  line_valid == true for 3 consecutive 50 Hz frames
  ▼
PT_TASK2_RUN  ◄────────────────────────────────────────────────┐
  │  Segment-aware speed + PD ω (see §6)                       │
  │  dist > 5.0 m → reduce v to v_approach (still in RUN)      │
  │  dist > 5.5 m AND transverse_line_detected                 │
  ▼                                                             │
PT_TASK2_APPROACH_A                                             │
  │  v_cmd = 0; MotionControl ramps down at 3.0 m/s²           │
  │  Continue lateral PD ω while decelerating                   │
  │  v_actual < 0.05 m/s                                        │
  ▼
PT_STOPPED  (motors off)

  From PT_TASK2_RUN: line_valid == false for > 10 frames → PT_FAULT → Motor_Stop()
```

### 5.2 Task 4 — A→B Straight (PLAYGROUND_TASK_AB_STRAIGHT)

```
PT_IDLE
  │  line_valid == true for 3 frames
  ▼
PT_TASK4_ACCEL
  │  v_cmd += a_task4 × dt each 50 Hz cycle
  │  v_cmd ≥ v_task4_max
  ▼
PT_TASK4_CRUISE
  │  v_cmd = v_task4_max (constant)
  │  dist ≥ d_decel_start (1.083 m)
  ▼
PT_TASK4_DECEL
  │  v_cmd -= a_task4 × dt each 50 Hz cycle
  │  v_cmd ≤ 0
  ▼
PT_STOPPED

  Any state: line_valid == false for > 5 frames → PT_FAULT → Motor_Stop()
```

---

## 6. Control Algorithms

### 6.1 Omega (ω) Calculation — All States

```c
float omega = -(kp * lateral_error + kd * heading_error);
omega = clamp(omega, -omega_max, +omega_max);
MotionControl_SetVelocityCommand(&g_mc, v_cmd, omega);
```

`lateral_error` is in **cm** (IR weighted-centroid output from `perception.c`).
`heading_error` is dimensionless (low-pass-filtered derivative).

### 6.2 Task 2 Segment Speed and Gain Table

Selection is based on `g_dist_m` alone — no IR pattern interpretation required.

| Distance range | Segment | v_target | kp_lat | kd_head | ω_max (rad/s) |
|---|---|---|---|---|---|
| 0.000 – 1.500 m | Straight A→B | 1.00 m/s | 1.5 | 1.0 | 3.0 |
| 1.500 – 3.071 m | Curve B→C (right) | 0.60 m/s | 2.5 | 1.5 | 3.0 |
| 3.071 – 4.571 m | Straight C→D | 1.00 m/s | 1.5 | 1.0 | 3.0 |
| 4.571 – 5.000 m | Curve D→A (early) | 0.60 m/s | 2.5 | 1.5 | 3.0 |
| 5.000 m – A-line | Curve D→A (approach) | **0.25 m/s** | 2.0 | 1.2 | 2.0 |

**Rationale for 0.25 m/s approach speed**:
MotionControl built-in decel limit = 3.0 m/s².
Stop distance = v² / (2a) = 0.0625 / 6.0 ≈ **1 cm** — within the ≤ 2 cm requirement.

### 6.3 A-Line Detection (Transverse Stop Line)

```c
/* Count active IR channels from 8-bit active_mask */
static uint8_t count_active(uint16_t mask) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < 8; i++) n += (uint8_t)((mask >> i) & 1U);
    return n;
}

bool transverse_line = (count_active(result.active_mask) >= 6);
bool past_safety     = (g_dist_m > 5.5f);   /* avoid false trigger at start */

if (transverse_line && past_safety) → transition to PT_TASK2_APPROACH_A
```

Distinguishes transverse line (all channels active) from a tight curve (only one side
active) or normal tracking (1–3 central channels active).

### 6.4 Task 4 Trapezoid Speed Profile

| Parameter | Value | Derivation |
|---|---|---|
| v_task4_max | 0.50 m/s | pendulum-limited ceiling |
| a_task4 | 0.30 m/s² | θ = arctan(0.3/9.81) ≈ 1.75° → 4.6 mm at L=15 cm |
| d_ramp | 0.417 m | v² / (2a) = 0.25 / 0.6 |
| d_decel_start | 1.083 m | 1.500 − 0.417 |
| Predicted total time | ~4.7 s | 2×(0.5/0.3) + 0.666/0.5 |

```c
/* ACCEL (each 50 Hz cycle, dt = 0.020 s) */
v_cmd += 0.30f * 0.020f;   /* +0.006 m/s per cycle */

/* DECEL */
v_cmd -= 0.30f * 0.020f;   /* −0.006 m/s per cycle */
if (v_cmd < 0.0f) v_cmd = 0.0f;
```

Task 4 omega (all states — strict limit to protect pendulum):

```c
omega = -(0.8f * lateral_error + 0.5f * heading_error);
omega = clamp(omega, -1.0f, +1.0f);
```

---

## 7. Initialization Sequence

`PlaygroundTrack_Init()` follows the same hardware chain as `TrackControlApp_Init()`
with three items removed:

| Step | Component | Included |
|---|---|---|
| 1 | `Motor_Init()` + `Motor_Stop()` | ✅ |
| 2 | `Encoder_Init()` | ✅ |
| 3 | `IrUartSensor_Init()` + `RequestAnalogMode()` | ✅ |
| 4 | `sd_config_reset_defaults()` + `PlatformTime_Init()` | ✅ |
| 5 | ICM42688 init + gyro calibration (graceful skip on fail) | ✅ |
| 6 | `sensors_configure_hal()` + `sensors_init_all()` | ✅ |
| 7 | `perception_init()` | ✅ |
| — | `state_evaluator_init()` | ❌ removed (no EKF needed) |
| — | `behavior_planner_init()` | ❌ removed |
| — | `trajectory_generator_init()` + `trajectory_set_path()` | ❌ removed |
| 8 | `MotionControl_Init()` + `MotionControl_Start()` | ✅ |

---

## 8. Parameter Summary

All parameters are collected in `pg_config_t`, initialised in `PlaygroundTrack_Init()`.

| Field | Default | Tune when |
|---|---|---|
| `v_straight` | 1.00 m/s | Oscillation on straights → decrease |
| `v_curve` | 0.60 m/s | Line loss on curves → decrease |
| `v_approach` | 0.25 m/s | Stop overshoot → decrease |
| `kp_straight` | 1.5 | Lateral drift on straight → increase |
| `kd_straight` | 1.0 | Oscillation on straight → increase |
| `kp_curve` | 2.5 | Cutting curves → increase |
| `kd_curve` | 1.5 | Hunting on curves → increase |
| `kp_approach` | 2.0 | — |
| `kd_approach` | 1.2 | — |
| `v_task4_max` | 0.50 m/s | Time too slow → increase carefully |
| `a_task4` | 0.30 m/s² | Ball exceeds 1 cm → decrease |
| `kp_task4` | 0.8 | Drifting on straight → increase |
| `kd_task4` | 0.5 | — |
| `transverse_min_ch` | 6 | A-line not detected → decrease to 5 |
| `approach_start_dist` | 5.00 m | — |
| `a_detect_min_dist` | 5.50 m | False early trigger → increase |
| `line_lost_fault_lap` | 10 frames | 200 ms at 50 Hz |
| `line_lost_fault_ab` | 5 frames | 100 ms at 50 Hz |

---

## 9. Expected Performance

| Metric | Requirement | Predicted | Method |
|---|---|---|---|
| Task 2 total time | ≤ 20 s | ~10 s | 2×1.5 m @ 1.0 + 2×1.57 m @ 0.6 + transitions |
| Task 2 stop deviation | ≤ 2 cm | ~1 cm | v=0.25 m/s, MC decel=3 m/s², d=v²/2a |
| Task 4 time | ≤ 8 s | ~4.7 s | trapezoid profile |
| Task 4 pendulum | ≤ 1 cm | ~0.5 cm | a=0.3 m/s², L=15 cm, d=L·sin(arctan(a/g)) |

---

## 10. Verification Checklist

**P0 — Must pass before competition:**
- [ ] Compiles without errors or warnings
- [ ] Robot tracks black line for one full lap at `v_straight = 0.5 m/s` (half speed first)
- [ ] A-line detection fires on 3 consecutive manual crossings without false positives
- [ ] Task 2: robot stops within 2 cm of A-line mark
- [ ] Task 4: robot stops within B zone, no sharp steering corrections observed

**P1 — Tune after P0:**
- [ ] Raise `v_straight` to 1.0 m/s; no line loss on straights
- [ ] Curve transitions smooth (no oscillation entering/exiting)
- [ ] Task 4 pendulum deviation confirmed ≤ 1 cm at `v_task4_max = 0.5 m/s`

**P2 — Optional speed tuning:**
- [ ] Try `v_straight = 1.2 m/s` for faster lap time
- [ ] Try `v_curve = 0.7 m/s` if curves are stable at 0.6

# Validation Checklist - IR Algorithm Simplification (v1.3.0)

**Date**: 2026-07-30
**Status**: Pending verification

---

## Pre-Flight Checks

### 1. Compilation Verification
- [x] Compile with 0 errors, 0 warnings
- [ ] Grep verification:
  - [ ] `grep -r "road_event_t"` -- no matches in source (docs/ excluded)
  - [ ] `grep -r "ROAD_EVENT_"` -- no matches in source
  - [ ] `grep -r "intersection_active_channels"` -- no matches in source
  - [ ] `grep -r "curve_error_threshold\|curve_derivative_threshold\|approach_curve_speed\|curve_speed_mps\|curve_exit_stable"` -- no matches in source

### 2. Parameter Verification
- [ ] `sd_config_validate()` returns SD_OK (0)
- [ ] `behavior_planner_init()` does not access removed fields
- [ ] `speed_error_gain` initialized to 0.3f
- [ ] No stale references to removed parameters in any speed mode

### 3. Code State Verification
- [ ] `perception_result_t` struct has no `event` field
- [ ] `road_event_t` enum does not exist
- [ ] `behavior_state_t` has exactly 5 states: IDLE, RUNNING, LINE_LOST_DEGRADED, STOPPED, FAULT
- [ ] `behavior_planner_t` has no `stable_straight_frames` field
- [ ] `sd_behavior_config_t` has `speed_error_gain` field
- [ ] `sd_behavior_config_t` has no `approach_curve_speed_mps`, `curve_speed_mps`, `curve_exit_stable_frames` fields
- [ ] `sd_perception_config_t` has no `curve_error_threshold`, `curve_derivative_threshold`, `intersection_active_channels` fields

---

## Test Scenarios

### Test 1: Straight Line Tracking (P0 - Critical)

**Purpose**: Verify basic line following stability with new speed control.

| Item | Detail |
|------|--------|
| **Condition** | 1m long straight line, black line width 15mm |
| **Initial Position** | Car centered on line |
| **Speed Mode** | DEBUG (0.2 m/s) |
| **Duration** | Run for 10 seconds |

**Expected Behavior**:
- Stable tracking, no significant S-shaped oscillation
- lateral_error stays within +/-0.5
- speed_factor stays near 1.0 (small deviations)
- No speed oscillation (speed_factor changes should be < 10%)

**Pass Criteria**:
- [ ] Car stays on the line for the entire 10 seconds
- [ ] No sudden speed changes or jerks
- [ ] lateral_error mean < 0.3
- [ ] speed_factor not dropping below 0.9

---

### Test 2: Straight Line with Perturbation (P0 - Critical)

**Purpose**: Verify error recovery with the new speed control.

| Item | Detail |
|------|--------|
| **Condition** | Same straight line as Test 1 |
| **Initial Position** | Car offset by ~10mm to one side |
| **Speed Mode** | DEBUG (0.2 m/s) |

**Expected Behavior**:
- Car automatically corrects position
- Returns to centered tracking within ~2 seconds
- speed_factor temporarily decreases during correction, then recovers

**Pass Criteria**:
- [ ] Car converges to center within 2 seconds
- [ ] After convergence, lateral_error < 0.2
- [ ] No overshoot beyond 5mm on the opposite side
- [ ] speed_factor returns to > 0.95 after convergence

---

### Test 3: Gentle Curve Tracking (P1 - Important)

**Purpose**: Verify curve following with error-based speed reduction.

| Item | Detail |
|------|--------|
| **Condition** | R = 500mm arc |
| **Speed Mode** | SLOW (0.5 m/s) |
| **Initial Position** | Car in straight approach before curve |

**Expected Behavior**:
- Smooth transition into curve
- Speed naturally reduces as lateral_error increases
- Car does not run off the black line

**Pass Criteria**:
- [ ] Car stays on the line throughout the curve (at least 1 of 8 sensors sees black)
- [ ] speed_factor drop correlates with curve entry (expected: ~0.6-0.8)
- [ ] Speed recovery after curve exit is smooth (no sudden jumps)
- [ ] No loss of line detection

---

### Test 4: Sharp Curve Tracking (P1 - Important)

**Purpose**: Verify maximum curvature handling without dedicated curve state.

| Item | Detail |
|------|--------|
| **Condition** | R = 300mm arc (sharpest turn on track) |
| **Speed Mode** | DEBUG (0.2 m/s) |
| **Initial Position** | Car in straight approach before curve |

**Expected Behavior**:
- Steering responds in time
- Speed drops to compensate for large lateral error
- Car stays on the line

**Pass Criteria**:
- [ ] Car stays on the line throughout the curve
- [ ] speed_factor approaches 0.4 (maximum speed reduction) at peak error
- [ ] No line loss
- [ ] Steering is sufficient without needing dedicated curve state

**If this test FAILS**: Consider:
1. Increase PD gains (Kp, Kd) for stronger steering
2. Increase `speed_error_gain` to 0.5 for more aggressive slowdown
3. As fallback, consider implementing curvature-based speed control (Option A2)

---

### Test 5: Straight-Curve-Straight Transition (P2 - Optional)

**Purpose**: Verify smooth behavior across transitions.

| Item | Detail |
|------|--------|
| **Condition** | Straight -> R=500mm curve -> straight |
| **Speed Mode** | SLOW (0.5 m/s) |

**Expected Behavior**:
- Full speed on straights (~0.5 m/s)
- Gradual speed reduction entering curve
- Gradual speed recovery exiting curve
- No abrupt speed jumps

**Pass Criteria**:
- [ ] Car stays on the line through the entire transition
- [ ] Speed change throughout the run < 30% peak-to-peak
- [ ] No oscillation at transition points
- [ ] Speed transitions are smooth (no visible jerks)

---

### Test 6: Speed Response Verification (P2 - Optional)

**Purpose**: Verify the speed control formula through serial output.

| Item | Detail |
|------|--------|
| **Condition** | Slow manual offset of the car, observe serial output |
| **Speed Mode** | DEBUG (0.2 m/s) |

**Expected Behavior**:
- As car is manually moved off-center, speed_factor decreases continuously
- As car is restored to center, speed_factor recovers
- No discontinuous jumps in speed_factor

**Pass Criteria**:
- [ ] lateral_error increases -> speed_factor decreases monotonically
- [ ] Speed response is continuous (no discrete steps)
- [ ] speed_factor is clamped to [0.4, 1.0] at extremes

---

## Rollback Plan

### Rollback Conditions
Roll back to v1.2.1 if ANY of the following occur:
1. Test 1 or Test 2 fails (P0 criteria)
2. Test 4 fails and cannot be fixed by PD gain tuning within 30 minutes
3. Speed oscillations > 20% amplitude on straight lines
4. Car repeatedly loses the line where it previously tracked successfully

### Rollback Steps
```bash
# Method 1: Git revert (recommended)
git revert <v1.3.0-commit-hash>

# Method 2: Checkout previous version
git checkout v1.2.1 -- modules/Sens-Decision/
git checkout v1.2.1 -- Core/Src/app/speed_mode.c
git checkout v1.2.1 -- Core/Src/app/ir_calibration.c
```

### Rollback Verification
After rollback:
- [ ] Re-compile (must pass with 0 errors)
- [ ] Run straight line test (must pass as before)
- [ ] Original curve/approach speed settings restored in speed modes

---

## Summary

| Test | Priority | Status | Notes |
|------|----------|--------|-------|
| Pre-flight checks | P0 | [ ] | |
| Test 1: Straight line | P0 | [ ] | |
| Test 2: Perturbation recovery | P0 | [ ] | |
| Test 3: Gentle curve | P1 | [ ] | |
| Test 4: Sharp curve | P1 | [ ] | |
| Test 5: Transition | P2 | [ ] | |
| Test 6: Speed response | P2 | [ ] | |

# Oscillation and Premature Stop Fix - 2026-07-30

## Executive Summary

修复了两个关键问题：
1. **直线振荡**：小车在直线上持续前后摆动（已降低PD增益60-70%但仍振荡）
2. **中途停止**：小车无法完整跑完一圈，中途意外停止

## Problem Analysis

### Problem 1: Oscillation on Straight Lines

**Root Cause: kd Gain Dimensional Mismatch**

#### Lateral Error Analysis
- **Unit**: cm (centimeters)
- **Range**: -3.99 to +3.99 cm (based on IR sensor weights)
- **Typical values**: -4 to +4 cm when line is at edge sensors

#### Heading Error Analysis  
- **Unit**: cm/s (centimeters per second)
- **Calculation**: Time derivative of lateral_error
- **Magnitude Problem**:
  ```
  If lateral_error jumps 2cm in one frame (dt=0.02s):
  heading_error = 2cm / 0.02s = 100 cm/s
  Even with filtering (alpha=0.3), heading_error can reach 30-70 cm/s
  ```

#### Omega Calculation Problem
```c
omega = -(kp × lateral_error + kd × heading_error)
      = -(0.5 × 2cm + 0.3 × 50cm/s)
      = -(1 + 15) = -16 rad/s  (clamped to 2 rad/s)
```

**Issue**: 
- kd term dominates (15x larger than kp term)
- heading_error (cm/s) has wrong dimensional scale compared to lateral_error (cm)
- Should be: `omega = -(kp × lat_err + kd × head_err × time_constant)`
- Or simply: reduce kd by 10-20x

**Physical Consequence**:
- Vehicle overcorrects → lateral_error rapidly changes → huge heading_error → excessive omega → overshoot → oscillation

### Problem 2: Premature Stop

**Root Causes Identified**:

1. **line_lost_fault threshold too low**
   - Current: 10 frames (200ms at 50Hz)
   - During oscillation, perception may fail intermittently
   - Accumulated line_lost_count triggers FAULT state

2. **A-line false detection**
   - Detection condition: distance > 5.5m AND active_count >= 6
   - Track perimeter: ~6.14m
   - If vehicle oscillates severely, multiple sensors activate simultaneously
   - Triggers premature A-line detection before completing the lap

3. **No fault tolerance for perception failures**
   - No retry mechanism when perception_update fails
   - Single sustained failure cascade leads to FAULT state

## Solution Implementation

### Fix 1: Reduce kd Gains (Primary Fix)

**Straight segments**:
```c
kd_straight: 0.3 → 0.02  (reduced by 15x)
```

**Curve segments**:
```c
kd_curve: 0.6 → 0.04  (reduced by 15x)
```

**Approach phase**:
```c
kd_approach: 0.5 → 0.03  (reduced by 17x)
```

**Task 4**:
```c
kd_task4: 0.2 → 0.01  (reduced by 20x)
```

**Rationale**:
- heading_error (cm/s) is ~100x larger than lateral_error (cm) during transients
- Reducing kd by 15-20x brings the two terms into balance
- kp term now dominates for steady-state correction
- kd term provides gentle damping without overshoot

### Fix 2: Increase Fault Tolerance

**Line lost thresholds**:
```c
line_lost_fault_lap: 10 → 25  (200ms → 500ms)
line_lost_fault_ab:  5 → 15   (100ms → 300ms)
```

**Rationale**:
- Allows vehicle to recover from brief perception failures
- 500ms tolerance is reasonable for oscillation recovery
- Prevents premature FAULT state during aggressive maneuvering

### Fix 3: Stricter A-Line Detection

**A-line detection parameters**:
```c
transverse_min_ch: 6 → 7     (require one more sensor)
a_detect_min_dist: 5.5 → 5.8 (detect later, closer to actual lap end)
```

**Rationale**:
- Requires 7/8 sensors active (87.5% coverage) vs 6/8 (75%)
- Reduces false positives from oscillation-induced multi-sensor activation
- Later detection (5.8m vs 5.5m) ensures vehicle is actually near lap end

### Fix 4: Enhanced Debug Logging

**Added diagnostic outputs**:

1. **Periodic state logging** (every 500ms):
   ```c
   printf("[PG] State=%d, Dist=%.2f, LineValid=%d, LineLost=%d\n", ...);
   ```

2. **Control value logging** (every 500ms during RUN state):
   ```c
   printf("[PG] lat_err=%.2f, head_err=%.2f, omega=%.2f, v_cmd=%.2f\n", ...);
   ```

3. **State transition logging**:
   ```c
   printf("[PG] IDLE->TASK2_RUN (line detected)\n");
   printf("[PG] TASK2_RUN->FAULT (line_lost=%d > %d)\n", ...);
   printf("[PG] A-line detected! active=%d, dist=%.3f\n", ...);
   ```

4. **Perception failure logging** (every 10th failure):
   ```c
   printf("[PG] perception failed: status=%d\n", status);
   ```

## Files Modified

### Core/Src/app/playground_track.c

**Lines modified**:
- Line 108: Added `g_debug_counter` static variable
- Lines 293-319: Updated PD gains (reduced kd by 15-20x)
- Lines 327-337: Stricter A-line detection + increased fault thresholds
- Lines 340-348: Enhanced config print with kd values
- Lines 355-387: Added debug logging in pg_decide_50hz()
- Lines 397-410: Added state transition logging in PT_IDLE
- Lines 414-422: Enhanced FAULT transition logging
- Lines 458-467: Added A-line detection debug output
- Lines 469-476: Added control value logging
- Lines 477-574: Updated state transition messages throughout

**Total changes**: ~20 locations, primarily gain adjustments and debug logging

## Expected Behavior After Fix

### Oscillation Fix

**Before**:
- Vehicle oscillates with period ~0.5-1s
- Large heading_error causes overcorrection
- Oscillation persists even at low speed

**After**:
- Smooth exponential convergence to line center
- Small overshoot (< 1cm) with quick settling
- heading_error provides gentle damping without dominating
- Control output dominated by proportional term (kp)

**Typical control values** (expected):
```
lat_err=2.0cm, head_err=30cm/s, omega = -(0.5×2 + 0.02×30) = -1.6 rad/s
```
Now the two terms are balanced: kp_term=1.0, kd_term=0.6

### Premature Stop Fix

**Before**:
- Vehicle stops at random distances (2-5m)
- Fault triggered by brief line loss
- A-line detection may trigger early

**After**:
- Vehicle completes full lap (~6.14m)
- 500ms tolerance for line recovery
- A-line detection only near actual lap end (>5.8m, 7+ sensors)

### Debug Output Analysis

**Healthy run example**:
```
[PG] State=1, Dist=0.50, LineValid=3, LineLost=0
[PG] lat_err=1.2, head_err=15.3, omega=-0.9, v_cmd=0.50
[PG] State=1, Dist=1.00, LineValid=3, LineLost=0
[PG] lat_err=0.5, head_err=8.2, omega=-0.4, v_cmd=0.50
...
[PG] A-line detected! active=7, dist=6.100
[PG] TASK2_RUN->APPROACH_A
[PG] *** Task 2 Complete! Final dist=6.140m ***
```

**Problematic run indicators**:
```
[PG] perception failed: status=1    ← Sensor problem
[PG] lat_err=3.5, head_err=120.5    ← Severe oscillation (if still present)
[PG] State=1, Dist=2.50, LineValid=0, LineLost=18  ← Approaching fault threshold
[PG] TASK2_RUN->FAULT (line_lost=26 > 25)  ← Fault triggered
```

## Validation Checklist

### P0 - Critical Tests

- [ ] **Compile test**: Code compiles without errors
- [ ] **Straight line test**: Place vehicle on 2m straight black line
  - Vehicle should track smoothly without oscillation
  - Maximum overshoot < 1cm
  - Settling time < 0.5s after disturbance
- [ ] **Full lap test**: Run complete Task 2 lap
  - Vehicle completes full lap (6.0-6.2m)
  - No premature FAULT state
  - A-line detection occurs only at end

### P1 - Performance Tests

- [ ] **Curve tracking**: Verify smooth curve navigation without oscillation
- [ ] **Speed test**: Test at various speeds (0.3, 0.5, 0.8 m/s)
- [ ] **Disturbance rejection**: Manually push vehicle sideways, observe recovery
- [ ] **Debug output review**: Analyze log for anomalies
  - heading_error magnitude < 100 cm/s
  - omega rarely hits saturation limits
  - line_lost_count stays < 5 during normal operation

### P2 - Edge Case Tests

- [ ] **Tape gap test**: Introduce 2cm gap in black line, verify recovery
- [ ] **Sharp corner test**: Navigate 90-degree turn
- [ ] **Lighting variation**: Test under different ambient light conditions

## Tuning Guide

If oscillation persists after this fix:

1. **Further reduce kd** (try 0.01 for straight, 0.02 for curve)
2. **Reduce kp slightly** (try kp_straight=0.3)
3. **Check heading_error values** in debug output:
   - Should be < 50 cm/s during normal operation
   - If > 100 cm/s, consider increasing heading_filter_alpha (0.3 → 0.5)
4. **Check lateral_error calibration**:
   - Manually move vehicle left/right
   - Verify lateral_error sign is correct (right → positive, left → negative)

If premature stops still occur:

1. **Increase line_lost_fault_lap** (try 35-50 frames = 700ms-1s)
2. **Check perception failure rate** in debug output
3. **Verify IR sensor calibration**:
   - white_reference values correct
   - black_strength_threshold appropriate
4. **A-line detection**: If still triggering early, increase transverse_min_ch to 8

## Technical Notes

### PD Control Dimensional Analysis

Correct form of PD controller for line following:
```
omega = -(kp × e_lateral + kd × e_heading × τ)
```

Where:
- `e_lateral`: lateral error [cm]
- `e_heading`: heading error [cm/s]
- `τ`: time constant [s] to match dimensions

Alternatively, kd can be interpreted as having implicit time dimension:
```
kd_effective = kd_raw × dt_equivalent
```

In our fix, we reduced kd_raw to achieve proper dimensional balance.

### Heading Error Filter

Current implementation uses exponential moving average:
```c
heading_error = α × heading_error_old + (1-α) × derivative
```
With α = 0.3, the effective time constant is ~3 samples (60ms).

If more aggressive filtering is needed, increase α (e.g., 0.5-0.7).

### State Machine Robustness

Added tolerance mechanisms:
- **Hysteresis**: Requires 3 consecutive valid frames to start (g_line_valid_count >= 3)
- **Fault tolerance**: Requires 25 consecutive lost frames to fault (g_line_lost_count > 25)
- **Detection confirmation**: Requires 7/8 sensors AND distance > 5.8m for A-line

These prevent spurious state transitions from single-frame glitches.

## References

- **Perception algorithm**: `modules/Sens-Decision/src/perception.c`
- **IR sensor weights**: `modules/Sens-Decision/src/config.c` (lines 78-80)
- **Motion control**: `modules/MotionControl/src/motion_control.c`
- **Track geometry**: `docs/superpowers/specs/2026-07-30-playground-track-design.md`

## Revision History

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-07-30 | 1.0 | Claude (调试专家) | Initial fix: kd reduction, fault tolerance, debug logging |

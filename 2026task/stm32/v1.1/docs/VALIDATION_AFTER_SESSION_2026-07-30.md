# Validation Checklist - After Session 2026-07-30

**Status**: Pending real-vehicle testing
**Version**: v1.2.1

---

## Step 1: Compile Firmware (P0)

**Task**: Compile the project with your build system.

- [ ] No compilation errors
- [ ] No linker errors
- [ ] Firmware binary (.bin/.hex) generated

---

## Step 2: Wheel Track Verification (P0)

**Method**: In-place rotation test (原地旋转法)

**Procedure**:
1. Place car on flat ground
2. Mark initial heading angle
3. Command car to rotate 10 full turns in place (3600 degrees)
4. Measure actual rotation angle
5. Calculate: `actual_track = 0.214m × (actual_angle / 3600°)`
6. Verify error < 5% (214 ± 10.7mm)

**Expected result**: Actual wheel track = 214mm ± 10.7mm

- [ ] Actual track measured: ___ mm
- [ ] Error: ___%

---

## Step 3: IR Weight Sign Verification (P0 - CRITICAL)

> ⚠️ This is the most important verification. It confirms the lateral_error sign convention.

**Method**: Manual push test

**Procedure**:
1. Place car on track with black line centered
2. Print `lateral_error` via UART in real-time at 50Hz
3. Manually push car to the **RIGHT** (black line appears in LEFT sensors, channels 0-3)
4. Observe `lateral_error` — **Expected**: > 0 (positive)
5. Manually push car to the **LEFT** (black line appears in RIGHT sensors, channels 4-7)
6. Observe `lateral_error` — **Expected**: < 0 (negative)

**Results**:
- [ ] Push right → lateral_error = ___ (expected: > 0)
- [ ] Push left → lateral_error = ___ (expected: < 0)

### If Signs Are WRONG

Flip all 8 `ir_weights` signs in `modules/Sens-Decision/src/config.c`:

**Current (correct if test passes)**:
```c
{3.9861f, 2.8472f, 1.7083f, 0.5694f, -0.5694f, -1.7083f, -2.8472f, -3.9861f}
```

**Change to (if test fails)**:
```c
{-3.9861f, -2.8472f, -1.7083f, -0.5694f, 0.5694f, 1.7083f, 2.8472f, 3.9861f}
```

---

## Step 4: Sensor Initialization Check (P0)

**Method**: Observe UART output during boot.

- [ ] All 4 sensors initialized successfully
- [ ] No "FATAL" error messages
- [ ] IR sensor frames received (88-125 Hz expected)
- [ ] Encoder counts incrementing with wheel rotation
- [ ] IMU data streaming (if enabled)

---

## Step 5: Low-Speed Tracking Test (P1)

**Mode**: `SPEED_MODE_DEBUG` (0.2 m/s straight, 0.15 m/s curve)

| # | Scenario | Speed (m/s) | Expected | Pass? |
|---|----------|-------------|----------|-------|
| 1 | Straight line (2m+) | 0.2 | Stable, lateral_error < 5mm | [ ] |
| 2 | Gentle curve (R > 0.5m) | 0.2 | Stable following | [ ] |
| 3 | Sharp curve (R ≈ 0.3m) | 0.15 | May need speed reduction | [ ] |
| 4 | S-curve | 0.2 | Smooth transition | [ ] |

**Observations**:
- [ ] Max lateral_error during straight line: ___ mm
- [ ] Steering oscillation? (Y/N): ___
- [ ] Track lost? (Y/N, at which curve): ___

---

## Step 6: Steering PID Tuning (P1)

**Issue**: Wheelbase increased 86.1% → same wheel speed diff produces only 53.7% of previous angular velocity.

**Initial adjustment**: Increase Kp by ~86%
```
Old Kp: ≈ 2.0
New Kp: ≈ 3.7  (2.0 × 1.86)
```

**Tuning procedure**:
1. Set Kp = 2.0 (baseline)
2. Test straight line tracking
3. Increase Kp by 0.5 increments until oscillation appears
4. Back off by 20%
5. Test curves at increasing speeds

| Iteration | Kp | Straight | Gentle Curve | Sharp Curve | Notes |
|-----------|-----|----------|-------------|-------------|-------|
| Baseline | 2.0 | | | | |
| Tune 1 | 2.5 | | | | |
| Tune 2 | 3.0 | | | | |
| Tune 3 | 3.5 | | | | |
| Tune 4 | 4.0 | | | | |
| Final | ___ | | | | |

---

## Step 7: Medium-Speed Progression (P2)

**Mode**: `SPEED_MODE_SLOW` (0.5 m/s straight, 0.3 m/s curve)

| # | Scenario | Speed (m/s) | Pass? |
|---|----------|-------------|-------|
| 1 | Straight line | 0.5 | [ ] |
| 2 | Gentle curve | 0.5 | [ ] |
| 3 | Sharp curve | 0.3 | [ ] |

---

## Step 8: EKF Covariance Recalibration (P2)

**Issue**: IR array moved forward 50.9mm, wheel track changed 86.1%.

**Method**:
1. Drive car in straight line at constant speed (0.3 m/s) for 3m+
2. Record encoder data and EKF state estimates via UART
3. Calculate innovation sequence statistics
4. Adjust Q (process noise) and R (observation noise) based on actual noise levels

- [ ] Data collected
- [ ] Q matrix updated
- [ ] R matrix updated

---

## Rollback Plan

If testing reveals major issues:

| Issue | Fix |
|-------|-----|
| IR weight sign wrong | Flip all 8 weights in config.c |
| Wheel track wrong | Revert WHEEL_BASE to 0.115f |
| Steering unstable | Revert PID Kp to v1.2.0 value |
| Full rollback | `git checkout` pre-session commit |

---

## Speed Mode Reference

| Mode | Straight | Approach | Curve | Use Case |
|------|----------|----------|-------|----------|
| DEBUG | 0.2 m/s | 0.18 m/s | 0.15 m/s | First-time testing, sensor verification |
| SLOW | 0.5 m/s | 0.4 m/s | 0.3 m/s | Routine debugging, PID tuning |
| NORMAL | 1.0 m/s | 0.7 m/s | 0.5 m/s | Normal operation |
| FAST | 1.5 m/s | 1.0 m/s | 0.8 m/s | Race mode |

---

## Test Results Log

| Date | Tester | Step | Result | Notes |
|------|--------|------|--------|-------|
| | | 1. Compile | | |
| | | 2. Wheel track | | |
| | | 3. IR sign | | |
| | | 4. Sensor init | | |
| | | 5. Low-speed | | |
| | | 6. PID tune | | |
| | | 7. Medium-speed | | |
| | | 8. EKF calib | | |

---

**Document created**: 2026-07-30
**Next review**: After first real-vehicle test

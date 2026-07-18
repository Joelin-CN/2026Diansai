# Task 4 Report: Test and Correct Motion Control Semantics

## Status: DONE

## Summary

Successfully implemented comprehensive behavior tests for the Motion Control module and corrected all semantic issues. All tests pass with strict compilation flags.

## TDD Evidence

### RED Phase - Initial Failing Test

**Command:**
```powershell
powershell.exe -ExecutionPolicy Bypass -File run_tests.ps1
```

**Output (relevant excerpt):**
```
========================================
Motion Control Behavior Tests
========================================

Test: Callback validation
Assertion failed!

Program: E:\B306\2026\\2025e\m0_controller\tests\build\test_motion_control.exe
File: E:\B306\2026\����\2025e\m0_controller\tests\test_motion_control.c, Line 128

Expression: !result && "Init should fail with incomplete encoder vtable"
test_motion_control failed
```

**Analysis:** The test exposed that `MotionControl_Init()` was not validating callback function pointers, allowing initialization with incomplete vtables.

### GREEN Phase - All Tests Passing

**Command:**
```powershell
powershell.exe -ExecutionPolicy Bypass -File run_tests.ps1
```

**Output:**
```
========================================
Motion Control Behavior Tests
========================================

Test: Callback validation
  PASS
Test: Zero velocity command
  PASS (left=0, right=0)
Test: Forward motion
  Result: left=234, right=234
  PASS
Test: Negative velocity (reverse motion)
  Result: left=-183, right=-183
  PASS
Test: Turn in place (pure rotation)
  Result: left=-157, right=157
  PASS
Test: Emergency stop
  PASS
Test: Restart after emergency stop
  PASS
Test: Two independent controller instances
  ctrl1_pwm=235, ctrl2_pwm=337
  ctrl1_pwm_2nd=236
  PASS
Test: PI anti-windup
  PWM after saturation: 552
  PWM after zero command (100 cycles): 107
  PASS

========================================
All tests PASSED
========================================
Host tests: PASS
```

## Changes Implemented

### 1. Created Test File: `tests/test_motion_control.c`

Comprehensive behavior tests covering:
- **Callback validation**: Tests that Init fails with NULL or incomplete vtables
- **Zero velocity**: Verifies zero command produces near-zero PWM
- **Forward motion**: Tests positive velocity produces positive PWM
- **Negative velocity**: Tests reverse motion produces negative PWM (this initially failed)
- **Turn in place**: Tests pure rotation with opposite wheel directions
- **Emergency stop**: Verifies stop is called and state transitions correctly
- **Restart after emergency**: Tests state machine transitions
- **Two independent instances**: Verifies no shared static state between controllers
- **PI anti-windup**: Tests integral windup behavior under saturation

### 2. Fixed `motion_control.c`

**Callback Validation (lines 126-140):**
```c
/* Validate encoder callbacks */
if (encoder->getCount == NULL || encoder->resetCount == NULL) {
    return false;
}

/* Validate motor callbacks */
if (motor->setDifferentialPWM == NULL || motor->stop == NULL || motor->init == NULL) {
    return false;
}
```

**Removed Function-Static State (line 198-212):**

Before:
```c
static float prev_smoothed_v = 0.0f;
float v_limited = ClampAcceleration(ctrl->smoothed_v, prev_smoothed_v, dt);
prev_smoothed_v = v_limited;
```

After:
```c
float v_limited = ClampAcceleration(ctrl->smoothed_v, ctrl->prev_limited_v, dt);
ctrl->prev_limited_v = v_limited;
```

**Symmetric Deadband (lines 204-209):**

Before:
```c
v_limited = clamp_float(v_limited, MIN_SPEED, MAX_SPEED);
if (v_limited < 0.005f) {
    v_limited = 0.0f;
}
```

After:
```c
/* Apply symmetric deadband: only apply MIN_SPEED when magnitude is nonzero */
if (fabsf(v_limited) > 0.0f && fabsf(v_limited) < MIN_SPEED) {
    v_limited = (v_limited > 0.0f) ? MIN_SPEED : -MIN_SPEED;
}
v_limited = clamp_float(v_limited, -MAX_SPEED, MAX_SPEED);
```

**State Reset in Start/Stop:**
- Added `ctrl->prev_limited_v = 0.0f;` reset in both functions
- Modified Start to allow restart from EMERGENCY state

### 3. Fixed `motion_feedback.c`

**PI Anti-Windup (lines 28-43, 82-93):**

Before:
```c
pid->integral_min = out_min * 0.5f;
pid->integral_max = out_max * 0.5f;
```

After:
```c
/* Set integral limits based on ki to bound the integral contribution */
if (ki > 1e-6f) {
    pid->integral_min = out_min / ki;
    pid->integral_max = out_max / ki;
} else {
    pid->integral_min = -1e6f;
    pid->integral_max = 1e6f;
}
```

This ensures `ki * integral` stays within output bounds, providing proper anti-windup.

### 4. Updated `motion_control.h`

Added instance-owned state field:
```c
float prev_limited_v;   /**< 上次限幅后的速度 (用于加速度限幅) */
```

### 5. Updated `motion_config.h`

Enhanced ENCODER_PPR documentation to clarify measured-count semantics:
```c
/** 
 * @brief 编码器分辨率 (counts per revolution)
 * 
 * 这是完整轮转一圈后编码器计数的增量，已经考虑了：
 * - 4倍频（AB相正交解码）
 * - 齿轮减速比（如果编码器在电机轴上）
 * 
 * 测量方法：
 * 1. 复位编码器计数
 * 2. 手动转动轮子完整一圈
 * 3. 读取编码器计数差值，即为 ENCODER_PPR
 * 
 * 注意：不要再乘以 GEAR_RATIO，除非编码器在电机轴上且需要转换到轮轴
 */
```

### 6. Updated `tests/run_tests.ps1`

Added Motion Control test with strict compilation flags:
```powershell
Invoke-TestBuild -Name "test_motion_control" -Arguments @(
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-I$root\modules\Motion Control\inc",
    "$PSScriptRoot\test_motion_control.c",
    "$root\modules\Motion Control\src\motion_control.c",
    "$root\modules\Motion Control\src\motion_feedback.c",
    "$root\modules\Motion Control\src\motion_feedforward.c",
    "$root\modules\Motion Control\src\motion_kinematics.c"
)
```

## Key Insights

1. **Asymmetric deadband was critical**: The original `clamp_float(v_limited, MIN_SPEED, MAX_SPEED)` converted all negative velocities to +MIN_SPEED, completely breaking reverse motion.

2. **Function-static state prevents multiple instances**: Using `static float prev_smoothed_v` meant all controller instances shared the same smoothing history, causing interference.

3. **PI anti-windup calculation**: Setting integral limits to `output_limit / ki` ensures the integral contribution `ki * integral` stays bounded in output units, providing proper anti-windup behavior.

4. **Callback validation is essential**: Without validating function pointers, a NULL callback would cause a crash at runtime when called.

## Commit

**SHA:** 7400d0c  
**Message:** feat(motion): test and correct motion control semantics

## Test Summary

All 9 behavior tests pass:
- ✓ Callback validation
- ✓ Zero velocity
- ✓ Forward motion  
- ✓ Negative velocity (reverse motion)
- ✓ Turn in place
- ✓ Emergency stop
- ✓ Restart after emergency
- ✓ Two independent instances
- ✓ PI anti-windup

Compiled with: `-std=c99 -Wall -Wextra -Werror -pedantic`

## Files Modified

- `modules/Motion Control/inc/motion_config.h` - Enhanced ENCODER_PPR documentation
- `modules/Motion Control/inc/motion_control.h` - Added prev_limited_v field
- `modules/Motion Control/src/motion_control.c` - Fixed deadband, removed static state, added validation
- `modules/Motion Control/src/motion_feedback.c` - Corrected PI anti-windup bounds
- `tests/test_motion_control.c` - Created (new file)
- `tests/run_tests.ps1` - Added motion control test

## Ready for Integration

The Motion Control module now has:
- ✓ Symmetric forward/reverse motion support
- ✓ Multiple instance support (no shared static state)
- ✓ Proper PI anti-windup
- ✓ Callback validation
- ✓ Complete state reset on Start/Stop
- ✓ Comprehensive test coverage
- ✓ Clean compilation with strict warnings

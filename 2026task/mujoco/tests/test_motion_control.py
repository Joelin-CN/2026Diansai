"""
Unit tests for motion control module.
Validates FF+PI controller behavior.
"""

import pytest
import numpy as np
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from firmware_bridge.motion_control import (
    MotionControlState,
    motion_control_update,
    WHEEL_BASE,
    WHEEL_RADIUS,
    PWM_MAX,
    MIN_SPEED,
)


def test_motion_control_forward():
    """Test forward motion (v > 0, omega = 0)."""
    state = MotionControlState()

    v_cmd = 0.5  # m/s
    omega_cmd = 0.0
    v_left_actual = 0.0
    v_right_actual = 0.0
    dt = 0.002

    pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
        state, v_cmd, omega_cmd, v_left_actual, v_right_actual, dt
    )

    # Both wheels should get same positive PWM
    assert pwm_l > 0
    assert pwm_r > 0
    assert abs(pwm_l - pwm_r) < 10, "Left/right should be similar for straight motion"


def test_motion_control_rotation():
    """Test pure rotation (v = 0, omega > 0)."""
    state = MotionControlState()

    v_cmd = 0.0
    omega_cmd = 2.0  # rad/s, turning left
    v_left_actual = 0.0
    v_right_actual = 0.0
    dt = 0.002

    pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
        state, v_cmd, omega_cmd, v_left_actual, v_right_actual, dt
    )

    # Left wheel should be negative, right positive (differential drive)
    assert pwm_l < 0, "Left wheel should reverse for CCW rotation"
    assert pwm_r > 0, "Right wheel should forward for CCW rotation"


def test_motion_control_stop():
    """Test stop command (v = 0, omega = 0)."""
    state = MotionControlState()

    v_cmd = 0.0
    omega_cmd = 0.0
    v_left_actual = 0.0
    v_right_actual = 0.0
    dt = 0.002

    pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
        state, v_cmd, omega_cmd, v_left_actual, v_right_actual, dt
    )

    # Both PWM should be zero
    assert pwm_l == 0
    assert pwm_r == 0


def test_motion_control_speed_tracking():
    """Test PI controller tracks target speed over time."""
    state = MotionControlState()

    v_cmd = 0.5
    omega_cmd = 0.0
    dt = 0.002

    # Simulate closed-loop: actual speed gradually approaches target
    v_actual = 0.0
    pwm_history = []

    for step in range(100):
        pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
            state, v_cmd, omega_cmd, v_actual, v_actual, dt
        )

        pwm_history.append(pwm_l)

        # Simple first-order plant model for test
        v_actual += (pwm_l / 1000.0) * 0.5 * dt  # Crude velocity response

    # After 100 steps, error should decrease
    # (Not a perfect test, but validates PI accumulation)
    assert len(pwm_history) == 100
    # Initial PWM should be higher (large error)
    # Final PWM should be lower (small error + steady-state)
    assert pwm_history[0] > pwm_history[-1]


def test_motion_control_integral_windup_protection():
    """Test integral anti-windup."""
    state = MotionControlState()

    v_cmd = 1.0  # High target
    omega_cmd = 0.0
    v_actual = 0.0  # Stuck at zero (saturated actuator scenario)
    dt = 0.002

    # Run for many steps with persistent error
    for _ in range(500):
        pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
            state, v_cmd, omega_cmd, v_actual, v_actual, dt
        )

    # Integrator should be clamped, not infinite
    from firmware_bridge.motion_control import INTEGRAL_MAX
    assert abs(state.left_ctrl.integrator) <= INTEGRAL_MAX


def test_motion_control_deadzone():
    """Test minimum speed deadzone."""
    state = MotionControlState()

    # Command below MIN_SPEED threshold
    v_cmd = MIN_SPEED * 0.5
    omega_cmd = 0.0
    v_left_actual = 0.0
    v_right_actual = 0.0
    dt = 0.002

    pwm_l, pwm_r, omega_l, omega_r = motion_control_update(
        state, v_cmd, omega_cmd, v_left_actual, v_right_actual, dt
    )

    # Should output zero (deadzone)
    assert pwm_l == 0
    assert pwm_r == 0
    # Integrator should be cleared
    assert state.left_ctrl.integrator == 0.0


if __name__ == '__main__':
    pytest.main([__file__, '-v'])

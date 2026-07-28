"""
Motion control module - Python port of motion_control.c
FF+PI wheel speed control with inverse kinematics.
"""

import numpy as np

# Physical parameters (from motion_config.h)
WHEEL_BASE = 0.150        # m, distance between left and right wheels
WHEEL_RADIUS = 0.033      # m

# PI controller parameters
SPEED_KP = 200.0
SPEED_KI = 50.0
SPEED_OUTPUT_MAX = 500.0  # PI output limit (PWM units)

# Feedforward parameters
FF_K_ACCEL = 50.0         # PWM per m/s²
FF_K_FRICTION = 300.0     # PWM per m/s
FF_K_STATIC = 80.0        # PWM (static friction breakaway)
FF_STATIC_DEADZONE = 0.01 # m/s, below this don't apply static friction

# PWM limits
PWM_MAX = 1000
PWM_MIN = -1000

# Integral limits
INTEGRAL_MAX = 10.0       # m/s * s

# Minimum speed (deadzone)
MIN_SPEED = 0.01          # m/s (reduced from 0.10 for gentler startup)


class WheelController:
    """Per-wheel PI controller with feedforward"""
    def __init__(self):
        self.integrator = 0.0
        self.prev_target = 0.0


class MotionControlState:
    """Motion control state for both wheels"""
    def __init__(self):
        self.left_ctrl = WheelController()
        self.right_ctrl = WheelController()


def motion_control_update(state: MotionControlState,
                           v_cmd: float,
                           omega_cmd: float,
                           v_left_actual: float,
                           v_right_actual: float,
                           dt: float) -> tuple:
    """
    Compute wheel PWM commands using FF+PI control.

    Args:
        state: MotionControlState
        v_cmd: Linear velocity command (m/s)
        omega_cmd: Angular velocity command (rad/s)
        v_left_actual: Measured left wheel speed (m/s)
        v_right_actual: Measured right wheel speed (m/s)
        dt: Control time step (seconds)

    Returns:
        Tuple of (pwm_left, pwm_right, omega_left, omega_right):
        - pwm_left, pwm_right: int, PWM values (-1000 to +1000)
        - omega_left, omega_right: float, target wheel angular velocities (rad/s)
    """
    # Inverse kinematics: convert (v, ω) to left/right wheel velocities
    v_left_target = v_cmd - (WHEEL_BASE / 2.0) * omega_cmd
    v_right_target = v_cmd + (WHEEL_BASE / 2.0) * omega_cmd

    pwm_left, omega_left = _control_single_wheel(
        state.left_ctrl, v_left_target, v_left_actual, dt)
    pwm_right, omega_right = _control_single_wheel(
        state.right_ctrl, v_right_target, v_right_actual, dt)

    return pwm_left, pwm_right, omega_left, omega_right


def _control_single_wheel(ctrl: WheelController,
                           v_target: float,
                           v_actual: float,
                           dt: float) -> tuple:
    """
    Control single wheel with FF+PI.

    Args:
        ctrl: WheelController state
        v_target: Target wheel velocity (m/s)
        v_actual: Measured wheel velocity (m/s)
        dt: Time step (s)

    Returns:
        Tuple of (pwm, omega_target):
        - pwm: int, PWM command
        - omega_target: float, target wheel angular velocity (rad/s)
    """
    # Minimum speed deadzone
    if abs(v_target) < MIN_SPEED:
        # Stop wheel, clear integrator
        ctrl.integrator = 0.0
        ctrl.prev_target = 0.0
        return 0, 0.0

    # Feedforward: compensate for acceleration and friction
    if dt > 1e-6:
        accel = (v_target - ctrl.prev_target) / dt
    else:
        accel = 0.0

    ff_accel = FF_K_ACCEL * accel
    ff_friction = FF_K_FRICTION * v_target

    # Static friction (only if speed above deadzone)
    if abs(v_target) > FF_STATIC_DEADZONE:
        ff_static = FF_K_STATIC * np.sign(v_target)
    else:
        ff_static = 0.0

    ff_total = ff_accel + ff_friction + ff_static

    # PI feedback
    error = v_target - v_actual

    # Integrator with anti-windup
    ctrl.integrator += error * dt
    ctrl.integrator = np.clip(ctrl.integrator, -INTEGRAL_MAX, INTEGRAL_MAX)

    fb = SPEED_KP * error + SPEED_KI * ctrl.integrator

    # Clamp feedback output
    fb = np.clip(fb, -SPEED_OUTPUT_MAX, SPEED_OUTPUT_MAX)

    # Total PWM
    pwm_raw = ff_total + fb
    pwm = int(np.clip(pwm_raw, PWM_MIN, PWM_MAX))

    # Update state
    ctrl.prev_target = v_target

    # Convert PWM to target wheel angular velocity for actuator
    # PWM to velocity mapping: full PWM (1000) -> MAX_SPEED / WHEEL_RADIUS
    omega_target = (pwm / float(PWM_MAX)) * (1.0 / WHEEL_RADIUS)

    return pwm, omega_target

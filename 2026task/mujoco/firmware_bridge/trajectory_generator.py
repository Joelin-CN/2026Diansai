"""
Trajectory generator - Python port of trajectory_generator.c
Converts perception errors to velocity commands using Pure Pursuit.
"""

import numpy as np

# Parameters from config.c and motion_config.h
LOOKAHEAD_DISTANCE = 0.25          # m, Pure Pursuit lookahead
MAX_SPEED = 1.0                    # m/s
MAX_ACCEL = 2.0                    # m/s²
MAX_DECEL = 3.0                    # m/s²
MAX_JERK = 5.0                     # m/s³
MAX_OMEGA = 6.0                    # rad/s

# Command smoothing (from motion_config.h)
CMD_SMOOTH_TAU = 0.05              # s, time constant
# Alpha calculated for 50Hz decision rate (dt=0.02s)
CMD_SMOOTH_ALPHA = 0.02 / (CMD_SMOOTH_TAU + 0.02)  # ≈ 0.3297


class TrajectoryState:
    """Internal state for trajectory generator"""
    def __init__(self):
        self.v_smooth = 0.0
        self.omega_smooth = 0.0
        self.v_prev = 0.0


def trajectory_generator_update(state: TrajectoryState,
                                 behavior_state,
                                 speed_limit: float,
                                 lateral_error: float,
                                 heading_error: float,
                                 dt_s: float) -> tuple:
    """
    Generate velocity commands from perception errors.

    Uses Pure Pursuit algorithm:
    - Curvature κ = 2 * lateral_error / L²
    - Angular velocity ω = v * κ
    - Applies jerk limiting and command smoothing

    Args:
        state: TrajectoryState object
        behavior_state: Current behavior state (for special handling)
        speed_limit: Maximum allowed speed (m/s)
        lateral_error: Lateral error from perception (-1 to +1, normalized)
        heading_error: Heading error (derivative of lateral)
        dt_s: Time step (seconds)

    Returns:
        Tuple of (v_cmd, omega_cmd) in m/s and rad/s
    """
    from firmware_bridge.behavior_planner import BehaviorState

    # Stop if in stopped/degraded state
    if behavior_state == BehaviorState.LINE_LOST_DEGRADED:
        state.v_smooth = 0.0
        state.omega_smooth = 0.0
        state.v_prev = 0.0
        return 0.0, 0.0

    if behavior_state == BehaviorState.STOPPED:
        state.v_smooth = 0.0
        state.omega_smooth = 0.0
        state.v_prev = 0.0
        return 0.0, 0.0

    # Pure Pursuit: compute curvature from lateral error
    L_sq = LOOKAHEAD_DISTANCE ** 2
    kappa = 2.0 * lateral_error / L_sq if L_sq > 0 else 0.0

    # Target speed (apply speed limit)
    v_raw = min(speed_limit, MAX_SPEED)

    # Jerk limiting (rate limit on acceleration)
    if dt_s > 0:
        max_accel_step = MAX_ACCEL * dt_s
        max_decel_step = MAX_DECEL * dt_s

        # Limit acceleration
        v_raw = min(v_raw, state.v_prev + max_accel_step)
        # Limit deceleration
        v_raw = max(v_raw, state.v_prev - max_decel_step)

    # Target angular velocity
    omega_raw = v_raw * kappa

    # Clamp omega to max
    omega_raw = np.clip(omega_raw, -MAX_OMEGA, MAX_OMEGA)

    # Command smoothing (first-order low-pass filter)
    alpha = CMD_SMOOTH_ALPHA
    v_cmd = alpha * v_raw + (1.0 - alpha) * state.v_smooth
    omega_cmd = alpha * omega_raw + (1.0 - alpha) * state.omega_smooth

    # Update state
    state.v_smooth = v_cmd
    state.omega_smooth = omega_cmd
    state.v_prev = v_cmd

    return v_cmd, omega_cmd

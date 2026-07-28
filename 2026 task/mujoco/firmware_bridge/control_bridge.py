"""
Control bridge - unified interface combining all firmware modules.
"""

import numpy as np
from firmware_bridge.perception import PerceptionState, perception_update
from firmware_bridge.behavior_planner import BehaviorPlannerState, behavior_planner_update
from firmware_bridge.trajectory_generator import TrajectoryState, trajectory_generator_update
from firmware_bridge.motion_control import MotionControlState, motion_control_update


class ControlBridge:
    """
    Unified control bridge combining all firmware modules.

    Implements the full control pipeline:
    Sensors → Perception → Behavior → Trajectory → Motion Control → Actuators
    """

    def __init__(self):
        """Initialize all control module states."""
        self.perception_state = PerceptionState()
        self.behavior_state = BehaviorPlannerState()
        self.trajectory_state = TrajectoryState()
        self.motion_state = MotionControlState()

        self.enabled = True
        self.step_count = 0

    def sens_decision_update(self,
                              ir_raw: np.ndarray,
                              timestamp_us: int,
                              dt_s: float) -> tuple:
        """
        Run Sens-Decision pipeline (50 Hz).

        Args:
            ir_raw: uint16[8] raw IR ADC values
            timestamp_us: Current timestamp (microseconds)
            dt_s: Time step (seconds)

        Returns:
            Tuple of (v_cmd, omega_cmd) in m/s and rad/s
        """
        # Convert IR raw to reflectance
        reflectance = 1.0 - ir_raw.astype(np.float32) / 4096.0
        reflectance = np.clip(reflectance, 0.0, 1.0)

        # Perception
        perception_result = perception_update(
            self.perception_state,
            reflectance,
            timestamp_us,
            dt_s
        )

        # Behavior planning
        behavior, speed_limit = behavior_planner_update(
            self.behavior_state,
            perception_result,
            self.enabled
        )

        # Trajectory generation
        v_cmd, omega_cmd = trajectory_generator_update(
            self.trajectory_state,
            behavior,
            speed_limit,
            perception_result['lateral_error'],
            perception_result['heading_error'],
            dt_s
        )

        return v_cmd, omega_cmd

    def motion_control_update(self,
                               v_cmd: float,
                               omega_cmd: float,
                               v_left_actual: float,
                               v_right_actual: float,
                               dt: float) -> tuple:
        """
        Run motion control (500 Hz).

        Args:
            v_cmd: Linear velocity command (m/s)
            omega_cmd: Angular velocity command (rad/s)
            v_left_actual: Measured left speed (m/s)
            v_right_actual: Measured right speed (m/s)
            dt: Control time step (s)

        Returns:
            Tuple of (pwm_left, pwm_right, omega_left, omega_right)
        """
        return motion_control_update(
            self.motion_state,
            v_cmd,
            omega_cmd,
            v_left_actual,
            v_right_actual,
            dt
        )

    def get_perception_result(self) -> dict:
        """Get latest perception result for logging."""
        return {
            'lateral_error': self.perception_state.prev_lateral_error,
            'heading_error': self.perception_state.heading_error,
            'line_valid': self.perception_state.lost_count == 0,
        }

    def get_behavior_state(self):
        """Get current behavior state."""
        return self.behavior_state.state

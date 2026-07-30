"""
Encoder simulation for MuJoCo differential drive robot.
Converts MuJoCo joint angles to firmware-compatible encoder counts.
"""

import numpy as np
import mujoco

# Encoder parameters (from motion_config.h)
ENCODER_PPR = 334  # Pulses per revolution at wheel shaft

# Encoder logical IDs (from motion_feedback.h: EncoderId_t)
# These map to firmware's logical encoder indices
ENCODER_LEFT_FRONT = 0
ENCODER_LEFT_REAR = 1
ENCODER_RIGHT_FRONT = 2
ENCODER_RIGHT_REAR = 3
ENCODER_COUNT = 4

# Joint names in MuJoCo model (physical joints)
JOINT_NAMES = {
    ENCODER_LEFT_FRONT: "jwheel_lf",
    ENCODER_LEFT_REAR: "jwheel_lr",
    ENCODER_RIGHT_FRONT: "jwheel_rf",
    ENCODER_RIGHT_REAR: "jwheel_rr",
}


class EncoderSim:
    """
    Encoder simulator: converts MuJoCo joint angles to encoder counts.

    Maintains cumulative count like real quadrature encoders.
    Output format matches firmware: int32[4] cumulative counts.
    """

    def __init__(self, model: mujoco.MjModel):
        """Initialize encoder simulator.

        Args:
            model: MuJoCo model containing wheel joints
        """
        # Precompute sensor IDs for fast lookup
        self.sensor_ids = []
        for enc_id in range(ENCODER_COUNT):
            joint_name = JOINT_NAMES[enc_id]
            # Find jointpos sensor for this joint
            sensor_name = f"enc_{joint_name[7:]}"  # "enc_lf", "enc_lr", etc.
            sensor_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SENSOR, sensor_name)
            if sensor_id < 0:
                raise ValueError(f"Encoder sensor '{sensor_name}' not found")
            self.sensor_ids.append(sensor_id)

        # State: previous angles and cumulative counts
        self.prev_angles = np.zeros(ENCODER_COUNT, dtype=np.float64)
        self.counts = np.zeros(ENCODER_COUNT, dtype=np.int32)

    def update(self, data: mujoco.MjData) -> np.ndarray:
        """
        Update encoder counts based on current joint angles.

        Args:
            data: MuJoCo simulation data

        Returns:
            int32[4] array of cumulative encoder counts
        """
        # Read current joint angles (radians)
        angles = np.array([data.sensordata[sid] for sid in self.sensor_ids])

        # Calculate angular displacement since last update
        delta_rad = angles - self.prev_angles

        # Convert radians to encoder counts
        # counts = delta_rad / (2π) * PPR
        delta_counts = delta_rad / (2.0 * np.pi) * ENCODER_PPR

        # Round to integer counts and accumulate
        self.counts += np.round(delta_counts).astype(np.int32)

        # Update previous angles
        self.prev_angles = angles.copy()

        return self.counts.copy()

    def get_wheel_speeds(self, data: mujoco.MjData, dt: float) -> tuple:
        """
        Calculate wheel speeds from joint velocities.

        Args:
            data: MuJoCo simulation data
            dt: Time step (seconds)

        Returns:
            Tuple of (v_left, v_right) in m/s
        """
        # Get joint velocity sensor IDs
        # MuJoCo stores velocities in qvel, indexed by joint DOF
        # For hinge joints, we can read directly from joint velocity
        import mujoco

        # Get joint IDs and read velocities
        joint_names = ["jwheel_lf", "jwheel_lr", "jwheel_rf", "jwheel_rr"]
        omega = np.zeros(4)

        for i, name in enumerate(joint_names):
            joint_id = mujoco.mj_name2id(data.model, mujoco.mjtObj.mjOBJ_JOINT, name)
            qvel_adr = data.model.jnt_dofadr[joint_id]
            omega[i] = data.qvel[qvel_adr]

        # Convert to linear velocities (m/s)
        # v = omega * radius
        from firmware_bridge.motion_control import WHEEL_RADIUS
        wheel_speeds = omega * WHEEL_RADIUS

        # Average left and right sides
        v_left = (wheel_speeds[ENCODER_LEFT_FRONT] + wheel_speeds[ENCODER_LEFT_REAR]) / 2.0
        v_right = (wheel_speeds[ENCODER_RIGHT_FRONT] + wheel_speeds[ENCODER_RIGHT_REAR]) / 2.0

        return v_left, v_right

    def reset(self):
        """Reset encoder counts to zero."""
        self.counts.fill(0)
        self.prev_angles.fill(0.0)

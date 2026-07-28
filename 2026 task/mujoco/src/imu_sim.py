"""
IMU simulation for MuJoCo robot.
Simulates ICM42688P IMU with proper coordinate frame transformation.
"""

import numpy as np
import mujoco

# IMU sensor parameters (ICM42688P, ±16g / ±2000dps)
ACCEL_SCALE_LSB_PER_G = 2048.0       # LSB/g for ±16g range, 16-bit
GYRO_SCALE_LSB_PER_DPS = 16.4        # LSB/dps for ±2000dps range, 16-bit

# Noise parameters
ACCEL_NOISE_SIGMA = 3  # LSB
GYRO_NOISE_SIGMA = 2   # LSB


class ImuSim:
    """
    IMU simulator: converts MuJoCo frame velocities to IMU raw data.

    IMPORTANT: MuJoCo framelinvel/frameangvel are in WORLD frame.
    Real ICM42688P outputs BODY frame data.
    We transform world → body using chassis rotation matrix.

    Output format matches firmware:
    - accel_raw: int16[3] in LSB (±16g range)
    - gyro_raw: int16[3] in LSB (±2000dps range)
    """

    def __init__(self, model: mujoco.MjModel):
        """Initialize IMU simulator.

        Args:
            model: MuJoCo model
        """
        # Find sensor IDs
        self.linvel_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SENSOR, "imu_linvel")
        self.angvel_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SENSOR, "imu_angvel")
        self.chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")

        if self.linvel_id < 0 or self.angvel_id < 0 or self.chassis_id < 0:
            raise ValueError("IMU sensors or chassis body not found in model")

        # Previous linear velocity (body frame) for acceleration calculation
        self.prev_linvel_body = np.zeros(3, dtype=np.float64)

    def read(self, data: mujoco.MjData, dt: float) -> tuple:
        """
        Read IMU data and convert to firmware format.

        Args:
            data: MuJoCo simulation data
            dt: Time step since last read (seconds)

        Returns:
            Tuple of (accel_raw, gyro_raw):
            - accel_raw: int16[3] acceleration in LSB
            - gyro_raw: int16[3] angular velocity in LSB
        """
        # Get chassis rotation matrix (3x3)
        # data.xmat is row-major 3x3 stored as 9 consecutive floats
        R_world_to_body = data.xmat[self.chassis_id].reshape(3, 3).T
        # Note: xmat is world→body rotation, so R_world_to_body = R^T

        # Read linear and angular velocities (WORLD frame)
        linvel_world = data.sensordata[self.linvel_id:self.linvel_id+3].copy()
        angvel_world = data.sensordata[self.angvel_id:self.angvel_id+3].copy()

        # Transform to BODY frame
        linvel_body = R_world_to_body @ linvel_world
        angvel_body = R_world_to_body @ angvel_world

        # Calculate acceleration (body frame derivative)
        # Note: This is the body-frame acceleration, not accounting for gravity
        # Real IMU measures specific force (includes gravity), but for motion control
        # we primarily care about dynamic acceleration
        if dt > 1e-6:
            accel_body_mps2 = (linvel_body - self.prev_linvel_body) / dt
        else:
            accel_body_mps2 = np.zeros(3)

        # Convert to g's
        accel_g = accel_body_mps2 / 9.80665

        # Convert angular velocity to degrees/second
        gyro_dps = np.degrees(angvel_body)

        # Convert to raw LSB values
        accel_raw = np.round(accel_g * ACCEL_SCALE_LSB_PER_G).astype(np.int16)
        gyro_raw = np.round(gyro_dps * GYRO_SCALE_LSB_PER_DPS).astype(np.int16)

        # Add Gaussian noise
        accel_raw = accel_raw + np.random.normal(0, ACCEL_NOISE_SIGMA, 3).astype(np.int16)
        gyro_raw = gyro_raw + np.random.normal(0, GYRO_NOISE_SIGMA, 3).astype(np.int16)

        # Clip to int16 range
        accel_raw = np.clip(accel_raw, -32768, 32767).astype(np.int16)
        gyro_raw = np.clip(gyro_raw, -32768, 32767).astype(np.int16)

        # Update state
        self.prev_linvel_body = linvel_body.copy()

        return accel_raw, gyro_raw

    def reset(self):
        """Reset IMU state."""
        self.prev_linvel_body.fill(0.0)

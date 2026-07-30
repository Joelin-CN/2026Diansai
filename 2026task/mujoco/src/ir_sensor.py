"""
IR sensor simulation for MuJoCo line-following robot.
Simulates 8-channel IR reflectance array using geometric distance to track.
"""

import numpy as np
import mujoco
from src.track_generator import min_dist_to_track

# IR sensor parameters (matching firmware expectations)
HALF_WIDTH = 0.0125      # m, 25mm black tape half-width
SIGMOID_K = 300.0        # Sigmoid steepness (光斑扩散系数)
NOISE_SIGMA = 30         # ADC noise standard deviation (LSB)

# IR site names in MuJoCo model
IR_SITE_NAMES = [f"ir{i}" for i in range(8)]


class IrSensorSim:
    """
    Simulate 8-channel IR reflectance sensor array.

    Output format matches firmware: uint16[8] with range 0-4095
    - 0-2047: on black line (reflectance > 0.5)
    - 2048-4095: on white surface (reflectance < 0.5)
    """

    def __init__(self, model: mujoco.MjModel):
        """Initialize IR sensor simulator.

        Args:
            model: MuJoCo model containing IR sites
        """
        # Precompute site IDs for fast lookup
        self.site_ids = []
        for name in IR_SITE_NAMES:
            site_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SITE, name)
            if site_id < 0:
                raise ValueError(f"IR site '{name}' not found in model")
            self.site_ids.append(site_id)

    def _sigmoid_raw(self, dist: float) -> float:
        """
        Calculate expected IR raw value at distance from track centerline.

        Uses sigmoid model to simulate optical diffusion:
        - At track center (dist=0): raw ≈ 94 (reflectance ≈ 0.98)
        - At track edge (dist=12.5mm): raw = 2048 (reflectance = 0.5)
        - Off track (dist=20mm): raw ≈ 3710 (reflectance ≈ 0.09)

        Args:
            dist: Distance from track centerline (meters)

        Returns:
            Expected raw ADC value (0-4095)
        """
        x = SIGMOID_K * (dist - HALF_WIDTH)
        # Clip to prevent overflow in exp()
        x = np.clip(x, -30, 30)
        frac_white = 1.0 / (1.0 + np.exp(-x))
        return 4095.0 * frac_white

    def read(self, model: mujoco.MjModel, data: mujoco.MjData) -> np.ndarray:
        """
        Read IR sensor array.

        Args:
            model: MuJoCo model
            data: MuJoCo simulation data

        Returns:
            uint16[8] array of raw ADC values (0-4095)
        """
        raw = np.empty(8, dtype=np.uint16)

        for i, site_id in enumerate(self.site_ids):
            # Get sensor world position (X, Y) from site
            world_pos = data.site_xpos[site_id, :2]  # Only XY, ignore Z

            # Calculate distance to nearest track segment
            dist = min_dist_to_track(world_pos)

            # Convert distance to raw value with sigmoid response
            raw_value = self._sigmoid_raw(dist)

            # Add Gaussian noise
            raw_value += np.random.normal(0, NOISE_SIGMA)

            # Clip to ADC range
            raw[i] = int(np.clip(raw_value, 0, 4095))

        return raw

    def raw_to_reflectance(self, raw: np.ndarray) -> np.ndarray:
        """
        Convert raw ADC values to reflectance (0-1).

        Matches firmware formula: reflectance = 1.0 - raw/4096

        Args:
            raw: uint16[8] raw ADC values

        Returns:
            float[8] reflectance values (0=white, 1=black)
        """
        return 1.0 - raw.astype(np.float32) / 4096.0

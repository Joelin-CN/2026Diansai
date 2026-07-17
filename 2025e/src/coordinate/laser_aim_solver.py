"""Laser-to-target geometry and incremental two-axis gimbal commands."""

from __future__ import print_function

import math

import cv2
import numpy as np


class AimOffsets(object):
    """One incremental command for the encapsulated two-axis gimbal."""

    def __init__(self, yaw_deg, pitch_deg, target_camera, distance_m):
        self.yaw_deg = float(yaw_deg)
        self.pitch_deg = float(pitch_deg)
        self.target_camera = tuple(float(value) for value in target_camera)
        self.distance_m = float(distance_m)

    def as_tuple(self):
        return self.yaw_deg, self.pitch_deg


class LaserAimSolver(object):
    """Convert a detected A4 frame into yaw/pitch offset commands.

    ``laser_origin_camera`` is the laser emitter position relative to the
    camera optical centre in OpenCV camera coordinates: x right, y down,
    z forward. ``laser_axis_bias_deg`` is the laser axis direction relative
    to the camera axis: positive yaw points right, positive pitch points up.

    The black-frame corners must be ordered TL, TR, BR, BL. The solver uses
    ``solvePnP`` to estimate the target centre in 3D, so lateral laser/camera
    separation (parallax) is compensated instead of assuming coaxial mounting.
    """

    def __init__(
            self, camera_matrix, distortion_coefficients=None,
            target_width_m=0.297, target_height_m=0.210,
            laser_origin_camera=(0.0, 0.0, 0.0),
            laser_axis_bias_deg=(0.0, 0.0),
            max_yaw_step_deg=12.0, max_pitch_step_deg=8.0,
            deadband_deg=0.03):
        self.camera_matrix = np.asarray(camera_matrix, dtype=np.float64)
        if self.camera_matrix.shape != (3, 3):
            raise ValueError("camera_matrix must have shape (3, 3)")
        if distortion_coefficients is None:
            distortion_coefficients = np.zeros((1, 5), dtype=np.float64)
        self.distortion_coefficients = np.asarray(
            distortion_coefficients, dtype=np.float64
        )
        self.target_width_m = float(target_width_m)
        self.target_height_m = float(target_height_m)
        if self.target_width_m <= 0 or self.target_height_m <= 0:
            raise ValueError("target dimensions must be positive")

        self.laser_origin_camera = np.asarray(
            laser_origin_camera, dtype=np.float64
        ).reshape(3)
        self.laser_axis_bias_deg = np.asarray(
            laser_axis_bias_deg, dtype=np.float64
        ).reshape(2)
        self.max_yaw_step_deg = abs(float(max_yaw_step_deg))
        self.max_pitch_step_deg = abs(float(max_pitch_step_deg))
        self.deadband_deg = abs(float(deadband_deg))

        half_width = self.target_width_m / 2.0
        half_height = self.target_height_m / 2.0
        self.object_corners = np.array([
            (-half_width, -half_height, 0.0),
            (+half_width, -half_height, 0.0),
            (+half_width, +half_height, 0.0),
            (-half_width, +half_height, 0.0),
        ], dtype=np.float32)

    @classmethod
    def from_npz(cls, calibration_path, **kwargs):
        """Build a solver from the camera calibration file already in src."""
        with np.load(calibration_path, allow_pickle=False) as calibration:
            camera_matrix = calibration["camera_matrix"]
            distortion = calibration["distortion_coefficients"]
        return cls(camera_matrix, distortion, **kwargs)

    def solve_from_corners(self, corners):
        """Estimate target centre from four image corners and return offsets."""
        image_corners = np.asarray(corners, dtype=np.float32).reshape(4, 2)
        success, _rotation, translation = cv2.solvePnP(
            self.object_corners,
            image_corners,
            self.camera_matrix,
            self.distortion_coefficients,
            flags=cv2.SOLVEPNP_ITERATIVE,
        )
        if not success:
            return None
        target_camera = translation.reshape(3)
        if target_camera[2] <= 0:
            return None
        return self.solve_from_target_point(target_camera)

    def solve_from_target_point(self, target_camera):
        """Solve offsets when the target centre in camera coordinates is known."""
        target = np.asarray(target_camera, dtype=np.float64).reshape(3)
        direction = target - self.laser_origin_camera
        distance = float(np.linalg.norm(direction))
        if distance <= 1e-9 or direction[2] <= 0:
            return None

        yaw_deg = math.degrees(math.atan2(direction[0], direction[2]))
        pitch_deg = math.degrees(math.atan2(
            -direction[1], math.hypot(direction[0], direction[2])
        ))
        yaw_deg -= float(self.laser_axis_bias_deg[0])
        pitch_deg -= float(self.laser_axis_bias_deg[1])

        yaw_deg = self._apply_deadband_and_limit(
            yaw_deg, self.max_yaw_step_deg
        )
        pitch_deg = self._apply_deadband_and_limit(
            pitch_deg, self.max_pitch_step_deg
        )
        return AimOffsets(yaw_deg, pitch_deg, target, distance)

    def _apply_deadband_and_limit(self, value, limit):
        if abs(value) <= self.deadband_deg:
            return 0.0
        return max(-limit, min(limit, value))

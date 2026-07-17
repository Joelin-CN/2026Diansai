"""Tests for PnP target location and laser/gimbal offset calculation."""

import math
import os
import sys
import unittest

import cv2
import numpy as np


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from src.coordinate.laser_aim_solver import LaserAimSolver  # noqa: E402


CAMERA_MATRIX = np.array([
    [900.0, 0.0, 640.0],
    [0.0, 900.0, 360.0],
    [0.0, 0.0, 1.0],
], dtype=np.float64)


class LaserAimSolverTest(unittest.TestCase):
    def test_centered_coaxial_target_needs_no_offset(self):
        solver = LaserAimSolver(CAMERA_MATRIX)
        command = solver.solve_from_target_point((0.0, 0.0, 1.0))
        self.assertIsNotNone(command)
        self.assertEqual(command.as_tuple(), (0.0, 0.0))

    def test_compensates_lateral_laser_origin(self):
        solver = LaserAimSolver(
            CAMERA_MATRIX,
            laser_origin_camera=(0.03, 0.0, 0.0),
            max_yaw_step_deg=90,
        )
        command = solver.solve_from_target_point((0.0, 0.0, 1.0))
        self.assertIsNotNone(command)
        self.assertAlmostEqual(
            command.yaw_deg, math.degrees(math.atan2(-0.03, 1.0)), places=4
        )

    def test_recovers_target_from_projected_a4_corners(self):
        solver = LaserAimSolver(
            CAMERA_MATRIX, max_yaw_step_deg=90, max_pitch_step_deg=90
        )
        translation = np.array([[0.12], [-0.08], [1.35]], dtype=np.float64)
        projected, _ = cv2.projectPoints(
            solver.object_corners,
            np.zeros((3, 1)),
            translation,
            CAMERA_MATRIX,
            np.zeros((1, 5)),
        )
        command = solver.solve_from_corners(projected.reshape(4, 2))
        self.assertIsNotNone(command)
        self.assertTrue(np.allclose(command.target_camera, translation.ravel(), atol=1e-4))

    def test_limits_one_cycle_command(self):
        solver = LaserAimSolver(
            CAMERA_MATRIX, max_yaw_step_deg=5, max_pitch_step_deg=3
        )
        command = solver.solve_from_target_point((1.0, -1.0, 1.0))
        self.assertEqual(command.as_tuple(), (5.0, 3.0))


if __name__ == "__main__":
    unittest.main()

"""Unit tests for camera calibration loading and frame undistortion."""

import os
import sys
import tempfile
import unittest

import numpy as np


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from src.vision.camera_undistorter import CameraUndistorter  # noqa: E402


def write_calibration(path, image_size=(1280, 720)):
    np.savez(
        path,
        camera_matrix=np.array([
            [1080.0, 0.0, 640.0],
            [0.0, 1080.0, 360.0],
            [0.0, 0.0, 1.0],
        ]),
        distortion_coefficients=np.zeros((1, 5)),
        image_size=np.asarray(image_size, dtype=np.int32),
    )


class CameraUndistorterTest(unittest.TestCase):
    def test_prepares_map_once_and_preserves_frame_shape(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "camera.npz")
            write_calibration(path)
            undistorter = CameraUndistorter(path)
            frame = np.full((720, 1280, 3), 120, dtype=np.uint8)

            result = undistorter.undistort(frame)
            first_map = undistorter.map_x
            second_result = undistorter.undistort(frame)

            self.assertEqual(result.shape, frame.shape)
            self.assertEqual(second_result.shape, frame.shape)
            self.assertIs(first_map, undistorter.map_x)
            self.assertEqual(undistorter.frame_size, (1280, 720))

    def test_rejects_different_camera_resolution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "camera.npz")
            write_calibration(path)
            undistorter = CameraUndistorter(path)
            wrong_size = np.zeros((480, 640, 3), dtype=np.uint8)

            with self.assertRaises(ValueError):
                undistorter.undistort(wrong_size)


if __name__ == "__main__":
    unittest.main()

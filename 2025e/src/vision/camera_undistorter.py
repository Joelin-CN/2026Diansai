"""Camera intrinsic calibration loading and low-overhead frame undistortion."""

from __future__ import print_function

import os

import cv2
import numpy as np


class CameraUndistorter(object):
    """Apply a saved OpenCV calibration before the target detector runs.

    The remapping tables are created once on the first frame, then reused.
    This keeps the per-frame work to one ``cv2.remap`` call.
    """

    def __init__(self, calibration_path, alpha=0.0):
        path = os.path.abspath(os.path.expanduser(calibration_path))
        if not os.path.isfile(path):
            raise ValueError("Calibration file does not exist: %s" % path)
        if alpha < 0.0 or alpha > 1.0:
            raise ValueError("alpha must be between 0.0 and 1.0")

        try:
            with np.load(path, allow_pickle=False) as data:
                self.camera_matrix = np.asarray(
                    data["camera_matrix"], dtype=np.float64
                )
                self.distortion_coefficients = np.asarray(
                    data["distortion_coefficients"], dtype=np.float64
                )
                self.calibration_size = tuple(
                    int(value) for value in data["image_size"].ravel()[:2]
                )
        except (IOError, KeyError, ValueError) as error:
            raise ValueError("Invalid calibration file %s: %s" % (path, error))

        if self.camera_matrix.shape != (3, 3):
            raise ValueError("camera_matrix must have shape (3, 3)")
        if len(self.calibration_size) != 2 or min(self.calibration_size) <= 0:
            raise ValueError("image_size in calibration file is invalid")

        self.path = path
        self.alpha = float(alpha)
        self.map_x = None
        self.map_y = None
        self.new_camera_matrix = None
        self.valid_roi = None
        self.frame_size = None

    def _prepare_maps(self, frame_size):
        """Create fixed-point remap tables for one exact camera resolution."""
        if tuple(frame_size) != self.calibration_size:
            raise ValueError(
                "Calibration is for %dx%d, but received %dx%d. "
                "Use the calibrated camera resolution."
                % (
                    self.calibration_size[0], self.calibration_size[1],
                    frame_size[0], frame_size[1],
                )
            )

        new_matrix, roi = cv2.getOptimalNewCameraMatrix(
            self.camera_matrix,
            self.distortion_coefficients,
            frame_size,
            self.alpha,
            frame_size,
        )
        self.map_x, self.map_y = cv2.initUndistortRectifyMap(
            self.camera_matrix,
            self.distortion_coefficients,
            None,
            new_matrix,
            frame_size,
            cv2.CV_16SC2,
        )
        self.new_camera_matrix = new_matrix
        self.valid_roi = tuple(int(value) for value in roi)
        self.frame_size = tuple(frame_size)

    def undistort(self, frame):
        """Return an undistorted BGR frame with the same width and height."""
        if frame is None or frame.size == 0:
            return frame
        frame_size = (int(frame.shape[1]), int(frame.shape[0]))
        if self.frame_size != frame_size:
            self._prepare_maps(frame_size)
        return cv2.remap(
            frame,
            self.map_x,
            self.map_y,
            cv2.INTER_LINEAR,
            borderMode=cv2.BORDER_CONSTANT,
        )

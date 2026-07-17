"""Tests for target locking, loss handling, smoothing and pixel errors."""

import os
import sys
import unittest

import numpy as np


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from src.vision.black_frame_detector import BlackFrameDetection  # noqa: E402
from src.vision.target_tracker import TargetTracker  # noqa: E402


def make_detection(center_x, center_y):
    """生成正方形黑框检测结果，供状态机单元测试使用。"""
    corners = np.array([
        (center_x - 20, center_y - 20),
        (center_x + 20, center_y - 20),
        (center_x + 20, center_y + 20),
        (center_x - 20, center_y + 20),
    ], dtype=np.float32)
    return BlackFrameDetection(corners, 0.1)


class TargetTrackerTest(unittest.TestCase):
    def test_locks_after_required_consecutive_frames(self):
        tracker = TargetTracker(320, 240, lock_frames=3, smooth_alpha=1.0)
        first = tracker.update(make_detection(330, 250))
        second = tracker.update(make_detection(330, 250))
        third = tracker.update(make_detection(330, 250))

        self.assertEqual(first.state, TargetTracker.ACQUIRING)
        self.assertEqual(second.state, TargetTracker.ACQUIRING)
        self.assertEqual(third.state, TargetTracker.LOCKED)
        self.assertEqual(third.error, (10.0, 10.0))

    def test_lost_target_disables_old_error_immediately(self):
        tracker = TargetTracker(320, 240, lock_frames=1, lost_frames=3)
        tracker.update(make_detection(330, 250))
        lost = tracker.update(None)

        self.assertEqual(lost.state, TargetTracker.LOST)
        self.assertIsNone(lost.error)
        self.assertFalse(lost.locked)

    def test_resets_to_search_after_loss_limit(self):
        tracker = TargetTracker(320, 240, lock_frames=1, lost_frames=2)
        tracker.update(make_detection(320, 240))
        tracker.update(None)
        result = tracker.update(None)

        self.assertEqual(result.state, TargetTracker.SEARCH)
        self.assertIsNone(result.center)

    def test_smooths_center_before_calculating_error(self):
        tracker = TargetTracker(0, 0, lock_frames=1, smooth_alpha=0.25)
        tracker.update(make_detection(100, 100))
        result = tracker.update(make_detection(140, 140))

        self.assertEqual(result.error, (110.0, 110.0))

    def test_rejects_implausible_center_jump_after_lock(self):
        tracker = TargetTracker(320, 240, lock_frames=1, max_center_jump=80)
        tracker.update(make_detection(330, 250))
        result = tracker.update(make_detection(600, 450))

        self.assertEqual(result.state, TargetTracker.LOST)
        self.assertFalse(result.locked)
        self.assertIsNone(result.error)


if __name__ == "__main__":
    unittest.main()

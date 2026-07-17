"""Tests for search, lock, fire, loss handling and timeout."""

import os
import sys
import unittest


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from src.control.auto_aim_controller import AutoAimController  # noqa: E402


class Offsets(object):
    def __init__(self, yaw, pitch):
        self.yaw_deg = yaw
        self.pitch_deg = pitch


class AutoAimControllerTest(unittest.TestCase):
    def test_searches_and_returns_pitch_to_search_angle(self):
        controller = AutoAimController(4.0, lost_frames=1)
        command = controller.update(None, 0.1, current_pitch_deg=8.0)
        self.assertEqual(command.state, controller.SEARCH)
        self.assertAlmostEqual(command.yaw_deg, 18.0)
        self.assertAlmostEqual(command.pitch_deg, -8.0)

    def test_requires_consecutive_lock_frames_then_fires_once(self):
        controller = AutoAimController(2.0, lock_frames=3)
        first = controller.update(Offsets(0.05, -0.04), 0.02)
        second = controller.update(Offsets(0.03, -0.02), 0.02)
        third = controller.update(Offsets(0.02, 0.01), 0.02)
        fourth = controller.update(Offsets(0.0, 0.0), 0.02)
        self.assertEqual(first.state, controller.LOCK)
        self.assertEqual(second.lock_count, 2)
        self.assertTrue(third.fire)
        self.assertFalse(fourth.fire)

    def test_large_error_resets_lock_counter(self):
        controller = AutoAimController(2.0, lock_frames=2)
        controller.update(Offsets(0.02, 0.02), 0.02)
        command = controller.update(Offsets(1.0, 0.0), 0.02)
        self.assertEqual(command.state, controller.AIM)
        self.assertEqual(command.lock_count, 0)

    def test_short_detection_loss_does_not_start_scanning(self):
        controller = AutoAimController(2.0, lost_frames=2)
        controller.update(Offsets(1.0, 0.0), 0.02)
        command = controller.update(None, 0.02)
        self.assertEqual(command.state, controller.AIM)
        self.assertEqual(command.yaw_deg, 0.0)

    def test_timeout_never_fires(self):
        controller = AutoAimController(0.05)
        command = controller.update(None, 0.06)
        self.assertEqual(command.state, controller.TIMEOUT)
        self.assertFalse(command.fire)


if __name__ == "__main__":
    unittest.main()

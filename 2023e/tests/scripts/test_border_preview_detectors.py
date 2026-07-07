import sys
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = PROJECT_ROOT / "scripts" / "test"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from border_preview_detectors import detect_a4_tape_border  # noqa: E402


def test_a4_tape_detector_refines_inner_and_outer_quads_on_synthetic_ring():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    outer = np.array([[80, 50], [250, 60], [235, 190], [65, 180]], dtype=np.int32)
    inner = np.array([[105, 78], [222, 84], [213, 160], [92, 154]], dtype=np.int32)
    cv2.fillPoly(image, [outer], (20, 20, 20))
    cv2.fillPoly(image, [inner], (230, 230, 230))

    detection = detect_a4_tape_border(image)

    assert detection.found is True
    assert detection.outer_quad is not None
    assert detection.inner_quad is not None
    assert _max_corner_distance(detection.outer_quad, outer) <= 4.0
    assert _max_corner_distance(detection.inner_quad, inner) <= 4.0


def _max_corner_distance(actual_quad, expected_points) -> float:
    actual = actual_quad.reshape(4, 2).astype(np.float32)
    expected = expected_points.reshape(4, 2).astype(np.float32)
    actual_center = actual.mean(axis=0)
    expected_center = expected.mean(axis=0)
    actual = actual[np.argsort(np.arctan2(actual[:, 1] - actual_center[1], actual[:, 0] - actual_center[0]))]
    expected = expected[np.argsort(np.arctan2(expected[:, 1] - expected_center[1], expected[:, 0] - expected_center[0]))]
    return float(np.max(np.linalg.norm(actual - expected, axis=1)))

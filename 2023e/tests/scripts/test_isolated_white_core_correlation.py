import sys
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = PROJECT_ROOT / "scripts" / "test"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from isolated_white_core_correlation import _write_image, analyze_frame, classify_scores, sample_frame_indices  # noqa: E402


def test_classify_scores_labels_red_when_red_is_strong_and_separated():
    assert classify_scores(0.40, 0.05) == "red"


def test_classify_scores_labels_green_when_green_is_strong_and_separated():
    assert classify_scores(0.05, 0.40) == "green"


def test_classify_scores_labels_white_no_color_when_both_scores_are_low():
    assert classify_scores(0.02, 0.03) == "white/no-color"


def test_classify_scores_labels_uncertain_when_color_scores_are_close():
    assert classify_scores(0.22, 0.18) == "uncertain"


def test_analyze_frame_labels_white_core_with_red_ring_as_red():
    frame = np.zeros((100, 120, 3), dtype=np.uint8)
    cv2.circle(frame, (60, 50), 9, (0, 0, 180), -1)
    cv2.circle(frame, (60, 50), 4, (245, 245, 245), -1)

    candidates = analyze_frame(frame)

    assert candidates
    best = candidates[0]
    assert abs(best.center[0] - 60) <= 1.0
    assert abs(best.center[1] - 50) <= 1.0
    assert best.red_score > best.green_score
    assert best.label == "red"


def test_analyze_frame_labels_white_core_with_green_ring_as_green():
    frame = np.zeros((100, 120, 3), dtype=np.uint8)
    cv2.circle(frame, (65, 45), 9, (0, 180, 0), -1)
    cv2.circle(frame, (65, 45), 4, (245, 245, 245), -1)

    candidates = analyze_frame(frame)

    assert candidates
    best = candidates[0]
    assert abs(best.center[0] - 65) <= 1.0
    assert abs(best.center[1] - 45) <= 1.0
    assert best.green_score > best.red_score
    assert best.label == "green"


def test_analyze_frame_labels_pure_white_core_as_white_no_color():
    frame = np.zeros((100, 120, 3), dtype=np.uint8)
    cv2.circle(frame, (60, 50), 4, (245, 245, 245), -1)

    candidates = analyze_frame(frame)

    assert candidates
    best = candidates[0]
    assert best.red_score < 0.05
    assert best.green_score < 0.05
    assert best.label == "white/no-color"


def test_sample_frame_indices_are_after_start_and_reproducible():
    first = sample_frame_indices(total_frames=100, start_frame=41, sample_count=10, seed=7)
    second = sample_frame_indices(total_frames=100, start_frame=41, sample_count=10, seed=7)

    assert first == second
    assert len(first) == 10
    assert all(41 < frame_index <= 100 for frame_index in first)
    assert first == sorted(first)


def test_sample_frame_indices_tolerates_short_video():
    assert sample_frame_indices(total_frames=45, start_frame=41, sample_count=30, seed=7) == [42, 43, 44, 45]


def test_write_image_supports_unicode_paths(tmp_path):
    output = tmp_path / "电赛" / "frame_0042.png"

    assert _write_image(output, np.zeros((5, 5, 3), dtype=np.uint8))
    assert output.exists()

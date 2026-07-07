from dataclasses import replace
from pathlib import Path

import cv2
import numpy as np

from vision.config import load_vision_config
from vision.laser_detector import LaserDetector
from vision.types import DetectionSource, FailureReason, ImagePoint, ImageRoi, LaserColor, LaserDetection


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _red_params():
    return load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json").red_laser


def _green_params():
    return load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json").green_laser


def _blank(width=120, height=100, value=30):
    return np.full((height, width, 3), value, dtype=np.uint8)


def _draw_bgr_dot(center, radius, bgr, width=120, height=100, value=30):
    image = _blank(width, height, value)
    cv2.circle(image, center, radius, bgr, -1, lineType=cv2.LINE_AA)
    return image


def _draw_hsv_dot(center, radius, hsv_color, width=120, height=100):
    hsv = np.zeros((height, width, 3), dtype=np.uint8)
    hsv[:, :, 2] = 30
    cv2.circle(hsv, center, radius, hsv_color, -1, lineType=cv2.LINE_AA)
    return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)


def _draw_core_ring_dot(center, core_radius, ring_radius, ring_bgr, width=120, height=100, value=30):
    image = _blank(width, height, value)
    cv2.circle(image, center, ring_radius, ring_bgr, -1, lineType=cv2.LINE_AA)
    cv2.circle(image, center, core_radius, (255, 255, 255), -1, lineType=cv2.LINE_AA)
    return image


def assert_center_near(detection, expected, tolerance=1.5):
    assert detection.image_center is not None
    assert abs(detection.image_center.u_px - expected[0]) <= tolerance
    assert abs(detection.image_center.v_px - expected[1]) <= tolerance


def test_detects_synthetic_red_dot():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_bgr_dot((35, 42), 5, (0, 0, 255))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert detection.failure_reason is None
    assert_center_near(detection, (35, 42))
    assert detection.brightness >= _red_params().value_min
    assert detection.confidence >= _red_params().confidence_min


def test_detects_synthetic_green_dot():
    detector = LaserDetector(LaserColor.GREEN, _green_params())
    image = _draw_bgr_dot((80, 22), 6, (0, 255, 0))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert_center_near(detection, (80, 22))


def test_red_detector_supports_low_and_high_hue_ranges():
    detector = LaserDetector(LaserColor.RED, _red_params())

    low_hue = detector.detect(_draw_hsv_dot((30, 30), 5, (0, 255, 255)))
    high_hue = detector.detect(_draw_hsv_dot((90, 70), 5, (179, 255, 255)))

    assert low_hue.found is True
    assert_center_near(low_hue, (30, 30))
    assert high_hue.found is True
    assert_center_near(high_hue, (90, 70))


def test_laser_detectors_do_not_cross_detect_other_color():
    red_detector = LaserDetector(LaserColor.RED, _red_params())
    green_detector = LaserDetector(LaserColor.GREEN, _green_params())

    red_image = _draw_bgr_dot((40, 40), 5, (0, 0, 255))
    green_image = _draw_bgr_dot((40, 40), 5, (0, 255, 0))

    assert red_detector.detect(green_image).found is False
    assert red_detector.detect(green_image).failure_reason == FailureReason.NOT_FOUND
    assert green_detector.detect(red_image).found is False
    assert green_detector.detect(red_image).failure_reason == FailureReason.NOT_FOUND


def test_roi_detection_returns_full_frame_coordinates():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_bgr_dot((82, 74), 5, (0, 0, 255))

    detection = detector.detect(image, roi=ImageRoi(60, 50, 40, 40))

    assert detection.found is True
    assert_center_near(detection, (82, 74))
    assert detection.diagnostics["roi_used"] == ImageRoi(60, 50, 40, 40)


def test_target_outside_roi_is_not_found():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_bgr_dot((82, 74), 5, (0, 0, 255))

    detection = detector.detect(image, roi=ImageRoi(0, 0, 30, 30))
    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND


def test_blank_frame_reports_not_found():
    detector = LaserDetector(LaserColor.RED, _red_params())

    detection = detector.detect(_blank())

    assert detection.found is False
    assert detection.source == DetectionSource.LOST
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.image_center is None


def test_tiny_candidate_reports_too_small():
    params = replace(_red_params(), area_px_min=20)
    detector = LaserDetector(LaserColor.RED, params)
    image = _draw_bgr_dot((50, 50), 2, (0, 0, 255))

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.TOO_SMALL
    assert detection.image_center is None


def test_huge_candidate_reports_too_large():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_bgr_dot((60, 50), 35, (0, 0, 255))

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.TOO_LARGE
    assert detection.image_center is None


def test_low_brightness_candidate_reports_low_brightness():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_hsv_dot((60, 50), 5, (0, 255, 80))

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.LOW_BRIGHTNESS
    assert detection.image_center is None


def test_dim_same_hue_tail_does_not_pull_measured_center():
    detector = LaserDetector(LaserColor.RED, _red_params())
    hsv = np.zeros((100, 120, 3), dtype=np.uint8)
    hsv[:, :, 2] = 30
    cv2.rectangle(hsv, (20, 45), (70, 55), (0, 255, 80), -1)
    cv2.circle(hsv, (75, 50), 5, (0, 255, 255), -1, lineType=cv2.LINE_AA)
    image = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    detection = detector.detect(image)

    assert detection.found is True
    assert_center_near(detection, (75, 50), tolerance=2.0)


def test_fully_clipped_roi_returns_lost_instead_of_crashing():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_bgr_dot((50, 50), 5, (0, 0, 255))

    detection = detector.detect(image, roi=ImageRoi(200, 200, 20, 20))

    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.image_center is None


def test_previous_detection_selects_temporally_consistent_candidate():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (35, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (95, 80), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.RED,
        ImagePoint(34, 44),
        image_radius_px=5,
        area_px=80,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    assert detection.found is True
    assert_center_near(detection, (35, 45))


def test_ambiguous_multiple_candidates_reports_multiple_candidates():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (35, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (95, 80), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.MULTIPLE_CANDIDATES
    assert detection.image_center is None


def test_multiple_candidate_diagnostics_include_scores_and_best_center():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (35, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (95, 80), 5, (0, 0, 220), -1, lineType=cv2.LINE_AA)

    detection = detector.detect(image)

    assert detection.diagnostics["candidate_count"] == 2
    assert detection.diagnostics["best_center"] is not None
    assert detection.diagnostics["best_score"] is not None
    assert detection.diagnostics["second_best_score"] is not None
    assert len(detection.diagnostics["candidate_scores"]) == 2
    assert detection.diagnostics["candidate_scores"][0]["score"] >= detection.diagnostics["candidate_scores"][1]["score"]


def test_lost_detection_includes_failure_reason_in_diagnostics():
    detector = LaserDetector(LaserColor.RED, _red_params())

    detection = detector.detect(_blank())

    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.diagnostics["failure_reason"] == FailureReason.NOT_FOUND.value


def test_detects_red_ring_with_white_core_at_core_center():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_core_ring_dot((55, 48), core_radius=3, ring_radius=7, ring_bgr=(0, 0, 255))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert_center_near(detection, (55, 48), tolerance=1.5)
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True
    assert detection.diagnostics["candidate_scores"][0]["bright_core_area_px"] > 0


def test_detects_green_ring_with_white_core_at_core_center():
    detector = LaserDetector(LaserColor.GREEN, _green_params())
    image = _draw_core_ring_dot((70, 52), core_radius=3, ring_radius=7, ring_bgr=(0, 255, 0))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert_center_near(detection, (70, 52), tolerance=1.5)
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True


def test_small_red_ring_on_dark_background_is_rescued_by_white_core():
    params = replace(_red_params(), area_px_min=70)
    detector = LaserDetector(LaserColor.RED, params)
    image = _draw_core_ring_dot((62, 50), core_radius=3, ring_radius=4, ring_bgr=(0, 0, 255), value=5)

    detection = detector.detect(image)

    assert detection.found is True
    assert_center_near(detection, (62, 50), tolerance=1.5)
    assert detection.area_px is not None
    assert detection.area_px >= params.area_px_min
    assert detection.diagnostics["candidate_scores"][0]["color_area_px"] < params.area_px_min
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True


def test_pure_white_bright_point_without_color_ring_is_rejected():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (60, 50), 5, (255, 255, 255), -1, lineType=cv2.LINE_AA)

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.diagnostics["candidate_count"] == 0


def test_white_core_diagnostics_score_red_support_above_green():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank(value=5)
    cv2.circle(image, (60, 50), 6, (0, 0, 180), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (60, 50), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.RED,
        ImagePoint(59, 50),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert abs(best["center"][0] - 60) <= 2.0
    assert abs(best["center"][1] - 50) <= 2.0
    assert best["local_red_score"] > best["local_green_score"]
    assert best["local_red_score"] > 0.0


def test_white_core_diagnostics_score_green_support_above_red():
    detector = LaserDetector(LaserColor.GREEN, _green_params())
    image = _blank(value=5)
    cv2.circle(image, (65, 45), 6, (0, 180, 0), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (65, 45), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.GREEN,
        ImagePoint(66, 45),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert abs(best["center"][0] - 65) <= 2.0
    assert abs(best["center"][1] - 45) <= 2.0
    assert best["local_green_score"] > best["local_red_score"]
    assert best["local_green_score"] > 0.0


def test_white_core_diagnostics_keep_pure_white_color_scores_low():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank(value=5)
    cv2.circle(image, (60, 50), 4, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.RED,
        ImagePoint(60, 50),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert best["local_red_score"] < 0.05
    assert best["local_green_score"] < 0.05

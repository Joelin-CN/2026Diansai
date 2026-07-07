import cv2
import numpy as np

from vision.quad_utils import contour_to_refined_quad, quad_to_image_quad
from vision.tape_quad_detector import TapeQuadDetector
from vision.types import DetectionSource, FailureReason


def test_contour_to_refined_quad_recovers_skewed_quad():
    contour = np.array([[[80, 50]], [[250, 60]], [[235, 190]], [[65, 180]]], dtype=np.int32)

    quad = contour_to_refined_quad(contour)

    assert quad is not None
    assert _max_corner_distance(quad, contour.reshape(4, 2)) <= 4.0


def test_quad_to_image_quad_preserves_four_image_points():
    quad = np.array([[[80, 50]], [[250, 60]], [[235, 190]], [[65, 180]]], dtype=np.int32)

    image_quad = quad_to_image_quad(quad)

    assert len(image_quad.points) == 4
    assert image_quad.points[0].u_px == 80
    assert image_quad.points[0].v_px == 50


def test_tape_quad_detector_finds_outer_and_inner_quads():
    image, outer, inner = _make_a4_tape_ring()

    detection = TapeQuadDetector().detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert detection.outer_quad is not None
    assert detection.inner_quad is not None
    assert detection.tape_ratio is not None
    assert 0.08 <= detection.tape_ratio <= 0.65
    assert _max_image_quad_distance(detection.outer_quad, outer) <= 5.0
    assert _max_image_quad_distance(detection.inner_quad, inner) <= 5.0


def test_tape_quad_detector_requires_inner_contour():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    outer = np.array([[80, 50], [250, 60], [235, 190], [65, 180]], dtype=np.int32)
    cv2.fillPoly(image, [outer], (20, 20, 20))

    detection = TapeQuadDetector().detect(image)

    assert detection.found is False
    assert detection.source == DetectionSource.LOST
    assert detection.outer_quad is None
    assert detection.inner_quad is None
    assert detection.failure_reason == FailureReason.NOT_FOUND


def _make_a4_tape_ring():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    outer = np.array([[80, 50], [250, 60], [235, 190], [65, 180]], dtype=np.int32)
    inner = np.array([[105, 78], [222, 84], [213, 160], [92, 154]], dtype=np.int32)
    cv2.fillPoly(image, [outer], (20, 20, 20))
    cv2.fillPoly(image, [inner], (230, 230, 230))
    return image, outer, inner


def _max_image_quad_distance(image_quad, expected_points) -> float:
    actual = np.array([[point.u_px, point.v_px] for point in image_quad.points], dtype=np.float32)
    return _max_corner_distance(actual, expected_points)


def _max_corner_distance(actual_points, expected_points) -> float:
    actual = np.asarray(actual_points).reshape(4, 2).astype(np.float32)
    expected = np.asarray(expected_points).reshape(4, 2).astype(np.float32)
    actual_center = actual.mean(axis=0)
    expected_center = expected.mean(axis=0)
    actual = actual[np.argsort(np.arctan2(actual[:, 1] - actual_center[1], actual[:, 0] - actual_center[0]))]
    expected = expected[np.argsort(np.arctan2(expected[:, 1] - expected_center[1], expected[:, 0] - expected_center[0]))]
    return float(np.max(np.linalg.norm(actual - expected, axis=1)))

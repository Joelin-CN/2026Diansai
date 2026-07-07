import cv2
import numpy as np

from vision.screen_detector import ScreenDetector
from vision.tape_quad_detector import TapeQuadDetector
from vision.types import DetectionSource


def test_screen_detector_finds_gray_thin_square():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    points = np.array([[70, 45], [250, 45], [250, 205], [70, 205]], dtype=np.int32)
    cv2.polylines(image, [points], isClosed=True, color=(120, 120, 120), thickness=2)

    detection = ScreenDetector().detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert detection.image_corners is not None
    assert len(detection.image_corners) == 4


def test_screen_detector_does_not_accept_excluded_a4_tape_quad():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    outer = np.array([[80, 50], [250, 60], [235, 190], [65, 180]], dtype=np.int32)
    inner = np.array([[105, 78], [222, 84], [213, 160], [92, 154]], dtype=np.int32)
    cv2.fillPoly(image, [outer], (20, 20, 20))
    cv2.fillPoly(image, [inner], (230, 230, 230))
    tape = TapeQuadDetector().detect(image)

    detection = ScreenDetector().detect(image, exclude_quad=tape.outer_quad)

    assert detection.found is False

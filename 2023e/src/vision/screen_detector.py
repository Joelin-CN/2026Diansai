from __future__ import annotations

import cv2
import numpy as np

from vision.quad_utils import contour_to_refined_quad, image_quad_to_numpy, quad_overlap_ratio, quad_to_image_quad, quad_touches_frame_edge
from vision.types import DetectionSource, FailureReason, ImageQuad, ImagePoint, ScreenDetection


class ScreenDetector:
    def __init__(self, min_area_ratio: float = 0.03, max_area_ratio: float = 0.6):
        self.min_area_ratio = min_area_ratio
        self.max_area_ratio = max_area_ratio

    def detect(self, image: np.ndarray, exclude_quad: ImageQuad | None = None) -> ScreenDetection:
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        enhanced = clahe.apply(gray)
        edges = cv2.Canny(cv2.GaussianBlur(enhanced, (5, 5), 0), 40, 120)
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        edges = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, kernel, iterations=1)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        diagnostics = {"candidate_count": 0, "best_score": None, "failure_reason": None}
        if not contours:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return self._lost(diagnostics)

        exclude_np = image_quad_to_numpy(exclude_quad) if exclude_quad is not None else None
        frame_area = image.shape[0] * image.shape[1]
        candidates: list[tuple[float, np.ndarray, float]] = []
        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area < frame_area * self.min_area_ratio or area > frame_area * self.max_area_ratio:
                continue
            quad = contour_to_refined_quad(contour)
            if quad is None or quad_touches_frame_edge(quad, image.shape, margin=8):
                continue
            if exclude_np is not None and quad_overlap_ratio(quad, exclude_np) > 0.85:
                continue
            score = self._score(quad, area, image.shape)
            candidates.append((score, quad, area))

        diagnostics["candidate_count"] = len(candidates)
        if not candidates:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return self._lost(diagnostics)

        score, quad, area = max(candidates, key=lambda item: item[0])
        diagnostics["best_score"] = score
        image_quad = quad_to_image_quad(quad)
        return ScreenDetection(
            found=True,
            image_corners=list(image_quad.points),
            image_roi=None,
            confidence=score,
            source=DetectionSource.MEASURED,
            failure_reason=None,
            diagnostics=diagnostics,
        )

    def _lost(self, diagnostics: dict) -> ScreenDetection:
        return ScreenDetection(
            found=False,
            image_corners=None,
            image_roi=None,
            confidence=0.0,
            source=DetectionSource.LOST,
            failure_reason=FailureReason.NOT_FOUND,
            diagnostics=diagnostics,
        )

    def _score(self, quad: np.ndarray, area: float, shape: tuple[int, ...]) -> float:
        frame_area = shape[0] * shape[1]
        area_score = 1.0 - min(1.0, abs((area / frame_area) - 0.25) / 0.25)
        center_score = self._center_score(quad, shape)
        square_score = self._square_like_score(quad)
        return 0.35 * area_score + 0.35 * center_score + 0.30 * square_score

    def _center_score(self, quad: np.ndarray, shape: tuple[int, ...]) -> float:
        height, width = shape[:2]
        center = quad.reshape(-1, 2).mean(axis=0)
        distance = np.linalg.norm(center - np.array([width / 2.0, height / 2.0]))
        max_distance = np.linalg.norm(np.array([width / 2.0, height / 2.0]))
        return float(max(0.0, 1.0 - distance / max_distance))

    def _square_like_score(self, quad: np.ndarray) -> float:
        pts = quad.reshape(4, 2).astype(np.float32)
        lengths = [float(np.linalg.norm(pts[(i + 1) % 4] - pts[i])) for i in range(4)]
        longest = max(lengths)
        shortest = min(lengths)
        if longest == 0:
            return 0.0
        return shortest / longest

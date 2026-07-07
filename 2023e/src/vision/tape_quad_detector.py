from __future__ import annotations

import cv2
import numpy as np

from vision.quad_utils import contour_to_refined_quad, quad_inside_quad, quad_to_image_quad, quad_touches_frame_edge
from vision.types import FailureReason, ImageRoi, TapeQuadDetection


class TapeQuadDetector:
    def __init__(
        self,
        min_area_ratio: float = 0.005,
        max_area_ratio: float = 0.45,
        min_tape_ratio: float = 0.08,
        max_tape_ratio: float = 0.65,
    ):
        self.min_area_ratio = min_area_ratio
        self.max_area_ratio = max_area_ratio
        self.min_tape_ratio = min_tape_ratio
        self.max_tape_ratio = max_tape_ratio

    def detect(self, image: np.ndarray, roi: ImageRoi | None = None) -> TapeQuadDetection:
        crop, offset_x, offset_y = self._crop(image, roi)
        diagnostics = {
            "candidate_count": 0,
            "roi_used": roi,
            "failure_reason": None,
            "best_score": None,
        }
        if crop.size == 0:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return TapeQuadDetection.lost(FailureReason.NOT_FOUND, diagnostics)

        gray, mask = self._build_black_mask(crop)
        contours, hierarchy = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
        if hierarchy is None or not contours:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return TapeQuadDetection.lost(FailureReason.NOT_FOUND, diagnostics)

        frame_area = crop.shape[0] * crop.shape[1]
        candidates: list[tuple[float, np.ndarray, np.ndarray, float, float, float]] = []
        hierarchy_rows = hierarchy[0]
        for outer_index, outer in enumerate(contours):
            outer_area = float(cv2.contourArea(outer))
            if outer_area < frame_area * self.min_area_ratio or outer_area > frame_area * self.max_area_ratio:
                continue
            outer_quad = contour_to_refined_quad(outer)
            if outer_quad is None or quad_touches_frame_edge(outer_quad, crop.shape, margin=4):
                continue

            child_index = hierarchy_rows[outer_index][2]
            while child_index != -1:
                inner = contours[child_index]
                inner_area = float(cv2.contourArea(inner))
                child_index = hierarchy_rows[child_index][0]
                if inner_area < outer_area * 0.25 or inner_area > outer_area * 0.9:
                    continue
                inner_quad = contour_to_refined_quad(inner)
                if inner_quad is None or not quad_inside_quad(inner_quad, outer_quad):
                    continue
                if self._mean_inside_quad(gray, inner_quad) < 100.0:
                    continue
                tape_ratio = (outer_area - inner_area) / outer_area
                if tape_ratio < self.min_tape_ratio or tape_ratio > self.max_tape_ratio:
                    continue
                score = self._score(outer_area, inner_area, tape_ratio, outer_quad, crop.shape)
                candidates.append((score, outer_quad, inner_quad, outer_area, inner_area, tape_ratio))

        diagnostics["candidate_count"] = len(candidates)
        if not candidates:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return TapeQuadDetection.lost(FailureReason.NOT_FOUND, diagnostics)

        score, outer_quad, inner_quad, outer_area, inner_area, tape_ratio = max(candidates, key=lambda item: item[0])
        diagnostics["best_score"] = score
        outer_quad = self._offset_quad(outer_quad, offset_x, offset_y)
        inner_quad = self._offset_quad(inner_quad, offset_x, offset_y)
        return TapeQuadDetection.measured(
            outer_quad=quad_to_image_quad(outer_quad),
            inner_quad=quad_to_image_quad(inner_quad),
            contour_area_px=outer_area,
            inner_area_px=inner_area,
            tape_ratio=tape_ratio,
            confidence=score,
            diagnostics=diagnostics,
        )

    def _build_black_mask(self, image: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        mask = cv2.adaptiveThreshold(
            blurred,
            255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY_INV,
            41,
            8,
        )
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (7, 7))
        return gray, cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)

    def _mean_inside_quad(self, gray: np.ndarray, quad: np.ndarray) -> float:
        mask = np.zeros(gray.shape, dtype=np.uint8)
        cv2.fillPoly(mask, [quad.astype(np.int32)], 255)
        values = gray[mask > 0]
        return float(values.mean()) if values.size else 0.0

    def _score(self, outer_area: float, inner_area: float, tape_ratio: float, outer_quad: np.ndarray, shape: tuple[int, ...]) -> float:
        ratio_score = 1.0 - min(1.0, abs(tape_ratio - 0.3) / 0.3)
        nesting_score = min(1.0, inner_area / outer_area / 0.75)
        center_score = self._center_score(outer_quad, shape)
        return 0.45 * ratio_score + 0.30 * center_score + 0.25 * nesting_score

    def _center_score(self, quad: np.ndarray, shape: tuple[int, ...]) -> float:
        height, width = shape[:2]
        center = quad.reshape(-1, 2).mean(axis=0)
        distance = np.linalg.norm(center - np.array([width / 2.0, height / 2.0]))
        max_distance = np.linalg.norm(np.array([width / 2.0, height / 2.0]))
        return float(max(0.0, 1.0 - distance / max_distance))

    def _crop(self, image: np.ndarray, roi: ImageRoi | None) -> tuple[np.ndarray, int, int]:
        if roi is None:
            return image, 0, 0
        x1 = max(0, min(image.shape[1], roi.x_px))
        y1 = max(0, min(image.shape[0], roi.y_px))
        x2 = max(x1, min(image.shape[1], roi.x_px + roi.width_px))
        y2 = max(y1, min(image.shape[0], roi.y_px + roi.height_px))
        return image[y1:y2, x1:x2], x1, y1

    def _offset_quad(self, quad: np.ndarray, offset_x: int, offset_y: int) -> np.ndarray:
        offset = np.array([offset_x, offset_y], dtype=np.int32)
        return (quad.reshape(4, 2) + offset).reshape(-1, 1, 2).astype(np.int32)

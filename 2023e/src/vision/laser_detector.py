from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any

import cv2
import numpy as np

from vision.config import LaserDetectionParams
from vision.types import FailureReason, ImagePoint, ImageRoi, LaserColor, LaserDetection


@dataclass(frozen=True)
class _Candidate:
    center: ImagePoint
    radius_px: float
    area_px: float
    brightness: float
    circularity: float
    score: float
    color_area_px: float
    bright_core_area_px: float
    combined_area_px: float
    core_found: bool


@dataclass(frozen=True)
class _WhiteCoreCandidate:
    center: ImagePoint
    core_area_px: float
    brightness: float
    distance_to_previous: float | None
    local_red_score: float
    local_green_score: float
    rank_score: float


class LaserDetector:
    def __init__(self, color: LaserColor, params: LaserDetectionParams):
        self.color = color
        self.params = params
        self.multi_candidate_margin = 0.08

    def detect(
        self,
        image: np.ndarray,
        roi: ImageRoi | None = None,
        previous_detection: LaserDetection | None = None,
    ) -> LaserDetection:
        roi_used = self._clip_roi(roi, image.shape[1], image.shape[0])
        crop, offset_x, offset_y = self._crop(image, roi_used)
        diagnostics: dict[str, Any] = {
            "roi_used": roi_used,
            "candidate_count": 0,
            "rejected_candidates": [],
            "best_center": None,
            "best_score": None,
            "second_best_score": None,
            "candidate_scores": [],
            "failure_reason": None,
            "white_core_candidate_count": 0,
            "white_core_candidates": [],
        }
        if crop.size == 0:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return LaserDetection.lost(self.color, FailureReason.NOT_FOUND, diagnostics)

        mask, low_value_mask, bright_core_mask, hsv = self._build_mask(crop)
        white_core_candidates = self._analyze_white_core_candidates(
            bright_core_mask,
            hsv,
            offset_x,
            offset_y,
            previous_detection,
        )
        diagnostics["white_core_candidate_count"] = len(white_core_candidates)
        diagnostics["white_core_candidates"] = self._white_core_candidate_diagnostics(white_core_candidates)
        candidates, rejected = self._extract_candidates(mask, low_value_mask, bright_core_mask, hsv, offset_x, offset_y, previous_detection)
        diagnostics["candidate_count"] = len(candidates)
        diagnostics["rejected_candidates"] = rejected

        if not candidates:
            reason = self._dominant_rejection_reason(rejected)
            diagnostics["failure_reason"] = reason.value
            return LaserDetection.lost(self.color, reason, diagnostics)

        candidates.sort(key=lambda c: c.score, reverse=True)
        best = candidates[0]
        second = candidates[1] if len(candidates) > 1 else None
        diagnostics["best_center"] = (best.center.u_px, best.center.v_px)
        diagnostics["best_score"] = best.score
        diagnostics["second_best_score"] = second.score if second else None
        diagnostics["candidate_scores"] = self._candidate_diagnostics(candidates)

        if best.score < self.params.confidence_min:
            diagnostics["failure_reason"] = FailureReason.LOW_CONFIDENCE.value
            return LaserDetection.lost(self.color, FailureReason.LOW_CONFIDENCE, diagnostics)
        if second and best.score - second.score < self.multi_candidate_margin:
            distance = math.hypot(best.center.u_px - second.center.u_px, best.center.v_px - second.center.v_px)
            if distance > max(best.radius_px, second.radius_px) * 2.0:
                diagnostics["failure_reason"] = FailureReason.MULTIPLE_CANDIDATES.value
                return LaserDetection.lost(self.color, FailureReason.MULTIPLE_CANDIDATES, diagnostics)

        diagnostics["failure_reason"] = None
        return LaserDetection.measured(
            color=self.color,
            image_center=best.center,
            image_radius_px=best.radius_px,
            area_px=best.area_px,
            brightness=best.brightness,
            confidence=min(1.0, best.score),
            diagnostics=diagnostics,
        )

    def _build_mask(self, image: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
        low_value_mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
        for hue_range in self.params.hue_ranges_deg:
            low_h = max(0, min(179, int(hue_range.min)))
            high_h = max(0, min(179, int(hue_range.max)))
            lower = np.array([low_h, self.params.saturation_min, self.params.value_min], dtype=np.uint8)
            upper = np.array([high_h, 255, 255], dtype=np.uint8)
            mask = cv2.bitwise_or(mask, cv2.inRange(hsv, lower, upper))
            low_value_lower = np.array([low_h, self.params.saturation_min, 1], dtype=np.uint8)
            low_value_upper = np.array([high_h, 255, max(0, self.params.value_min - 1)], dtype=np.uint8)
            low_value_mask = cv2.bitwise_or(low_value_mask, cv2.inRange(hsv, low_value_lower, low_value_upper))

        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        low_value_mask = cv2.morphologyEx(low_value_mask, cv2.MORPH_OPEN, kernel)
        low_value_mask = cv2.morphologyEx(low_value_mask, cv2.MORPH_CLOSE, kernel)
        bright_core_lower = np.array([0, 0, max(self.params.value_min, 180)], dtype=np.uint8)
        bright_core_upper = np.array([179, 80, 255], dtype=np.uint8)
        bright_core_mask = cv2.inRange(hsv, bright_core_lower, bright_core_upper)
        bright_core_mask = cv2.morphologyEx(bright_core_mask, cv2.MORPH_OPEN, kernel)
        bright_core_mask = cv2.morphologyEx(bright_core_mask, cv2.MORPH_CLOSE, kernel)
        return mask, low_value_mask, bright_core_mask, hsv

    def _analyze_white_core_candidates(
        self,
        bright_core_mask: np.ndarray,
        hsv: np.ndarray,
        offset_x: int,
        offset_y: int,
        previous_detection: LaserDetection | None,
    ) -> list[_WhiteCoreCandidate]:
        contours, _ = cv2.findContours(bright_core_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[_WhiteCoreCandidate] = []
        value_channel = hsv[:, :, 2]
        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area < 2.0 or area > 300.0:
                continue
            contour_mask = np.zeros(bright_core_mask.shape, dtype=np.uint8)
            cv2.drawContours(contour_mask, [contour], -1, 255, -1)
            center = self._brightness_weighted_center(contour_mask, value_channel, offset_x, offset_y)
            brightness_values = value_channel[contour_mask > 0]
            brightness = float(brightness_values.max() if brightness_values.size else 0.0)
            local_red_score = self._local_color_score(hsv, center, offset_x, offset_y, LaserColor.RED)
            local_green_score = self._local_color_score(hsv, center, offset_x, offset_y, LaserColor.GREEN)
            distance = None
            temporal_score = 0.5
            if previous_detection and previous_detection.found and previous_detection.image_center:
                distance = math.hypot(center.u_px - previous_detection.image_center.u_px, center.v_px - previous_detection.image_center.v_px)
                temporal_score = max(0.0, 1.0 - distance / 100.0)
            color_score = local_red_score if self.color == LaserColor.RED else local_green_score
            brightness_score = min(1.0, brightness / 255.0)
            area_score = min(1.0, area / 30.0)
            rank_score = 0.35 * color_score + 0.30 * temporal_score + 0.20 * brightness_score + 0.15 * area_score
            candidates.append(
                _WhiteCoreCandidate(
                    center=center,
                    core_area_px=area,
                    brightness=brightness,
                    distance_to_previous=distance,
                    local_red_score=local_red_score,
                    local_green_score=local_green_score,
                    rank_score=rank_score,
                )
            )
        candidates.sort(key=lambda candidate: candidate.rank_score, reverse=True)
        return candidates

    def _local_color_score(
        self,
        hsv: np.ndarray,
        center: ImagePoint,
        offset_x: int,
        offset_y: int,
        color: LaserColor,
    ) -> float:
        radius = 9
        center_x = int(round(center.u_px - offset_x))
        center_y = int(round(center.v_px - offset_y))
        x1 = max(0, center_x - radius)
        y1 = max(0, center_y - radius)
        x2 = min(hsv.shape[1], center_x + radius + 1)
        y2 = min(hsv.shape[0], center_y + radius + 1)
        if x1 >= x2 or y1 >= y2:
            return 0.0

        patch = hsv[y1:y2, x1:x2]
        hue = patch[:, :, 0]
        saturation = patch[:, :, 1].astype(np.float32) / 255.0
        value = patch[:, :, 2].astype(np.float32) / 255.0
        if color == LaserColor.RED:
            hue_mask = (hue <= 10) | (hue >= 170)
        else:
            hue_mask = (hue >= 45) & (hue <= 85)

        yy, xx = np.indices(hue.shape)
        local_center_x = center_x - x1
        local_center_y = center_y - y1
        distance = np.hypot(xx - local_center_x, yy - local_center_y)
        spatial_weight = np.clip(1.0 - distance / float(radius + 1), 0.0, 1.0)
        weighted_support = hue_mask.astype(np.float32) * saturation * value * spatial_weight
        return float(min(1.0, weighted_support.sum() / 6.0))

    def _white_core_candidate_diagnostics(self, candidates: list[_WhiteCoreCandidate]) -> list[dict[str, Any]]:
        return [
            {
                "center": (candidate.center.u_px, candidate.center.v_px),
                "core_area_px": candidate.core_area_px,
                "brightness": candidate.brightness,
                "distance_to_previous": candidate.distance_to_previous,
                "local_red_score": candidate.local_red_score,
                "local_green_score": candidate.local_green_score,
                "rank_score": candidate.rank_score,
            }
            for candidate in candidates[:10]
        ]

    def _extract_candidates(
        self,
        mask: np.ndarray,
        low_value_mask: np.ndarray,
        bright_core_mask: np.ndarray,
        hsv: np.ndarray,
        offset_x: int,
        offset_y: int,
        previous_detection: LaserDetection | None,
    ) -> tuple[list[_Candidate], list[str]]:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[_Candidate] = []
        rejected: list[str] = []
        value_channel = hsv[:, :, 2]

        if not contours and cv2.countNonZero(low_value_mask) > 0:
            rejected.append(FailureReason.LOW_BRIGHTNESS.value)

        for contour in contours:
            color_area = float(cv2.contourArea(contour))
            contour_mask = np.zeros(mask.shape, dtype=np.uint8)
            cv2.drawContours(contour_mask, [contour], -1, 255, -1)
            core_mask = self._associated_core_mask(contour_mask, bright_core_mask)
            bright_core_area = float(cv2.countNonZero(core_mask))
            core_found = bright_core_area > 0.0
            combined_area = color_area + bright_core_area

            if combined_area < self.params.area_px_min:
                rejected.append(FailureReason.TOO_SMALL.value)
                continue
            if combined_area > self.params.area_px_max:
                rejected.append(FailureReason.TOO_LARGE.value)
                continue

            values = value_channel[contour_mask > 0]
            core_values = value_channel[core_mask > 0]
            brightness = float(max(values.max() if values.size else 0.0, core_values.max() if core_values.size else 0.0))
            if brightness < self.params.value_min:
                rejected.append(FailureReason.LOW_BRIGHTNESS.value)
                continue

            center = (
                self._brightness_weighted_center(core_mask, value_channel, offset_x, offset_y)
                if core_found
                else self._brightness_weighted_center(contour_mask, value_channel, offset_x, offset_y)
            )
            (_, _), radius = cv2.minEnclosingCircle(contour)
            perimeter = float(cv2.arcLength(contour, True))
            circularity = 0.0 if perimeter == 0 else min(1.0, 4.0 * math.pi * color_area / (perimeter * perimeter))
            score = self._score_candidate(combined_area, brightness, circularity, center, previous_detection, core_found)
            candidates.append(
                _Candidate(
                    center=center,
                    radius_px=float(max(radius, math.sqrt(combined_area / math.pi))),
                    area_px=combined_area,
                    brightness=brightness,
                    circularity=circularity,
                    score=score,
                    color_area_px=color_area,
                    bright_core_area_px=bright_core_area,
                    combined_area_px=combined_area,
                    core_found=core_found,
                )
            )

        return candidates, rejected

    def _associated_core_mask(self, contour_mask: np.ndarray, bright_core_mask: np.ndarray) -> np.ndarray:
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
        local_mask = cv2.dilate(contour_mask, kernel, iterations=1)
        return cv2.bitwise_and(bright_core_mask, local_mask)

    def _brightness_weighted_center(
        self,
        contour_mask: np.ndarray,
        value_channel: np.ndarray,
        offset_x: int,
        offset_y: int,
    ) -> ImagePoint:
        weights = value_channel.astype(np.float32) * (contour_mask > 0)
        total = float(weights.sum())
        if total > 0:
            ys, xs = np.indices(weights.shape)
            u = float((xs * weights).sum() / total) + offset_x
            v = float((ys * weights).sum() / total) + offset_y
            return ImagePoint(u, v)

        moments = cv2.moments(contour_mask)
        if moments["m00"]:
            return ImagePoint(float(moments["m10"] / moments["m00"]) + offset_x, float(moments["m01"] / moments["m00"]) + offset_y)
        return ImagePoint(float(offset_x), float(offset_y))

    def _score_candidate(
        self,
        area: float,
        brightness: float,
        circularity: float,
        center: ImagePoint,
        previous_detection: LaserDetection | None,
        core_found: bool,
    ) -> float:
        brightness_score = min(1.0, brightness / 255.0)
        area_mid = (self.params.area_px_min + self.params.area_px_max) / 2.0
        area_score = 1.0 - min(1.0, abs(area - area_mid) / area_mid)
        temporal_score = 0.5
        if previous_detection and previous_detection.found and previous_detection.image_center:
            distance = math.hypot(center.u_px - previous_detection.image_center.u_px, center.v_px - previous_detection.image_center.v_px)
            temporal_score = max(0.0, 1.0 - distance / 100.0)
        core_score = 1.0 if core_found else 0.5
        return 0.38 * brightness_score + 0.22 * circularity + 0.15 * area_score + 0.15 * temporal_score + 0.10 * core_score

    def _candidate_diagnostics(self, candidates: list[_Candidate]) -> list[dict[str, Any]]:
        return [
            {
                "center": (candidate.center.u_px, candidate.center.v_px),
                "score": candidate.score,
                "area_px": candidate.area_px,
                "color_area_px": candidate.color_area_px,
                "bright_core_area_px": candidate.bright_core_area_px,
                "combined_area_px": candidate.combined_area_px,
                "core_found": candidate.core_found,
                "brightness": candidate.brightness,
            }
            for candidate in candidates
        ]

    def _crop(self, image: np.ndarray, roi: ImageRoi | None) -> tuple[np.ndarray, int, int]:
        if roi is None:
            return image, 0, 0
        x1 = roi.x_px
        y1 = roi.y_px
        x2 = roi.x_px + roi.width_px
        y2 = roi.y_px + roi.height_px
        return image[y1:y2, x1:x2], x1, y1

    def _clip_roi(self, roi: ImageRoi | None, width: int, height: int) -> ImageRoi | None:
        if roi is None:
            return None
        x1 = max(0, min(width, roi.x_px))
        y1 = max(0, min(height, roi.y_px))
        x2 = max(x1, min(width, roi.x_px + roi.width_px))
        y2 = max(y1, min(height, roi.y_px + roi.height_px))
        return ImageRoi(x1, y1, x2 - x1, y2 - y1)

    def _dominant_rejection_reason(self, rejected: list[str]) -> FailureReason:
        if not rejected:
            return FailureReason.NOT_FOUND
        for reason in (FailureReason.TOO_SMALL, FailureReason.TOO_LARGE, FailureReason.LOW_BRIGHTNESS):
            if reason.value in rejected:
                return reason
        return FailureReason.NOT_FOUND

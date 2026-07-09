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
    score: float
    halo_area_px: float
    core_area_px: float | None
    core_found: bool


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
            "color_space": self.params.color_space,
        }
        if crop.size == 0:
            diagnostics["failure_reason"] = FailureReason.NOT_FOUND.value
            return LaserDetection.lost(self.color, FailureReason.NOT_FOUND, diagnostics)

        # Branch on color_space
        if self.params.color_space == "LAB_HALO_GUIDED":
            candidates, rejected = self._extract_candidates_lab(crop, offset_x, offset_y, previous_detection)
        else:
            # Legacy HSV fallback
            candidates, rejected = self._extract_candidates_hsv(crop, offset_x, offset_y, previous_detection)

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

    # ────────────────────────────────────────────────────────────────────
    # LAB halo-guided core detection (v3)
    # ────────────────────────────────────────────────────────────────────

    def _extract_candidates_lab(
        self,
        image: np.ndarray,
        offset_x: int,
        offset_y: int,
        previous_detection: LaserDetection | None,
    ) -> tuple[list[_Candidate], list[str]]:
        """LAB halo-guided core extraction (v3 validated algorithm)."""
        lab = cv2.cvtColor(image, cv2.COLOR_BGR2LAB)
        l_channel = lab[:, :, 0]
        a_delta = lab[:, :, 1].astype(np.int16) - 128

        # Build A-channel halo mask
        a_thresh = self.params.a_threshold
        if self.color == LaserColor.RED:
            halo_mask = np.where(a_delta > a_thresh, 255, 0).astype(np.uint8)
        else:
            halo_mask = np.where(a_delta < -a_thresh, 255, 0).astype(np.uint8)

        # Find halo contours (connected components)
        contours, _ = cv2.findContours(halo_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        candidates: list[_Candidate] = []
        rejected: list[str] = []

        for cnt in contours:
            halo_area = float(cv2.contourArea(cnt))
            if halo_area < self.params.halo_blob_min_area_px:
                rejected.append(FailureReason.TOO_SMALL.value)
                continue
            if halo_area > self.params.halo_blob_max_area_px:
                rejected.append(FailureReason.TOO_LARGE.value)
                continue

            # Build per-halo binary mask
            halo_mask_single = np.zeros(halo_mask.shape, dtype=np.uint8)
            cv2.drawContours(halo_mask_single, [cnt], -1, 255, -1)

            # Compute A-weighted halo centroid
            ys, xs = np.where(halo_mask_single > 0)
            if len(xs) == 0:
                continue
            weights = np.abs(a_delta[ys, xs]).astype(np.float32)
            ws = float(weights.sum())
            if ws > 0:
                halo_cx = float((xs * weights).sum() / ws)
                halo_cy = float((ys * weights).sum() / ws)
            else:
                halo_cx = float(xs.mean())
                halo_cy = float(ys.mean())
            halo_center = (halo_cx, halo_cy)

            # Search for white core in dilated halo region
            core_result = self._find_core_in_halo(lab, halo_mask_single, halo_center)

            if core_result is not None:
                center, core_area, mean_l, max_l = core_result
                core_found = True
            else:
                center = halo_center
                core_area = None
                mean_l = float(l_channel[ys, xs].mean()) if len(xs) > 0 else 0.0
                max_l = float(l_channel[ys, xs].max()) if len(xs) > 0 else 0.0
                core_found = False

            # Convert to full image coordinates
            center_full = ImagePoint(center[0] + offset_x, center[1] + offset_y)

            # Estimate radius from halo area
            radius = math.sqrt(halo_area / math.pi)

            # Score
            brightness_score = min(1.0, max_l / 255.0)
            area_mid = (self.params.area_px_min + self.params.area_px_max) / 2.0
            area_score = 1.0 - min(1.0, abs(halo_area - area_mid) / max(area_mid, 1.0))
            temporal_score = 0.5
            if previous_detection and previous_detection.found and previous_detection.image_center:
                distance = math.hypot(
                    center_full.u_px - previous_detection.image_center.u_px,
                    center_full.v_px - previous_detection.image_center.v_px,
                )
                temporal_score = max(0.0, 1.0 - distance / 100.0)
            core_score = 1.0 if core_found else 0.5
            score = 0.35 * brightness_score + 0.15 * area_score + 0.15 * temporal_score + 0.35 * core_score

            candidates.append(
                _Candidate(
                    center=center_full,
                    radius_px=float(radius),
                    area_px=float(halo_area),
                    brightness=float(max_l),
                    score=score,
                    halo_area_px=float(halo_area),
                    core_area_px=core_area,
                    core_found=core_found,
                )
            )

        return candidates, rejected

    def _find_core_in_halo(
        self,
        lab: np.ndarray,
        halo_mask: np.ndarray,
        halo_center: tuple[float, float],
    ) -> tuple[tuple[float, float], float, float, float] | None:
        """Find white core inside the dilated halo blob region.
        Returns (center, area, mean_l, max_l) or None if no valid core found.
        """
        # Dilate the halo mask to include edge/core pixels
        kernel_size = self.params.core_dilate_kernel_size
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (kernel_size, kernel_size))
        search_mask = cv2.dilate(halo_mask, kernel, iterations=1)

        # Find bright L pixels within the dilated region
        l_channel = lab[:, :, 0].astype(np.float32)
        region = search_mask > 0
        if not np.any(region):
            return None

        l_region = l_channel[region]
        if l_region.size == 0:
            return None

        # Top L-percentile threshold
        thresh = float(np.percentile(l_region, self.params.core_l_percentile))
        bright_mask = np.zeros_like(l_channel, dtype=np.uint8)
        bright_mask[region] = (l_region >= thresh).astype(np.uint8)

        # Find connected components
        component_count, labels, stats, _ = cv2.connectedComponentsWithStats(bright_mask, 8)
        best_label = -1
        best_area = -1.0
        for li in range(1, component_count):
            area = float(stats[li, cv2.CC_STAT_AREA])
            diameter = math.sqrt(4.0 * area / math.pi)
            if area < self.params.core_area_min_px or area > self.params.core_area_max_px:
                continue
            if diameter < self.params.core_diameter_min_px or diameter > self.params.core_diameter_max_px:
                continue
            if area > best_area:
                best_area = area
                best_label = li

        if best_label < 0:
            return None

        # L-brightness-weighted centroid
        component = labels == best_label
        comp_y, comp_x = np.where(component)
        if len(comp_x) == 0:
            return None

        weights = l_channel[comp_y, comp_x].astype(np.float32)
        ws = float(weights.sum())
        if ws <= 0:
            return None

        center_x = float((comp_x * weights).sum() / ws)
        center_y = float((comp_y * weights).sum() / ws)
        center = (center_x, center_y)

        comp_l = l_channel[comp_y, comp_x]
        mean_l = float(comp_l.mean())
        max_l = float(comp_l.max())

        return (center, best_area, mean_l, max_l)

    # ────────────────────────────────────────────────────────────────────
    # Legacy HSV detection (kept for backward compatibility)
    # ────────────────────────────────────────────────────────────────────

    def _extract_candidates_hsv(
        self,
        image: np.ndarray,
        offset_x: int,
        offset_y: int,
        previous_detection: LaserDetection | None,
    ) -> tuple[list[_Candidate], list[str]]:
        """Legacy HSV-based detection (kept for reference/fallback)."""
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        mask = np.zeros(hsv.shape[:2], dtype=np.uint8)

        for hue_range in self.params.hue_ranges_deg:
            low_h = max(0, min(179, int(hue_range.min)))
            high_h = max(0, min(179, int(hue_range.max)))
            lower = np.array([low_h, self.params.saturation_min, self.params.value_min], dtype=np.uint8)
            upper = np.array([high_h, 255, 255], dtype=np.uint8)
            mask = cv2.bitwise_or(mask, cv2.inRange(hsv, lower, upper))

        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[_Candidate] = []
        rejected: list[str] = []
        value_channel = hsv[:, :, 2]

        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area < self.params.area_px_min:
                rejected.append(FailureReason.TOO_SMALL.value)
                continue
            if area > self.params.area_px_max:
                rejected.append(FailureReason.TOO_LARGE.value)
                continue

            contour_mask = np.zeros(mask.shape, dtype=np.uint8)
            cv2.drawContours(contour_mask, [contour], -1, 255, -1)
            values = value_channel[contour_mask > 0]
            brightness = float(values.max() if values.size else 0.0)
            if brightness < self.params.value_min:
                rejected.append(FailureReason.LOW_BRIGHTNESS.value)
                continue

            center = self._brightness_weighted_center(contour_mask, value_channel, offset_x, offset_y)
            (_, _), radius = cv2.minEnclosingCircle(contour)
            perimeter = float(cv2.arcLength(contour, True))
            circularity = 0.0 if perimeter == 0 else min(1.0, 4.0 * math.pi * area / (perimeter * perimeter))

            brightness_score = min(1.0, brightness / 255.0)
            area_mid = (self.params.area_px_min + self.params.area_px_max) / 2.0
            area_score = 1.0 - min(1.0, abs(area - area_mid) / area_mid)
            temporal_score = 0.5
            if previous_detection and previous_detection.found and previous_detection.image_center:
                distance = math.hypot(center.u_px - previous_detection.image_center.u_px, center.v_px - previous_detection.image_center.v_px)
                temporal_score = max(0.0, 1.0 - distance / 100.0)
            score = 0.40 * brightness_score + 0.25 * circularity + 0.15 * area_score + 0.20 * temporal_score

            candidates.append(
                _Candidate(
                    center=center,
                    radius_px=float(radius),
                    area_px=area,
                    brightness=brightness,
                    score=score,
                    halo_area_px=area,
                    core_area_px=None,
                    core_found=False,
                )
            )

        return candidates, rejected

    # ────────────────────────────────────────────────────────────────────
    # Utilities
    # ────────────────────────────────────────────────────────────────────

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

    def _candidate_diagnostics(self, candidates: list[_Candidate]) -> list[dict[str, Any]]:
        return [
            {
                "center": (c.center.u_px, c.center.v_px),
                "score": c.score,
                "area_px": c.area_px,
                "halo_area_px": c.halo_area_px,
                "core_area_px": c.core_area_px,
                "core_found": c.core_found,
                "brightness": c.brightness,
            }
            for c in candidates
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

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class HueRangeDeg:
    min: int
    max: int


@dataclass(frozen=True)
class LaserDetectionParams:
    enabled: bool
    color_space: str
    hue_ranges_deg: list[HueRangeDeg]
    saturation_min: int
    value_min: int
    area_px_min: float
    area_px_max: float
    confidence_min: float


@dataclass(frozen=True)
class TapeDetectionParams:
    enabled: bool
    color_space: str
    threshold_mode: str
    expected_sides: int
    polygon_approx_epsilon_ratio: float
    min_contour_area_px: float
    corner_order: str


@dataclass(frozen=True)
class LostTargetPolicy:
    max_missing_frames: int
    action: str


@dataclass(frozen=True)
class VisionRuntimeConfig:
    red_laser: LaserDetectionParams | None
    green_laser: LaserDetectionParams | None
    tape_quad: TapeDetectionParams | None
    lost_target_policy: LostTargetPolicy


def load_vision_config(path: str | Path) -> VisionRuntimeConfig:
    with Path(path).open("r", encoding="utf-8") as f:
        raw = json.load(f)

    detections = raw.get("detections", {})
    lost_policy = raw["lost_target_policy"]

    return VisionRuntimeConfig(
        red_laser=_load_laser_params(detections.get("red_laser")),
        green_laser=_load_laser_params(detections.get("green_laser")),
        tape_quad=_load_tape_params(detections.get("a4_black_tape_quad")),
        lost_target_policy=LostTargetPolicy(
            max_missing_frames=lost_policy["max_missing_frames"],
            action=lost_policy["action"],
        ),
    )


def _load_laser_params(raw: dict | None) -> LaserDetectionParams | None:
    if raw is None:
        return None
    return LaserDetectionParams(
        enabled=raw["enabled"],
        color_space=raw["color_space"],
        hue_ranges_deg=[HueRangeDeg(min=r["min"], max=r["max"]) for r in raw["hue_ranges_deg"]],
        saturation_min=raw["saturation_min"],
        value_min=raw["value_min"],
        area_px_min=raw["area_px_min"],
        area_px_max=raw["area_px_max"],
        confidence_min=raw["confidence_min"],
    )


def _load_tape_params(raw: dict | None) -> TapeDetectionParams | None:
    if raw is None:
        return None
    return TapeDetectionParams(
        enabled=raw["enabled"],
        color_space=raw["color_space"],
        threshold_mode=raw["threshold_mode"],
        expected_sides=raw["expected_sides"],
        polygon_approx_epsilon_ratio=raw["polygon_approx_epsilon_ratio"],
        min_contour_area_px=raw["min_contour_area_px"],
        corner_order=raw["corner_order"],
    )

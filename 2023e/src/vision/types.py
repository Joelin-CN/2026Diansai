from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class LaserColor(Enum):
    RED = "red"
    GREEN = "green"


class DetectionSource(Enum):
    MEASURED = "measured"
    PREDICTED = "predicted"
    LOST = "lost"


class FailureReason(Enum):
    NOT_FOUND = "not_found"
    TOO_SMALL = "too_small"
    TOO_LARGE = "too_large"
    LOW_BRIGHTNESS = "low_brightness"
    MULTIPLE_CANDIDATES = "multiple_candidates"
    OUTSIDE_ROI = "outside_roi"
    LOW_CONFIDENCE = "low_confidence"
    SCREEN_NOT_FOUND = "screen_not_found"


class TrackerState(Enum):
    UNINITIALIZED = "uninitialized"
    TRACKING = "tracking"
    TEMP_LOST = "temp_lost"
    REACQUIRING = "reacquiring"
    LOST = "lost"


class VisionMode(Enum):
    CALIBRATION = "calibration"
    RUNTIME_SCREEN_RED = "runtime_screen_red"
    RUNTIME_A4_RED = "runtime_a4_red"
    RUNTIME_TRACK_RED_GREEN = "runtime_track_red_green"
    RUNTIME_A4_TRACK_RED_GREEN = "runtime_a4_track_red_green"
    DEBUG = "debug"


@dataclass(frozen=True)
class ImagePoint:
    u_px: float
    v_px: float


@dataclass(frozen=True)
class ImageQuad:
    points: tuple[ImagePoint, ImagePoint, ImagePoint, ImagePoint]


@dataclass(frozen=True)
class ImageRoi:
    x_px: int
    y_px: int
    width_px: int
    height_px: int


@dataclass(frozen=True)
class CameraFrame:
    image: Any
    frame_id: int
    timestamp_ms: float
    camera_id: str
    width_px: int
    height_px: int
    color_format: str = "BGR"


@dataclass(frozen=True)
class LaserDetection:
    color: LaserColor
    found: bool
    image_center: ImagePoint | None
    image_radius_px: float | None
    area_px: float | None
    brightness: float | None
    confidence: float
    source: DetectionSource
    failure_reason: FailureReason | None
    diagnostics: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def measured(
        cls,
        color: LaserColor,
        image_center: ImagePoint,
        image_radius_px: float,
        area_px: float,
        brightness: float,
        confidence: float,
        diagnostics: dict[str, Any] | None = None,
    ) -> "LaserDetection":
        return cls(
            color=color,
            found=True,
            image_center=image_center,
            image_radius_px=image_radius_px,
            area_px=area_px,
            brightness=brightness,
            confidence=confidence,
            source=DetectionSource.MEASURED,
            failure_reason=None,
            diagnostics=diagnostics or {},
        )

    @classmethod
    def lost(
        cls,
        color: LaserColor,
        failure_reason: FailureReason,
        diagnostics: dict[str, Any] | None = None,
    ) -> "LaserDetection":
        return cls(
            color=color,
            found=False,
            image_center=None,
            image_radius_px=None,
            area_px=None,
            brightness=None,
            confidence=0.0,
            source=DetectionSource.LOST,
            failure_reason=failure_reason,
            diagnostics=diagnostics or {},
        )


@dataclass(frozen=True)
class TapeQuadDetection:
    found: bool
    outer_quad: ImageQuad | None
    inner_quad: ImageQuad | None
    image_corners: list[ImagePoint] | None
    image_corners_ordered: list[ImagePoint] | None
    contour_area_px: float | None
    inner_area_px: float | None
    tape_ratio: float | None
    aspect_ratio: float | None
    confidence: float
    source: DetectionSource
    failure_reason: FailureReason | None
    diagnostics: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def measured(
        cls,
        outer_quad: ImageQuad,
        inner_quad: ImageQuad,
        contour_area_px: float,
        inner_area_px: float,
        tape_ratio: float,
        confidence: float,
        diagnostics: dict[str, Any] | None = None,
    ) -> "TapeQuadDetection":
        return cls(
            found=True,
            outer_quad=outer_quad,
            inner_quad=inner_quad,
            image_corners=list(outer_quad.points),
            image_corners_ordered=list(outer_quad.points),
            contour_area_px=contour_area_px,
            inner_area_px=inner_area_px,
            tape_ratio=tape_ratio,
            aspect_ratio=None,
            confidence=confidence,
            source=DetectionSource.MEASURED,
            failure_reason=None,
            diagnostics=diagnostics or {},
        )

    @classmethod
    def lost(cls, failure_reason: FailureReason, diagnostics: dict[str, Any] | None = None) -> "TapeQuadDetection":
        return cls(
            found=False,
            outer_quad=None,
            inner_quad=None,
            image_corners=None,
            image_corners_ordered=None,
            contour_area_px=None,
            inner_area_px=None,
            tape_ratio=None,
            aspect_ratio=None,
            confidence=0.0,
            source=DetectionSource.LOST,
            failure_reason=failure_reason,
            diagnostics=diagnostics or {},
        )


@dataclass(frozen=True)
class ScreenDetection:
    found: bool
    image_corners: list[ImagePoint] | None
    image_roi: ImageRoi | None
    confidence: float
    source: DetectionSource
    failure_reason: FailureReason | None
    diagnostics: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class VisionFrameDiagnostics:
    processing_time_ms: float
    enabled_detectors: list[str]
    roi_used: dict[str, ImageRoi | None]
    candidate_count: dict[str, int]
    failure_reason: dict[str, str | None]
    rejected_candidates: dict[str, list[str]]
    warnings: list[str]
    tracker_state: dict[str, TrackerState]
    lost_frame_count: dict[str, int]
    stage_time_ms: dict[str, float]
    debug_artifacts: dict[str, Any] | None = None


@dataclass(frozen=True)
class VisionFrameResult:
    frame_id: int
    timestamp_ms: float
    red_laser: LaserDetection | None
    green_laser: LaserDetection | None
    tape_quad: TapeQuadDetection | None
    screen_detection: ScreenDetection | None
    diagnostics: VisionFrameDiagnostics

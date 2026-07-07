from pathlib import Path

import cv2
import numpy as np

from vision.config import load_vision_config
from vision.pipeline import VisionPipeline
from vision.types import CameraFrame, DetectionSource, LaserColor, TrackerState, VisionMode


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def _blank(width=120, height=100):
    return np.full((height, width, 3), 30, dtype=np.uint8)


def _frame(image, frame_id=7, timestamp_ms=123.0):
    return CameraFrame(
        image=image,
        frame_id=frame_id,
        timestamp_ms=timestamp_ms,
        camera_id="synthetic",
        width_px=image.shape[1],
        height_px=image.shape[0],
        color_format="BGR",
    )


def test_runtime_screen_red_runs_only_red_runtime_path():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(120, 100))
    image = _blank()
    cv2.circle(image, (40, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)

    result = pipeline.process(_frame(image), VisionMode.RUNTIME_SCREEN_RED)

    assert result.frame_id == 7
    assert result.timestamp_ms == 123.0
    assert result.red_laser is not None
    assert result.red_laser.found is True
    assert result.red_laser.source == DetectionSource.MEASURED
    assert result.green_laser is None
    assert result.tape_quad is None
    assert result.screen_detection is None
    assert result.diagnostics.enabled_detectors == ["red_laser"]
    assert result.diagnostics.tracker_state["red_laser"] == TrackerState.TRACKING


def test_runtime_track_red_green_runs_red_and_green_lasers_only():
    config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    pipeline = VisionPipeline("green", config, frame_size=(120, 100))
    image = _blank()
    cv2.circle(image, (30, 40), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (85, 70), 5, (0, 255, 0), -1, lineType=cv2.LINE_AA)

    result = pipeline.process(_frame(image), VisionMode.RUNTIME_TRACK_RED_GREEN)

    assert result.red_laser is not None
    assert result.red_laser.found is True
    assert result.green_laser is not None
    assert result.green_laser.found is True
    assert result.diagnostics.enabled_detectors == ["red_laser", "green_laser"]
    assert result.diagnostics.candidate_count["red_laser"] == 1
    assert result.diagnostics.candidate_count["green_laser"] == 1


def test_pipeline_instances_keep_tracker_state_independent():
    red_config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    green_config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    red_pipeline = VisionPipeline("red", red_config, frame_size=(120, 100))
    green_pipeline = VisionPipeline("green", green_config, frame_size=(120, 100))

    red_image = _blank()
    cv2.circle(red_image, (40, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    red_result = red_pipeline.process(_frame(red_image), VisionMode.RUNTIME_SCREEN_RED)
    green_result = green_pipeline.process(_frame(_blank()), VisionMode.RUNTIME_TRACK_RED_GREEN)

    assert red_result.diagnostics.tracker_state["red_laser"] == TrackerState.TRACKING
    assert green_result.diagnostics.tracker_state["red_laser"] == TrackerState.UNINITIALIZED
    assert red_pipeline.tracker is not green_pipeline.tracker


def test_pipeline_diagnostics_include_roi_and_lost_counts():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(120, 100))

    result = pipeline.process(_frame(_blank()), VisionMode.RUNTIME_SCREEN_RED)

    assert "red_laser" in result.diagnostics.roi_used
    assert result.diagnostics.candidate_count["red_laser"] == 0
    assert result.diagnostics.lost_frame_count["red_laser"] == 0
    assert result.diagnostics.failure_reason["red_laser"] == "not_found"
    assert result.red_laser is not None
    assert result.red_laser.found is False
    assert result.diagnostics.debug_artifacts is None
    assert result.diagnostics.stage_time_ms["red_laser"] >= 0.0
    assert result.diagnostics.stage_time_ms["total_processing"] >= result.diagnostics.stage_time_ms["red_laser"]


def test_runtime_a4_red_runs_red_and_a4_support_path():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(320, 240))
    image = _a4_tape_frame()

    result = pipeline.process(_frame(image), VisionMode.RUNTIME_A4_RED)

    assert result.red_laser is not None
    assert result.tape_quad is not None
    assert result.tape_quad.found is True
    assert result.tape_quad.outer_quad is not None
    assert result.tape_quad.inner_quad is not None
    assert "a4_tape" in result.diagnostics.candidate_count
    assert result.green_laser is None
    assert result.diagnostics.stage_time_ms["a4_tape"] >= 0.0


def test_runtime_screen_red_does_not_run_a4_support_path():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(320, 240))

    result = pipeline.process(_frame(_a4_tape_frame()), VisionMode.RUNTIME_SCREEN_RED)

    assert result.tape_quad is None


def test_runtime_track_red_green_does_not_run_a4_support_path():
    config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    pipeline = VisionPipeline("green", config, frame_size=(320, 240))

    result = pipeline.process(_frame(_a4_tape_frame()), VisionMode.RUNTIME_TRACK_RED_GREEN)

    assert result.tape_quad is None


def test_runtime_a4_track_red_green_runs_lasers_and_a4_support_path():
    config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    pipeline = VisionPipeline("green", config, frame_size=(320, 240))
    image = _a4_tape_frame()
    cv2.circle(image, (110, 110), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (180, 130), 5, (0, 255, 0), -1, lineType=cv2.LINE_AA)

    result = pipeline.process(_frame(image), VisionMode.RUNTIME_A4_TRACK_RED_GREEN)

    assert result.red_laser is not None
    assert result.red_laser.found is True
    assert result.green_laser is not None
    assert result.green_laser.found is True
    assert result.tape_quad is not None
    assert result.tape_quad.found is True


def test_runtime_a4_red_reuses_cached_a4_support_between_refresh_frames():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(320, 240))

    first = pipeline.process(_frame(_a4_tape_frame(), frame_id=1), VisionMode.RUNTIME_A4_RED)
    second = pipeline.process(_frame(_blank(320, 240), frame_id=2), VisionMode.RUNTIME_A4_RED)

    assert first.tape_quad is not None
    assert first.tape_quad.found is True
    assert second.tape_quad is not None
    assert second.tape_quad is first.tape_quad
    assert "a4_tape" not in second.diagnostics.enabled_detectors


def test_debug_mode_may_run_all_enabled_detectors():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    pipeline = VisionPipeline("red", config, frame_size=(320, 240))
    image = _a4_tape_frame()
    cv2.circle(image, (110, 110), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)

    result = pipeline.process(_frame(image), VisionMode.DEBUG)

    assert result.red_laser is not None
    assert result.tape_quad is not None
    assert result.screen_detection is not None


def _a4_tape_frame():
    image = np.full((240, 320, 3), 230, dtype=np.uint8)
    outer = np.array([[80, 50], [250, 60], [235, 190], [65, 180]], dtype=np.int32)
    inner = np.array([[105, 78], [222, 84], [213, 160], [92, 154]], dtype=np.int32)
    cv2.fillPoly(image, [outer], (20, 20, 20))
    cv2.fillPoly(image, [inner], (230, 230, 230))
    return image

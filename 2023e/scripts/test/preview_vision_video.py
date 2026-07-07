from __future__ import annotations

import argparse
import sys
import time
from dataclasses import replace
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from border_preview_detectors import draw_quad  # noqa: E402
from preview_vision_detection import draw_laser_detection  # noqa: E402
from vision.config import HueRangeDeg, load_vision_config  # noqa: E402
from vision.laser_detector import LaserDetector  # noqa: E402
from vision.pipeline import VisionPipeline  # noqa: E402
from vision.quad_utils import image_quad_to_numpy  # noqa: E402
from vision.screen_detector import ScreenDetector  # noqa: E402
from vision.tape_quad_detector import TapeQuadDetector  # noqa: E402
from vision.types import CameraFrame, LaserColor, LaserDetection, ScreenDetection, TapeQuadDetection, VisionFrameResult, VisionMode  # noqa: E402


DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Preview red/green laser and border tracking on a video.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO, help="Input video path.")
    parser.add_argument(
        "--path-mode",
        choices=("debug", "runtime"),
        default="debug",
        help="debug keeps strict/relaxed analysis and overlays; runtime uses the formal VisionPipeline path.",
    )
    parser.add_argument(
        "--runtime-mode",
        choices=tuple(mode.value for mode in VisionMode if mode not in {VisionMode.CALIBRATION, VisionMode.DEBUG}),
        default=VisionMode.RUNTIME_A4_RED.value,
        help="Formal runtime mode used when --path-mode=runtime.",
    )
    parser.add_argument("--strict-only", action="store_true", help="Only use configs/*/vision.json thresholds.")
    parser.add_argument("--border-interval", type=int, default=1, help="Re-detect borders every N frames; default 1 tracks every frame.")
    parser.add_argument("--max-frames", type=int, default=0, help="Stop after N frames; 0 means play full video.")
    parser.add_argument("--no-window", action="store_true", help="Run without cv2.imshow for smoke checks.")
    args = parser.parse_args(argv)

    cap = cv2.VideoCapture(str(args.video))
    if not cap.isOpened():
        print(f"Failed to open video: {args.video}")
        return 1

    fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
    delay_ms = max(1, int(round(1000.0 / fps)))
    preview_runner = build_preview_runner(args)
    frame_index = 0
    paused = False

    print(f"Preview path={args.path_mode} runtime_mode={args.runtime_mode if args.path_mode == 'runtime' else 'debug-analysis'}")
    print("Controls: q/ESC quit, space pause/resume, s step one frame while paused.")
    while True:
        if not paused:
            ok, frame = cap.read()
            if not ok:
                print("End of video.")
                break
            frame_index += 1
            annotated, summary = preview_runner(frame, frame_index)
            print(summary)

        if not args.no_window:
            cv2.imshow("2023E video vision preview", annotated)
            key = cv2.waitKey(0 if paused else delay_ms) & 0xFF
            if key in (27, ord("q")):
                break
            if key == ord(" "):
                paused = not paused
            if paused and key == ord("s"):
                paused = False
        elif args.max_frames and frame_index >= args.max_frames:
            break

        if args.max_frames and frame_index >= args.max_frames:
            break

    cap.release()
    if not args.no_window:
        cv2.destroyAllWindows()
    return 0


def build_preview_runner(args: argparse.Namespace):
    if args.path_mode == "runtime":
        runtime_mode = VisionMode(args.runtime_mode)
        if runtime_mode == VisionMode.DEBUG:
            raise ValueError("--runtime-mode cannot be debug when --path-mode=runtime")
        return RuntimePreviewRunner(runtime_mode)
    return DebugPreviewRunner(relaxed=not args.strict_only, border_interval=args.border_interval)


class RuntimePreviewRunner:
    def __init__(self, mode: VisionMode):
        self.mode = mode
        config_path = PROJECT_ROOT / "configs" / ("green" if mode in {VisionMode.RUNTIME_TRACK_RED_GREEN, VisionMode.RUNTIME_A4_TRACK_RED_GREEN} else "red") / "vision.json"
        config = load_vision_config(config_path)
        self.pipeline: VisionPipeline | None = None
        self.profile = "green" if mode in {VisionMode.RUNTIME_TRACK_RED_GREEN, VisionMode.RUNTIME_A4_TRACK_RED_GREEN} else "red"

        self._config = config

    def __call__(self, frame: np.ndarray, frame_index: int) -> tuple[np.ndarray, str]:
        if self.pipeline is None:
            self.pipeline = VisionPipeline(self.profile, self._config, frame_size=(frame.shape[1], frame.shape[0]))

        start = time.perf_counter()
        result = self.pipeline.process(_camera_frame(frame, frame_index), self.mode)
        annotated = frame.copy()
        _draw_runtime_result(annotated, result)
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        cv2.putText(
            annotated,
            f"runtime {self.mode.value} frame={frame_index} time={elapsed_ms:.1f}ms",
            (20, annotated.shape[0] - 25),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (255, 255, 255),
            2,
        )
        return annotated, build_runtime_summary(frame_index, self.mode, result)


class DebugPreviewRunner:
    def __init__(self, relaxed: bool, border_interval: int):
        self.trackers = PreviewDetectors(relaxed=relaxed)
        self.tape_detector = TapeQuadDetector()
        self.screen_detector = ScreenDetector()
        self.border_interval = border_interval
        self.cached_screen_square: ScreenDetection | None = None
        self.cached_a4_tape: TapeQuadDetection | None = None

    def __call__(self, frame: np.ndarray, frame_index: int) -> tuple[np.ndarray, str]:
        annotated, self.cached_screen_square, self.cached_a4_tape, summary = annotate_debug_frame(
            frame,
            self.trackers,
            self.tape_detector,
            self.screen_detector,
            self.cached_screen_square,
            self.cached_a4_tape,
            frame_index,
            self.border_interval,
        )
        return annotated, summary


class PreviewDetectors:
    def __init__(self, relaxed: bool):
        red_config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
        green_config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")

        self.strict_red = LaserDetector(LaserColor.RED, red_config.red_laser)
        self.strict_green = LaserDetector(LaserColor.GREEN, green_config.green_laser)
        self.relaxed_red = None
        self.relaxed_green = None
        if relaxed:
            red_params = replace(
                red_config.red_laser,
                saturation_min=30,
                value_min=30,
                area_px_min=2,
                area_px_max=8000,
                confidence_min=0.3,
            )
            green_params = replace(
                green_config.green_laser,
                hue_ranges_deg=[HueRangeDeg(35, 95)],
                saturation_min=30,
                value_min=30,
                area_px_min=2,
                area_px_max=8000,
                confidence_min=0.3,
            )
            self.relaxed_red = LaserDetector(LaserColor.RED, red_params)
            self.relaxed_green = LaserDetector(LaserColor.GREEN, green_params)

        self.previous_red: LaserDetection | None = None
        self.previous_green: LaserDetection | None = None

    def detect(self, frame: np.ndarray) -> tuple[LaserDetection, LaserDetection, LaserDetection | None, LaserDetection | None]:
        strict_red = self.strict_red.detect(frame, previous_detection=self.previous_red)
        strict_green = self.strict_green.detect(frame, previous_detection=self.previous_green)

        red_for_tracking = strict_red
        green_for_tracking = strict_green
        relaxed_red = relaxed_green = None

        if self.relaxed_red is not None and not strict_red.found:
            relaxed_red = self.relaxed_red.detect(frame, previous_detection=self.previous_red)
            if relaxed_red.found:
                red_for_tracking = relaxed_red
        if self.relaxed_green is not None and not strict_green.found:
            relaxed_green = self.relaxed_green.detect(frame, previous_detection=self.previous_green)
            if relaxed_green.found:
                green_for_tracking = relaxed_green

        if red_for_tracking.found:
            self.previous_red = red_for_tracking
        if green_for_tracking.found:
            self.previous_green = green_for_tracking

        return strict_red, strict_green, relaxed_red, relaxed_green


def annotate_debug_frame(
    frame: np.ndarray,
    trackers: PreviewDetectors,
    tape_detector: TapeQuadDetector,
    screen_detector: ScreenDetector,
    cached_screen_square: ScreenDetection | None,
    cached_a4_tape: TapeQuadDetection | None,
    frame_index: int,
    border_interval: int,
) -> tuple[np.ndarray, ScreenDetection | None, TapeQuadDetection | None, str]:
    start = time.perf_counter()
    annotated = frame.copy()
    strict_red, strict_green, relaxed_red, relaxed_green = trackers.detect(frame)

    should_refresh_border = cached_a4_tape is None or border_interval <= 1 or (frame_index - 1) % border_interval == 0
    if should_refresh_border:
        cached_a4_tape = tape_detector.detect(frame)
    if should_refresh_border or cached_screen_square is None:
        exclude_quad = cached_a4_tape.outer_quad if cached_a4_tape and cached_a4_tape.found else None
        cached_screen_square = screen_detector.detect(frame, exclude_quad=exclude_quad)

    draw_laser_detection(annotated, strict_red, (0, 0, 255), "red")
    draw_laser_detection(annotated, strict_green, (0, 255, 0), "green")
    if relaxed_red is not None:
        draw_laser_detection(annotated, relaxed_red, (0, 128, 255), "red relaxed")
    if relaxed_green is not None:
        draw_laser_detection(annotated, relaxed_green, (0, 255, 255), "green relaxed")
    if cached_screen_square is not None and cached_screen_square.found and cached_screen_square.image_corners is not None:
        screen_quad = np.array([[[int(point.u_px), int(point.v_px)]] for point in cached_screen_square.image_corners], dtype=np.int32)
        draw_quad(annotated, screen_quad, (255, 255, 0), "screen_square", 2)
    if cached_a4_tape is not None and cached_a4_tape.found and cached_a4_tape.outer_quad is not None and cached_a4_tape.inner_quad is not None:
        draw_quad(annotated, image_quad_to_numpy(cached_a4_tape.outer_quad), (255, 0, 255), "a4_outer", 3)
        draw_quad(annotated, image_quad_to_numpy(cached_a4_tape.inner_quad), (0, 255, 255), "a4_inner", 2)

    elapsed_ms = (time.perf_counter() - start) * 1000.0
    cv2.putText(
        annotated,
        f"debug frame={frame_index} time={elapsed_ms:.1f}ms q/ESC quit space pause",
        (20, annotated.shape[0] - 25),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (255, 255, 255),
        2,
    )
    summary = build_summary(frame_index, strict_red, strict_green, relaxed_red, relaxed_green, cached_screen_square, cached_a4_tape, elapsed_ms)
    return annotated, cached_screen_square, cached_a4_tape, summary


def _camera_frame(frame: np.ndarray, frame_id: int) -> CameraFrame:
    return CameraFrame(
        image=frame,
        frame_id=frame_id,
        timestamp_ms=float(frame_id),
        camera_id="preview-video",
        width_px=frame.shape[1],
        height_px=frame.shape[0],
        color_format="BGR",
    )


def _draw_runtime_result(image: np.ndarray, result: VisionFrameResult) -> None:
    if result.red_laser is not None:
        draw_laser_detection(image, result.red_laser, (0, 0, 255), "red")
    if result.green_laser is not None:
        draw_laser_detection(image, result.green_laser, (0, 255, 0), "green")
    if result.tape_quad is not None and result.tape_quad.found and result.tape_quad.outer_quad is not None and result.tape_quad.inner_quad is not None:
        draw_quad(image, image_quad_to_numpy(result.tape_quad.outer_quad), (255, 0, 255), "a4_outer", 3)
        draw_quad(image, image_quad_to_numpy(result.tape_quad.inner_quad), (0, 255, 255), "a4_inner", 2)
    if result.screen_detection is not None and result.screen_detection.found and result.screen_detection.image_corners is not None:
        screen_quad = np.array([[[int(point.u_px), int(point.v_px)]] for point in result.screen_detection.image_corners], dtype=np.int32)
        draw_quad(image, screen_quad, (255, 255, 0), "screen_square", 2)


def build_runtime_summary(frame_index: int, mode: VisionMode, result: VisionFrameResult) -> str:
    stage_times = result.diagnostics.stage_time_ms
    parts = [
        f"frame={frame_index}",
        f"path=runtime",
        f"mode={mode.value}",
        f"total_ms={stage_times.get('total_processing', result.diagnostics.processing_time_ms):.1f}",
        f"red_ms={stage_times.get('red_laser', 0.0):.1f}",
        f"green_ms={stage_times.get('green_laser', 0.0):.1f}",
        f"a4_ms={stage_times.get('a4_tape', 0.0):.1f}",
        f"screen_ms={stage_times.get('screen_square', 0.0):.1f}",
    ]
    if result.red_laser is not None:
        parts.append(format_detection("red", result.red_laser))
    if result.green_laser is not None:
        parts.append(format_detection("green", result.green_laser))
    if result.tape_quad is None:
        parts.append("a4_tape=lost:not_run")
    else:
        parts.append(
            f"a4_tape={result.tape_quad.found}:outer={(result.tape_quad.contour_area_px or 0.0):.1f}:"
            f"inner={(result.tape_quad.inner_area_px or 0.0):.1f}:reason={result.tape_quad.failure_reason}"
        )
    if result.screen_detection is None:
        parts.append("screen_square=lost:not_run")
    else:
        parts.append(f"screen_square={result.screen_detection.found}:reason={result.screen_detection.failure_reason}")
    return " ".join(parts)


def build_summary(
    frame_index: int,
    strict_red: LaserDetection,
    strict_green: LaserDetection,
    relaxed_red: LaserDetection | None,
    relaxed_green: LaserDetection | None,
    screen_square: ScreenDetection | None,
    a4_tape: TapeQuadDetection | None,
    elapsed_ms: float,
) -> str:
    parts = [f"frame={frame_index}", "path=debug", f"time_ms={elapsed_ms:.1f}"]
    parts.append(format_detection("red", strict_red))
    parts.append(format_detection("green", strict_green))
    parts.append(format_white_core_summary("red", strict_red))
    parts.append(format_white_core_summary("green", strict_green))
    if relaxed_red is not None:
        parts.append(format_detection("red_relaxed", relaxed_red))
    if relaxed_green is not None:
        parts.append(format_detection("green_relaxed", relaxed_green))
    if relaxed_red is not None:
        parts.append(format_white_core_summary("red_relaxed", relaxed_red))
    if relaxed_green is not None:
        parts.append(format_white_core_summary("green_relaxed", relaxed_green))
    if screen_square is None:
        parts.append("screen_square=lost:not_run")
    else:
        parts.append(
            f"screen_square={screen_square.found}:area={screen_square.diagnostics.get('area', 0.0):.1f}:"
            f"score={screen_square.confidence:.2f}:reason={screen_square.failure_reason}"
        )
    if a4_tape is None:
        parts.append("a4_tape=lost:not_run")
    else:
        parts.append(
            f"a4_tape={a4_tape.found}:outer={(a4_tape.contour_area_px or 0.0):.1f}:inner={(a4_tape.inner_area_px or 0.0):.1f}:"
            f"ratio={(a4_tape.tape_ratio or 0.0):.2f}:score={a4_tape.confidence:.2f}:reason={a4_tape.failure_reason}"
        )
    return " ".join(parts)


def format_detection(label: str, detection: LaserDetection) -> str:
    if detection.found and detection.image_center is not None:
        return f"{label}=({detection.image_center.u_px:.1f},{detection.image_center.v_px:.1f},conf={detection.confidence:.2f})"
    reason = detection.failure_reason.value if detection.failure_reason else "unknown"
    return f"{label}=lost:{reason}"


def format_white_core_summary(label: str, detection: LaserDetection) -> str:
    diagnostics = detection.diagnostics or {}
    count = diagnostics.get("white_core_candidate_count")
    candidates = diagnostics.get("white_core_candidates") or []
    if count is None:
        return f"{label}_wc=na"
    if not candidates:
        return f"{label}_wc={count}:top=none"
    top = candidates[0]
    center = top.get("center") or (0.0, 0.0)
    return (
        f"{label}_wc={count}:top=({center[0]:.1f},{center[1]:.1f})"
        f":r={top.get('local_red_score', 0.0):.2f}"
        f":g={top.get('local_green_score', 0.0):.2f}"
    )


if __name__ == "__main__":
    raise SystemExit(main())

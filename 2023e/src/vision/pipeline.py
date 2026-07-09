from __future__ import annotations

import time

from vision.config import VisionRuntimeConfig
from vision.diagnostics import VisionDiagnosticsBuilder
from vision.laser_detector import LaserDetector
from vision.screen_detector import ScreenDetector
from vision.state_tracker import VisionStateTracker
from vision.tape_quad_detector import TapeQuadDetector
from vision.types import CameraFrame, ImageRoi, LaserColor, LaserDetection, ScreenDetection, TapeQuadDetection, VisionFrameResult, VisionMode


class VisionPipeline:
    def __init__(
        self,
        profile: str,
        config: VisionRuntimeConfig,
        frame_size: tuple[int, int],
        screen_roi: ImageRoi | None = None,
    ):
        self.profile = profile
        self.config = config
        self.tracker = VisionStateTracker(
            max_missing_frames=config.lost_target_policy.max_missing_frames,
            frame_size=frame_size,
            screen_roi=screen_roi,
        )
        self._red_detector = LaserDetector(LaserColor.RED, config.red_laser) if config.red_laser and config.red_laser.enabled else None
        self._green_detector = LaserDetector(LaserColor.GREEN, config.green_laser) if config.green_laser and config.green_laser.enabled else None
        self._tape_detector = TapeQuadDetector()
        self._screen_detector = ScreenDetector()
        self._runtime_support_refresh_interval = 20
        self._last_tape_quad: TapeQuadDetection | None = None
        self._last_tape_refresh_frame_id: int | None = None
        self._last_screen_detection: ScreenDetection | None = None
        self._last_screen_refresh_frame_id: int | None = None
        self._tape_roi: ImageRoi | None = None  # Track A4 ROI
        self._frame_width, self._frame_height = frame_size

    def process(self, frame: CameraFrame, mode: VisionMode = VisionMode.DEBUG) -> VisionFrameResult:
        start = time.perf_counter()
        diagnostics = VisionDiagnosticsBuilder()
        red_laser: LaserDetection | None = None
        green_laser: LaserDetection | None = None
        tape_quad: TapeQuadDetection | None = None
        screen_detection: ScreenDetection | None = None

        if self._should_run_red(mode):
            red_start = time.perf_counter()
            red_laser = self._run_laser("red_laser", LaserColor.RED, self._red_detector, frame, diagnostics)
            diagnostics.add_stage_time("red_laser", (time.perf_counter() - red_start) * 1000.0)
        if self._should_run_green(mode):
            green_start = time.perf_counter()
            green_laser = self._run_laser("green_laser", LaserColor.GREEN, self._green_detector, frame, diagnostics)
            diagnostics.add_stage_time("green_laser", (time.perf_counter() - green_start) * 1000.0)
        tape_start = time.perf_counter()
        tape_quad = self._run_tape_support(frame, mode, diagnostics)
        diagnostics.add_stage_time("a4_tape", (time.perf_counter() - tape_start) * 1000.0)
        screen_start = time.perf_counter()
        screen_detection = self._run_screen_support(frame, mode, tape_quad, diagnostics)
        diagnostics.add_stage_time("screen_square", (time.perf_counter() - screen_start) * 1000.0)

        processing_time_ms = (time.perf_counter() - start) * 1000.0
        return VisionFrameResult(
            frame_id=frame.frame_id,
            timestamp_ms=frame.timestamp_ms,
            red_laser=red_laser,
            green_laser=green_laser,
            tape_quad=tape_quad,
            screen_detection=screen_detection,
            diagnostics=diagnostics.build(processing_time_ms),
        )

    def _run_laser(
        self,
        name: str,
        color: LaserColor,
        detector: LaserDetector | None,
        frame: CameraFrame,
        diagnostics: VisionDiagnosticsBuilder,
    ) -> LaserDetection | None:
        if detector is None:
            return None
        roi = self.tracker.suggest_roi(color)
        previous = self.tracker.last_detection_for(color)
        detection = detector.detect(frame.image, roi=roi, previous_detection=previous)
        self.tracker.update(color, detection)
        diagnostics.add_laser(
            name,
            detection.diagnostics,
            self.tracker.state_for(color),
            self.tracker.lost_count_for(color),
        )
        return detection

    def _run_tape_support(
        self,
        frame: CameraFrame,
        mode: VisionMode,
        diagnostics: VisionDiagnosticsBuilder,
    ) -> TapeQuadDetection | None:
        if not self._should_run_tape(mode):
            return None
        if not self._should_refresh_runtime_support(mode, frame.frame_id, self._last_tape_refresh_frame_id):
            return self._last_tape_quad

        # Use ROI if we have a previous successful detection
        roi = self._tape_roi
        tape_quad = self._tape_detector.detect(frame.image, roi=roi) if self._tape_detector else None
        
        if tape_quad is not None:
            diagnostics.add_detector("a4_tape", tape_quad.diagnostics)
            self._last_tape_refresh_frame_id = frame.frame_id
            if tape_quad.found:
                self._last_tape_quad = tape_quad
                # Update ROI based on outer quad for next detection
                self._tape_roi = self._compute_tape_roi(tape_quad)
                return tape_quad
            else:
                # Lost the quad - reset ROI to search full frame next time
                self._tape_roi = None
        return self._last_tape_quad if mode != VisionMode.DEBUG else tape_quad

    def _run_screen_support(
        self,
        frame: CameraFrame,
        mode: VisionMode,
        tape_quad: TapeQuadDetection | None,
        diagnostics: VisionDiagnosticsBuilder,
    ) -> ScreenDetection | None:
        if not self._should_run_screen(mode):
            return None
        if not self._should_refresh_runtime_support(mode, frame.frame_id, self._last_screen_refresh_frame_id):
            return self._last_screen_detection

        screen_detection = self._screen_detector.detect(frame.image, exclude_quad=tape_quad.outer_quad if tape_quad and tape_quad.found else None)
        diagnostics.add_detector("screen_square", screen_detection.diagnostics)
        self._last_screen_refresh_frame_id = frame.frame_id
        if screen_detection.found:
            self._last_screen_detection = screen_detection
            return screen_detection
        return self._last_screen_detection if mode != VisionMode.DEBUG else screen_detection

    def _should_refresh_runtime_support(self, mode: VisionMode, frame_id: int, last_refresh_frame_id: int | None) -> bool:
        if mode == VisionMode.DEBUG:
            return True
        if last_refresh_frame_id is None:
            return True
        return (frame_id - last_refresh_frame_id) >= self._runtime_support_refresh_interval
    
    def _compute_tape_roi(self, tape_quad: TapeQuadDetection) -> ImageRoi | None:
        """Compute ROI around the detected tape quad with margin."""
        if not tape_quad.found or not tape_quad.outer_quad:
            return None
        
        # Find bounding box of outer quad
        points = tape_quad.outer_quad.points
        u_coords = [pt.u_px for pt in points]
        v_coords = [pt.v_px for pt in points]
        
        min_u = min(u_coords)
        max_u = max(u_coords)
        min_v = min(v_coords)
        max_v = max(v_coords)
        
        # Add margin (30% on each side)
        width = max_u - min_u
        height = max_v - min_v
        margin_u = int(width * 0.3)
        margin_v = int(height * 0.3)
        
        # Clip to frame bounds
        x = max(0, int(min_u) - margin_u)
        y = max(0, int(min_v) - margin_v)
        w = min(self._frame_width - x, int(max_u - min_u) + 2 * margin_u)
        h = min(self._frame_height - y, int(max_v - min_v) + 2 * margin_v)
        
        return ImageRoi(x, y, w, h)

    def _should_run_red(self, mode: VisionMode) -> bool:
        if self._red_detector is None:
            return False
        return mode in {
            VisionMode.DEBUG,
            VisionMode.RUNTIME_SCREEN_RED,
            VisionMode.RUNTIME_A4_RED,
            VisionMode.RUNTIME_TRACK_RED_GREEN,
            VisionMode.RUNTIME_A4_TRACK_RED_GREEN,
        }

    def _should_run_green(self, mode: VisionMode) -> bool:
        if self._green_detector is None:
            return False
        return mode in {
            VisionMode.DEBUG,
            VisionMode.RUNTIME_TRACK_RED_GREEN,
            VisionMode.RUNTIME_A4_TRACK_RED_GREEN,
        }

    def _should_run_tape(self, mode: VisionMode) -> bool:
        if self._tape_detector is None:
            return False
        return mode in {
            VisionMode.DEBUG,
            VisionMode.RUNTIME_A4_RED,
            VisionMode.RUNTIME_A4_TRACK_RED_GREEN,
        }

    def _should_run_screen(self, mode: VisionMode) -> bool:
        return mode in {
            VisionMode.DEBUG,
            VisionMode.RUNTIME_SCREEN_RED,
        }

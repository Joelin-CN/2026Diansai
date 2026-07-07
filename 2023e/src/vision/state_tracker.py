from __future__ import annotations

from dataclasses import dataclass

from vision.types import ImageRoi, LaserColor, LaserDetection, TrackerState


@dataclass
class _ColorState:
    state: TrackerState = TrackerState.UNINITIALIZED
    lost_count: int = 0
    last_detection: LaserDetection | None = None
    last_roi: ImageRoi | None = None
    last_search_roi: ImageRoi | None = None


class VisionStateTracker:
    def __init__(
        self,
        max_missing_frames: int,
        frame_size: tuple[int, int],
        screen_roi: ImageRoi | None = None,
        local_roi_radius_px: int = 30,
    ):
        self.max_missing_frames = max_missing_frames
        self.frame_width, self.frame_height = frame_size
        self.screen_roi = screen_roi
        self.local_roi_radius_px = local_roi_radius_px
        self._states = {
            LaserColor.RED: _ColorState(),
            LaserColor.GREEN: _ColorState(),
        }

    def suggest_roi(self, color: LaserColor) -> ImageRoi | None:
        color_state = self._states[color]
        if color_state.state == TrackerState.UNINITIALIZED:
            roi = self.screen_roi or self._full_frame_roi()
            color_state.last_search_roi = roi
            return roi
        if color_state.state == TrackerState.LOST:
            if self.screen_roi is not None:
                color_state.last_search_roi = self.screen_roi
                return self.screen_roi
            roi = color_state.last_search_roi or color_state.last_roi or self._full_frame_roi()
            color_state.last_search_roi = roi
            return roi
        if color_state.last_roi is None:
            roi = self.screen_roi or self._full_frame_roi()
            color_state.last_search_roi = roi
            return roi
        if color_state.state == TrackerState.TRACKING:
            color_state.last_search_roi = color_state.last_roi
            return color_state.last_roi
        if color_state.state == TrackerState.TEMP_LOST:
            roi = self._expand_roi(color_state.last_roi, 2.0)
            color_state.last_search_roi = roi
            return roi
        if color_state.state == TrackerState.REACQUIRING:
            roi = self._expand_roi(color_state.last_roi, 3.0)
            color_state.last_search_roi = roi
            return roi
        roi = self.screen_roi or self._full_frame_roi()
        color_state.last_search_roi = roi
        return roi

    def update(self, color: LaserColor, detection: LaserDetection) -> None:
        color_state = self._states[color]
        if detection.found:
            color_state.state = TrackerState.TRACKING
            color_state.lost_count = 0
            color_state.last_detection = detection
            color_state.last_roi = self._roi_around_detection(detection)
            color_state.last_search_roi = color_state.last_roi
            return

        if color_state.state == TrackerState.UNINITIALIZED:
            return

        color_state.lost_count += 1
        if color_state.lost_count == 1:
            color_state.state = TrackerState.TEMP_LOST
        elif color_state.lost_count <= self.max_missing_frames:
            color_state.state = TrackerState.REACQUIRING
        else:
            color_state.state = TrackerState.LOST

    def state_for(self, color: LaserColor) -> TrackerState:
        return self._states[color].state

    def lost_count_for(self, color: LaserColor) -> int:
        return self._states[color].lost_count

    def last_detection_for(self, color: LaserColor) -> LaserDetection | None:
        return self._states[color].last_detection

    def _roi_around_detection(self, detection: LaserDetection) -> ImageRoi:
        if detection.image_center is None:
            return self.screen_roi or self._full_frame_roi()
        radius = self.local_roi_radius_px
        center_u = int(round(detection.image_center.u_px))
        center_v = int(round(detection.image_center.v_px))
        return self._clip_roi(ImageRoi(center_u - radius, center_v - radius, radius * 2, radius * 2))

    def _expand_roi(self, roi: ImageRoi, factor: float) -> ImageRoi:
        center_x = roi.x_px + roi.width_px / 2.0
        center_y = roi.y_px + roi.height_px / 2.0
        width = int(round(roi.width_px * factor))
        height = int(round(roi.height_px * factor))
        return self._clip_roi(ImageRoi(int(round(center_x - width / 2.0)), int(round(center_y - height / 2.0)), width, height))

    def _clip_roi(self, roi: ImageRoi) -> ImageRoi:
        x1 = max(0, min(self.frame_width, roi.x_px))
        y1 = max(0, min(self.frame_height, roi.y_px))
        x2 = max(x1, min(self.frame_width, roi.x_px + roi.width_px))
        y2 = max(y1, min(self.frame_height, roi.y_px + roi.height_px))
        return ImageRoi(x1, y1, x2 - x1, y2 - y1)

    def _full_frame_roi(self) -> ImageRoi:
        return ImageRoi(0, 0, self.frame_width, self.frame_height)

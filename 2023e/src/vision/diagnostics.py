from __future__ import annotations

from vision.types import ImageRoi, TrackerState, VisionFrameDiagnostics


class VisionDiagnosticsBuilder:
    def __init__(self):
        self.enabled_detectors: list[str] = []
        self.roi_used: dict[str, ImageRoi | None] = {}
        self.candidate_count: dict[str, int] = {}
        self.failure_reason: dict[str, str | None] = {}
        self.rejected_candidates: dict[str, list[str]] = {}
        self.warnings: list[str] = []
        self.tracker_state: dict[str, TrackerState] = {}
        self.lost_frame_count: dict[str, int] = {}
        self.stage_time_ms: dict[str, float] = {}

    def add_laser(self, name: str, detection_diagnostics: dict, tracker_state: TrackerState, lost_count: int) -> None:
        self.enabled_detectors.append(name)
        self.roi_used[name] = detection_diagnostics.get("roi_used")
        self.candidate_count[name] = int(detection_diagnostics.get("candidate_count", 0))
        self.failure_reason[name] = detection_diagnostics.get("failure_reason")
        self.rejected_candidates[name] = list(detection_diagnostics.get("rejected_candidates", []))
        self.tracker_state[name] = tracker_state
        self.lost_frame_count[name] = lost_count

    def add_detector(self, name: str, detection_diagnostics: dict) -> None:
        self.enabled_detectors.append(name)
        self.roi_used[name] = detection_diagnostics.get("roi_used")
        self.candidate_count[name] = int(detection_diagnostics.get("candidate_count", 0))
        failure_reason = detection_diagnostics.get("failure_reason")
        self.failure_reason[name] = failure_reason
        self.rejected_candidates[name] = [failure_reason] if failure_reason else []

    def add_stage_time(self, name: str, elapsed_ms: float) -> None:
        self.stage_time_ms[name] = elapsed_ms

    def build(self, processing_time_ms: float) -> VisionFrameDiagnostics:
        self.stage_time_ms.setdefault("total_processing", processing_time_ms)
        return VisionFrameDiagnostics(
            processing_time_ms=processing_time_ms,
            enabled_detectors=self.enabled_detectors,
            roi_used=self.roi_used,
            candidate_count=self.candidate_count,
            failure_reason=self.failure_reason,
            rejected_candidates=self.rejected_candidates,
            warnings=self.warnings,
            tracker_state=self.tracker_state,
            lost_frame_count=self.lost_frame_count,
            stage_time_ms=self.stage_time_ms,
            debug_artifacts=None,
        )

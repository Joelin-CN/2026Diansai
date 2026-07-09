from __future__ import annotations

import time
from pathlib import Path

import cv2
import numpy as np

from device.types import CameraPort
from vision.types import CameraFrame


class VideoFileCameraPort(CameraPort):
    def __init__(self, video_path: str | Path, camera_id: str = "video_file"):
        self._path = Path(video_path)
        self._camera_id = camera_id
        self._cap: cv2.VideoCapture | None = None
        self._frame_id: int = 0
        self._fps: float = 30.0
        self._start_time_ms: float = 0.0

    @property
    def width_px(self) -> int:
        return int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH)) if self._cap else 0

    @property
    def height_px(self) -> int:
        return int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) if self._cap else 0

    @property
    def fps(self) -> float:
        return self._fps

    def open(self) -> None:
        if not self._path.exists():
            raise FileNotFoundError(f"Video file not found: {self._path}")
        self._cap = cv2.VideoCapture(str(self._path))
        if not self._cap.isOpened():
            raise RuntimeError(f"Cannot open video: {self._path}")
        self._fps = self._cap.get(cv2.CAP_PROP_FPS)
        if self._fps <= 0:
            self._fps = 30.0
        self._frame_id = 0
        self._start_time_ms = time.time() * 1000.0

    def read(self) -> CameraFrame | None:
        if self._cap is None:
            raise RuntimeError("Camera not opened")
        ok, image = self._cap.read()
        if not ok:
            return None
        frame = CameraFrame(
            image=image,
            frame_id=self._frame_id,
            timestamp_ms=time.time() * 1000.0 - self._start_time_ms,
            camera_id=self._camera_id,
            width_px=self.width_px,
            height_px=self.height_px,
            color_format="BGR",
        )
        self._frame_id += 1
        return frame

    def close(self) -> None:
        if self._cap is not None:
            self._cap.release()
        self._cap = None

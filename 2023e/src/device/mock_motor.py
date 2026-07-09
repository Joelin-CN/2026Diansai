from __future__ import annotations

import csv
import time
from pathlib import Path

from control.types import MotorCommand
from device.types import MotorPort


class MockMotorPort(MotorPort):
    """离线验证用电机端口：将 MotorCommand 记录到 CSV 文件。"""

    def __init__(self, log_dir: str | Path = "outputs/mock_test"):
        self._log_dir = Path(log_dir)
        self._csv_path: Path | None = None
        self._writer: object | None = None
        self._file_handle: object | None = None
        self._start_time_ms: float = 0.0

    def open(self) -> None:
        self._log_dir.mkdir(parents=True, exist_ok=True)
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        self._csv_path = self._log_dir / f"motor_log_{timestamp}.csv"
        self._file_handle = open(str(self._csv_path), "w", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file_handle)
        self._writer.writerow(
            ["timestamp_ms", "frame_id", "yaw_delta_steps", "pitch_delta_steps", "enable"]
        )
        self._start_time_ms = time.time() * 1000.0

    def send(self, command: MotorCommand, *, frame_id: int = 0) -> None:
        if self._writer is None:
            raise RuntimeError("MockMotorPort not opened")
        elapsed_ms = time.time() * 1000.0 - self._start_time_ms
        self._writer.writerow(
            [
                f"{elapsed_ms:.1f}",
                frame_id,
                command.yaw_delta_steps,
                command.pitch_delta_steps,
                command.enable,
            ]
        )

    def close(self) -> None:
        if self._file_handle is not None:
            self._file_handle.close()
            self._file_handle = None
        self._writer = None
        if self._csv_path:
            print(f"[MockMotor] Log saved to: {self._csv_path}")

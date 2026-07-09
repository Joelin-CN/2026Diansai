from __future__ import annotations

import json
import logging
import sys
from pathlib import Path
from typing import Any

from application.runtime import RuntimeContext
from calibration.loader import load_calibration
from control.visual_servo import VisualServoController
from coordinate.transformer import CoordinateTransformer
from control.types import MotorCommand
from device.mock_motor import MockMotorPort
from device.types import CameraPort, MotorPort
from device.video_camera import VideoFileCameraPort
from kalman.laser_kalman import LaserKalmanFilter
from task.green_tasks import GreenTrackRedTask
from task.red_tasks import LazyPathFollowerTask
from task.types import TaskTarget
from vision.config import load_vision_config
from vision.pipeline import VisionPipeline
from vision.types import ImagePoint, VisionMode

logger = logging.getLogger(__name__)


class SystemFactory:
    """根据 system/mode/task 参数初始化完整的 RuntimeContext。"""

    @classmethod
    def create(
        cls,
        system: str,
        mode: str,
        task: str | None = None,
        device_config_path: str = "device/stage1_mock.json",
    ) -> RuntimeContext:
        if system not in ("red", "green"):
            raise ValueError(f"Unknown system: {system}. Must be 'red' or 'green'.")
        if mode not in ("calibration", "competition"):
            raise ValueError(f"Unknown mode: {mode}. Must be 'calibration' or 'competition'.")

        project_root = _project_root()

        # 1. 设备
        camera, motor = cls._create_devices(project_root / device_config_path)

        # 2. Vision
        vision_config = load_vision_config(project_root / "configs" / system / "vision.json")
        camera.open()
        frame = camera.read()
        if frame is None:
            camera.close()
            raise RuntimeError("Failed to read first frame from camera")
        frame_size = (frame.width_px, frame.height_px)
        camera.close()
        vision = VisionPipeline(profile=system, config=vision_config, frame_size=frame_size)

        # 3. 标定
        calibration_path = project_root / "calibration" / f"{system}_calibration.json"
        calibration = load_calibration(calibration_path)
        transformer = CoordinateTransformer(calibration.H_img_to_screen)
        controller = VisualServoController(calibration.servo_config)

        # 4. Kalman
        kalmans: dict[str, LaserKalmanFilter] = {}
        kalmans["red"] = LaserKalmanFilter()
        if system == "green":
            kalmans["green"] = LaserKalmanFilter()

        # 5. VisionMode
        vision_mode = cls._resolve_vision_mode(system, task)

        # 6. Task（绿色系统直接创建；红色系统延迟到首次获取到 quad 后初始化）
        task_obj = None
        if system == "green":
            task_obj = GreenTrackRedTask()
        if system == "red" and mode == "competition" and task is not None:
            task_obj = LazyPathFollowerTask.init_lazy(
                task_name=task,
                arrive_dist_mm=5.0,
            )

        return RuntimeContext(
            camera=camera,
            vision=vision,
            vision_mode=vision_mode,
            transformer=transformer,
            task=task_obj,
            controller=controller,
            motor=motor,
            kalmans=kalmans,
            system=system,
            mode=mode,
        )

    @classmethod
    def _create_devices(cls, config_path: Path) -> tuple[CameraPort, MotorPort]:
        with config_path.open("r", encoding="utf-8") as f:
            cfg = json.load(f)

        cam_cfg = cfg["camera"]
        cam_type = cam_cfg["type"]
        if cam_type == "video_file":
            camera = VideoFileCameraPort(video_path=cam_cfg["video_path"], camera_id=cam_cfg.get("camera_id", "video_file"))
        else:
            raise ValueError(f"Unknown camera type: {cam_type}")

        motor_cfg = cfg["motor"]
        motor_type = motor_cfg["type"]
        if motor_type == "mock":
            motor = MockMotorPort(log_dir=motor_cfg.get("log_dir", "outputs/mock_test"))
        else:
            raise ValueError(f"Unknown motor type: {motor_type}")

        return camera, motor

    @classmethod
    def _resolve_vision_mode(cls, system: str, task: str | None) -> VisionMode:
        if system == "green":
            return VisionMode.RUNTIME_TRACK_RED_GREEN
        if system == "red":
            if task == "a4":
                return VisionMode.RUNTIME_A4_RED
            if task == "screen":
                return VisionMode.RUNTIME_SCREEN_RED
            if task == "origin":
                return VisionMode.RUNTIME_SCREEN_RED
        raise ValueError(f"Unknown task for {system}: {task}")


def _project_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent

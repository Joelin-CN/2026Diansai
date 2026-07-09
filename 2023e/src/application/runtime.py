from __future__ import annotations

import csv
import logging
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from threading import Event, Thread

try:
    from pynput import keyboard as pynput_keyboard
    _HAS_PYNPUT = True
except ImportError:
    _HAS_PYNPUT = False

from control.types import MotorCommand
from coordinate.transformer import CoordinateTransformer
from coordinate.types import ScreenPoint
from device.types import CameraPort, MotorPort
from kalman.laser_kalman import LaserKalmanFilter
from task.green_tasks import GreenTrackRedTask
from task.red_tasks import LazyPathFollowerTask
from task.types import TaskTarget
from vision.pipeline import VisionPipeline
from vision.types import ImagePoint, VisionFrameResult, VisionMode

logger = logging.getLogger(__name__)

_should_exit = Event()


@dataclass
class RuntimeContext:
    camera: CameraPort
    vision: VisionPipeline
    vision_mode: VisionMode
    transformer: CoordinateTransformer
    task: LazyPathFollowerTask | GreenTrackRedTask | None
    controller: object  # VisualServoController
    motor: MotorPort
    kalmans: dict[str, LaserKalmanFilter]
    system: str
    mode: str


def start_keyboard_listener() -> Thread | None:
    """启动 Ctrl+Q 键盘监听线程。

    无 GUI 环境（pynput 不可用）跳过。
    """
    if not _HAS_PYNPUT:
        logger.warning("pynput not installed; keyboard listener disabled")
        return None

    _should_exit.clear()

    def on_press(key):
        try:
            if key == pynput_keyboard.KeyCode.from_char('q') and (
                hasattr(pynput_keyboard.Key, 'ctrl') and False  # Ctrl handler
            ):
                _should_exit.set()
        except Exception:
            return True

    # Use a global hook approach via keyboard listener
    def run_listener():
        with pynput_keyboard.Events() as events:
            for event in events:
                if hasattr(event, 'key') and event.key == pynput_keyboard.KeyCode.from_char('q'):
                    try:
                        # Check Ctrl modifier
                        from pynput.keyboard import Key
                        _should_exit.set()
                    except Exception:
                        pass

    t = Thread(target=run_listener, daemon=True)
    t.start()
    return t


def run(context: RuntimeContext, mode: str) -> None:
    """统一主循环。"""
    logger.info(f"Starting {mode} mode for {context.system} system")
    logger.info(f"Vision mode: {context.vision_mode}")
    logger.info("Press Ctrl+Q to exit")

    listener = start_keyboard_listener()

    try:
        context.camera.open()
        context.motor.open()

        frame_count = 0
        while not _should_exit.is_set():
            # 1. 读取图像
            frame = context.camera.read()
            if frame is None:
                logger.info("Video ended")
                break

            # 2. 视觉检测
            vision_result = context.vision.process(frame, mode=context.vision_mode)

            # 3. 历史填补
            current_detections = _collect_detections(vision_result)
            filled = _fill_from_history(current_detections, context)

            # 4. 标定验证模式：仅显示 + 跳过控制
            if context.mode == "calibration":
                _handle_calibration(
                    frame, filled, context.transformer,
                    context.kalmans.get("red"),
                    vision_result,
                )
                frame_count += 1
                continue

            # 5. Kalman 平滑
            smoothed = _apply_kalman(filled, context.kalmans)

            # 6. 坐标转换
            red_screen = _maybe_transform(context.transformer, smoothed.get("red"))
            green_screen = _maybe_transform(context.transformer, smoothed.get("green"))

            # 7. 任务
            current_point = red_screen
            target_point = _resolve_target(context, vision_result, red_screen, green_screen)

            if target_point is None:
                logger.debug(f"Frame {frame.frame_id}: no target available, skip")
                frame_count += 1
                continue

            # 8. 控制
            command = context.controller.update(
                target=target_point,
                current=current_point,
            )

            # 9. 设备输出
            context.motor.send(command)
            logger.debug(
                f"Frame {frame.frame_id}: "
                f"current=({current_point.x_mm:.1f},{current_point.y_mm:.1f}) "
                f"target=({target_point.x_mm:.1f},{target_point.y_mm:.1f}) "
                f"motor=(yaw={command.yaw_delta_steps}, pitch={command.pitch_delta_steps}, en={command.enable})"
            )

            frame_count += 1

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        if listener:
            pass  # daemon thread will exit
        context.camera.close()
        context.motor.close()
        logger.info(f"Finished. Total frames: {frame_count}")


def _collect_detections(result: VisionFrameResult) -> dict[str, ImagePoint | None]:
    detections: dict[str, ImagePoint | None] = {}
    if result.red_laser and result.red_laser.found and result.red_laser.image_center:
        detections["red"] = result.red_laser.image_center
    else:
        detections["red"] = None
    if result.green_laser and result.green_laser.found and result.green_laser.image_center:
        detections["green"] = result.green_laser.image_center
    else:
        detections["green"] = None
    return detections


def _fill_from_history(
    detections: dict[str, ImagePoint | None],
    context: RuntimeContext,
) -> dict[str, ImagePoint | None]:
    """检测丢失时沿用历史有效数据。"""
    if not hasattr(context, '_last_valid'):
        context._last_valid: dict[str, ImagePoint | None] = {"red": None, "green": None}  # type: ignore

    filled: dict[str, ImagePoint | None] = {}
    for laser in ("red", "green"):
        measured = detections.get(laser)
        if measured is not None:
            filled[laser] = measured
            context._last_valid[laser] = measured  # type: ignore
        else:
            filled[laser] = context._last_valid.get(laser)  # type: ignore
    return filled


def _apply_kalman(
    filled: dict[str, ImagePoint | None],
    kalmans: dict[str, LaserKalmanFilter],
) -> dict[str, ImagePoint | None]:
    smoothed: dict[str, ImagePoint | None] = {}
    for laser, kf in kalmans.items():
        measurement = filled.get(laser)
        if measurement is not None:
            smoothed[laser] = kf.update(measurement)
        elif kf._initialized:
            smoothed[laser] = kf.predict()
        else:
            smoothed[laser] = None
    return smoothed


def _maybe_transform(
    transformer: CoordinateTransformer,
    image_point: ImagePoint | None,
) -> ScreenPoint | None:
    if image_point is None:
        return None
    return transformer.image_point_to_screen(image_point)


def _resolve_target(
    context: RuntimeContext,
    vision_result: VisionFrameResult,
    red_screen: ScreenPoint | None,
    green_screen: ScreenPoint | None,
) -> ScreenPoint | None:
    """根据系统和任务计算目标点。"""
    system = context.system

    if system == "green":
        # 绿色追踪红点
        if red_screen is None:
            return None
        if context.task and isinstance(context.task, GreenTrackRedTask):
            target = context.task.next_target(red_screen)
            return target.point
        return red_screen

    if system == "red":
        if red_screen is None:
            return None
        # 延迟初始化任务
        if isinstance(context.task, LazyPathFollowerTask) and not context.task.is_initialized():
            if not context.task.try_initialize(vision_result, context.transformer):
                logger.debug("Waiting for first valid tape/screen detection...")
                return None
            logger.info(f"Red task initialized: {context.task.task_name}")
        if context.task:
            target = context.task.next_target(red_screen)
            return target.point

    return None


def _handle_calibration(
    frame: object,
    filled: dict[str, ImagePoint | None],
    transformer: CoordinateTransformer,
    red_kalman: LaserKalmanFilter | None,
    vision_result: VisionFrameResult,
) -> None:
    """标定验证模式：显示图像坐标和屏幕坐标。"""
    import cv2
    import numpy as np

    display = frame.image.copy()
    h, w = display.shape[:2]
    y_offset = 30

    lasers = [("red", (0, 0, 255)), ("green", (0, 255, 0))]
    for laser_tag, color in lasers:
        img_pt = filled.get(laser_tag)
        if img_pt is None:
            continue
        u, v = int(img_pt.u_px), int(img_pt.v_px)
        cv2.circle(display, (u, v), 5, color, -1)

        try:
            screen_pt = transformer.image_point_to_screen(img_pt)
            text = f"[{laser_tag}] img: ({img_pt.u_px:.0f},{img_pt.v_px:.0f}) -> screen: ({screen_pt.x_mm:.1f},{screen_pt.y_mm:.1f}) mm"
        except Exception:
            text = f"[{laser_tag}] img: ({img_pt.u_px:.0f},{img_pt.v_px:.0f}) -> screen: ERR"

        cv2.putText(display, text, (10, y_offset), cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 2)
        y_offset += 28

    # 帧信息
    cv2.putText(
        display,
        f"Frame: {vision_result.frame_id} | Press Ctrl+Q to exit",
        (10, h - 12),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (255, 255, 255),
        1,
    )

    cv2.imshow("Calibration Verification", display)
    cv2.waitKey(1)

from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from calibration.types import CalibrationResult
from control.visual_servo import ServoConfig


def load_calibration(path: str | Path) -> CalibrationResult:
    with Path(path).open("r", encoding="utf-8") as f:
        raw = json.load(f)

    H = np.array(raw["H_img_to_screen"], dtype=np.float32)
    if H.shape != (3, 3):
        raise ValueError(f"H_img_to_screen must be 3x3, got {H.shape}")

    sc = raw["servo_config"]
    servo = ServoConfig(
        yaw_kp_steps_per_mm=float(sc["yaw_kp_steps_per_mm"]),
        pitch_kp_steps_per_mm=float(sc["pitch_kp_steps_per_mm"]),
        deadband_mm=float(sc["deadband_mm"]),
        max_yaw_delta_steps=int(sc["max_yaw_delta_steps"]),
        max_pitch_delta_steps=int(sc["max_pitch_delta_steps"]),
    )
    return CalibrationResult(H_img_to_screen=H, servo_config=servo)


def save_calibration(calib: CalibrationResult, path: str | Path) -> None:
    data = {
        "H_img_to_screen": calib.H_img_to_screen.tolist(),
        "servo_config": {
            "yaw_kp_steps_per_mm": calib.servo_config.yaw_kp_steps_per_mm,
            "pitch_kp_steps_per_mm": calib.servo_config.pitch_kp_steps_per_mm,
            "deadband_mm": calib.servo_config.deadband_mm,
            "max_yaw_delta_steps": calib.servo_config.max_yaw_delta_steps,
            "max_pitch_delta_steps": calib.servo_config.max_pitch_delta_steps,
        },
    }
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with Path(path).open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

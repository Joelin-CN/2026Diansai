from __future__ import annotations

from dataclasses import dataclass

from control.types import MotorCommand
from coordinate.types import ScreenPoint


@dataclass(frozen=True)
class ServoConfig:
    yaw_kp_steps_per_mm: float = 1.0
    pitch_kp_steps_per_mm: float = 1.0
    deadband_mm: float = 3.0
    max_yaw_delta_steps: int = 80
    max_pitch_delta_steps: int = 80


class VisualServoController:
    def __init__(self, config: ServoConfig):
        self.config = config

    def update(self, target: ScreenPoint, current: ScreenPoint) -> MotorCommand:
        error_x = target.x_mm - current.x_mm
        error_y = target.y_mm - current.y_mm

        if abs(error_x) < self.config.deadband_mm:
            error_x = 0.0
        if abs(error_y) < self.config.deadband_mm:
            error_y = 0.0

        yaw_steps = int(error_x * self.config.yaw_kp_steps_per_mm)
        pitch_steps = int(error_y * self.config.pitch_kp_steps_per_mm)

        yaw_steps = _clamp(yaw_steps, self.config.max_yaw_delta_steps)
        pitch_steps = _clamp(pitch_steps, self.config.max_pitch_delta_steps)
        return MotorCommand(yaw_delta_steps=yaw_steps, pitch_delta_steps=pitch_steps)


def _clamp(value: int, limit: int) -> int:
    limit = abs(limit)
    return max(-limit, min(limit, value))

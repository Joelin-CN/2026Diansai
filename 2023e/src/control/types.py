from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class MotorCommand:
    yaw_delta_steps: int
    pitch_delta_steps: int
    enable: bool = True

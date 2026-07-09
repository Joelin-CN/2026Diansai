from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from control.visual_servo import ServoConfig


@dataclass
class CalibrationResult:
    """标定产物最小必要字段。

    Attributes:
        H_img_to_screen: 3x3 homography matrix, ImagePoint -> ScreenPoint.
        servo_config: yaw/pitch 控制增益与限幅参数。
    """

    H_img_to_screen: np.ndarray
    servo_config: ServoConfig

    def __post_init__(self) -> None:
        self.H_img_to_screen = np.asarray(self.H_img_to_screen, dtype=np.float32)
        if self.H_img_to_screen.shape != (3, 3):
            raise ValueError("H_img_to_screen must be a 3x3 matrix")

from __future__ import annotations

import numpy as np

from vision.types import ImagePoint


class LaserKalmanFilter:
    """对单个激光点的图像坐标做 Kalman 平滑。

    状态: [u, v, du, dv]  (位置 + 速度, 图像坐标系)
    观测: [u, v]
    """

    def __init__(self, process_noise: float = 1e-3, measurement_noise: float = 1e-1):
        self._dt: float = 1.0 / 30.0  # default 30fps, will be updated
        self._x = np.zeros((4, 1), dtype=np.float32)
        self._P = np.eye(4, dtype=np.float32) * 1000.0
        self._initialized: bool = False

        # Transition matrix
        self._F = np.array([
            [1, 0, self._dt, 0],
            [0, 1, 0, self._dt],
            [0, 0, 1, 0],
            [0, 0, 0, 1],
        ], dtype=np.float32)

        # Measurement matrix (observe position only)
        self._H = np.array([
            [1, 0, 0, 0],
            [0, 1, 0, 0],
        ], dtype=np.float32)

        self._Q = np.eye(4, dtype=np.float32) * process_noise
        self._R = np.eye(2, dtype=np.float32) * measurement_noise

    @property
    def dt(self) -> float:
        return self._dt

    @dt.setter
    def dt(self, value: float) -> None:
        if value <= 0:
            return
        self._dt = value
        self._F[0, 2] = self._dt
        self._F[1, 3] = self._dt

    def predict(self) -> ImagePoint:
        """无观测时的纯预测，用于丢失帧。"""
        self._x = self._F @ self._x
        self._P = self._F @ self._P @ self._F.T + self._Q
        return ImagePoint(
            u_px=float(self._x[0, 0]),
            v_px=float(self._x[1, 0]),
        )

    def update(self, measurement: ImagePoint) -> ImagePoint:
        """有观测时先预测再更新。"""
        z = np.array([[measurement.u_px], [measurement.v_px]], dtype=np.float32)

        if not self._initialized:
            self._x[0, 0] = measurement.u_px
            self._x[1, 0] = measurement.v_px
            self._initialized = True
            return measurement

        # Predict
        self._x = self._F @ self._x
        self._P = self._F @ self._P @ self._F.T + self._Q

        # Update
        y = z - self._H @ self._x
        S = self._H @ self._P @ self._H.T + self._R
        K = self._P @ self._H.T @ np.linalg.inv(S)
        self._x = self._x + K @ y
        self._P = (np.eye(4, dtype=np.float32) - K @ self._H) @ self._P

        return ImagePoint(
            u_px=float(self._x[0, 0]),
            v_px=float(self._x[1, 0]),
        )

    def reset(self) -> None:
        self._x = np.zeros((4, 1), dtype=np.float32)
        self._P = np.eye(4, dtype=np.float32) * 1000.0
        self._initialized = False

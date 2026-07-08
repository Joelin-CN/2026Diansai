from __future__ import annotations

import numpy as np

from coordinate.types import ScreenPoint, ScreenQuad
from vision.types import ImagePoint, ImageQuad


class CoordinateTransformer:
    def __init__(self, H_img_to_screen: np.ndarray | list[list[float]]):
        self.H_img_to_screen = np.asarray(H_img_to_screen, dtype=np.float32)
        if self.H_img_to_screen.shape != (3, 3):
            raise ValueError("H_img_to_screen must be a 3x3 matrix")

    def image_point_to_screen(self, point: ImagePoint) -> ScreenPoint:
        src = np.array([point.u_px, point.v_px, 1.0], dtype=np.float32)
        dst = self.H_img_to_screen @ src
        if dst[2] == 0:
            raise ZeroDivisionError("homography produced a point at infinity")
        x_mm = dst[0] / dst[2]
        y_mm = dst[1] / dst[2]
        return ScreenPoint(float(x_mm), float(y_mm))

    def image_quad_to_screen(self, quad: ImageQuad) -> ScreenQuad:
        return ScreenQuad(tuple(self.image_point_to_screen(point) for point in quad.points))

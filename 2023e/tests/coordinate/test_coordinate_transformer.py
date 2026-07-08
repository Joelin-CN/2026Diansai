import numpy as np
import pytest

from coordinate.quad import tape_centerline_quad
from coordinate.transformer import CoordinateTransformer
from coordinate.types import ScreenPoint, ScreenQuad
from vision.types import ImagePoint, ImageQuad


def test_image_point_to_screen_uses_homography_fake_data():
    H = np.array(
        [
            [2.0, 0.0, 10.0],
            [0.0, 3.0, 20.0],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )
    transformer = CoordinateTransformer(H)

    point = transformer.image_point_to_screen(ImagePoint(u_px=5, v_px=7))

    assert point == ScreenPoint(x_mm=20.0, y_mm=41.0)


def test_image_quad_to_screen_converts_all_four_points():
    transformer = CoordinateTransformer(np.eye(3, dtype=np.float32))
    image_quad = ImageQuad(
        points=(
            ImagePoint(0, 0),
            ImagePoint(100, 0),
            ImagePoint(100, 50),
            ImagePoint(0, 50),
        )
    )

    screen_quad = transformer.image_quad_to_screen(image_quad)

    assert screen_quad.points == (
        ScreenPoint(0.0, 0.0),
        ScreenPoint(100.0, 0.0),
        ScreenPoint(100.0, 50.0),
        ScreenPoint(0.0, 50.0),
    )


def test_tape_centerline_quad_averages_outer_and_inner_corners():
    outer = ScreenQuad(
        points=(
            ScreenPoint(0, 0),
            ScreenPoint(120, 0),
            ScreenPoint(120, 80),
            ScreenPoint(0, 80),
        )
    )
    inner = ScreenQuad(
        points=(
            ScreenPoint(20, 10),
            ScreenPoint(100, 10),
            ScreenPoint(100, 70),
            ScreenPoint(20, 70),
        )
    )

    center = tape_centerline_quad(outer, inner)

    assert center.points == (
        ScreenPoint(10.0, 5.0),
        ScreenPoint(110.0, 5.0),
        ScreenPoint(110.0, 75.0),
        ScreenPoint(10.0, 75.0),
    )


def test_screen_quad_rejects_non_four_point_input():
    with pytest.raises(ValueError):
        ScreenQuad(points=(ScreenPoint(0, 0), ScreenPoint(1, 1)))

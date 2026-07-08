from __future__ import annotations

from coordinate.types import ScreenPoint, ScreenQuad


def midpoint(a: ScreenPoint, b: ScreenPoint) -> ScreenPoint:
    return ScreenPoint(
        x_mm=(a.x_mm + b.x_mm) / 2.0,
        y_mm=(a.y_mm + b.y_mm) / 2.0,
    )


def tape_centerline_quad(outer: ScreenQuad, inner: ScreenQuad) -> ScreenQuad:
    return ScreenQuad(tuple(midpoint(outer_point, inner_point) for outer_point, inner_point in zip(outer.points, inner.points)))

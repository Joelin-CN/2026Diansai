from __future__ import annotations

from coordinate.types import ScreenPoint, ScreenQuad


def interpolate_quad_path(quad: ScreenQuad, points_per_edge: int = 40) -> list[ScreenPoint]:
    if points_per_edge <= 0:
        raise ValueError("points_per_edge must be positive")

    corners = list(quad.points)
    path: list[ScreenPoint] = []
    for i in range(4):
        start = corners[i]
        end = corners[(i + 1) % 4]
        for step in range(points_per_edge):
            ratio = step / points_per_edge
            path.append(
                ScreenPoint(
                    x_mm=start.x_mm * (1.0 - ratio) + end.x_mm * ratio,
                    y_mm=start.y_mm * (1.0 - ratio) + end.y_mm * ratio,
                )
            )
    return path

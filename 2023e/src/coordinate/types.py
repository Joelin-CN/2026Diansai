from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ScreenPoint:
    x_mm: float
    y_mm: float


@dataclass(frozen=True)
class ScreenQuad:
    points: tuple[ScreenPoint, ScreenPoint, ScreenPoint, ScreenPoint]

    def __post_init__(self) -> None:
        if len(self.points) != 4:
            raise ValueError("ScreenQuad must contain exactly four points")

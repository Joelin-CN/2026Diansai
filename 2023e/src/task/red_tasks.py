from __future__ import annotations

import math

from coordinate.types import ScreenPoint
from task.types import TaskTarget


class PathFollowerTask:
    def __init__(self, path: list[ScreenPoint], arrive_dist_mm: float = 5.0):
        if not path:
            raise ValueError("path must not be empty")
        if arrive_dist_mm < 0:
            raise ValueError("arrive_dist_mm must not be negative")
        self.path = path
        self.arrive_dist_mm = arrive_dist_mm
        self.index = 0

    def next_target(self, current: ScreenPoint) -> TaskTarget:
        target = self.path[self.index]
        if self._distance(current, target) <= self.arrive_dist_mm:
            self.index = (self.index + 1) % len(self.path)
            target = self.path[self.index]
        return TaskTarget(point=target)

    def _distance(self, a: ScreenPoint, b: ScreenPoint) -> float:
        return math.hypot(a.x_mm - b.x_mm, a.y_mm - b.y_mm)

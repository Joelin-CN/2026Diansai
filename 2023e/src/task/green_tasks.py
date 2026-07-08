from __future__ import annotations

from coordinate.types import ScreenPoint
from task.types import TaskTarget


class GreenTrackRedTask:
    def next_target(self, detected_red_point: ScreenPoint) -> TaskTarget:
        return TaskTarget(point=detected_red_point)

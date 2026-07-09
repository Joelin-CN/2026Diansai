from __future__ import annotations

from coordinate.types import ScreenPoint
from task.types import TaskTarget


class GreenTrackRedTask:
    """绿色系统追踪任务：目标 = 检测到的红点屏幕坐标。"""

    def next_target(self, detected_red_point: ScreenPoint) -> TaskTarget:
        return TaskTarget(point=detected_red_point)

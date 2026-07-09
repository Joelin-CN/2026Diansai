from __future__ import annotations

import math
from typing import TYPE_CHECKING

from coordinate.types import ScreenPoint
from coordinate.quad import tape_centerline_quad
from task.path import interpolate_quad_path
from task.types import TaskTarget

if TYPE_CHECKING:
    from coordinate.transformer import CoordinateTransformer
    from vision.types import VisionFrameResult


class PathFollowerTask:
    """路径跟随任务：沿给定路径点顺时针循环移动。"""

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


class LazyPathFollowerTask:
    """延迟初始化的路径跟随任务。

    等待首次视觉检测到 tape_quad 或 screen_detection 后，
    转换 Quad → centerline → path。
    """

    def __init__(
        self,
        task_name: str,
        arrive_dist_mm: float = 5.0,
        points_per_edge: int = 40,
    ):
        self.task_name = task_name
        self.arrive_dist_mm = arrive_dist_mm
        self.points_per_edge = points_per_edge
        self._inner: PathFollowerTask | None = None
        self._init_done: bool = False

    @classmethod
    def init_lazy(
        cls,
        task_name: str,
        arrive_dist_mm: float = 5.0,
        points_per_edge: int = 40,
    ) -> "LazyPathFollowerTask":
        return cls(
            task_name=task_name,
            arrive_dist_mm=arrive_dist_mm,
            points_per_edge=points_per_edge,
        )

    def is_initialized(self) -> bool:
        return self._init_done

    def try_initialize(
        self,
        vision_result: "VisionFrameResult",
        transformer: "CoordinateTransformer",
    ) -> bool:
        """尝试从 VisionFrameResult 生成路径，成功返回 True。"""
        if self.task_name == "origin":
            self._init_origin(transformer, vision_result)
            return True

        if self.task_name == "a4":
            return self._init_from_tape(vision_result, transformer)

        if self.task_name == "screen":
            return self._init_from_screen(vision_result, transformer)

        return False

    def _init_origin(self, transformer: "CoordinateTransformer", vision_result: "VisionFrameResult") -> None:
        """回原点任务：将当前红点位置设为目标。"""
        if vision_result.red_laser and vision_result.red_laser.found and vision_result.red_laser.image_center:
            origin = transformer.image_point_to_screen(vision_result.red_laser.image_center)
            self._inner = PathFollowerTask(
                path=[ScreenPoint(origin.x_mm, origin.y_mm)],
                arrive_dist_mm=0.0,
            )
            self._init_done = True

    def _init_from_tape(
        self,
        vision_result: "VisionFrameResult",
        transformer: "CoordinateTransformer",
    ) -> bool:
        """从 A4 黑胶带检测生成中心线路径。"""
        if vision_result.tape_quad is None or not vision_result.tape_quad.found:
            return False
        if vision_result.tape_quad.outer_quad is None or vision_result.tape_quad.inner_quad is None:
            return False

        outer_screen = transformer.image_quad_to_screen(vision_result.tape_quad.outer_quad)
        inner_screen = transformer.image_quad_to_screen(vision_result.tape_quad.inner_quad)
        centerline = tape_centerline_quad(outer_screen, inner_screen)
        path = interpolate_quad_path(centerline, points_per_edge=self.points_per_edge)

        self._inner = PathFollowerTask(path=path, arrive_dist_mm=self.arrive_dist_mm)
        self._init_done = True
        return True

    def _init_from_screen(
        self,
        vision_result: "VisionFrameResult",
        transformer: "CoordinateTransformer",
    ) -> bool:
        """从屏幕铅笔方框检测生成路径。"""
        if vision_result.screen_detection is None or not vision_result.screen_detection.found:
            return False
        if vision_result.screen_detection.image_corners is None:
            return False

        from coordinate.types import ScreenQuad
        corners = vision_result.screen_detection.image_corners
        if len(corners) != 4:
            return False

        screen_points = tuple(transformer.image_point_to_screen(c) for c in corners)
        screen_quad = ScreenQuad(screen_points)
        path = interpolate_quad_path(screen_quad, points_per_edge=self.points_per_edge)

        self._inner = PathFollowerTask(path=path, arrive_dist_mm=self.arrive_dist_mm)
        self._init_done = True
        return True

    def next_target(self, current: ScreenPoint) -> TaskTarget:
        if not self._init_done or self._inner is None:
            # 未初始化时返回当前位置作为目标（原地等待）
            return TaskTarget(point=current)
        return self._inner.next_target(current)

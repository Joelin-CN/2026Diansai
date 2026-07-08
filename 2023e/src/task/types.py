from __future__ import annotations

from dataclasses import dataclass

from coordinate.types import ScreenPoint


@dataclass(frozen=True)
class TaskTarget:
    point: ScreenPoint
    done: bool = False

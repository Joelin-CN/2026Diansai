from __future__ import annotations

from abc import ABC, abstractmethod

from control.types import MotorCommand
from vision.types import CameraFrame


class CameraPort(ABC):
    @abstractmethod
    def open(self) -> None:
        ...

    @abstractmethod
    def read(self) -> CameraFrame | None:
        """返回下一帧，视频结束时返回 None。"""
        ...

    @abstractmethod
    def close(self) -> None:
        ...


class MotorPort(ABC):
    @abstractmethod
    def send(self, command: MotorCommand) -> None:
        ...

    @abstractmethod
    def open(self) -> None:
        ...

    @abstractmethod
    def close(self) -> None:
        ...

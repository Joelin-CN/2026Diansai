"""Target lock, loss handling, smoothing and pixel-error output."""

import numpy as np


class TargetTrackingResult(object):
    """单帧跟踪结果；``error`` 为 None 时禁止驱动云台。"""

    def __init__(self, state, found, locked, raw_center=None, center=None,
                 error=None, area_ratio=0.0, lost_frames=0):
        self.state = state
        self.found = bool(found)
        self.locked = bool(locked)
        self.raw_center = raw_center
        self.center = center
        self.error = error
        self.area_ratio = float(area_ratio)
        self.lost_frames = int(lost_frames)


class TargetTracker(object):
    """将黑框检测结果变成可安全交给控制端的像素误差。

    状态含义：
    - SEARCH：当前无可靠目标；
    - ACQUIRING：连续检测中，尚未达到锁定帧数；
    - LOCKED：允许输出 ``error``；
    - LOST：刚从锁定状态丢失，输出被立即禁止。
    """

    SEARCH = "SEARCH"
    ACQUIRING = "ACQUIRING"
    LOCKED = "LOCKED"
    LOST = "LOST"

    def __init__(self, reference_x, reference_y, lock_frames=3,
                 lost_frames=3, smooth_alpha=0.35, max_center_jump=90):
        self.reference = np.array([reference_x, reference_y], dtype=np.float32)
        self.lock_frames_required = max(1, int(lock_frames))
        self.lost_frames_limit = max(1, int(lost_frames))
        self.smooth_alpha = min(1.0, max(0.0, float(smooth_alpha)))
        # 720p@30fps 下，单帧数百像素跳变通常是误检而非真实靶纸运动。
        # 设为 None 可关闭；实际安装后可按最高云台扫描速度调整。
        self.max_center_jump = (
            None if max_center_jump is None else max(0.0, float(max_center_jump))
        )
        self.reset()

    def reset(self):
        """清空锁定状态；可在模式切换或云台复位时调用。"""
        self.state = self.SEARCH
        self.hit_frames = 0
        self.miss_frames = 0
        self.smoothed_center = None

    def update(self, detection):
        """输入本帧黑框检测结果，返回平滑中心与有效像素误差。"""
        if detection is None:
            return self._handle_lost_target()

        raw_center = np.asarray(detection.center, dtype=np.float32)
        if (self.smoothed_center is not None
                and self.max_center_jump is not None
                and np.linalg.norm(raw_center - self.smoothed_center)
                > self.max_center_jump):
            # 不让偶发环境误检改变当前锁定目标，更不能输出错误控制量。
            return self._handle_lost_target()

        self.miss_frames = 0
        if self.state != self.LOCKED:
            self.hit_frames += 1
        else:
            self.hit_frames = self.lock_frames_required

        # 一阶低通仅作用于中心坐标，避免小车/云台微振造成电机抖动。
        if self.smoothed_center is None:
            self.smoothed_center = raw_center
        else:
            alpha = self.smooth_alpha
            self.smoothed_center = (
                (1.0 - alpha) * self.smoothed_center + alpha * raw_center
            )

        locked = self.hit_frames >= self.lock_frames_required
        self.state = self.LOCKED if locked else self.ACQUIRING
        center = tuple(float(value) for value in self.smoothed_center)
        error = None
        if locked:
            # 正值表示靶心位于标定参考点的右方/下方；控制端决定电机正反。
            error_vector = self.smoothed_center - self.reference
            error = tuple(float(value) for value in error_vector)

        return TargetTrackingResult(
            state=self.state,
            found=True,
            locked=locked,
            raw_center=tuple(float(value) for value in raw_center),
            center=center,
            error=error,
            area_ratio=detection.area_ratio,
            lost_frames=0,
        )

    def _handle_lost_target(self):
        was_locked = self.state in (self.LOCKED, self.LOST)
        self.hit_frames = 0
        self.miss_frames += 1

        if was_locked and self.miss_frames < self.lost_frames_limit:
            self.state = self.LOST
        else:
            self.state = self.SEARCH
            if self.miss_frames >= self.lost_frames_limit:
                self.smoothed_center = None

        return TargetTrackingResult(
            state=self.state,
            found=False,
            locked=False,
            # 不保留上一帧误差，避免目标丢失后云台继续朝旧方向转动。
            error=None,
            lost_frames=self.miss_frames,
        )

"""与电机协议无关的自动搜索、瞄准、锁定和发射状态机。"""

from __future__ import print_function


class AimControlCommand(object):
    """一个控制周期的输出；角度均为增量，不是绝对位置。"""

    def __init__(self, state, yaw_deg=0.0, pitch_deg=0.0,
                 fire=False, target_found=False, lock_count=0):
        self.state = str(state)
        self.yaw_deg = float(yaw_deg)
        self.pitch_deg = float(pitch_deg)
        self.fire = bool(fire)
        self.target_found = bool(target_found)
        self.lock_count = int(lock_count)


class AutoAimController(object):
    """把视觉解算结果转换为搜索/瞄准/发射控制命令。

    ``aim_offsets`` 为 ``LaserAimSolver`` 的输出；目标不可见或解算失败
    时传入 ``None``。本类不接触 GPIO、串口和 OpenCV，便于先在
    PyBullet 中验证，再连接 PD42S1 的实际协议。
    """

    SEARCH = "SEARCH"
    AIM = "AIM"
    LOCK = "LOCK"
    FIRE = "FIRE"
    TIMEOUT = "TIMEOUT"

    def __init__(
            self, time_limit_s, lock_frames=3, lost_frames=2,
            lock_tolerance_yaw_deg=0.12, lock_tolerance_pitch_deg=0.12,
            search_yaw_rate_deg_s=180.0, search_pitch_deg=0.0,
            search_pitch_rate_deg_s=90.0):
        if time_limit_s <= 0:
            raise ValueError("time_limit_s must be positive")
        if lock_frames <= 0 or lost_frames <= 0:
            raise ValueError("frame counts must be positive")
        self.time_limit_s = float(time_limit_s)
        self.lock_frames_required = int(lock_frames)
        self.lost_frames_required = int(lost_frames)
        self.lock_tolerance_yaw_deg = abs(float(lock_tolerance_yaw_deg))
        self.lock_tolerance_pitch_deg = abs(float(lock_tolerance_pitch_deg))
        self.search_yaw_rate_deg_s = float(search_yaw_rate_deg_s)
        self.search_pitch_deg = float(search_pitch_deg)
        self.search_pitch_rate_deg_s = abs(float(search_pitch_rate_deg_s))
        self.reset()

    def reset(self):
        self.state = self.SEARCH
        self.elapsed_s = 0.0
        self.lock_count = 0
        self.lost_count = 0
        self.fire_latched = False

    def update(self, aim_offsets, dt_s, current_pitch_deg=0.0):
        """推进一个控制周期并返回角度增量和单次发射标志。"""
        dt_s = float(dt_s)
        if dt_s <= 0:
            raise ValueError("dt_s must be positive")
        if self.fire_latched:
            return AimControlCommand(self.FIRE)

        self.elapsed_s += dt_s
        if self.elapsed_s > self.time_limit_s:
            self.state = self.TIMEOUT
            return AimControlCommand(self.TIMEOUT)

        if aim_offsets is None:
            self.lock_count = 0
            self.lost_count += 1
            # 短暂丢帧先保持不动，避免一帧检测失败就重新高速扫描。
            if self.lost_count < self.lost_frames_required:
                return AimControlCommand(self.state)
            self.state = self.SEARCH
            pitch_error = self.search_pitch_deg - float(current_pitch_deg)
            max_pitch_step = self.search_pitch_rate_deg_s * dt_s
            pitch_step = max(-max_pitch_step, min(max_pitch_step, pitch_error))
            return AimControlCommand(
                self.SEARCH,
                yaw_deg=self.search_yaw_rate_deg_s * dt_s,
                pitch_deg=pitch_step,
            )

        self.lost_count = 0
        yaw = float(aim_offsets.yaw_deg)
        pitch = float(aim_offsets.pitch_deg)
        inside_tolerance = (
            abs(yaw) <= self.lock_tolerance_yaw_deg
            and abs(pitch) <= self.lock_tolerance_pitch_deg
        )
        if inside_tolerance:
            self.lock_count += 1
            self.state = self.LOCK
        else:
            self.lock_count = 0
            self.state = self.AIM

        if self.lock_count >= self.lock_frames_required:
            self.state = self.FIRE
            self.fire_latched = True
            return AimControlCommand(
                self.FIRE, yaw, pitch, fire=True,
                target_found=True, lock_count=self.lock_count,
            )

        return AimControlCommand(
            self.state, yaw, pitch, target_found=True,
            lock_count=self.lock_count,
        )

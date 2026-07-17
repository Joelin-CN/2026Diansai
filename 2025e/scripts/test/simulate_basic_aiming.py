"""PyBullet simulation for 2025 E basic requirements (2) and (3).

The car stays fixed. The simulated vision system projects the four A4 target
corners into the camera, then the production LaserAimSolver outputs only two
incremental commands: yaw offset and pitch offset.
"""

from __future__ import print_function

import argparse
import math
import os
import random
import sys
import time

import numpy as np
import pybullet as p


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
SRC_DIR = os.path.join(PROJECT_DIR, "src")
if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)

from coordinate.laser_aim_solver import LaserAimSolver  # noqa: E402
from control.auto_aim_controller import AutoAimController  # noqa: E402


IMAGE_WIDTH = 1280
IMAGE_HEIGHT = 720
CAMERA_MATRIX = np.array([
    [700.0, 0.0, IMAGE_WIDTH / 2.0],
    [0.0, 700.0, IMAGE_HEIGHT / 2.0],
    [0.0, 0.0, 1.0],
], dtype=np.float64)
# A4 目标靶横放：水平方向 297 mm，竖直方向 210 mm。
TARGET_WIDTH_M = 0.297
TARGET_HEIGHT_M = 0.210
TARGET_CENTER = np.array([0.5, -0.5, 0.25], dtype=np.float64)
GIMBAL_HEIGHT_M = 0.18
CONTROL_FPS = 30.0
DT = 1.0 / CONTROL_FPS

# Example mounting calibration. The laser is 3 cm right and 1 cm above the
# camera, and its mechanical axis has a small fixed angular bias.
LASER_ORIGIN_CAMERA = np.array([0.03, -0.01, 0.0], dtype=np.float64)
LASER_AXIS_BIAS_DEG = np.array([0.8, -0.5], dtype=np.float64)


def wrap_angle(angle):
    return (angle + math.pi) % (2.0 * math.pi) - math.pi


def normalize(vector):
    length = float(np.linalg.norm(vector))
    if length <= 1e-12:
        return vector
    return vector / length


def camera_basis(global_yaw, pitch):
    """Return world-space forward, right and up axes for the camera."""
    forward = np.array([
        math.cos(pitch) * math.cos(global_yaw),
        math.cos(pitch) * math.sin(global_yaw),
        math.sin(pitch),
    ], dtype=np.float64)
    right = np.array([
        -math.sin(global_yaw), math.cos(global_yaw), 0.0
    ], dtype=np.float64)
    up = normalize(np.cross(forward, right))
    return normalize(forward), normalize(right), up


def target_world_corners():
    half_width = TARGET_WIDTH_M / 2.0
    half_height = TARGET_HEIGHT_M / 2.0
    return np.array([
        TARGET_CENTER + (-half_width, 0.0, +half_height),
        TARGET_CENTER + (+half_width, 0.0, +half_height),
        TARGET_CENTER + (+half_width, 0.0, -half_height),
        TARGET_CENTER + (-half_width, 0.0, -half_height),
    ], dtype=np.float64)


def project_target(camera_origin, global_yaw, pitch, noise_pixels, rng):
    """Project TL/TR/BR/BL target corners; return None unless fully visible."""
    forward, right, up = camera_basis(global_yaw, pitch)
    projected = []
    for world_point in target_world_corners():
        relative = world_point - camera_origin
        camera_x = float(np.dot(relative, right))
        camera_y = float(np.dot(relative, -up))
        camera_z = float(np.dot(relative, forward))
        if camera_z <= 0.05:
            return None
        image_x = CAMERA_MATRIX[0, 2] + CAMERA_MATRIX[0, 0] * camera_x / camera_z
        image_y = CAMERA_MATRIX[1, 2] + CAMERA_MATRIX[1, 1] * camera_y / camera_z
        image_x += rng.gauss(0.0, noise_pixels)
        image_y += rng.gauss(0.0, noise_pixels)
        if not (0 <= image_x < IMAGE_WIDTH and 0 <= image_y < IMAGE_HEIGHT):
            return None
        projected.append((image_x, image_y))
    return np.asarray(projected, dtype=np.float32)


def laser_ray(camera_origin, global_yaw, pitch):
    """Return laser origin/direction after applying mounting offset and bias."""
    forward, right, up = camera_basis(global_yaw, pitch)
    origin = (
        camera_origin
        + right * LASER_ORIGIN_CAMERA[0]
        - up * LASER_ORIGIN_CAMERA[1]
        + forward * LASER_ORIGIN_CAMERA[2]
    )
    yaw_bias = math.radians(float(LASER_AXIS_BIAS_DEG[0]))
    pitch_bias = math.radians(float(LASER_AXIS_BIAS_DEG[1]))
    direction = normalize(
        forward
        + right * math.tan(yaw_bias)
        + up * math.tan(pitch_bias)
    )
    return origin, direction


def sample_track_position(rng):
    """Sample one specified position on the 100 cm square track."""
    distance = rng.random()
    side = rng.randrange(4)
    if side == 0:
        return np.array([distance, 0.0], dtype=np.float64)
    if side == 1:
        return np.array([1.0, distance], dtype=np.float64)
    if side == 2:
        return np.array([1.0 - distance, 1.0], dtype=np.float64)
    return np.array([0.0, 1.0 - distance], dtype=np.float64)


class SimulationResult(object):
    def __init__(self, success, elapsed, miss_distance, visible_time):
        self.success = bool(success)
        self.elapsed = float(elapsed)
        self.miss_distance = float(miss_distance)
        self.visible_time = visible_time


class BasicAimingSimulation(object):
    def __init__(self, gui=False, seed=1, pixel_noise=0.20,
                 playback_speed=1.0):
        self.gui = bool(gui)
        self.rng = random.Random(seed)
        self.pixel_noise = float(pixel_noise)
        self.playback_speed = float(playback_speed)
        if self.playback_speed <= 0:
            raise ValueError("playback_speed must be positive")
        connection_mode = p.GUI if self.gui else p.DIRECT
        self.client = p.connect(connection_mode)
        if self.client < 0:
            raise RuntimeError("Cannot connect to PyBullet")
        p.setGravity(0, 0, -9.81, physicsClientId=self.client)
        p.setTimeStep(DT, physicsClientId=self.client)
        self.target_body = None
        self.car_body = None
        self.laser_line = -1
        self.camera_axis_line = -1
        self.status_text = -1
        self._build_scene()

        self.solver = LaserAimSolver(
            CAMERA_MATRIX,
            target_width_m=TARGET_WIDTH_M,
            target_height_m=TARGET_HEIGHT_M,
            laser_origin_camera=LASER_ORIGIN_CAMERA,
            laser_axis_bias_deg=LASER_AXIS_BIAS_DEG,
            max_yaw_step_deg=12.0,
            max_pitch_step_deg=8.0,
            deadband_deg=0.02,
        )

    def close(self):
        if self.client >= 0:
            p.disconnect(self.client)
            self.client = -1

    def _build_scene(self):
        if self.gui:
            # 隐藏 ExampleBrowser 的 RGB/Depth/Mask 小窗和右侧参数栏。
            p.configureDebugVisualizer(
                p.COV_ENABLE_GUI, 0, physicsClientId=self.client
            )
            for preview_flag in (
                    p.COV_ENABLE_RGB_BUFFER_PREVIEW,
                    p.COV_ENABLE_DEPTH_BUFFER_PREVIEW,
                    p.COV_ENABLE_SEGMENTATION_MARK_PREVIEW):
                p.configureDebugVisualizer(
                    preview_flag, 0, physicsClientId=self.client
                )

        ground_collision = p.createCollisionShape(
            p.GEOM_BOX, halfExtents=[1.2, 1.2, 0.01],
            physicsClientId=self.client,
        )
        ground_visual = p.createVisualShape(
            p.GEOM_BOX, halfExtents=[1.2, 1.2, 0.01],
            rgbaColor=[0.92, 0.92, 0.92, 1.0],
            physicsClientId=self.client,
        )
        p.createMultiBody(
            baseMass=0,
            baseCollisionShapeIndex=ground_collision,
            baseVisualShapeIndex=ground_visual,
            basePosition=[0.5, 0.25, -0.015],
            physicsClientId=self.client,
        )
        track_points = [(0, 0), (1, 0), (1, 1), (0, 1), (0, 0)]
        for start, end in zip(track_points[:-1], track_points[1:]):
            p.addUserDebugLine(
                [start[0], start[1], 0.005], [end[0], end[1], 0.005],
                [0.05, 0.05, 0.05], 6, physicsClientId=self.client,
            )
        if self.gui:
            labels = (
                ("A", [0.0, 0.0, 0.025]),
                ("B", [1.0, 0.0, 0.025]),
                ("C", [1.0, 1.0, 0.025]),
                ("D", [0.0, 1.0, 0.025]),
            )
            for label, position in labels:
                p.addUserDebugText(
                    label, position, [0.05, 0.05, 0.05], 1.4,
                    physicsClientId=self.client,
                )

        panel_collision = p.createCollisionShape(
            p.GEOM_BOX,
            halfExtents=[TARGET_WIDTH_M / 2.0, 0.006, TARGET_HEIGHT_M / 2.0],
            physicsClientId=self.client,
        )
        panel_visual = p.createVisualShape(
            p.GEOM_BOX,
            halfExtents=[TARGET_WIDTH_M / 2.0, 0.006, TARGET_HEIGHT_M / 2.0],
            rgbaColor=[0.95, 0.95, 0.95, 1.0],
            physicsClientId=self.client,
        )
        self.target_body = p.createMultiBody(
            baseMass=0,
            baseCollisionShapeIndex=panel_collision,
            baseVisualShapeIndex=panel_visual,
            basePosition=TARGET_CENTER.tolist(),
            physicsClientId=self.client,
        )
        self._draw_target_markings()
        if self.gui:
            p.resetDebugVisualizerCamera(
                cameraDistance=1.85,
                cameraYaw=90,
                cameraPitch=-38,
                cameraTargetPosition=[0.5, 0.18, 0.10],
                physicsClientId=self.client,
            )

    def _draw_target_markings(self):
        y = TARGET_CENTER[1] + 0.008
        half_width = TARGET_WIDTH_M / 2.0
        half_height = TARGET_HEIGHT_M / 2.0
        border = [
            TARGET_CENTER + (-half_width, 0.008, +half_height),
            TARGET_CENTER + (+half_width, 0.008, +half_height),
            TARGET_CENTER + (+half_width, 0.008, -half_height),
            TARGET_CENTER + (-half_width, 0.008, -half_height),
        ]
        for index in range(4):
            p.addUserDebugLine(
                border[index].tolist(), border[(index + 1) % 4].tolist(),
                [0, 0, 0], 5, physicsClientId=self.client,
            )
        circle_points = []
        for index in range(49):
            angle = 2.0 * math.pi * index / 48.0
            circle_points.append([
                TARGET_CENTER[0] + 0.02 * math.cos(angle),
                y,
                TARGET_CENTER[2] + 0.02 * math.sin(angle),
            ])
        for start, end in zip(circle_points[:-1], circle_points[1:]):
            p.addUserDebugLine(
                start, end, [1, 0, 0], 2, physicsClientId=self.client
            )

    def _set_car(self, position_xy, body_yaw):
        if self.car_body is not None:
            p.removeBody(self.car_body, physicsClientId=self.client)
        collision = p.createCollisionShape(
            p.GEOM_BOX, halfExtents=[0.11, 0.065, 0.045],
            physicsClientId=self.client,
        )
        visual = p.createVisualShape(
            p.GEOM_BOX, halfExtents=[0.11, 0.065, 0.045],
            rgbaColor=[0.15, 0.35, 0.85, 1.0],
            physicsClientId=self.client,
        )
        quaternion = p.getQuaternionFromEuler([0, 0, body_yaw])
        self.car_body = p.createMultiBody(
            baseMass=0,
            baseCollisionShapeIndex=collision,
            baseVisualShapeIndex=visual,
            basePosition=[position_xy[0], position_xy[1], 0.055],
            baseOrientation=quaternion,
            physicsClientId=self.client,
        )

    def _laser_miss_distance(self, camera_origin, global_yaw, pitch):
        origin, direction = laser_ray(camera_origin, global_yaw, pitch)
        end = origin + direction * 5.0
        hit = p.rayTest(
            origin.tolist(), end.tolist(), physicsClientId=self.client
        )[0]
        if hit[0] != self.target_body:
            miss = float("inf")
            hit_point = end
        else:
            hit_point = np.asarray(hit[3], dtype=np.float64)
            miss = float(np.linalg.norm(
                hit_point[[0, 2]] - TARGET_CENTER[[0, 2]]
            ))
        if self.gui:
            color = [0, 1, 0] if miss <= 0.02 else [1, 0, 0]
            self.laser_line = p.addUserDebugLine(
                origin.tolist(), hit_point.tolist(), color, 2,
                replaceItemUniqueId=self.laser_line,
                physicsClientId=self.client,
            )
        return miss

    def _update_gui_status(self, camera_origin, global_yaw, pitch, state,
                           elapsed, miss, target_visible):
        """在场景中显示相机朝向和当前视觉控制状态。"""
        if not self.gui:
            return
        forward, _right, _up = camera_basis(global_yaw, pitch)
        axis_end = camera_origin + forward * 0.28
        self.camera_axis_line = p.addUserDebugLine(
            camera_origin.tolist(), axis_end.tolist(), [0.0, 0.35, 1.0], 4,
            replaceItemUniqueId=self.camera_axis_line,
            physicsClientId=self.client,
        )
        miss_text = "MISS=--"
        if math.isfinite(miss):
            miss_text = "D1=%.2fcm" % (miss * 100.0)
        text_value = (
            "%s  t=%.2fs  target=%s  yaw=%+.1f  pitch=%+.1f  %s"
            % (
                state, elapsed, "FOUND" if target_visible else "NOT FOUND",
                math.degrees(global_yaw), math.degrees(pitch), miss_text,
            )
        )
        self.status_text = p.addUserDebugText(
            text_value, [0.02, 1.08, 0.18], [0.05, 0.05, 0.05], 1.25,
            replaceItemUniqueId=self.status_text,
            physicsClientId=self.client,
        )

    def run_trial(self, requirement):
        if requirement == "basic2":
            limit_seconds = 2.0
            position = np.array([0.50, 0.55], dtype=np.float64)
            direction_to_target = math.atan2(
                TARGET_CENTER[1] - position[1], TARGET_CENTER[0] - position[0]
            )
            body_yaw = direction_to_target + math.radians(
                self.rng.uniform(-15.0, 15.0)
            )
            relative_yaw = math.radians(self.rng.uniform(-12.0, 12.0))
            pitch = math.radians(self.rng.uniform(-6.0, 6.0))
        elif requirement == "basic3":
            limit_seconds = 4.0
            position = sample_track_position(self.rng)
            body_yaw = self.rng.uniform(-math.pi, math.pi)
            # "aiming direction arbitrarily specified": full 360-degree start.
            relative_yaw = self.rng.uniform(-math.pi, math.pi)
            pitch = math.radians(self.rng.uniform(-12.0, 12.0))
        else:
            raise ValueError("requirement must be basic2 or basic3")

        self._set_car(position, body_yaw)
        camera_origin = np.array(
            [position[0], position[1], GIMBAL_HEIGHT_M], dtype=np.float64
        )
        visible_time = None
        best_miss = float("inf")
        total_steps = int(math.ceil(limit_seconds / DT))
        controller = AutoAimController(
            time_limit_s=limit_seconds,
            lock_frames=3,
            lost_frames=1,
            lock_tolerance_yaw_deg=0.12,
            lock_tolerance_pitch_deg=0.12,
            search_yaw_rate_deg_s=180.0,
            search_pitch_deg=0.0,
            search_pitch_rate_deg_s=90.0,
        )

        for step in range(total_steps):
            elapsed = (step + 1) * DT
            global_yaw = body_yaw + relative_yaw
            corners = project_target(
                camera_origin, global_yaw, pitch, self.pixel_noise, self.rng
            )
            aim_offsets = None
            if corners is not None:
                if visible_time is None:
                    visible_time = elapsed
                aim_offsets = self.solver.solve_from_corners(corners)

            command = controller.update(
                aim_offsets, DT, current_pitch_deg=math.degrees(pitch)
            )
            # 电机包装层只执行状态机给出的两个角度增量，并按当前假设限速。
            yaw_move = math.radians(command.yaw_deg)
            pitch_move = math.radians(command.pitch_deg)
            max_yaw_move = math.radians(180.0) * DT
            max_pitch_move = math.radians(90.0) * DT
            relative_yaw = wrap_angle(relative_yaw + max(
                -max_yaw_move, min(max_yaw_move, yaw_move)
            ))
            pitch += max(-max_pitch_move, min(max_pitch_move, pitch_move))
            pitch = max(math.radians(-35), min(math.radians(45), pitch))

            global_yaw = body_yaw + relative_yaw
            miss = self._laser_miss_distance(camera_origin, global_yaw, pitch)
            best_miss = min(best_miss, miss)
            self._update_gui_status(
                camera_origin, global_yaw, pitch, command.state,
                elapsed, miss, corners is not None,
            )
            if command.fire:
                return SimulationResult(
                    miss <= 0.02, elapsed, miss, visible_time
                )

            p.stepSimulation(physicsClientId=self.client)
            if self.gui:
                # 只改变演示播放速度，不改变仿真的控制周期和计时结果。
                time.sleep(DT / self.playback_speed)

        return SimulationResult(False, limit_seconds, best_miss, visible_time)


def run_batch(mode, trials, seed, gui, pixel_noise, hold_seconds,
              playback_speed):
    requirements = [mode] if mode != "all" else ["basic2", "basic3"]
    overall_success = True
    simulation = BasicAimingSimulation(
        gui=gui, seed=seed, pixel_noise=pixel_noise,
        playback_speed=playback_speed,
    )
    try:
        for requirement in requirements:
            results = []
            count = 1 if gui else trials
            for _index in range(count):
                result = simulation.run_trial(requirement)
                results.append(result)
            successes = [result for result in results if result.success]
            pass_rate = len(successes) / float(len(results))
            worst_time = max(
                (result.elapsed for result in successes), default=float("inf")
            )
            worst_miss_cm = max(
                (result.miss_distance * 100.0 for result in successes),
                default=float("inf"),
            )
            limit = 2.0 if requirement == "basic2" else 4.0
            print(
                "%s: pass=%d/%d (%.1f%%), worst_time=%.3fs/%.1fs, "
                "worst_D1=%.3fcm/2.000cm"
                % (
                    requirement, len(successes), len(results), pass_rate * 100.0,
                    worst_time, limit, worst_miss_cm,
                )
            )
            if len(successes) != len(results):
                overall_success = False
            if gui and hold_seconds > 0:
                time.sleep(hold_seconds)
    finally:
        simulation.close()
    return 0 if overall_success else 1


def parse_args():
    parser = argparse.ArgumentParser(
        description="PyBullet simulation of 2025E basic aiming requirements"
    )
    parser.add_argument(
        "--mode", choices=("basic2", "basic3", "all"), default="all"
    )
    parser.add_argument("--trials", type=int, default=100)
    parser.add_argument("--seed", type=int, default=2025)
    parser.add_argument("--pixel-noise", type=float, default=0.20)
    parser.add_argument("--gui", action="store_true")
    parser.add_argument(
        "--hold-seconds", type=float, default=3.0,
        help="keep the GUI open after each result",
    )
    parser.add_argument(
        "--playback-speed", type=float, default=0.25,
        help="GUI playback multiplier; 0.25 means four-times slow motion",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    if args.trials <= 0:
        raise ValueError("--trials must be positive")
    if args.playback_speed <= 0:
        raise ValueError("--playback-speed must be positive")
    return run_batch(
        args.mode, args.trials, args.seed, args.gui, args.pixel_noise,
        args.hold_seconds, args.playback_speed,
    )


if __name__ == "__main__":
    sys.exit(main())

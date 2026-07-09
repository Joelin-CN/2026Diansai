# 2026-07-09 全工程分层集成架构

## 概述

本文档描述 Vision v3.0 基础上，向上集成 Coordinate -> Task -> Control -> Device 各层的完整分层架构。

## 核心决策

| 项目 | 决策 | 原因 |
|------|------|------|
| 程序入口 | 单一 main.py，参数切换红/绿 | 同一套代码部署两块树莓派 |
| 运行模式 | calibration / competition | 标定验证和比赛运行分离 |
| 架构模式 | SystemFactory 初始化工厂 | 统一 RuntimeContext，主循环复用 |
| 标定字段 | 最小集：H + servo_config | 满足当前需求，后续扩展 |
| 硬件策略 | 先 mock 再真机 | 离线验证数据流后再接硬件 |
| 检测丢失 | 历史帧填补 + Kalman 平滑 | 避免抖动和任务中断 |

## 分层关系

```
main.py ──→ SystemFactory ──→ RuntimeContext
                                  │
    ┌─────────────────────────────┘
    ▼
runtime.py (统一主循环)
    │
    ├── CameraPort.read()           → frame                          [Device Layer]
    ├── VisionPipeline.process()    → VisionFrameResult              [Vision Layer]
    ├── history_fill()              → filled_detections              [Runtime]
    ├── KalmanFilter.update()       → smoothed_image_points          [Kalman Layer]
    ├── CoordinateTransformer       → ScreenPoint                    [Coordinate Layer]
    ├── Task.next_target()          → TaskTarget                     [Task Layer]
    ├── VisualServoController       → MotorCommand                   [Control Layer]
    └── MotorPort.send()            → None                           [Device Layer]
```

## 数据流接口

```
Layer         Input                     Output
─────────────────────────────────────────────────────────
Device        -                         CameraFrame
                                          .image, .frame_id, .timestamp_ms

Vision        CameraFrame               VisionFrameResult
                                          .red_laser.image_center: ImagePoint(u, v)
                                          .green_laser.image_center: ImagePoint(u, v)
                                          .tape_quad.outer/inner: ImageQuad
                                          .screen_detection.image_corners

Kalman        ImagePoint                ImagePoint (smoothed)
                measurement                  .u_px, .v_px

Coordinate    ImagePoint                ScreenPoint (mm)
                                          .x_mm, .y_mm

Task          ScreenPoint               TaskTarget
                current_position            .point: ScreenPoint
                                            .done: bool

Control       ScreenPoint × 2           MotorCommand
                target, current             .yaw_delta_steps: int
                                            .pitch_delta_steps: int
                                            .enable: bool

Device        MotorCommand              - (sent to hardware/log)
```

## 红绿系统对比

| | 红色 | 绿色 |
|---|---|---|
| Vision profile | configs/red/vision.json | configs/green/vision.json |
| Calibration | calibration/red_calibration.json | calibration/green_calibration.json |
| VisionMode | RUNTIME_A4_RED / RUNTIME_SCREEN_RED | RUNTIME_TRACK_RED_GREEN |
| Task | PathFollowerTask | GreenTrackRedTask |
| Kalman filters | 1 (red laser) | 2 (red + green laser) |
| Detectors | red_laser, tape/screen | red_laser, green_laser |
| Tasks | origin, screen, a4 | 仅追踪（无需 --task） |

## RuntimeContext

```python
@dataclass
class RuntimeContext:
    camera: CameraPort
    vision: VisionPipeline
    vision_mode: VisionMode
    transformer: CoordinateTransformer
    task: Task | None       # 可能延迟初始化（A4/Screen从frame生成路径）
    controller: VisualServoController
    motor: MotorPort
    kalman: dict[str, LaserKalmanFilter]  # {'red': ..., 'green': ...}
    last_valid: dict[str, ImagePoint | None]  # 历史有效检测
```

## detection source 标注

每帧标记检测来源，便于调试：

```python
@dataclass
class DetectionContext:
    laser: str            # "red" | "green"
    image_center: ImagePoint
    source: str           # "measured" | "history" | "kalman_predicted"
    frame_age: int        # 0=当前帧, >0=历史帧数
```

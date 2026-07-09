# 2023 E 题运动目标控制与自动追踪系统

本项目用于 2023 年电赛 E 题"运动目标控制与自动追踪系统"的完整实现。采用分层架构，同一套代码部署到两块树莓派，通过命令行参数切换红色系统 / 绿色系统。

## 已确认约束

- 使用固定摄像头，摄像头不随云台转动。
- 使用树莓派运行 OpenCV。
- 云台执行器为步进电机。
- 红色系统和绿色系统硬件完全独立，二者不通信。
- 接受每次摆放后的短标定。
- 标定阶段允许人工干预，但只用于计算参数。
- 正式运行阶段不允许人工兜底。
- 主方案采用屏幕平面二维坐标、相机标定/单应性、短标定和视觉伺服闭环。

## 快速开始

```bash
# 标定验证模式（验证坐标映射精度）
python main.py --system red --mode calibration
python main.py --system green --mode calibration

# 比赛模式 - 红色系统
python main.py --system red --mode competition --task a4
python main.py --system red --mode competition --task screen
python main.py --system red --mode competition --task origin

# 比赛模式 - 绿色系统
python main.py --system green --mode competition

# 指定设备配置（默认离线mock）
python main.py --system red --mode competition --task a4 --device device/stage2_hardware.json
```

退出：**Ctrl+Q**（键盘监听线程，兼容GUI和无头运行）

## 架构分层

```
main.py                      程序入口（命令行参数解析 → SystemFactory → run）
    │
    ▼
Application Layer            src/application/runtime.py  统一主循环
    │                         src/application/factory.py  SystemFactory + RuntimeContext
    ▼
Task Layer                   src/task/red_tasks.py   PathFollowerTask（回原点/Screen/A4）
    │                         src/task/green_tasks.py GreenTrackRedTask（追踪红点）
    ▼
Control Layer                src/control/visual_servo.py  VisualServoController
    │                         src/control/types.py          MotorCommand
    ▼
Coordinate Layer             src/coordinate/transformer.py  CoordinateTransformer
    │                         src/coordinate/types.py        ScreenPoint/ScreenQuad
    ▼
Kalman Layer                 src/kalman/laser_kalman.py  LaserKalmanFilter（平滑）
    │
    ▼
Vision Layer v3.0            src/vision/pipeline.py  VisionPipeline（LAB halo-guided）
    │                         src/vision/types.py    ImagePoint / VisionFrameResult
    ▼
Device Layer                 src/device/video_camera.py  VideoFileCameraPort（Stage1）
    │                         src/device/mock_motor.py    MockMotorPort（Stage1）
    ▼
Calibration Layer            src/calibration/types.py  CalibrationResult
                             src/calibration/loader.py  load/save
```

## 统一主循环数据流

```
每帧循环：
  1. Camera.read()                      → CameraFrame
  2. VisionPipeline.process()           → VisionFrameResult（ImagePoint 图像坐标）
  3. 历史填补（检测丢失时沿用上帧）     → 保证数据连续
  4. LaserKalmanFilter.update()         → 平滑后的 ImagePoint
  5. CoordinateTransformer.image_to_screen() → ScreenPoint（屏幕坐标 mm）
  6. Task.next_target()                 → TaskTarget（目标点）
  7. VisualServoController.update()     → MotorCommand（舵机增量步数）
  8. MotorPort.send()                   → 发送指令 / 记录CSV
```

## 标定系统

标定产物最小字段：

```json
{
  "H_img_to_screen": [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
  "servo_config": {
    "yaw_kp_steps_per_mm": 1.0,
    "pitch_kp_steps_per_mm": 1.0,
    "deadband_mm": 3.0,
    "max_yaw_delta_steps": 80,
    "max_pitch_delta_steps": 80
  }
}
```

红绿系统各自独立标定：`calibration/red_calibration.json` / `calibration/green_calibration.json`

标定数据采集在程序外完成（单独脚本或手动），`--mode calibration` 用于加载标定文件并验证坐标映射精度。

## 红色系统

红色系统只控制红色激光点，不依赖绿色系统，不使用绿色激光检测。

命令示例：
```bash
python main.py --system red --mode competition --task a4     # 沿A4黑胶带矩形移动
python main.py --system red --mode competition --task screen  # 沿屏幕方框移动
python main.py --system red --mode competition --task origin  # 回原点
```

## 绿色系统

绿色系统只通过自己的摄像头观测红点和绿点，不读取红色系统的状态、路径或指令。

命令示例：
```bash
python main.py --system green --mode competition
```

控制目标：`target = detected_red_point, current = detected_green_point`

## 坐标系统一

主坐标系统一为屏幕平面二维坐标，单位 `mm`。

```
屏幕左下角: ScreenPoint(0, 0)
屏幕右下角: ScreenPoint(W, 0)
屏幕左上角: ScreenPoint(0, H)
屏幕右上角: ScreenPoint(W, H)
```

转换链路：`ImagePoint(u, v) → Homography → ScreenPoint(x_mm, y_mm)`

## 设备接口

```
CameraPort.read()   → CameraFrame      VideoFileCameraPort | OpenCVCameraPort
MotorPort.send()    → None             MockMotorPort（写CSV） | SerialMotorPort（串口）
```

设备配置文件：`device/stage1_mock.json`（离线验证）/ `device/stage2_hardware.json`（真实硬件）

## 目录结构

```
2023e/
  main.py              程序入口
  configs/              运行参数（red/vision.json, green/vision.json）
  calibration/          标定产物（red_calibration.json, green_calibration.json）
  device/               设备配置（stage1_mock.json, stage2_hardware.json）
  docs/                 文档（架构、算法、结构、计划、交接）
  src/                  源码
    application/        runtime.py（主循环）, factory.py（工厂）
    calibration/        types.py, loader.py
    control/            visual_servo.py, types.py
    coordinate/         transformer.py, types.py, quad.py
    device/             types.py, video_camera.py, mock_motor.py
    kalman/            laser_kalman.py
    task/              red_tasks.py, green_tasks.py, path.py, types.py
    vision/            pipeline.py, laser_detector.py, ... (v3.0)
  scripts/              测试、调试、验证脚本
  tests/                单元测试
  data/                 测试图像、视频
  outputs/              运行输出、轨迹记录
```

## 实施阶段

| Phase | 内容 | 状态 |
|-------|------|------|
| Vision v3.0 | LAB halo-guided 激光检测 | 已完成 |
| 4 Runtime Mode | 视频验证 | 已完成 |
| Phase 1 | 基础设施（Calibration + Device + Kalman） | 进行中 |
| Phase 2 | 离线闭环 - 绿色追踪 | 待实施 |
| Phase 3 | 离线闭环 - 红色任务 | 待实施 |
| Phase 4 | 接入硬件（OpenCV相机 + 串口电机） | 待实施 |
| Phase 5 | 标定完善 | 待实施 |

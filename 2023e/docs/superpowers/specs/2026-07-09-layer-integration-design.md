# 2026-07-09 全工程分层对接架构设计

## 1. 设计决策

| 决策项 | 选择 | 原因 |
|--------|------|------|
| 程序入口 | 单一 `main.py`，两套独立程序入口 | 同一套代码部署两块树莓派，通过参数切换 |
| 运行模式 | `--mode calibration` / `--mode competition` | 标定验证与比赛运行分离 |
| 任务选择 | `--task origin|screen|a4` 启动时指定 | 一次运行一个任务，绿色系统无需指定 |
| 架构模式 | SystemFactory 初始化工厂 | 统一的 RuntimeContext，主循环代码复用 |
| CalibrationResult | 最小字段：H_img_to_screen + servo_config | 红绿系统各存一份独立标定 |
| 硬件策略 | 先离线mock闭环，再接真实硬件 | VideoFileCameraPort + MockMotorPort → OpenCVCameraPort + SerialMotorPort |
| 检测丢失 | 沿用历史帧数据 + Kalman 平滑 | 避免抖动和任务中断 |
| 退出方式 | Ctrl+Q，通过 pynput 键盘监听线程 | 兼容 GUI 和无头运行 |

## 2. 程序入口

```
main.py
├── 参数解析：argparse 处理 --system, --mode, --task, --device
├── SystemFactory.create() → 返回 RuntimeContext
├── 统一主循环 run()
└── Ctrl+Q 优雅退出，释放资源
```

### 命令行接口

```bash
# 标定验证（验证坐标映射精度）
python main.py --system red --mode calibration
python main.py --system green --mode calibration

# 比赛运行（红色系统）
python main.py --system red --mode competition --task a4
python main.py --system red --mode competition --task screen
python main.py --system red --mode competition --task origin

# 比赛运行（绿色系统，无需 --task）
python main.py --system green --mode competition

# 指定设备配置（默认 stage1_mock）
python main.py --system red --mode competition --task a4 --device device/stage2_hardware.json
```

## 3. 架构分层

```
Application Layer   main.py + runtime.py + factory.py
    │
    ▼
Task Layer          task/red_tasks.py (PathFollowerTask)
                    task/green_tasks.py (GreenTrackRedTask)
    │
    ▼
Control Layer       control/visual_servo.py (VisualServoController)
    │
    ▼
Coordinate Layer    coordinate/transformer.py (CoordinateTransformer)
    │
    ▼
Kalman Layer         kalman/laser_kalman.py (LaserKalmanFilter)
    │
    ▼
Vision Layer        vision/pipeline.py (VisionPipeline) v3.0
    │
    ▼
Device Layer        device/video_camera.py, device/mock_motor.py
    │
    ▼
Calibration Layer   calibration/types.py, calibration/loader.py
```

## 4. 统一主循环数据流

```python
while not should_exit:
    # 1. 读取图像
    frame = camera.read()

    # 2. 视觉检测（图像坐标）
    vision_result = vision_pipeline.process(frame, mode=vision_mode)

    # 3. 历史填补（检测丢失时沿用上帧数据）
    fill_from_history(vision_result, last_valid)

    # 4. Kalman 平滑（图像坐标）
    smoothed = apply_kalman(kalman_filters, current_detections)

    # 5. 坐标转换（图像坐标 → 屏幕坐标 mm）
    screen_points = transform(image_points)

    # 6. 任务初始化（首次获取到 quad 时生成路径）
    init_task_if_needed(vision_result, screen_points)

    # 7. 任务目标生成
    target = task.next_target(current_screen_point)

    # 8. 控制计算（误差 → 舵机增量步数）
    command = controller.update(target, current)

    # 9. 设备输出
    motor.send(command)

    # 10. 日志记录
    logger.record(...)
```

### 标定验证模式 vs 比赛模式差异

| | calibration | competition |
|---|---|---|
| **任务** | 无（验证坐标映射） | 有（PathFollowerTask / GreenTrackRedTask） |
| **显示** | OpenCV 窗口 + 坐标叠加 | 无 GUI（Stage1 有窗口调试，Stage2 无头） |
| **motor** | MockMotorPort（仅记录 CSV） | MockMotorPort（Stage1）→ SerialMotorPort（Stage2） |
| **日志** | 坐标映射日志 | 控制指令日志 |

## 5. CalibrationResult

```python
@dataclass
class CalibrationResult:
    H_img_to_screen: np.ndarray  # 3x3 单应性矩阵
    servo_config: ServoConfig     # 控制增益参数

    @classmethod
    def load(cls, path: str) -> CalibrationResult: ...
    def save(self, path: str) -> None: ...
```

### 标定文件格式

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

## 6. Device Layer 接口

```python
class CameraPort(ABC):
    def open(self) -> None: ...
    def read(self) -> CameraFrame: ...
    def close(self) -> None: ...

class MotorPort(ABC):
    def send(self, command: MotorCommand) -> None: ...
    def open(self) -> None: ...
    def close(self) -> None: ...
```

### 实现矩阵

| 接口 | Stage 1 (mock) | Stage 2 (hardware) |
|------|----------------|---------------------|
| CameraPort | VideoFileCameraPort | OpenCVCameraPort |
| MotorPort | MockMotorPort（写CSV） | SerialMotorPort（串口） |

## 7. Kalman 层

```python
class LaserKalmanFilter:
    """状态: [u, v, du, dv], 观测: [u, v]"""
    def predict(self) -> ImagePoint: ...
    def update(self, measurement: ImagePoint) -> ImagePoint: ...
```

## 8. 检测丢失处理

```python
# 历史填补
last_valid = {"red": None, "green": None}

if vision_result.red_laser and vision_result.red_laser.found:
    last_valid["red"] = vision_result.red_laser.image_center
if vision_result.green_laser and vision_result.green_laser.found:
    last_valid["green"] = vision_result.green_laser.image_center

# 如果有历史数据，使用历史数据
# Kalman 进一步平滑后进入坐标转换
```

## 9. 红绿系统差异

| | 红色系统 | 绿色系统 |
|---|---|---|
| vision config | configs/red/vision.json | configs/green/vision.json |
| calibration | calibration/red_calibration.json | calibration/green_calibration.json |
| vision mode | RUNTIME_A4_RED / RUNTIME_SCREEN_RED | RUNTIME_TRACK_RED_GREEN |
| task | PathFollowerTask | GreenTrackRedTask |
| kalman filters | red 1 | red 1 + green 1 |
| detectors | red_laser + tape/screen | red_laser + green_laser |

## 10. 配置文件布局

```
configs/
  red/vision.json
  green/vision.json

calibration/
  red_calibration.json
  green_calibration.json

device/
  stage1_mock.json
  stage2_hardware.json
```

## 11. 文件变更清单

```
新增:
  src/calibration/types.py
  src/calibration/loader.py
  src/device/types.py
  src/device/video_camera.py
  src/device/mock_motor.py
  src/kalman/laser_kalman.py
  src/application/runtime.py
  src/application/factory.py
  main.py
  device/stage1_mock.json
  calibration/red_calibration.json
  calibration/green_calibration.json

修改:
  src/coordinate/types.py        # 确保 ScreenPoint, ScreenQuad 可用
  src/task/red_tasks.py          # 补充延迟初始化，origin/screen 任务路径生成
  src/task/green_tasks.py        # 确认与 Kalman 坐标兼容
  README.md                      # 更新架构说明

删除:
  src/application/red_runtime.py  # 被 runtime.py + factory.py 替代
```

## 12. 分阶段实施计划

### Phase 1: 基础设施
- CalibrationResult + loader
- Device Layer 抽象接口 + VideoFileCameraPort + MockMotorPort
- LaserKalmanFilter（状态: 位置 + 速度，观测: 位置）
- 历史填补逻辑

### Phase 2: 离线闭环 - 绿色追踪
- SystemFactory
- 统一主循环 runtime.py
- main.py 入口
- 离线视频 + mock motor 闭环验证

### Phase 3: 离线闭环 - 红色任务
- PathFollowerTask 完善
- A4任务：延迟初始化路径
- 离线视频闭环验证

### Phase 4: 接入硬件（下次）
- OpenCVCameraPort + SerialMotorPort

### Phase 5: 标定完善（下次）
- 标定数据采集脚本
- 标定验证模式完善

## 13. 已验证的 Vision Layer 状态

| VisionMode | 状态 | 备注 |
|---|---|---|
| RUNTIME_SCREEN_RED | 已实现 | 测试视频无真实铅笔边框，screen 检测待验证 |
| RUNTIME_A4_RED | 已实现 | A4 黑胶带检测正常 |
| RUNTIME_TRACK_RED_GREEN | 已实现 | 红绿追踪检测正常 |
| RUNTIME_A4_TRACK_RED_GREEN | 已实现 | 仅作开发验证模式，不做正式运行 |
| CALIBRATION | 未实现 | 暂不需要，标定在程序外完成 |
| DEBUG | 已实现 | 支持 |

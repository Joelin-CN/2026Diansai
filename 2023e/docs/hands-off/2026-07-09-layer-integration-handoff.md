# 2026-07-09 全工程分层对接交接文档

## 交接目标

本文用于继续讨论 2023 年电赛 E 题“运动目标控制与自动追踪系统”的全工程收尾工作。当前重点不是立即改代码，而是先把各层现状、接口边界、缺口和讨论问题整理清楚，后续再决定实施顺序。

当前 Vision Layer 已完成 LAB halo-guided core v3 / v3.0 迁移，并完成 4 个 runtime mode 的初步视频验证。下一阶段目标是把 Vision、Coordinate、Task、Control、Application、Device/Hardware、Calibration 这些层真正串起来，形成可运行的红色系统和绿色系统闭环。

---

## 一、当前关键修改状态

### 1.1 Vision Layer v3.0 状态

LAB 算法已迁移到正式路径：

```text
src/vision/laser_detector.py
```

核心配置已支持：

```text
color_space = "LAB_HALO_GUIDED"
```

已验证的 runtime mode：

```text
RUNTIME_SCREEN_RED
RUNTIME_A4_RED
RUNTIME_TRACK_RED_GREEN
RUNTIME_A4_TRACK_RED_GREEN
```

### 1.2 本轮新增代码修改

文件：

```text
src/vision/pipeline.py
```

修改 1：A4 / screen support detector 刷新间隔从 10 帧改为 20 帧。

```python
self._runtime_support_refresh_interval = 20
```

修改 2：`RUNTIME_SCREEN_RED` 中启用 screen 方框检测。

```python
def _should_run_screen(self, mode: VisionMode) -> bool:
    return mode in {
        VisionMode.DEBUG,
        VisionMode.RUNTIME_SCREEN_RED,
    }
```

注意：测试视频中没有真实铅笔灰色边框，所以 `RUNTIME_SCREEN_RED` 的 screen 检测找不到或误检都是可接受现象，正式验收时需要使用包含屏幕铅笔边框的视频或现场图像验证。

### 1.3 新增测试脚本

文件：

```text
scripts/test/test_all_runtime_modes.py
```

作用：对 4 个 runtime mode 生成标注视频和 summary。

运行命令：

```powershell
cd "E:\B306\2026\电赛\2023e"
$env:PYTHONPATH="src"
& "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" scripts/test/test_all_runtime_modes.py
```

最近一次输出目录：

```text
outputs/runtime_modes_test_20260709_140639/
```

输出视频：

```text
runtime_screen_red_annotated.mp4
runtime_a4_red_annotated.mp4
runtime_track_red_green_annotated.mp4
runtime_a4_track_red_green_annotated.mp4
```

---

## 二、工程分层总览

当前架构采用：

```text
Application Layer      应用组织层
Task Layer             任务生成层
Control Layer          控制计算层
Coordinate Layer       坐标与标定层
Vision Layer           视觉检测层
Device Interface Layer 设备接口层
Hardware Layer         硬件抽象层
Calibration System     标定系统/参数产物
```

运行主链路：

```text
Camera Frame
  -> Vision Detection
  -> Coordinate Mapping
  -> Task Planner
  -> Visual Servo Controller
  -> Motor Command Port
  -> Hardware Motion
  -> Camera Feedback Loop
```

从 `Coordinate Layer` 往上，统一使用屏幕平面二维坐标：

```text
ScreenPoint(x_mm, y_mm)
```

`Vision Layer` 只输出图像坐标：

```text
ImagePoint(u_px, v_px)
ImageQuad(...)
```

---

## 三、各层现状

### 3.1 Vision Layer

路径：

```text
src/vision/
```

主要文件：

```text
src/vision/pipeline.py
src/vision/laser_detector.py
src/vision/tape_quad_detector.py
src/vision/screen_detector.py
src/vision/state_tracker.py
src/vision/types.py
src/vision/config.py
src/vision/diagnostics.py
```

当前职责：

- 从 `CameraFrame` 中检测红色激光、绿色激光、A4 黑胶带、screen 铅笔边框。
- 输出 `VisionFrameResult`。
- 维护红/绿激光 ROI 跟踪状态。
- 提供轻量 diagnostics 和 stage timing。

当前输出：

```python
VisionFrameResult:
    frame_id: int
    timestamp_ms: float
    red_laser: LaserDetection | None
    green_laser: LaserDetection | None
    tape_quad: TapeQuadDetection | None
    screen_detection: ScreenDetection | None
    diagnostics: VisionFrameDiagnostics
```

完成度：高。

已知问题：

- `tests/vision/test_laser_detector.py` 仍有部分测试依赖旧 HSV 诊断字段，需要适配 LAB 字段。
- `RUNTIME_SCREEN_RED` 的 screen 检测需要用包含铅笔边框的素材重新验收。
- 当前 runtime mode 文档中 `RUNTIME_SCREEN_RED` 调用链需要同步更新，体现 screen support detector 已接入。

### 3.2 Coordinate Layer

路径：

```text
src/coordinate/
```

主要文件：

```text
src/coordinate/types.py
src/coordinate/transformer.py
src/coordinate/quad.py
```

已有能力：

```python
CoordinateTransformer.image_point_to_screen(point: ImagePoint) -> ScreenPoint
CoordinateTransformer.image_quad_to_screen(quad: ImageQuad) -> ScreenQuad
```

核心数据结构：

```python
ScreenPoint(x_mm: float, y_mm: float)
ScreenQuad(points: tuple[ScreenPoint, ScreenPoint, ScreenPoint, ScreenPoint])
```

缺口：

- `CalibrationResult` 尚未成型，`H_img_to_screen` 来源未统一。
- 去畸变链路尚未接入。
- A4 黑胶带 inner/outer 到中心线 quad 的实现需要确认和测试。
- Screen 铅笔边框到屏幕坐标系的关系需要明确：是运行时自检，还是可用于动态更新任务区域。

### 3.3 Task Layer

路径：

```text
src/task/
```

主要文件：

```text
src/task/types.py
src/task/red_tasks.py
src/task/green_tasks.py
src/task/path.py
```

已有能力：

```python
PathFollowerTask.next_target(current: ScreenPoint) -> TaskTarget
GreenTrackRedTask.next_target(detected_red_point: ScreenPoint) -> TaskTarget
```

核心输出：

```python
TaskTarget(point: ScreenPoint, done: bool = False)
```

缺口：

- 红色系统的任务类型需要补齐：回原点、屏幕方框路径、A4 黑胶带路径。
- `interpolate_quad_path()` 的实现和测试需要确认。
- 任务状态机需要明确：启动、运行、暂停、完成、失败。
- A4 任务路径使用 `tape_quad.inner_quad`、`outer_quad` 还是中心线，需要统一。

### 3.4 Control Layer

路径：

```text
src/control/
```

主要文件：

```text
src/control/types.py
src/control/visual_servo.py
```

已有能力：

```python
VisualServoController.update(target: ScreenPoint, current: ScreenPoint) -> MotorCommand
```

当前控制器：

```python
ServoConfig:
    yaw_kp_steps_per_mm: float = 1.0
    pitch_kp_steps_per_mm: float = 1.0
    deadband_mm: float = 3.0
    max_yaw_delta_steps: int = 80
    max_pitch_delta_steps: int = 80
```

输出：

```python
MotorCommand:
    yaw_delta_steps: int
    pitch_delta_steps: int
    enable: bool = True
```

缺口：

- 电机方向符号、屏幕 x/y 与 yaw/pitch 的正负关系需要通过标定确定。
- 粗映射/增益参数尚未接入 `CalibrationResult`。
- 暂停、安全停机、检测丢失时输出策略需要在 Application 或 Control 边界明确。

### 3.5 Application Layer

路径：

```text
src/application/
```

主要文件：

```text
src/application/red_runtime.py
src/application/green_runtime.py
```

现状：

- `red_runtime.py` 是示意性质代码，不是可直接运行的正式主循环。
- `green_runtime.py` 需要进一步读取确认，预计也未完成正式运行入口。

需要完成：

- 红色系统正式 runtime app。
- 绿色系统正式 runtime app。
- 模式选择与任务创建。
- 每帧闭环：camera -> vision -> coordinate -> task -> control -> motor。
- 检测丢失处理、暂停键、安全停机、日志。

### 3.6 Device Interface / Hardware Layer

当前源码目录中尚未看到正式实现。

需要定义：

```python
CameraPort.read() -> CameraFrame
MotorPort.send(command: MotorCommand) -> None
```

建议先做 mock / video file 版本，离线跑通完整闭环，再接真实硬件。

### 3.7 Calibration System

当前尚未形成统一代码入口。

需要产物：

```python
CalibrationResult:
    camera_matrix
    distortion_coefficients
    H_img_to_screen
    screen_roi
    screen_width_mm
    screen_height_mm
    task_region_corners
    red_motor_coarse_map
    green_motor_coarse_map
    servo_config
```

需要讨论：

- 标定文件放在 `calibration/` 还是 `configs/<profile>/`。
- 红色系统和绿色系统是否各有独立 calibration result。
- `vision.json` 与 `CalibrationResult` 的边界：视觉阈值属于运行配置，还是标定产物。

---

## 四、四个 Runtime Mode 的对接含义

### 4.1 RUNTIME_SCREEN_RED

题目对应：

- 红点回原点。
- 红点沿屏幕中心铅笔方框顺时针移动。

Vision 输出：

```text
red_laser
screen_detection
```

Coordinate 需要转换：

```text
red_laser.image_center -> red_current_screen_point
screen_detection.image_corners -> screen_square_quad
```

Task 需要生成：

```text
origin_target
screen_square_path
```

Control 输入：

```text
target_screen_point
current_red_screen_point
```

### 4.2 RUNTIME_A4_RED

题目对应：

- 红点沿 A4 黑胶带矩形顺时针移动。
- A4 任意旋转角度后继续沿胶带移动。

Vision 输出：

```text
red_laser
tape_quad.outer_quad
tape_quad.inner_quad
```

Coordinate 需要转换：

```text
red_laser.image_center -> red_current_screen_point
tape_quad.outer_quad -> outer_screen_quad
tape_quad.inner_quad -> inner_screen_quad
```

Task 需要生成：

```text
tape_centerline_quad
tape_centerline_path
```

### 4.3 RUNTIME_TRACK_RED_GREEN

题目对应：

- 绿色光斑追踪红色光斑。

Vision 输出：

```text
red_laser
green_laser
```

Coordinate 需要转换：

```text
red_laser.image_center -> red_screen_point
green_laser.image_center -> green_screen_point
```

Task 需要生成：

```text
target = red_screen_point
```

Control 输入：

```text
target = red_screen_point
current = green_screen_point
```

### 4.4 RUNTIME_A4_TRACK_RED_GREEN

题目对应：

- 红点执行 A4 黑胶带路径。
- 绿点同时自动追踪红点。

注意：题目要求红色系统和绿色系统硬件独立且不能通信。实际比赛中这应拆成两个独立运行进程/设备：

```text
Red Runtime:
  uses red camera / red motor / red vision config
  controls red laser along A4 path

Green Runtime:
  uses green camera / green motor / green vision config
  observes red and green laser
  controls green laser to track red laser
```

`RUNTIME_A4_TRACK_RED_GREEN` 更适合作为绿色系统视觉观测模式或离线联合验证模式。正式物理系统中，绿色系统不能读取红色系统的 task/path/command，只能从视觉中看到红点。

---

## 五、建议讨论顺序

### 5.1 先定运行进程边界

需要确认：

1. 红色系统和绿色系统是否分别有独立程序入口。
2. `RUNTIME_A4_TRACK_RED_GREEN` 在正式工程中是红色系统模式、绿色系统模式，还是只作为视觉联合验证模式。
3. 红色系统是否需要检测绿色激光。按题目约束，红色系统通常不需要。

### 5.2 再定 CalibrationResult

需要确认：

1. 标定文件格式。
2. 红/绿系统是否各保存一份独立标定。
3. `H_img_to_screen` 的屏幕坐标原点定义。
4. 屏幕方框、A4 黑胶带、红绿激光都映射到同一 `ScreenPoint` 平面。

### 5.3 再补 Task Layer

优先补齐：

```text
screen_square_path
tape_centerline_path
origin_target
```

### 5.4 最后补 Application 和 Device

建议先做离线版本：

```text
VideoFileCameraPort
MockMotorPort
```

离线跑通后再接：

```text
OpenCVCameraPort
SerialMotorPort / GpioMotorPort
```

---

## 六、当前不建议马上改的点

1. 不建议马上重构 Vision Layer。当前已可工作，后续只做接口适配。
2. 不建议马上写真实电机驱动。先用 mock 输出验证闭环数据流。
3. 不建议把坐标变换塞进 Vision Layer。Vision 继续只输出图像坐标。
4. 不建议让绿色系统读取红色系统内部目标点或路径，这违反题目独立约束。
5. 不建议用 screen 检测结果在 runtime 中动态重建坐标系，除非先明确标定策略。

---

## 七、下一步待讨论问题

请优先讨论以下问题：

1. 红色系统和绿色系统是否各自一个 `application/*_runtime.py` 正式入口。
2. `RUNTIME_A4_TRACK_RED_GREEN` 的定位：正式 runtime mode 还是联合视觉测试 mode。
3. `CalibrationResult` 的最小字段集合。
4. `ScreenPoint` 原点：继续使用屏幕左下角，还是题目原点（屏幕中心）。
5. 题目中的“原点”是否在 Task Layer 中用 `ScreenPoint(W/2, H/2)` 表达。
6. 屏幕铅笔方框路径是从 `screen_detection.image_corners` 动态生成，还是从标定固定边框生成。
7. A4 黑胶带路径应沿胶带中心线、内边界、还是外边界生成。建议中心线。
8. 检测丢失时 motor command 是 `enable=False`，还是发送零步进。
9. 离线闭环测试是否先使用录制视频和 mock motor。
10. 真实硬件电机接口协议是什么。

---

## 八、建议下一阶段产物

讨论完成后，建议先产出一个实施计划，而不是直接动代码。计划应包含：

1. `CalibrationResult` 类型和加载器。
2. `coordinate.quad` 和 `task.path` 的测试补齐。
3. 红色 runtime 离线闭环。
4. 绿色 runtime 离线闭环。
5. mock device 层。
6. 真实 device 层适配。
7. 端到端集成测试脚本。

---

## 九、继续讨论用提示词

可以在下一轮直接使用以下提示词：

```text
我们继续讨论 2023 年电赛 E 题“运动目标控制与自动追踪系统”的全工程分层对接方案。

请先读取：
1. docs/hands-off/2026-07-09-layer-integration-handoff.md
2. docs/hands-off/2026-07-09-handoff-to-next-session.md
3. docs/hands-off/2026-07-09-lab-halo-migration.md
4. src/vision/pipeline.py
5. src/vision/types.py
6. src/coordinate/transformer.py
7. src/task/red_tasks.py
8. src/task/green_tasks.py
9. src/control/visual_servo.py
10. src/application/red_runtime.py

当前状态：
1. Vision Layer v3.0 LAB halo-guided core 已迁移完成。
2. 4 个 runtime mode 已完成初步视频验证。
3. A4 / screen support detector 刷新间隔已从 10 帧改为 20 帧。
4. RUNTIME_SCREEN_RED 已接入 screen_detection，但当前测试视频没有真实铅笔灰色边框，所以 screen 找不到或误检都不作为失败。
5. 现在不要马上改代码，先讨论全工程分层对接方案。

请帮我重点讨论：
1. 红色系统和绿色系统正式运行进程如何划分。
2. RUNTIME_A4_TRACK_RED_GREEN 应该作为正式 runtime mode 还是联合视觉验证 mode。
3. CalibrationResult 的最小必要字段。
4. Vision -> Coordinate -> Task -> Control -> Device 的数据流接口。
5. 先做离线 mock 闭环还是直接接硬件。

讨论完成后，再帮我写一份可执行的分阶段实施计划。
```

---

## 十、相关文件索引

架构文档：

```text
README.md
docs/hands-off/2026-07-05-architecture-handoff.md
docs/structure/2026-07-09-vision-layer-structure-v3.0.md
```

Vision Layer：

```text
src/vision/pipeline.py
src/vision/laser_detector.py
src/vision/tape_quad_detector.py
src/vision/screen_detector.py
src/vision/types.py
```

Coordinate / Task / Control：

```text
src/coordinate/transformer.py
src/coordinate/types.py
src/coordinate/quad.py
src/task/red_tasks.py
src/task/green_tasks.py
src/task/path.py
src/control/visual_servo.py
src/control/types.py
```

Application：

```text
src/application/red_runtime.py
src/application/green_runtime.py
```

测试脚本：

```text
scripts/test/test_all_runtime_modes.py
scripts/test/eval_vision_runtime_video.py
scripts/test/preview_vision_video.py
```

---

**交接日期**: 2026-07-09  
**交接范围**: 全工程分层对接讨论准备  
**当前建议**: 先讨论边界和数据流，再写实施计划，最后改代码

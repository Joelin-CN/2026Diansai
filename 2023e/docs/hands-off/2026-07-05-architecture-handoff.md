# 2026-07-05 2023 E 题架构设计交接文档

> Note: 本文记录 2026-07-05 当时的交接状态；仓库后续已重构并补充实现，当前路径以仓库根目录下 `2023e/` 为准。

## 项目位置

当前项目目录：

```text
2023e/
```

当前已生成：

```text
2023e/
  README.md
  configs/.gitkeep
  docs/.gitkeep
  docs/hands-off/2026-07-05-architecture-handoff.md
  scripts/README.md
  src/.gitkeep
  tests/.gitkeep
  calibration/.gitkeep
  data/.gitkeep
  outputs/.gitkeep
  logs/.gitkeep
```

旧方案和上下文仍保留在：

```text
2023e/docs/plan/2023e/plan_v0.md
2023e/docs/plan/2023e/handoff.md
2023e/docs/E_运动目标控制与自动追踪系统.pdf
```

## 当前阶段

当前只讨论架构设计和算法建模，不写具体 OpenCV 代码，不写步进电机驱动程序。

已经完成：

- 读取并整理了 `2023e/docs/plan/2023e/plan_v0.md`。
- 读取并整理了 `2023e/docs/plan/2023e/handoff.md`。
- 尝试读取 PDF，原生读取失败，后用 `pypdf` 提取题面文本；中文编码有乱码，但题面核心要求已经结合上下文确认。
- 明确了主方案：屏幕平面二维坐标、相机标定/单应性、短标定、视觉伺服闭环。
- 明确了红绿系统硬件完全独立且不通信。
- 明确了标定阶段和运行阶段的边界。
- 在 `2023e/README.md` 中写入了全流程架构。

## 已确认约束

- 使用固定摄像头，摄像头不随云台转动。
- 使用树莓派或香橙派运行 OpenCV。
- 云台执行器为步进电机。
- 红色系统和绿色系统硬件完全独立。
- 红色系统和绿色系统之间不允许通信。
- 接受每次设备摆放后的短标定。
- 暂不讨论步进电机驱动程序，只保留通信接口。
- 主方案采用屏幕二维坐标，而不是完整三维几何反解作为主控制链路。
- 红色路径生成采用“识别四角生成几何路径 + 运行时视觉误差修正”。
- 标定阶段允许人工干预。
- 标定阶段的人工干预只用于计算参数。
- 正式运行阶段不允许人工兜底。

## 标定边界

标定阶段允许人工参与，但只用于计算运行参数，例如：

- 屏幕四角点击。
- 相机内参和畸变参数标定。
- 现场屏幕单应性标定。
- 红色系统电机粗映射采样确认。
- 绿色系统电机粗映射采样确认。
- 光斑检测阈值、曝光、ROI 等参数确认。
- A4 黑胶带检测参数确认。

正式运行阶段不允许：

- 人工重新点击屏幕角点。
- 人工点击 A4 四角。
- 人工指定红点或绿点位置。
- 人工修正路径点。
- 检测失败后通过人工兜底。

因此运行期失败处理应通过自动检测、自动重捕获、暂停输出或安全停机完成。

## 架构设计结论

项目采用“标定应用 + 运行应用”的架构边界：

```text
Calibration App
  -> 允许人工参与
  -> 只计算参数
  -> 输出 CalibrationResult

Runtime App
  -> 不允许人工参与
  -> 加载 CalibrationResult
  -> 自动完成视觉检测、坐标变换、任务生成、控制计算和电机命令输出
```

推荐软件分层：

```text
Application Layer      应用组织层
Task Layer             任务生成层
Control Layer          控制计算层
Coordinate Layer       坐标与标定层
Vision Layer           视觉检测层
Device Interface Layer 设备接口层
Hardware Layer         硬件抽象层
```

注意：用户已明确指出，以下内容不是“层”的类别，而是闭环中的具体物理行为：

- 步进电机云台运动。
- 激光点投射到屏幕。
- 摄像头再次观测。

后续讨论架构时不要再把这些行为写成软件层级。

## 全流程架构

当前接受的全流程为：

```text
赛前相机标定
        |
        v
现场摆放设备
        |
        v
现场短标定
        |
        v
锁定运行参数
        |
        v
选择任务模式
        |
        v
视觉检测
        |
        v
屏幕坐标计算
        |
        v
目标生成
        |
        v
视觉伺服控制
        |
        v
发送电机命令
        |
        v
摄像头反馈闭环
```

运行主链路为：

```text
Camera Frame
    |
    v
Vision Detection
    |
    v
Coordinate Mapping
    |
    v
Task Planner
    |
    v
Visual Servo Controller
    |
    v
Motor Command Port
```

## 坐标系结论

算法主坐标系统一为屏幕平面二维坐标，单位 `mm`。

推荐定义：

```text
屏幕左下角: ScreenPoint(0, 0)
屏幕右下角: ScreenPoint(W, 0)
屏幕左上角: ScreenPoint(0, H)
屏幕右上角: ScreenPoint(W, H)
```

图像到屏幕坐标链路：

```text
ImagePoint(u, v)
-> 去畸变
-> Homography
-> ScreenPoint(x_mm, y_mm)
```

从坐标与标定层往上，所有模块统一使用 `ScreenPoint(x_mm, y_mm)`，不直接处理像素坐标。

## 红色系统架构

红色系统只控制红色激光点，不依赖绿色系统。

数据流：

```text
Camera Frame
    |
    v
Red Vision Pipeline
    |
    |-- RedLaserDetection
    |-- TapeQuadDetection
    v
Coordinate Transformer
    |
    |-- red_laser_screen_point
    |-- tape_quad_screen_points
    v
Red Task Planner
    |
    |-- target_screen_point
    v
Laser Tracking Controller
    |
    |-- MotorCommand
    v
Red Motor Port
```

红色系统任务来源包括：

- 屏幕原点。
- 屏幕正方形路径。
- A4 黑胶带矩形路径。

任务层只输出目标屏幕点，控制层只负责跟踪目标点。

## 绿色系统架构

绿色系统只通过自己的摄像头观测红点和绿点，不读取红色系统的状态、路径或指令。

数据流：

```text
Camera Frame
    |
    v
Green Vision Pipeline
    |
    |-- RedLaserDetection
    |-- GreenLaserDetection
    v
Coordinate Transformer
    |
    |-- red_laser_screen_point
    |-- green_laser_screen_point
    v
Green Tracking Task
    |
    |-- target_screen_point = red_laser_screen_point
    v
Laser Tracking Controller
    |
    |-- MotorCommand
    v
Green Motor Port
```

绿色系统的控制目标为：

```text
target = detected_red_point
current = detected_green_point
error = target - current
```

## 当前目录职责

- `configs/`: 保存运行配置、相机参数、阈值参数和任务参数。
- `docs/`: 保存系统架构、算法建模、控制接口、实验记录和方案说明。
- `docs/hands-off/`: 保存交接文档。
- `scripts/`: 保存后续启动、标定、评估等辅助脚本；当前只保留目录说明。
- `src/`: 保存后续程序源码；当前阶段不写实现。
- `tests/`: 保存后续单元测试、集成测试和离线视觉测试。
- `calibration/`: 保存标定流程文档和现场短标定结果。
- `data/`: 保存测试用图片、视频、样例输入和离线复现数据。
- `outputs/`: 保存运行输出、检测结果、轨迹记录和评估结果。
- `logs/`: 保存运行日志、调试日志和异常记录。

## 建议下一步

下一步建议继续讨论“模块边界和接口数据流”，不要直接写代码。

优先确定：

- 每一层的输入输出。
- 核心数据结构命名。
- `CalibrationResult` 的字段边界。
- `LaserDetection`、`TapeQuadDetection`、`ScreenPoint`、`MotorCommand` 的职责。
- 红色系统和绿色系统哪些模块可复用，哪些必须独立。
- 配置文件如何组织在 `configs/` 中。

暂不展开：

- 具体 OpenCV 阈值。
- 具体控制器参数。
- 步进电机驱动代码。
- 具体状态机内部状态。

## 新线程提示词

可以在新线程中直接使用以下提示词：

```text
我们继续讨论 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 架构方案。

请先读取以下文件：
1. 2023e/README.md
2. 2023e/docs/hands-off/2026-07-05-architecture-handoff.md
3. 2023e/docs/plan/2023e/handoff.md
4. 2023e/docs/plan/2023e/plan_v0.md

当前已确认：
1. 使用固定摄像头，摄像头不随云台转动。
2. 使用树莓派/香橙派运行 OpenCV。
3. 云台执行器是步进电机。
4. 红色系统和绿色系统硬件完全独立，不通信。
5. 接受每次摆放后的短标定。
6. 标定阶段允许人工干预，但只用于计算参数。
7. 正式运行阶段不允许人工兜底。
8. 暂不讨论步进电机驱动程序，只保留通信接口。
9. 主方案采用“屏幕平面二维坐标 + 相机标定/单应性 + 短标定 + 视觉伺服闭环”。
10. 红色路径生成选择“识别四角生成几何路径 + 运行时视觉误差修正”。
11. 软件架构分层为 Application、Task、Control、Coordinate、Vision、Device Interface、Hardware。
12. 不要把“步进电机云台运动、激光点投射到屏幕、摄像头再次观测”当作软件层级，它们只是闭环中的物理行为。

请继续从架构设计角度推进，不要直接写代码。下一步优先讨论模块边界和接口数据流：每一层的输入输出、核心数据结构命名、CalibrationResult / LaserDetection / TapeQuadDetection / ScreenPoint / MotorCommand 的职责，以及红色系统和绿色系统哪些模块可复用、哪些必须独立。
```

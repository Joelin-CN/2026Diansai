# 2023 E 题运动目标控制与自动追踪系统

本项目用于整理 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案、标定参数、运行输出和后续代码实现。当前阶段以架构设计和算法建模为主，暂不实现步进电机驱动程序。

## 已确认约束

- 使用固定摄像头，摄像头不随云台转动。
- 使用树莓派或香橙派运行 OpenCV。
- 云台执行器为步进电机。
- 红色系统和绿色系统硬件完全独立，二者不通信。
- 接受每次摆放后的短标定。
- 标定阶段允许人工干预，但只用于计算参数。
- 正式运行阶段不允许人工兜底。
- 主方案采用屏幕平面二维坐标、相机标定/单应性、短标定和视觉伺服闭环。
- 红色路径生成采用“识别四角生成几何路径 + 运行时视觉误差修正”。

## 架构边界

系统架构按职责分层，不把具体物理行为当作软件层级。

推荐分层：

```text
Application Layer      应用组织层
Task Layer             任务生成层
Control Layer          控制计算层
Coordinate Layer       坐标与标定层
Vision Layer           视觉检测层
Device Interface Layer 设备接口层
Hardware Layer         硬件抽象层
```

其中，步进电机云台运动、激光点投射到屏幕、摄像头再次观测属于闭环运行中的物理行为，不作为软件分层类别。

## 全流程

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

## 标定阶段

标定阶段允许人工参与，但人工输入只能用于计算运行参数。

标定输出统一保存为 `CalibrationResult`，供正式运行阶段读取：

```text
CalibrationResult:
- camera_intrinsics
- distortion_coefficients
- H_img_to_screen
- screen_roi
- screen_width_mm
- screen_height_mm
- task_region_corners
- red_motor_coarse_map
- green_motor_coarse_map
- red_laser_detection_params
- green_laser_detection_params
- tape_detection_params
```

正式运行阶段只加载这些参数，不再请求人工点击、人工指定目标、人工修正路径或人工选择光斑。

## 运行阶段

运行阶段完全自动。所有视觉识别、路径生成、追踪控制和异常处理均由程序完成。

运行主链路：

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

## 红色系统

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

红色系统任务来源包括屏幕原点、屏幕正方形路径和 A4 黑胶带矩形路径。任务层只输出目标屏幕点，控制层只负责跟踪目标点。

## 绿色系统

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

## 坐标系统一

算法主坐标系统一为屏幕平面二维坐标，单位 `mm`。

推荐定义：

```text
屏幕左下角: ScreenPoint(0, 0)
屏幕右下角: ScreenPoint(W, 0)
屏幕左上角: ScreenPoint(0, H)
屏幕右上角: ScreenPoint(W, H)
```

从坐标与标定层往上，所有模块统一使用 `ScreenPoint(x_mm, y_mm)`，不再直接处理像素坐标。

图像到屏幕坐标链路：

```text
ImagePoint(u, v)
-> 去畸变
-> Homography
-> ScreenPoint(x_mm, y_mm)
```

## 目录结构

```text
2023e/
  README.md
  configs/       运行参数、标定参数、检测阈值配置
  docs/          架构、建模、标定、接口、计划和交接文档
  scripts/       辅助、调试、预览和测试入口，不放核心业务逻辑
  src/           正式源码目录，当前包含 Vision Layer 实现
  tests/         单元测试、脚本测试和离线视觉测试
  calibration/   标定流程说明和标定产物
  data/          样例输入、测试图像、视频片段
  outputs/       运行输出、截图、轨迹记录、评估结果
  logs/          调试日志和运行日志
```

## 目录职责

- `configs/`: 保存运行配置、相机参数、阈值参数和任务参数。
- `docs/`: 保存系统架构、算法建模、控制接口、实验记录和方案说明。
- `scripts/`: 保存辅助、调试、预览和测试入口；核心算法逻辑不放在 scripts 中。
- `src/`: 保存正式程序源码；当前已有 Vision Layer 检测、跟踪、诊断和运行管线实现。
- `tests/`: 保存后续单元测试、集成测试和离线视觉测试。
- `calibration/`: 保存标定流程文档和现场短标定结果。
- `data/`: 保存测试用图片、视频、样例输入和离线复现数据。
- `outputs/`: 保存运行输出、检测结果、轨迹记录和评估结果。
- `logs/`: 保存运行日志、调试日志和异常记录。

## 当前阶段

当前已建立项目骨架、架构文档、Vision Layer 初版实现、离线测试和预览/调试脚本。后续应继续细化：

- 模块边界和接口数据结构。
- 标定参数文件格式。
- 红色系统任务生成接口。
- 绿色系统追踪接口。
- 视觉检测模块输入输出。
- 控制层到电机接口的命令协议。

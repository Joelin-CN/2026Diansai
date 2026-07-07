# 2026-07-05 视觉检测层算法讨论交接文档

## 项目位置

当前项目目录：

```text
2023e/
```

本阶段新增的视觉检测层结构文档：

```text
2023e/docs/structure/2026-07-05-vision-layer-structure.md
```

已有总架构交接文档：

```text
2023e/docs/hands-off/2026-07-05-architecture-handoff.md
```

## 当前阶段结论

当前已经完成视觉检测层的结构设计讨论，下一阶段准备开始讨论视觉检测层内部的具体 OpenCV 算法。

仍然保持以下边界：

- 当前先讨论架构和算法方案，不直接写代码。
- 不讨论步进电机驱动程序，只保留通信接口。
- 红色系统和绿色系统硬件完全独立，不通信。
- 标定阶段允许人工干预，但只用于计算参数。
- 正式运行阶段不允许人工兜底。
- 主方案仍为“屏幕平面二维坐标 + 相机标定/单应性 + 短标定 + 视觉伺服闭环”。

## 视觉层结构已确认

视觉检测层 `Vision Layer` 的职责是：

```text
从当前摄像头帧中检测可观测目标，并输出图像坐标系下的位置、形状、质量和置信度。
```

视觉层不负责：

- 将图像坐标转换为屏幕坐标。
- 生成红色系统路径。
- 计算控制误差。
- 输出步进电机命令。
- 正式运行阶段请求人工兜底。

核心边界：

```text
Vision Layer 输出 ImagePoint / ImageQuad
Coordinate Layer 输出 ScreenPoint / ScreenQuad
Task Layer 使用 ScreenPoint / ScreenQuad 生成目标点或路径
Control Layer 使用目标点与当前点计算 MotorCommand
```

## 坐标约定

已经明确区分：

```text
ImagePoint(u_px, v_px)
```

含义：图像像素坐标，原点在图像左上角，`u` 向右，`v` 向下。

```text
ScreenPoint(x_mm, y_mm)
```

含义：屏幕平面坐标，原点在屏幕左下角，`x` 向右，`y` 向上，单位为 mm。

下一阶段讨论具体算法时，不要把 detector 设计成直接输出 `ScreenPoint`。具体算法只产生图像空间结果，屏幕坐标转换由 `Coordinate Layer` 完成。

## 核心数据结构

视觉层统一输出：

```text
VisionFrameResult:
- frame_id
- timestamp_ms
- red_laser
- green_laser
- tape_quad
- screen_detection
- diagnostics
```

红点和绿点检测结果：

```text
LaserDetection:
- color
- found
- image_center
- image_radius_px
- area_px
- brightness
- confidence
- source
- failure_reason
```

A4 黑胶带四边形检测结果：

```text
TapeQuadDetection:
- found
- image_corners
- image_corners_ordered
- contour_area_px
- aspect_ratio
- confidence
- source
- failure_reason
```

屏幕区域检测结果：

```text
ScreenDetection:
- found
- image_corners
- image_roi
- confidence
- source
- failure_reason
```

诊断信息：

```text
VisionFrameDiagnostics:
- processing_time_ms
- enabled_detectors
- roi_used
- candidate_count
- rejected_candidates
- warnings
- debug_artifacts, optional
```

## ScreenDetection 当前定位

`ScreenDetection` 已被接纳，但定位必须保持清楚：

```text
ScreenDetection 主要服务短标定和运行自检，不是正式运行每帧控制链路的强依赖。
```

短标定阶段用途：

- 辅助检测屏幕四角。
- 配合人工确认或修正屏幕角点。
- 生成或校验 `screen_roi`。
- 为 `Coordinate Layer` 计算 `homography_img_to_screen` 提供图像角点。

正式运行阶段用途：

- 加载短标定产生的 `screen_roi`。
- 限制红点、绿点、A4 黑胶带的检测区域。
- 可低频自检屏幕区域是否仍可信。
- 自检失败时向上报告异常，不自动重置坐标系。

不建议用途：

- 每帧重新计算主单应性矩阵。
- 每帧重新定义屏幕坐标系。
- 直接生成屏幕任务方框路径。
- 代替 `Coordinate Layer` 输出 `ScreenPoint`。

## 当前配置文件状态

配置目录已经按系统实例划分：

```text
2023e/configs/
  common/
  red/
  green/
```

通用配置：

- `common/contest_geometry.json`: 题目几何尺寸。
- `common/screen_coordinate_system.json`: 屏幕坐标轴、任务方框角点、中心点、顺时针方向。
- `common/task_requirements.json`: 任务误差、时间和方向要求。

红色系统视觉相关配置：

- `red/camera.json`: 红色系统摄像头帧率、分辨率、曝光、内参文件路径。
- `red/calibration.json`: 红色系统标定产物路径。
- `red/vision.json`: 启用 `red_laser`、`a4_black_tape_quad`、`task_square`。
- `red/runtime.json`: 红色任务来源、A4 路径来源、电机接口、日志配置。

绿色系统视觉相关配置：

- `green/camera.json`: 绿色系统摄像头帧率、分辨率、曝光、内参文件路径。
- `green/calibration.json`: 绿色系统标定产物路径。
- `green/vision.json`: 启用 `red_laser` 和 `green_laser`。
- `green/runtime.json`: 绿色追踪目标来源、电机接口、日志配置。

结构结论：

```text
视觉算法结构可以复用，视觉参数实例必须按 red / green 系统独立保存。
```

原因：两套系统的摄像头、视角、曝光、背景、光斑大小都可能不同。即使都检测红点，也不能默认共用阈值。

## 下一阶段讨论重点

下一阶段应开始讨论视觉检测层内部的具体算法，但仍先做方案设计，不直接写代码。

建议讨论顺序：

1. `LaserDetector` 红点/绿点检测算法。
2. 红色 HSV hue 跨 0/180 的处理方式。
3. 亮度阈值、饱和度阈值、面积范围和候选点筛选。
4. 多候选光斑时的选择策略。
5. 光斑丢失、反光误检、过曝光斑的失败判定。
6. `TapeQuadDetector` A4 黑胶带四边形检测算法。
7. 黑胶带轮廓提取、四边形拟合、角点排序和置信度计算。
8. `ScreenDetector` 在短标定和运行自检中的算法边界。
9. `VisionStateTracker` 跨帧 ROI、丢失计数和重捕获策略。
10. 离线测试图像、调试输出和诊断指标应该如何设计。

暂不展开：

- 控制器增益和电机步数换算。
- 步进电机驱动协议细节。
- 任务状态机完整实现。
- 具体 Python/OpenCV 代码。

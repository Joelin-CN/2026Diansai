# 2026-07-05 Vision Layer 结构设计

Document Version: v1.0

## 1. 文档目的

本文定义 2023 年电赛 E 题 OpenCV 方案中 `Vision Layer` 的正式结构边界、输入输出、运行模式、内部模块职责和调试/运行分离原则。

本版本基于当前已落地实现重写，目标是让 `Vision Layer` 的 formal runtime path 朝 `<= 30 ms/frame` 收敛。

本文只讨论视觉层，不讨论：

- `Coordinate Layer` 的屏幕坐标变换实现。
- `Task Layer` 的路径生成实现。
- `Control Layer` 的误差计算与电机控制实现。
- 步进电机驱动细节。

## 2. 已确认边界

`Vision Layer` 只负责回答：

```text
这一帧图像中看到了哪些目标，
它们在图像坐标系中的位置、形状、质量、失败原因和阶段耗时是什么。
```

`Vision Layer` 只输出图像空间结构：

- `ImagePoint`
- `ImageQuad`
- detection dataclass
- diagnostics

`Vision Layer` 不输出：

- `ScreenPoint`
- `ScreenQuad`
- 任务路径
- 控制误差
- `MotorCommand`

核心分层边界：

```text
Vision Layer:
  输出 ImagePoint / ImageQuad / Detection / diagnostics

Coordinate Layer:
  将 ImagePoint / ImageQuad 转为 ScreenPoint / ScreenQuad

Task Layer:
  使用 ScreenPoint / ScreenQuad 生成目标点或路径

Control Layer:
  使用目标点和当前点计算 MotorCommand
```

因此必须严格区分：

```text
ImagePoint(u_px, v_px): 图像像素坐标，原点在图像左上角
ScreenPoint(x_mm, y_mm): 屏幕平面坐标，原点在屏幕左下角，单位 mm
```

## 3. 结构目标

当前 `Vision Layer` 的结构目标有两个：

1. 给上层提供可靠、显式、可诊断的图像观测结果。
2. 把 formal runtime path 从 preview/debug 重路径中剥离出来，为实时性优化提供清晰热路径。

这意味着：

- formal runtime path 必须按题目任务建模，而不是按 detector 任意组合建模。
- preview/debug 可以保留更重的对比分析逻辑，但不能泄漏到 runtime 热路径。
- 边框类 support detector 不能默认每帧运行。

## 4. 检测对象

当前 `Vision Layer` 关注的检测对象：

- 红色激光点
- 绿色激光点
- A4 黑胶带四边形
- 屏幕区域/屏幕方框检测结果

其中职责定位如下：

- 红/绿激光点：formal runtime 高频检测对象
- A4 黑胶带：A4 任务的低频 support detector
- 屏幕检测：标定辅助、debug 验证、运行自检候选基础，不是当前 formal runtime 每帧强依赖

## 5. Vision Layer 输入

### 5.1 CameraFrame

`CameraFrame` 是 `Vision Layer` 每帧处理的主输入。

当前字段：

```text
CameraFrame:
- image
- frame_id
- timestamp_ms
- camera_id
- width_px
- height_px
- color_format
```

约定：

- 红色系统和绿色系统分别使用各自摄像头输入。
- `Vision Layer` 不从 `CameraFrame` 推断任务模式。
- 任务模式由上层在调用 `VisionPipeline.process(..., mode=...)` 时显式给出。

### 5.2 VisionRuntimeConfig

`VisionRuntimeConfig` 来自 `configs/red/vision.json` 或 `configs/green/vision.json`。

当前结构：

```text
VisionRuntimeConfig:
- red_laser
- green_laser
- tape_quad
- lost_target_policy
```

约定：

- 红/绿系统参数实例必须独立保存，不共享阈值。
- `lost_target_policy` 只表达视觉层检测丢失后的上层处理建议，不由视觉层直接执行控制动作。
- 绿色配置中即使没有 `a4_black_tape_quad` 参数块，formal combined runtime mode 仍可使用统一的 A4 detector 结构。

### 5.3 标定相关输入

`Vision Layer` 可读取标定结果中的图像空间约束，例如：

```text
screen_roi
laser_detection_params
tape_detection_params
```

`Vision Layer` 不应读取或依赖：

```text
motor_coarse_map
task path
control gains
MotorCommand
```

`homography_img_to_screen` 属于 `Coordinate Layer` 核心输入，不属于底层 detector 输出。

## 6. 正式运行模式

formal runtime mode 按题目任务拆成 4 个，外加 1 个非实时 `DEBUG` 模式。

### 6.1 RUNTIME_SCREEN_RED

对应任务：

- 红点回原点
- 红点沿屏幕灰色方框运行

运行特点：

- 高频：`red_laser`
- 低频 support：`screen_square` 候选能力
- 当前 formal runtime 默认不逐帧跑 `screen_square`

### 6.2 RUNTIME_A4_RED

对应任务：

- 红点沿 A4 黑胶带运行
- A4 旋转后红点继续沿黑胶带运行

运行特点：

- 高频：`red_laser`
- 低频 support：`a4_tape`
- 当前实现中 A4 support 允许缓存复用，不默认每帧重跑

### 6.3 RUNTIME_TRACK_RED_GREEN

对应任务：

- 绿色光斑追踪红色光斑

运行特点：

- 高频：`red_laser`
- 高频：`green_laser`
- 无 runtime A4 support

### 6.4 RUNTIME_A4_TRACK_RED_GREEN

对应任务：

- 红点执行 A4 任务，同时绿点自动追踪红点

运行特点：

- 高频：`red_laser`
- 高频：`green_laser`
- 低频 support：`a4_tape`

### 6.5 DEBUG

`DEBUG` 只用于：

- preview
- strict/relaxed 对比
- overlay 验收
- richer diagnostics

`DEBUG` 明确不属于 `<= 30 ms/frame` 正式 runtime 目标。

## 7. Vision Layer 输出

统一输出 `VisionFrameResult`：

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

### 7.1 LaserDetection

`LaserDetection` 表示某一颜色激光点在当前帧中的检测结果。

当前字段：

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
- diagnostics
```

约定：

- `found=True` 只表示本帧真实测得目标。
- 当前 formal runtime 依赖 measured-only 语义，不允许 predicted-only 伪装成真实观测。
- `image_center` 仅是图像坐标，不允许直接上升为屏幕坐标。
- `failure_reason` 必须可用于诊断 runtime 丢失原因。

当前常见失败原因：

```text
not_found
too_small
too_large
low_brightness
multiple_candidates
outside_roi
low_confidence
```

### 7.2 TapeQuadDetection

`TapeQuadDetection` 表示 A4 黑胶带四边形检测结果。

当前结构重点：

```text
TapeQuadDetection:
- found
- outer_quad
- inner_quad
- image_corners
- image_corners_ordered
- contour_area_px
- inner_area_px
- tape_ratio
- aspect_ratio
- confidence
- source
- failure_reason
- diagnostics
```

关键约定：

```text
found=true 只有在 outer_quad 和 inner_quad 同时成立时为真
```

并且：

- 只输出图像坐标四边形
- 不生成 A4 路径
- 不输出 `ScreenQuad`

### 7.3 ScreenDetection

`ScreenDetection` 表示屏幕区域或屏幕方框检测结果。

当前职责定位：

- 标定辅助
- preview/debug 验证
- 运行自检候选基础

当前不作为正式 runtime 每帧主链强依赖。

当前字段：

```text
ScreenDetection:
- found
- image_corners
- image_roi
- confidence
- source
- failure_reason
- diagnostics
```

运行期如果失败，只允许向上报告：

```text
screen_detection.found = false
failure_reason = screen_not_found
```

视觉层不应因此人工兜底，也不应擅自重建坐标系。

### 7.4 VisionFrameDiagnostics

`VisionFrameDiagnostics` 用于 runtime 观测证据、日志和离线复盘。

当前字段：

```text
VisionFrameDiagnostics:
- processing_time_ms
- enabled_detectors
- roi_used
- candidate_count
- failure_reason
- rejected_candidates
- warnings
- tracker_state
- lost_frame_count
- stage_time_ms
- debug_artifacts
```

当前 runtime diagnostics 的核心要求：

- 保持轻量
- 可用于逼近 `<= 30 ms/frame`
- 不依赖 debug-only artifacts

当前已落地的关键 timing 字段：

```text
stage_time_ms.red_laser
stage_time_ms.green_laser
stage_time_ms.a4_tape
stage_time_ms.screen_square
stage_time_ms.total_processing
```

## 8. 内部模块结构

当前推荐结构：

```text
VisionPipeline
  -> LaserDetector
  -> TapeQuadDetector
  -> ScreenDetector
  -> VisionStateTracker
  -> VisionDiagnosticsBuilder
```

### 8.1 VisionPipeline

`VisionPipeline` 是 `Vision Layer` 对外入口。

职责：

- 接收 `CameraFrame`
- 根据 `VisionMode` 决定 formal runtime path
- 运行红/绿 laser 高频检测
- 运行或复用低频 support detector
- 聚合输出 `VisionFrameResult`
- 生成 lightweight runtime diagnostics

关键原则：

- formal runtime path 按任务 mode gating
- 不按 detector 任意组合建模
- runtime 与 debug 路径分离

### 8.2 LaserDetector

`LaserDetector` 复用同一算法结构，通过参数区分红点和绿点。

当前 detector 已支持 bright-core + color-ring 候选思路，用于改善黑胶带区域上的光斑检测稳定性。

输入：

```text
CameraFrame.image
LaserDetectionParams
ROI
optional previous_detection
```

输出：

```text
LaserDetection
```

### 8.3 TapeQuadDetector

`TapeQuadDetector` 负责检测 A4 黑胶带四边形。

当前定位：

- A4 任务的低频 support detector
- formal runtime 不默认每帧运行
- 可在 runtime 中复用最近一次有效结果

### 8.4 ScreenDetector

`ScreenDetector` 负责屏幕区域/屏幕方框检测。

当前定位：

```text
Calibration / preview / runtime self-check candidate
```

而不是：

```text
formal runtime 每帧必跑主链 detector
```

### 8.5 VisionStateTracker

`VisionStateTracker` 维护跨帧状态。

当前职责：

- 维护 red/green 各自 tracker state
- 保存最近一次测量结果
- 生成下一帧建议 ROI
- 维护 bounded reacquire 过程中的 search ROI

当前状态集合：

```text
UNINITIALIZED
TRACKING
TEMP_LOST
REACQUIRING
LOST
```

## 9. formal runtime 的高低频拆分

formal runtime 结构上明确区分：

### 9.1 高频路径

每帧运行：

- `red_laser`
- `green_laser`
- tracker ROI 更新

### 9.2 低频 support 路径

按初始化/刷新规则运行：

- `a4_tape`
- `screen_square`

当前实现原则：

- 有效 support 结果可缓存复用
- refresh 帧才重跑 support detector
- `DEBUG` 可继续逐帧运行重路径

## 10. bounded reacquire 原则

formal runtime 明确禁止：

```text
strict 失败 -> 全图 relaxed fallback
```

当前 formal runtime 的 ROI 行为应理解为：

```text
TRACKING:
  小 ROI

TEMP_LOST:
  第一阶段扩大 ROI

REACQUIRING:
  第二阶段扩大 ROI

LOST:
  有 screen_roi 时回到 screen_roi
  无 screen_roi 时保留最后一次 bounded search ROI
```

这意味着 runtime 热路径不会退化为 preview 风格的全图 relaxed 搜索。

## 11. runtime 与 debug 解耦

### 11.1 formal runtime

formal runtime 只保留：

- task-driven mode gating
- 高频 laser 检测
- 低频 support detector 刷新/缓存
- bounded reacquire
- lightweight diagnostics

### 11.2 DEBUG / preview

`DEBUG` 路径可保留：

- strict/relaxed 对比
- richer overlay
- 逐帧 border refresh
- 更重的候选分析输出

但这些行为不得进入 formal runtime 热路径。

## 12. 红绿系统复用与独立

可复用：

- `CameraFrame` 结构
- `ImagePoint` / `ImageQuad`
- `LaserDetection` / `TapeQuadDetection` / `ScreenDetection`
- `VisionFrameResult`
- `VisionPipeline` 组织方式
- `LaserDetector` / `TapeQuadDetector` / `ScreenDetector` 框架
- diagnostics 输出格式

必须独立：

- 红色系统摄像头输入
- 绿色系统摄像头输入
- 红/绿系统 `vision.json` 参数实例
- 红/绿系统 `CalibrationResult`
- 红/绿系统跨帧 tracker 状态
- 红/绿系统运行进程

## 13. 调用链路

### 13.1 RUNTIME_SCREEN_RED

```text
CameraFrame
  -> VisionPipeline(mode=RUNTIME_SCREEN_RED)
      -> red_laser
  -> VisionFrameResult
  -> Coordinate Layer
  -> Task Layer
  -> Control Layer
```

### 13.2 RUNTIME_A4_RED

```text
CameraFrame
  -> VisionPipeline(mode=RUNTIME_A4_RED)
      -> red_laser
      -> a4_tape refresh/reuse
  -> VisionFrameResult
  -> Coordinate Layer
  -> Task Layer
  -> Control Layer
```

### 13.3 RUNTIME_TRACK_RED_GREEN

```text
CameraFrame
  -> VisionPipeline(mode=RUNTIME_TRACK_RED_GREEN)
      -> red_laser
      -> green_laser
  -> VisionFrameResult
  -> Coordinate Layer
  -> Task Layer
  -> Control Layer
```

### 13.4 RUNTIME_A4_TRACK_RED_GREEN

```text
CameraFrame
  -> VisionPipeline(mode=RUNTIME_A4_TRACK_RED_GREEN)
      -> red_laser
      -> green_laser
      -> a4_tape refresh/reuse
  -> VisionFrameResult
  -> Coordinate Layer
  -> Task Layer
  -> Control Layer
```

### 13.5 DEBUG

```text
CameraFrame
  -> Debug preview path
      -> strict / relaxed compare
      -> overlay
      -> richer summary
```

## 14. 异常输出约定

视觉层遇到失败必须显式报告，不能返回假坐标。

例如：

```text
LaserDetection.found = false
LaserDetection.source = lost
LaserDetection.failure_reason = not_found
```

上层可根据：

```text
lost_target_policy.max_missing_frames
lost_target_policy.action
```

决定是否暂停运行或进入其他安全状态，但视觉层本身不直接调用电机接口。

## 15. 当前结构结论

当前 v1.0 结构结论如下：

```text
Vision Layer:
  只输出图像空间观测与 diagnostics

Formal runtime:
  按 4 个题目驱动 mode 组织

High-frequency path:
  red/green laser + tracker ROI

Low-frequency support path:
  A4 / screen support detector refresh or cache reuse

Bounded reacquire:
  不允许 formal runtime 回退到 preview 风格全图 relaxed fallback

DEBUG:
  保留为非实时分析路径
```

这使 `Vision Layer` 的 runtime 热路径、边界职责和调试路径分离关系都清晰可控，也为下一步继续以 timing diagnostics 驱动实时优化提供了稳定基础。

# 2026-07-05 视觉检测层代码落地交接文档

> Note: 本文记录 2026-07-05 当时的交接状态；仓库后续已重构并补充实现，当前路径以仓库根目录下 `2023e/` 为准。

## 项目位置

当前项目目录：

```text
2023e/
```

当前源码目录仍基本为空：

```text
2023e/src/.gitkeep
2023e/tests/.gitkeep
```

下一阶段可以开始落实 `Vision Layer` 的代码，但仍需保持现有架构边界，不直接实现步进电机驱动。

## 必读文档

新线程开始前应先读取：

```text
2023e/README.md
2023e/docs/structure/2026-07-05-vision-layer-structure.md
2023e/docs/algorithm/2026-07-05-vision-layer-algorithm-design.md
2023e/docs/hands-off/2026-07-05-vision-algorithm-handoff.md
2023e/configs/red/vision.json
2023e/configs/green/vision.json
2023e/configs/red/calibration.json
2023e/configs/green/calibration.json
```

如需回看总架构，再读取：

```text
2023e/docs/hands-off/2026-07-05-architecture-handoff.md
```

## 已确认边界

核心边界：

```text
Vision Layer     -> 输出 ImagePoint / ImageQuad / Detection 结果
Coordinate Layer -> 输出 ScreenPoint / ScreenQuad
Task Layer       -> 使用 ScreenPoint / ScreenQuad 生成目标点或路径
Control Layer    -> 使用目标点和当前点计算 MotorCommand
```

视觉层不负责：

- 输出 `ScreenPoint` 或 `ScreenQuad`。
- 生成任务路径。
- 计算控制误差。
- 输出 `MotorCommand`。
- 在正式运行阶段请求人工兜底。
- 每帧更新主 `homography_img_to_screen`。

运行阶段检测失败时，视觉层必须显式输出失败状态和失败原因，不能返回假坐标。

## 当前算法结论

### LaserDetector / ColorBlobDetector

主链路：

```text
CameraFrame.image
  -> active_roi
  -> HSV
  -> hue_ranges_deg + saturation_min + value_min mask
  -> small morphology open/close
  -> connected components or contours
  -> candidate filtering
  -> candidate scoring
  -> measured LaserDetection or lost
```

红色 hue 需要跨 `0/180`：

```text
red_mask = mask([0, 10]) OR mask([170, 180])
```

绿色使用单区间：

```text
green_mask = mask([45, 85])
```

注意 OpenCV HSV 通常为：

```text
H: 0..179
S: 0..255
V: 0..255
```

光斑中心估计优先级：

```text
brightness-weighted centroid
bright-core centroid
contour moments centroid
min-enclosing-circle center
```

候选评分包含：

```text
color_score
brightness_score
shape_score
area_score
temporal_score
roi_score
```

`HoughCircles` 不作为主链路，只可作为离线调试或备用诊断。

### VisionStateTracker

追踪不是替代检测，而是负责跨帧关联和 ROI 管理。

推荐状态：

```text
UNINITIALIZED
TRACKING
TEMP_LOST
REACQUIRING
LOST
```

ROI 策略：

```text
UNINITIALIZED -> screen_roi
TRACKING      -> local ROI around last measured point
TEMP_LOST     -> expanded previous ROI
REACQUIRING   -> larger ROI or screen_roi
LOST          -> screen_roi
```

Kalman 的位置：

```text
previous tracker state
  -> optional Kalman predict
  -> build active_roi around predicted point
  -> detector finds real candidates
  -> association selects measured candidate
  -> optional Kalman correct(measured)
  -> update tracker state
```

Kalman 只用于：

- 预测下一帧 ROI。
- 候选连续性评分。
- 诊断显示。

Kalman 不允许：

- 替代颜色检测。
- 输出 `found=true` 的检测点。
- 把预测点交给控制链路当作真实测量。

控制链路只能消费：

```text
found=true
source=measured
```

### TapeQuadDetector

第一阶段实现可暂缓，除非要直接做红色 A4 黑胶带任务。

主链路：

```text
screen_roi
  -> grayscale
  -> Otsu or adaptive threshold
  -> morphology close
  -> contours
  -> approxPolyDP
  -> quad filtering
  -> clockwise image corner ordering
```

Canny 和 HoughLinesP 可作为后续增强，不建议第一版主链路直接依赖。

### ScreenDetector

第一阶段代码可只保留接口或低频自检接口设计，不要实现运行期自动重标定。

用途：

- 短标定辅助找屏幕四角。
- 运行期低频自检 `screen_roi` 是否可信。
- 为 detector 提供 ROI 限制。

不允许：

- 运行期人工点击兜底。
- 每帧重算 Homography。
- 直接输出 ScreenPoint。

## 建议第一阶段代码范围

第一阶段建议只落实视觉层最小可测试闭环：

```text
data structures
config loading
LaserDetector / ColorBlobDetector
VisionStateTracker
VisionPipeline aggregation
diagnostics
offline unit tests with synthetic images
```

暂缓：

- 步进电机接口实现。
- Control Layer。
- Task Layer 完整状态机。
- Coordinate Layer 完整实现。
- TapeQuadDetector 高级增强。
- ScreenDetector 自动检测完整算法。
- Kalman 作为强依赖。

Kalman 建议作为可选增强开关，不要让第一版依赖它才能工作。

## 建议目录结构

下一阶段可以在 `2023e/src/` 下建立 Python 包结构。建议保持分层清晰：

```text
2023e/src/
  vision/
    __init__.py
    types.py
    config.py
    laser_detector.py
    state_tracker.py
    pipeline.py
    diagnostics.py
```

测试建议：

```text
2023e/tests/
  vision/
    test_laser_detector.py
    test_state_tracker.py
    test_pipeline.py
```

如果项目后续采用统一包名，也可以把 `vision/` 放在更外层包下，但不要把视觉层代码直接混入控制、电机或任务逻辑。

## 建议核心数据结构

先实现这些轻量数据结构即可：

```text
ImagePoint:
  u_px
  v_px

ImageQuad:
  points

CameraFrame:
  image
  frame_id
  timestamp_ms
  camera_id
  width_px
  height_px
  color_format

LaserDetection:
  color
  found
  image_center
  image_radius_px
  area_px
  brightness
  confidence
  source
  failure_reason

VisionFrameResult:
  frame_id
  timestamp_ms
  red_laser
  green_laser
  tape_quad
  screen_detection
  diagnostics
```

建议枚举：

```text
LaserColor: RED, GREEN
DetectionSource: measured, predicted, lost
FailureReason:
  not_found
  too_small
  too_large
  low_brightness
  multiple_candidates
  outside_roi
  low_confidence

TrackerState:
  UNINITIALIZED
  TRACKING
  TEMP_LOST
  REACQUIRING
  LOST
```

## 测试建议

第一阶段不需要真实摄像头才能测试。

建议先写合成图测试：

- 黑底或浅色背景上的红色圆点。
- 红色 hue 跨 `0/180` 的两段 mask。
- 绿色圆点。
- 多个红色候选时选择靠近上一帧的候选。
- 候选过小、过大、低亮度时输出失败原因。
- 连续丢失后 tracker 从 `TRACKING` 转到 `TEMP_LOST` / `REACQUIRING` / `LOST`。
- `source=predicted` 不得作为 `found=true` 输出。

如已有真实图片或视频，后续再增加离线回放测试。

## 实现注意事项

- 默认使用 OpenCV BGR 输入时，转换 HSV 应使用 BGR 到 HSV 的转换约定。
- `hue_ranges_deg` 配置中的 `180` 需要与 OpenCV `H=179` 上限做一致处理。
- ROI 裁剪后得到的候选坐标必须转换回整帧图像坐标。
- 形态学核不能太大，避免抹掉小光斑。
- 多候选分数接近时应输出 `multiple_candidates`，不要强行选一个。
- `VisionStateTracker` 可以影响下一帧 ROI，但不能伪造本帧测量值。
- 红色系统和绿色系统的配置、状态和 pipeline 实例必须独立。

## 新线程提示词

可以在新线程中直接使用以下提示词：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案，现在开始把 Vision Layer 视觉检测层的第一阶段代码落地。

请先读取以下文件：
1. 2023e/README.md
2. 2023e/docs/structure/2026-07-05-vision-layer-structure.md
3. 2023e/docs/algorithm/2026-07-05-vision-layer-algorithm-design.md
4. 2023e/docs/hands-off/2026-07-05-vision-implementation-handoff.md
5. 2023e/configs/red/vision.json
6. 2023e/configs/green/vision.json
7. 2023e/configs/red/calibration.json
8. 2023e/configs/green/calibration.json

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection 结果，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. Coordinate Layer 以后负责 ImagePoint -> ScreenPoint 的去畸变和 Homography 转换。
3. 第一阶段先实现视觉层最小可测试闭环：数据结构、配置加载、LaserDetector / ColorBlobDetector、VisionStateTracker、VisionPipeline 聚合、诊断输出和离线合成图测试。
4. LaserDetector 主算法为 HSV 多 hue 区间阈值 + 亮度增强 + 连通域/轮廓 + 多候选评分 + 跨帧 ROI。
5. 红色 hue 需要合并 [0, 10] 和 [170, 180] 两段；绿色使用 [45, 85]。
6. VisionStateTracker 负责 TRACKING / TEMP_LOST / REACQUIRING / LOST 状态、ROI 扩展和重捕获。
7. Kalman 只作为可选增强，用于 ROI 预测和候选连续性评分，不允许把预测点作为 found=true 的测量点。
8. 控制链路只能消费 found=true 且 source=measured 的观测。
9. 红色系统和绿色系统的配置、状态和 pipeline 实例必须独立。
10. 暂不实现步进电机驱动、Control Layer、Task Layer、Coordinate Layer 完整转换、TapeQuadDetector 高级增强和 ScreenDetector 自动检测完整算法。

请先检查当前 src/tests 结构，提出一个最小实现计划，然后开始按测试驱动方式实现第一阶段 Vision Layer。实现时保持模块边界清晰，优先写合成图离线测试验证红/绿光斑检测、红色 hue 跨界、多候选筛选、丢失状态和 ROI 重捕获。
```

## 下一阶段完成标准

下一阶段完成后至少应能做到：

- 通过合成图检测红点和绿点。
- 正确处理红色 hue 跨 `0/180`。
- 多候选时可按评分选择或报告 `multiple_candidates`。
- ROI 裁剪后的输出坐标仍是整帧图像坐标。
- 连续丢失时 tracker 状态正确变化。
- 不把 predicted 点作为 `found=true` 输出。
- 红绿系统可用同一算法结构但不同配置实例运行。

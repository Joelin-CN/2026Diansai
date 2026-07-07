# 2026-07-05 视觉检测层算法设计

## 1. 文档目的

本文定义 2023 年电赛 E 题“运动目标控制与自动追踪系统”中 `Vision Layer` 的 OpenCV 算法方案。

当前阶段只做算法设计，不写具体实现代码，不讨论步进电机驱动程序。

## 2. 已确认边界

视觉检测层只输出图像空间结果：

```text
ImagePoint(u_px, v_px)
ImageQuad([ImagePoint])
LaserDetection
TapeQuadDetection
ScreenDetection
VisionFrameResult
```

视觉检测层不输出：

```text
ScreenPoint
ScreenQuad
MotorCommand
```

职责边界保持为：

```text
Vision Layer     -> 检测图像中的目标，输出 ImagePoint / ImageQuad
Coordinate Layer -> 将 ImagePoint / ImageQuad 转为 ScreenPoint / ScreenQuad
Task Layer       -> 使用 ScreenPoint / ScreenQuad 生成目标点或路径
Control Layer    -> 根据目标点和当前点计算 MotorCommand
```

运行阶段不允许人工兜底。检测失败时，视觉层必须显式输出失败状态和失败原因，不能返回假坐标。

## 3. 总体算法选择

`LaserDetector` 主方案采用：

```text
颜色阈值检测 + 跨帧 ROI + 多候选综合评分
```

并吸收亮度峰值和过曝判据作为增强。

采用该方案的原因：

- 单帧 HSV 阈值检测简单，但在反光、多亮斑和红色 hue 跨界时容易跳点。
- 跨帧 ROI 和上一帧位置先验可以显著降低误检和跳变。
- 亮度峰值可以提升激光点中心定位质量，避免外接轮廓被反光拉偏。
- 预测信息只用于 ROI 和评分，不允许伪装成本帧真实测量值。

状态原则：

```text
found=true  只表示本帧真实检测到可信目标
source=measured 只表示真实观测
source=predicted 不能作为控制链路的测量点使用
source=lost 明确表示当前帧没有可信观测
```

上层控制链路只能消费 `found=true` 且 `source=measured` 的激光观测。`source=predicted` 只允许用于诊断、ROI 生成或状态展示。

### 3.1 可吸收算法范围

结合 OpenCV 常用视觉算法，本项目吸收以下算法作为主线或增强：

- HSV 颜色空间阈值和多 hue 区间合并，用于红绿激光点候选提取。
- 亮度峰值、高亮核心和亮度加权质心，用于提高光斑中心定位质量。
- 小核形态学开闭运算，用于 mask 去噪、补洞和连接断裂区域。
- 连通域或轮廓分析，用于提取光斑候选、胶带区域和屏幕候选区域。
- 轮廓矩、面积、外接框、最小外接圆和圆度，用于候选评分和失败判定。
- Otsu 和自适应阈值，用于黑胶带区域提取。
- Canny 和 HoughLinesP，作为屏幕边界或胶带边断裂时的辅助增强，不作为主链路强依赖。
- 相机去畸变和 Homography，作为 Coordinate Layer 的核心算法背景，与 Vision Layer 输出边界保持分离。
- 简单帧间 ROI、最近邻连续性和可选 Kalman 预测，用于重捕获和候选评分。

暂不作为主线的算法：

- HoughCircles：可用于离线调试圆形光斑，但参数敏感，不适合作为激光点主检测链路。
- Optical Flow：光斑缺少稳定纹理，容易跟到背景或反光，不作为主方案。
- Background Subtraction：光斑可静止，屏幕内容和环境亮度变化会干扰，不作为主方案。
- Template Matching：光斑形状、大小和过曝状态变化大，维护成本高。
- 深度学习检测：数据采集、标注和部署成本高，不符合当前可解释、易调参目标。
- ArUco / AprilTag：若规则允许可作为短标定辅助，但不作为默认现场条件。

## 4. LaserDetector 算法

### 4.1 输入输出

输入：

```text
CameraFrame.image
LaserDetectionParams
active_roi
optional last_detection
optional VisionTrackingState
```

输出：

```text
LaserDetection
```

红点和绿点复用同一套算法结构，只使用不同参数实例。

当前参数来源：

- `configs/red/vision.json` 的 `detections.red_laser`。
- `configs/green/vision.json` 的 `detections.red_laser`。
- `configs/green/vision.json` 的 `detections.green_laser`。

红色系统和绿色系统的参数实例、摄像头输入、标定结果和 `VisionTrackingState` 必须独立。

### 4.2 基本流程

推荐流程：

```text
CameraFrame.image
  -> 选择 active_roi
  -> 转 HSV
  -> 按 hue_ranges_deg / saturation_min / value_min 生成 mask
  -> 形态学去噪
  -> 连通域或轮廓提取
  -> 候选基础过滤
  -> 多候选综合评分
  -> 输出最佳 LaserDetection 或 lost
```

`active_roi` 的优先级：

```text
TRACKING 状态局部 ROI
TEMP_LOST / REACQUIRING 状态扩展 ROI
未锁定或 LOST 状态 screen_roi
```

`active_roi` 只能限制搜索范围和参与评分，不能改变输出坐标系。输出的 `image_center` 始终使用整帧图像坐标。

### 4.3 红色 HSV 跨 0/180 处理

OpenCV HSV 中 `H` 的实际取值通常是 `0..179`，`S` 和 `V` 的取值是 `0..255`。配置文件中的 `hue_ranges_deg` 使用接近角度语义的写法，落地实现时应明确映射到 OpenCV 的 hue 范围。

红色 hue 会跨越 0/180 边界。因此红色不能用单一区间表示。

推荐规则：

```text
hue_ranges_deg = [[0, 10], [170, 180]]
mask_red = mask([0, 10]) OR mask([170, 180])
```

该逻辑不应写死为红色专用，而应作为通用多 hue 区间机制：

```text
for each range in hue_ranges_deg:
  build partial mask
merge all partial masks by OR
```

绿色光斑使用单区间，例如：

```text
hue_ranges_deg = [[45, 85]]
```

后续如果现场光源导致 hue 偏移，只需要调整配置，不改变算法结构。

当红色光斑中心严重过曝接近白色时，中心像素的饱和度可能下降，单纯 hue mask 可能漏掉亮核。因此红色检测应允许：

- 用 HSV mask 找到彩色外圈或候选区域。
- 用 `V` 通道或灰度图在候选附近寻找高亮核心。
- 候选中心估计时结合彩色 mask 和高亮核心，而不是只依赖 hue 命中像素。

### 4.4 mask 后处理

mask 后处理目标是去除孤立噪声并保留小光斑。

建议步骤：

```text
small open operation  -> 去除孤立点
small close operation -> 填补光斑内部断裂
optional blur         -> 仅用于候选中心估计，不替代原始亮度判断
```

形态学核不能过大。激光点面积可能很小，过大的腐蚀会直接抹掉目标。

推荐将形态学核作为参数保存，并优先使用小尺寸椭圆核。矩形核容易改变小光斑形状，椭圆核更适合近圆形目标。

### 4.5 候选基础过滤

每个候选先经过硬条件过滤：

```text
area_px_min <= area_px <= area_px_max
mean_saturation >= saturation_min
max_value or mean_value >= value_min
center inside active_roi
not obviously elongated
not an invalid border artifact
```

失败原因建议映射：

```text
area < area_px_min          -> too_small
area > area_px_max          -> too_large
brightness below threshold  -> low_brightness
center outside active_roi   -> outside_roi
shape invalid               -> low_confidence
```

边界接触候选需要谨慎处理。如果光斑贴近屏幕边界，不能简单因为接触 ROI 边缘就丢弃；但如果候选被图像边界截断且形状异常，应降低置信度或拒绝。

### 4.6 候选中心估计

`image_center` 不建议简单使用外接矩形中心或外接圆中心。

优先级建议：

```text
亮度加权质心
高亮核心质心
轮廓质心
外接圆中心
```

原因：

- 激光点中心通常对应亮度峰值。
- 反光或过曝会扩大外缘，外接形状中心可能被拉偏。
- 亮度加权中心在过曝不严重时更接近真实投射点。

如果过曝区域形成平台，中心可取高亮平台连通域质心，而不是单个最大亮度像素。

`minEnclosingCircle` 可以用于估计 `image_radius_px`，但不应单独决定中心。`fitEllipse` 只适合候选面积足够大且轮廓点足够多的情况；小激光点下优先使用矩和亮度加权质心。

### 4.7 多候选综合评分

多候选时不直接选择面积最大候选。

推荐综合评分：

```text
score =
  color_score
  + brightness_score
  + shape_score
  + area_score
  + temporal_score
  + roi_score
```

各评分含义：

- `color_score`: hue 是否落在目标区间内，饱和度是否足够。
- `brightness_score`: 最大亮度、平均亮度、高亮核心比例。
- `shape_score`: 圆度、长宽比和紧凑性。
- `area_score`: 面积是否接近短标定得到的典型光斑面积。
- `temporal_score`: 锁定状态下与上一帧测量点的距离。
- `roi_score`: 是否位于 tracker 建议的高可信 ROI 内。

推荐选择规则：

```text
best_score >= confidence_min
and best_score clearly greater than second_best_score
```

如果第一和第二候选分数接近，且位置差异较大，应输出：

```text
found = false
source = lost
failure_reason = multiple_candidates
```

这样可以避免在反光点和真实点之间跳变。

### 4.8 丢失与误检判定

视觉层应显式区分以下情况：

```text
not_found           没有候选
too_small           候选面积太小
too_large           候选面积太大或过曝区域异常大
low_brightness      亮度不足
multiple_candidates 多候选无法明确区分
outside_roi         候选不在有效 ROI 内
low_confidence      综合评分不足
```

误检处理原则：

- 宁可输出丢失，也不要把可疑反光点当作真光斑。
- 突然大跳变的候选必须降权，除非当前处于全局重捕获阶段且候选置信度很高。
- 大面积过曝候选不应直接取中心，需要判断是否仍存在稳定高亮核心。
- 多个高亮候选同时存在时，应优先相信与上一帧连续的候选。

`HoughCircles` 不进入主链路。若后续离线测试发现某些相机下光斑边界稳定，可将它作为调试对照或备用诊断输出，但正式控制链路仍应以 HSV/亮度/连通域候选评分为主。

## 5. TapeQuadDetector 算法

### 5.1 使用范围

`TapeQuadDetector` 只在红色系统 A4 黑胶带任务中启用。

它负责输出图像空间四边形：

```text
TapeQuadDetection.image_corners
TapeQuadDetection.image_corners_ordered
```

它不负责：

- 生成 A4 运动路径。
- 输出 `ScreenQuad`。
- 判断屏幕坐标意义上的 `bottom_left`、`bottom_right`、`top_right`、`top_left`。
- 直接修改任务层已锁定路径。

### 5.2 基本流程

推荐流程：

```text
CameraFrame.image
  -> 限制在 screen_roi
  -> 灰度化
  -> 光照归一化或轻度模糊
  -> 自适应阈值或 Otsu 阈值提取黑色区域
  -> 形态学闭运算连接胶带边
  -> 轮廓提取
  -> 多边形拟合
  -> 四边形候选过滤
  -> 图像层角点排序
  -> 输出 TapeQuadDetection
```

`threshold_mode = adaptive_or_otsu` 表示可以根据现场光照选择自适应阈值或 Otsu 阈值。若屏幕亮度不均匀，自适应阈值优先；若背景稳定，Otsu 更简单。

阈值策略建议：

- 屏幕亮度均匀、胶带和背景灰度直方图分离明显时，优先使用 Otsu。
- 屏幕亮度不均、局部阴影明显或投影背景有渐变时，优先使用自适应阈值。
- 自适应阈值前可做轻度中值滤波或高斯滤波，降低噪声对局部阈值的影响。
- 阈值结果应保证黑胶带成为前景候选；若二值极性相反，应统一反转后再进入轮廓流程。

### 5.3 四边形候选过滤

候选过滤建议：

```text
polygon vertex count close to 4
contour_area_px >= min_contour_area_px
quad is convex
quad is not self-intersecting
corner distances are valid
aspect ratio is plausible
quad lies mostly inside screen_roi
```

`polygon_approx_epsilon_ratio` 当前配置为 `0.03`，用于多边形拟合。该参数过小会保留噪声边，过大可能损失真实角点。

胶带边缘可能断裂，因此允许通过闭运算和轮廓拟合恢复整体四边形。但如果拟合出的四边形在连续帧间跳变过大，应降低置信度。

Canny 和 HoughLinesP 可作为增强策略：

- 当阈值区域轮廓无法形成稳定四边形时，可用 Canny 提取边缘。
- 使用 HoughLinesP 查找长直线段，再通过线段聚类和交点估计四角。
- 该增强路径复杂度高，容易受到屏幕内容线条干扰，因此不作为初版主链路。
- 如果启用该增强，输出仍必须是 `TapeQuadDetection` 的图像角点，不改变层间边界。

`minAreaRect` 可用于诊断和辅助评分，例如估计候选整体方向、面积和长宽比。但 A4 胶带在透视下是一般四边形，不能直接用旋转矩形替代真实四角。

### 5.4 角点排序

图像层只保证角点顺序稳定，例如顺时针排序：

```text
image_corners_ordered = clockwise image points
```

不要在 Vision Layer 中把角点命名为屏幕意义上的左下、右下、右上、左上。

正确流程是：

```text
TapeQuadDetection.image_corners_ordered
  -> Coordinate Layer
  -> ScreenQuad
  -> Task Layer determines path order
```

### 5.5 A4 路径锁定策略

建议运行策略：

```text
初始阶段连续多帧稳定检测 A4 四边形
  -> Coordinate Layer 转为 ScreenQuad
  -> Task Layer 锁定 A4 路径
  -> 后续 Vision Layer 主要检测红点
  -> TapeQuadDetector 可低频复检
```

如果复检结果与锁定结果差异较大，不应由视觉层直接改写路径。视觉层只报告新的 `TapeQuadDetection` 和诊断信息，由上层决定是否暂停、重新锁定或安全处理。

## 6. ScreenDetector 算法边界

`ScreenDetector` 被接纳，但定位是短标定辅助和运行自检，不是正式运行每帧控制链路强依赖。

### 6.1 短标定阶段

短标定阶段允许人工参与，但只用于计算参数。

推荐流程：

```text
CameraFrame
  -> ScreenDetector optional auto detection
  -> manual confirmation or correction
  -> Coordinate Layer computes H_img_to_screen
  -> save CalibrationResult.screen_roi
  -> save CalibrationResult.homography_img_to_screen
```

自动检测可以尝试使用边缘、亮度区域、矩形轮廓或已知屏幕背景特征，但最终角点必须经过短标定流程确认。

可选自动辅助算法：

- 大亮度区域或大矩形区域检测，用于快速定位屏幕候选。
- Canny 边缘检测，用于提取屏幕边界。
- HoughLinesP，用于从断裂边缘中估计屏幕边线。
- 大四边形轮廓和多边形拟合，用于生成初始四角候选。

这些算法只服务短标定效率。短标定最终保存的是经确认的 `screen_roi` 和 `homography_img_to_screen`，不是 ScreenDetector 的运行期动态结果。

### 6.2 运行阶段

运行阶段使用短标定锁定的参数：

```text
load CalibrationResult.screen_roi
load CalibrationResult.homography_img_to_screen
Vision Layer uses screen_roi for detector ROI
optional low-frequency ScreenDetector runtime self-check
```

运行自检可以检查：

- `screen_roi` 是否仍在图像范围内。
- 屏幕区域是否严重遮挡。
- 屏幕边界或亮度分布是否明显偏离标定时状态。
- 激光点是否长期出现在 `screen_roi` 外。

运行自检可以低频执行，例如每隔固定帧数或在检测连续异常后触发。自检频率不应影响激光点主检测实时性。

### 6.3 禁止行为

运行阶段 `ScreenDetector` 不允许：

- 请求人工点击屏幕角点。
- 每帧重新计算主单应性矩阵。
- 自动重置屏幕坐标系。
- 直接输出 `ScreenPoint`。
- 代替 `Coordinate Layer` 完成坐标转换。

自检失败时，视觉层只报告：

```text
screen_detection.found = false
screen_detection.source = runtime_self_check
screen_detection.failure_reason = screen_not_found or low_confidence
```

暂停、重捕获或安全停机由 `Application Layer` 或运行状态机决定。

## 7. VisionStateTracker 策略

### 7.1 职责

`VisionStateTracker` 负责维护跨帧视觉状态：

- 连续丢失帧数。
- 上一帧真实检测结果。
- 当前建议 ROI。
- 是否处于重捕获。
- 诊断所需的状态信息。

它不负责：

- 发送电机命令。
- 修改任务路径。
- 伪造本帧测量点。
- 把预测点作为 `found=true` 输出。

### 7.2 状态定义

建议状态：

```text
UNINITIALIZED  尚未获得可信检测
TRACKING       正常跟踪
TEMP_LOST      短暂丢失
REACQUIRING    扩大 ROI 或全局重捕获
LOST           超过最大丢失帧数
```

### 7.3 状态转移

推荐状态转移：

```text
UNINITIALIZED -> TRACKING
  条件: screen_roi 内全局搜索到高置信候选

TRACKING -> TEMP_LOST
  条件: 当前帧未找到候选或候选低置信

TEMP_LOST -> TRACKING
  条件: 扩展 ROI 内重新找到可信候选

TEMP_LOST -> REACQUIRING
  条件: 连续丢失但未超过 max_missing_frames

REACQUIRING -> TRACKING
  条件: 扩大 ROI 或 screen_roi 内重捕获成功

REACQUIRING -> LOST
  条件: 连续丢失超过 max_missing_frames

LOST -> TRACKING
  条件: screen_roi 内全局重捕获成功
```

`max_missing_frames` 来自 `vision.json` 的 `lost_target_policy.max_missing_frames`，当前为 `5`。

### 7.4 ROI 策略

ROI 策略：

```text
UNINITIALIZED: screen_roi
TRACKING: last measured image_center around local_roi
TEMP_LOST: previous local_roi expanded
REACQUIRING: larger ROI or screen_roi
LOST: screen_roi
```

局部 ROI 的大小应考虑：

- 光斑上一帧位置。
- 允许的最大帧间运动。
- 当前控制误差或目标速度。
- 摄像头帧率。
- 光斑典型半径。

ROI 扩大策略应渐进，不要第一帧丢失就立即全图搜索。这样可以减少短暂遮挡或噪声造成的误捕获。

### 7.5 预测与测量边界

Tracker 可以预测下一帧 ROI，但不能把预测点当作测量值。

允许：

```text
use predicted position to build active_roi
use distance to last_detection in candidate scoring
report source=predicted only as diagnostic or non-control observation
```

不允许：

```text
found=true with predicted-only point
image_center = predicted point when no candidate exists
control chain treats predicted point as measured laser position
```

Kalman Filter 可作为后期增强，但不是第一版必要条件。若引入 Kalman，建议状态只用于预测下一帧 ROI 或估计运动趋势，例如：

```text
state = [u, v, du, dv]
measurement = [u, v]
```

Kalman 输出不能直接替代 `LaserDetection.image_center`。在控制链路中，测量点仍必须来自本帧真实候选。

Optical Flow 不作为主方案。激光点缺少稳定纹理，且光斑亮度和形状会随曝光变化，光流容易跟到背景纹理或反光点。

Background Subtraction 不作为主方案。光斑可能静止，屏幕内容和环境亮度可能变化，颜色和亮度特征比运动前景更可靠。

## 8. VisionFrameDiagnostics

每帧建议记录诊断信息，便于现场调参和离线复盘。

建议字段或内容：

```text
processing_time_ms
enabled_detectors
roi_used
candidate_count
rejected_candidates
best_candidate_score
second_best_candidate_score
failure_reason
tracker_state
lost_frame_count
reacquire_triggered
warnings
debug_artifacts optional
```

`debug_artifacts` 可以包括 mask、候选轮廓图、ROI 可视化图和检测结果截图，但这些调试产物不能成为控制链路输入。

离线调试时建议保存以下中间结果：

- HSV mask，包括红色两个 hue 区间的分 mask 和合并 mask。
- `V` 通道高亮核心 mask。
- 形态学处理前后的 mask。
- 光斑候选轮廓、候选编号和综合得分。
- 黑胶带二值图、轮廓图、多边形拟合结果。
- ScreenDetector 自检边缘图或线段图。
- tracker ROI、状态和丢失帧数叠加图。

## 9. 参数扩展建议

当前 `vision.json` 已包含核心参数：

```text
hue_ranges_deg
saturation_min
value_min
area_px_min
area_px_max
confidence_min
lost_target_policy.max_missing_frames
```

后续实现和调参时可以扩展：

```text
morphology_kernel_size
adaptive_threshold_block_size
adaptive_threshold_C
otsu_blur_kernel_size
typical_area_px
circularity_min
max_jump_px
local_roi_radius_px
reacquire_roi_growth_ratio
brightness_peak_min
overexposure_area_ratio_max
multi_candidate_margin
screen_self_check_interval_frames
enable_hough_line_fallback
kalman_roi_prediction_enabled
```

扩展原则：

- 参数实例按 `red` 和 `green` 分开保存。
- 红色系统检测红点与绿色系统检测红点也不能默认共用参数。
- 标定阶段可以辅助确认参数，正式运行阶段只能加载参数。
- 参数命名应表达图像检测含义，不混入屏幕坐标或电机控制含义。

## 10. 红绿系统复用与独立

可复用：

- `LaserDetector` 算法结构。
- `TapeQuadDetector` 算法结构。
- `ScreenDetector` 算法结构。
- `VisionStateTracker` 状态机结构。
- `VisionFrameDiagnostics` 输出格式。

必须独立：

- 红色系统摄像头输入。
- 绿色系统摄像头输入。
- 红色系统 `CalibrationResult`。
- 绿色系统 `CalibrationResult`。
- 红色系统 `vision.json` 参数实例。
- 绿色系统 `vision.json` 参数实例。
- 红色系统 `VisionTrackingState`。
- 绿色系统 `VisionTrackingState`。
- 红色系统运行进程。
- 绿色系统运行进程。

绿色系统检测到的红点只是外部视觉目标，不代表读取红色系统内部状态。

## 11. 与 Coordinate Layer 的算法衔接

相机标定、去畸变和 Homography 与视觉检测强相关，但职责归属不同。

赛前相机标定应得到：

```text
camera_intrinsics
distortion_coefficients
```

短标定应得到：

```text
screen_roi
homography_img_to_screen
```

运行期坐标转换链路应由 Coordinate Layer 完成：

```text
ImagePoint(u_px, v_px)
  -> undistort point or use undistorted image convention
  -> perspective transform by H_img_to_screen
  -> ScreenPoint(x_mm, y_mm)
```

Vision Layer 需要与 Coordinate Layer 约定一点：detector 输出的 `ImagePoint` 是原始图像坐标，还是去畸变图像坐标。该约定必须在后续接口设计中固定，避免 Homography 与检测坐标不在同一图像空间。

运行期不建议每帧通过 ScreenDetector 更新 `homography_img_to_screen`。如果运行自检发现屏幕偏移或 ROI 失效，视觉层只报告异常，由上层决定暂停或重新标定。

## 12. 当前结论

视觉检测层算法设计结论：

```text
LaserDetector:
  HSV 多 hue 区间阈值 + 亮度增强 + 连通域/轮廓 + 多候选评分 + 跨帧 ROI

TapeQuadDetector:
  screen_roi 内 Otsu/adaptive 灰度阈值 + 形态学 + 轮廓提取 + 四边形拟合 + 稳定角点排序

ScreenDetector:
  短标定自动辅助 + 人工确认 + 运行低频自检，不参与每帧主控制链路

VisionStateTracker:
  维护跨帧 ROI、丢失计数和重捕获状态，可选 Kalman ROI 预测，不伪造测量值
```

该方案保持了视觉层与坐标层、任务层、控制层的边界：视觉层只输出图像坐标、形状、质量、置信度和失败原因，所有屏幕坐标转换和控制行为仍由后续层完成。

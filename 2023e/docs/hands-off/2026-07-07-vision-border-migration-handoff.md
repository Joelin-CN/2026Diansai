# 2026-07-07 Vision Layer 边框正式迁移交接文档

## 1. 当前项目位置

当前项目目录：

```text
2023e/
```

本轮工作基于 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案，当前重点是 Vision Layer 第一阶段和视频预览验证。

## 2. 当前已完成内容

### 2.1 Vision Layer 第一阶段基础能力

已完成：

```text
src/vision/types.py
src/vision/config.py
src/vision/laser_detector.py
src/vision/state_tracker.py
src/vision/diagnostics.py
src/vision/pipeline.py
```

能力包括：

- 红/绿激光点基础检测。
- HSV 多 hue 区间检测。
- 红色 hue 跨界 `[0, 10] + [170, 180]`。
- 绿色 hue `[45, 85]`。
- ROI 裁剪后输出整帧图像坐标。
- `VisionStateTracker` 状态机。
- `VisionPipeline` 聚合输出。

### 2.2 A4 黑胶带边框正式迁移

已将脚本验证过的 A4 黑胶带边框逻辑正式迁移进 Vision Layer。

新增正式模块：

```text
src/vision/quad_utils.py
src/vision/tape_quad_detector.py
src/vision/screen_detector.py
```

核心约定：

```text
A4 tape found=true 只有在 outer_quad 和 inner_quad 同时存在时成立。
```

A4 detector 输出：

```text
TapeQuadDetection:
- outer_quad
- inner_quad
- image_corners
- image_corners_ordered
- contour_area_px
- inner_area_px
- tape_ratio
- confidence
- source
- failure_reason
- diagnostics
```

A4 检测流程：

```text
frame
-> gray
-> adaptive black mask
-> morphology close
-> findContours(..., RETR_TREE)
-> outer contour + inner child contour
-> contour_to_refined_quad
-> line-refined quad fitting
-> TapeQuadDetection
```

### 2.3 Screen pencil square 正式 detector

已新增 `src/vision/screen_detector.py`。

当前视频中没有屏幕铅笔方框，因此 `screen_square=False / not_found` 属于预期结果。

Screen detector 只用于：

- 后续短标定辅助。
- preview/debug 验证。
- 运行自检的候选基础。

它不输出 `ScreenPoint`，也不更新 homography。

### 2.4 Pipeline 接入

已修改：

```text
src/vision/pipeline.py
src/vision/diagnostics.py
```

当前模式约定：

```text
RUNTIME_RED_A4_TAPE:
  red_laser + a4_tape

RUNTIME_RED_CENTER:
  red_laser only

RUNTIME_GREEN_TRACKING:
  red_laser + green_laser

DEBUG:
  red/green laser + a4_tape + screen_square, subject to profile/config
```

绿色系统仍不在 `RUNTIME_GREEN_TRACKING` 中运行 A4 tape detector。

### 2.5 Preview 脚本已改为调用正式 detector

已修改：

```text
scripts/test/preview_vision_video.py
scripts/test/preview_vision_detection.py
scripts/test/debug_a4_border_overlay.py
```

这些脚本现在调用正式 Vision Layer：

```text
vision.tape_quad_detector.TapeQuadDetector
vision.screen_detector.ScreenDetector
vision.quad_utils
```

视频预览命令：

```powershell
conda run -n opencv-learning python "scripts/test/preview_vision_video.py"
```

窗口操作：

```text
q 或 ESC: 退出
空格: 暂停/继续
s: 暂停时单帧前进
```

视频预览中：

```text
a4_outer: magenta / 紫色
a4_inner: yellow / 黄色
screen_square: cyan / 浅蓝色
```

默认 `--border-interval=1`，即 A4 边框逐帧检测，避免画面抖动时 overlay 冻结。

## 3. 当前验证结果

已运行：

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest "tests/vision" "tests/scripts" -q
```

结果：

```text
36 passed in 0.18s
```

已运行视频 smoke test：

```powershell
conda run -n opencv-learning python "scripts/test/preview_vision_video.py" --no-window --max-frames 5
```

结果要点：

```text
a4_tape=True
outer_area ~= 15280..15300
inner_area ~= 10900..10940
tape_ratio ~= 0.28..0.29
screen_square=False reason=FailureReason.NOT_FOUND
```

已运行 debug overlay 导出：

```powershell
conda run -n opencv-learning python "scripts/test/debug_a4_border_overlay.py"
```

输出文件：

```text
outputs/a4_border_debug_thin.jpg
outputs/a4_border_debug_thick.jpg
```

注意：该脚本使用 `cv2.imencode(...).tofile(...)` 保存图片，避免 `cv2.imwrite()` 在中文路径下静默失败。

## 4. 当前待验收事项

用户正在验收正式版 A4 边框迁移效果。

验收重点：

- 视频中 `a4_outer` 是否稳定贴合黑胶带外边界。
- 视频中 `a4_inner` 是否稳定贴合黑胶带内边界。
- 逐帧检测是否跟随拍摄抖动，不再缓存冻结。
- 当前视频没有 screen pencil square，因此 `screen_square=False` 是正常结果。

## 5. 尚未开始的任务

下一阶段是问题 2：红绿点追踪不稳定。

当前现象：

- 严格配置下红/绿点经常 `low_brightness`、`too_small`、`not_found`。
- preview 的 relaxed 检测能看到候选，但仍有跳点、multiple_candidates、低亮度丢失等问题。
- 目前红绿点稳定性尚未修复。

建议下一阶段执行计划：

```text
Phase 2: Stabilize red/green laser tracking
```

计划文档位置：

```text
docs/superpowers/plans/2026-07-07-vision-border-migration-and-laser-stability.md
```

建议从该计划的 Phase 2 开始。

## 6. 下一阶段建议思路

不要一上来盲目调阈值。建议先系统诊断红/绿点不稳来源：

```text
1. 增强 LaserDetector diagnostics。
2. 记录每帧候选数量、best_score、second_best_score、failure_reason。
3. 明确 strict 和 relaxed 检测各自失败原因。
4. 再调整 temporal scoring、jump penalty、ROI 约束。
5. 必要时利用 A4 inner/outer quad 限定候选区域。
```

原则：

```text
found=true 仍只能表示真实 measured candidate。
不允许 predicted-only 点作为 found=true 输出。
不允许把红绿点结果转成 ScreenPoint。
不允许引入 MotorCommand 或 Control Layer。
```

## 7. 新线程提示词

可以在新线程中直接使用以下提示词：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案。当前 Vision Layer 第一阶段基础能力已经完成，A4 黑胶带边框 detector 也已经从 scripts/test 正式迁移到 src/vision。

请先读取以下文件：
1. 2023e/README.md
2. 2023e/docs/hands-off/2026-07-05-vision-implementation-handoff.md
3. 2023e/docs/hands-off/2026-07-07-vision-border-migration-handoff.md
4. 2023e/docs/superpowers/plans/2026-07-07-vision-border-migration-and-laser-stability.md
5. 2023e/src/vision/types.py
6. 2023e/src/vision/quad_utils.py
7. 2023e/src/vision/tape_quad_detector.py
8. 2023e/src/vision/screen_detector.py
9. 2023e/src/vision/laser_detector.py
10. 2023e/src/vision/state_tracker.py
11. 2023e/src/vision/pipeline.py
12. 2023e/scripts/test/preview_vision_video.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. A4 tape detector 已正式迁移到 src/vision/tape_quad_detector.py。
3. A4 tape found=true 只有在 outer_quad 和 inner_quad 同时存在时成立。
4. screen detector 已正式迁移到 src/vision/screen_detector.py。当前视频没有 screen pencil square，因此 screen_square=False 是正常结果。
5. preview_vision_video.py 已调用正式 TapeQuadDetector / ScreenDetector。
6. 当前 tests/vision 和 tests/scripts 已通过，最近结果是 36 passed。
7. 下一步要解决问题 2：红绿点追踪不稳定。
8. 解决红绿点问题前请先做 systematic debugging，不要直接猜阈值。
9. 优先增强 LaserDetector diagnostics，确认 strict/relaxed 检测失败原因、候选数量、best_score、second_best_score 和跳点原因。
10. 后续可以考虑 temporal scoring、jump penalty、ROI 约束，以及利用 A4 inner/outer quad 辅助候选过滤。

请先检查当前工作区状态，然后从 docs/superpowers/plans/2026-07-07-vision-border-migration-and-laser-stability.md 的 Phase 2 开始，按测试驱动方式解决红绿点追踪不稳定问题。实现时保持 Vision Layer 边界，不引入 ScreenPoint、MotorCommand、Control Layer 或 Kalman 强依赖。
```

## 8. 注意事项

- OpenCV 可用环境是 `conda` 的 `opencv-learning`，不是当前系统默认 Python。
- 多个 `conda run` 并行时容易触发临时文件冲突，测试命令建议顺序执行。
- 当前仓库中有新增未提交文件，未执行 git commit。
- 用户之前明确说 git/worktree 不用管，直接在当前目录工作即可。

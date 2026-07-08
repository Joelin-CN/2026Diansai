# 2026-07-08 Isolated White-Core Correlation 交接文档

## 1. 当前项目位置

当前工作目录：

```text
2023e/
```

本轮目标是隔离验证一个想法：

```text
先找白色 laser core，
再在 core 邻域计算 red / green 颜色相关性，
用局部颜色证据区分红绿激光。
```

该验证明确先走纯脚本实验，不直接修改正式 Vision Layer 行为。

## 2. 目录规范和边界

本轮按当前项目目录职责放置文件：

- `scripts/test/`: 放隔离实验脚本和调试入口。
- `tests/scripts/`: 放脚本级单元测试。
- `outputs/`: 放脚本生成的图片、CSV、TXT 证据。
- `docs/superpowers/specs/`: 放设计文档。
- `docs/superpowers/plans/`: 放实施计划。
- `docs/hands-off/`: 放交接文档。

本轮没有修改 `src/vision`，也没有接入 `VisionPipeline`。

必须继续保持：

- 不修改 formal runtime 主链。
- 不重新引入 full-frame relaxed fallback。
- 不引入 `ScreenPoint`、`ScreenQuad`、`MotorCommand`、Control Layer 或 Kalman 强依赖。
- white-core-first 仍是隔离实验 / evidence path，不是正式替换方案。

## 3. 已新增文档

设计文档：

```text
docs/superpowers/specs/2026-07-07-isolated-white-core-correlation-design.md
```

实施计划：

```text
docs/superpowers/plans/2026-07-07-isolated-white-core-correlation.md
```

本交接文档：

```text
docs/hands-off/2026-07-08-isolated-white-core-correlation-handoff.md
```

## 4. 已新增代码

隔离脚本：

```text
scripts/test/isolated_white_core_correlation.py
```

核心职责：

- 从视频中抽样帧。
- 在帧内找 bright low-saturation white core。
- 对每个 white core 计算邻域 `red_score` / `green_score`。
- 按分数和 gap 分类：`red`、`green`、`uncertain`、`white/no-color`。
- 画候选标注图。
- 输出 `summary.csv` 和 `summary.txt`。

脚本级测试：

```text
tests/scripts/test_isolated_white_core_correlation.py
```

测试覆盖：

- `classify_scores(...)` 阈值规则。
- 合成 red ring + white core。
- 合成 green ring + white core。
- pure white core 的 low color-score guard。
- `sample_frame_indices(...)` 可复现随机抽帧。
- 短视频帧数不足时的抽样行为。
- Windows 中文路径下 PNG 写入。

## 5. 算法当前版本

### 5.1 white core mask

输入帧先从 BGR 转 HSV。

white core mask 使用：

```text
lower = [0, 0, 180]
upper = [179, 80, 255]
```

然后做 3x3 ellipse kernel 的 open + close。

候选 contour 过滤：

```text
area < 2.0 reject
area > 300.0 reject
```

### 5.2 local color score

每个 white core 的中心用 HSV value channel 做 brightness-weighted center。

邻域半径：

```text
radius = 9 px
```

red hue support：

```text
hue <= 10 or hue >= 170
```

green hue support：

```text
45 <= hue <= 85
```

分数计算使用：

```text
hue_support * saturation * value * spatial_weight
```

总和除以 `6.0` 并 cap 到 `1.0`。

### 5.3 label 规则

当前规则：

```text
red:
  red_score >= 0.20 and red_score - green_score >= 0.15

green:
  green_score >= 0.20 and green_score - red_score >= 0.15

white/no-color:
  red_score < 0.05 and green_score < 0.05

uncertain:
  otherwise
```

这些阈值只是隔离实验参数，不是 formal runtime 参数。

## 6. 已完成的 TDD 和 review

本轮按 subagent-driven development 执行了两个 task。

### 6.1 Task 1: Core White-Core Correlation Helpers

新增：

- `WhiteCoreCandidate`
- `analyze_frame(frame)`
- `classify_scores(red_score, green_score)`
- 合成图测试

TDD RED：

```text
ModuleNotFoundError: No module named 'isolated_white_core_correlation'
```

TDD GREEN：

```text
7 passed in 0.11s
```

Task 1 reviewer 结论：

```text
Spec Compliance: PASS
Task quality: Approved
Critical: None
Important: None
Minor: None
```

### 6.2 Task 2: CLI Video Sampling And Output Evidence

新增：

- `sample_frame_indices(...)`
- CLI 参数：`--video`、`--start-frame`、`--sample-count`、`--seed`、`--output-dir`
- `process_video(...)`
- `draw_candidates(...)`
- `summary.csv` / `summary.txt` 输出
- `_write_image(...)`，用 `cv2.imencode(...).tofile(...)` 解决 Windows 中文路径下 `cv2.imwrite` 失败问题

TDD RED：

```text
ImportError: cannot import name 'sample_frame_indices'
```

补充 RED：

```text
ImportError: cannot import name '_write_image'
```

原因是实际视频运行时发现 `cv2.imwrite(...)` 在当前中文路径下返回 `False`，未生成 PNG。

TDD GREEN：

```text
10 passed in 0.16s
```

Task 2 reviewer 结论：

```text
Spec Compliance: PASS
Task quality: Approved
Critical: None
Important: None
Minor: diff artifact unreadable, reviewer 改为直接读 changed files
```

## 7. 验证命令和结果

最终单测命令：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

结果：

```text
10 passed in 0.12s
```

最终脚本验证命令：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30 --output-dir "outputs\white_core_correlation_verify"
```

结果：

```text
Wrote isolated white-core correlation output to: outputs\white_core_correlation_verify
Sampled frames: 30; failed frames: 0
```

文件检查结果：

```text
frame_*.png: 30
summary.csv: exists
summary.txt: exists
```

## 8. 当前输出目录

建议优先查看：

```text
outputs/white_core_correlation_verify/
```

该目录包含：

- 30 张标注图：`frame_*.png`
- `summary.csv`
- `summary.txt`

本次抽样帧：

```text
[80, 91, 101, 102, 113, 116, 130, 134, 138, 168, 196, 261, 270, 288, 373, 416, 446, 470, 476, 486, 561, 590, 606, 621, 638, 684, 687, 708, 882, 888]
```

本次 label 统计：

```text
red: 53
green: 1
uncertain: 9
white/no-color: 5
```

还存在 subagent 中间运行目录：

```text
outputs/white_core_correlation_20260707_223639/
outputs/white_core_correlation_20260707_223806/
```

其中 `223639` 是修复中文路径 PNG 写入前的中间输出，可能 summary 存在但 PNG 不完整；`223806` 是修复后成功输出目录。

## 9. 当前重要观察

本次隔离抽样结果中，`red` 标签明显多于 `green`：

```text
red: 53
green: 1
uncertain: 9
white/no-color: 5
```

这不能直接说明 green 激光不存在或算法已经可用，只说明在当前抽样、当前阈值和当前白 core 候选排序下，局部 red score 更常占优。

下一步建议人工打开 `outputs/white_core_correlation_verify/` 下的 PNG，逐帧观察：

- 标注圈是否落在真实 laser core 上。
- red/green 分数是否符合肉眼直觉。
- green laser 是否被漏检、被 red score 淹没，或被标成 uncertain。
- 是否存在白反光点被误判为 red。
- 是否应该限制 ROI 到 A4 / screen task region，而不是整帧找 white core。

## 10. 当前 worktree 注意事项

当前 worktree 有多类未提交 / 未跟踪文件。

本轮相关新增或修改包括：

```text
scripts/test/isolated_white_core_correlation.py
tests/scripts/test_isolated_white_core_correlation.py
docs/superpowers/specs/2026-07-07-isolated-white-core-correlation-design.md
docs/superpowers/plans/2026-07-07-isolated-white-core-correlation.md
docs/hands-off/2026-07-08-isolated-white-core-correlation-handoff.md
outputs/white_core_correlation_verify/
outputs/white_core_correlation_20260707_223806/
```

已有的 white-core reacquire 相关改动仍然存在，例如：

```text
scripts/test/preview_vision_video.py
tests/vision/test_laser_detector.py
```

还存在 `__pycache__` 变更和 `.superpowers/sdd/` handoff scratch 文件。

不要清理、回退或重置无关文件，除非用户明确要求。

## 11. 常用命令

跑隔离脚本单测：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

重新生成一组随机抽样输出：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30
```

生成到固定目录，方便覆盖对比：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30 --output-dir "outputs\white_core_correlation_manual"
```

换随机种子：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30 --seed 123 --output-dir "outputs\white_core_correlation_seed123"
```

## 12. 下一步建议

建议下一线程先做人工验收，不急着改 Vision Layer：

1. 打开 `outputs/white_core_correlation_verify/` 的 30 张 PNG。
2. 判断标注候选是否符合肉眼看到的红/绿激光。
3. 如果 green 明显漏判，先调整隔离脚本里的 scoring / threshold / ROI，而不是直接改 `src/vision`。
4. 如果白 core 候选经常落在非任务区域反光点，优先给隔离脚本增加 bounded ROI 支持。
5. 只有当隔离脚本在人工验收中稳定，才考虑抽成 `src/vision` 模块或接入 DEBUG evidence path。

## 13. 新线程提示词

可以在新线程中直接使用下面这段：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案。当前正在隔离验证 white-core local red/green correlation 思路：先找白色 laser core，再在 core 邻域计算 red_score / green_score，用于区分红绿激光。

请先读取以下文件：
1. README.md
2. docs/hands-off/2026-07-07-white-core-reacquire-handoff.md
3. docs/hands-off/2026-07-08-isolated-white-core-correlation-handoff.md
4. docs/superpowers/specs/2026-07-07-isolated-white-core-correlation-design.md
5. docs/superpowers/plans/2026-07-07-isolated-white-core-correlation.md
6. scripts/test/isolated_white_core_correlation.py
7. tests/scripts/test_isolated_white_core_correlation.py
8. outputs/white_core_correlation_verify/summary.txt
9. src/vision/laser_detector.py
10. scripts/test/preview_vision_video.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. formal runtime mode 已改成 4 个题目驱动模式：RUNTIME_SCREEN_RED、RUNTIME_A4_RED、RUNTIME_TRACK_RED_GREEN、RUNTIME_A4_TRACK_RED_GREEN。
3. DEBUG / preview 不属于 <= 30 ms/frame 的正式 runtime 目标。
4. formal runtime 中不允许重新引入 strict 失败后全图 relaxed fallback。
5. white-core-first 当前只做隔离实验和 DEBUG 采证，不直接替换 formal runtime 主链。
6. 当前已新增纯隔离脚本 scripts/test/isolated_white_core_correlation.py，不修改 src/vision。
7. 脚本会从第 41 帧后随机抽 30 帧，找 white core，计算 red_score / green_score，输出标注 PNG、summary.csv、summary.txt。
8. 最终验证目录是 outputs/white_core_correlation_verify/，其中有 30 张 PNG、summary.csv、summary.txt，failed frames 为 0。
9. 本次统计为 red: 53, green: 1, uncertain: 9, white/no-color: 5，需要人工验收 PNG 判断是否符合肉眼直觉。
10. 当前 worktree 很脏，有无关改动、pycache、outputs 和 .superpowers/sdd，不要清理、回退或重置无关文件。

下一步请先人工/脚本辅助检查 outputs/white_core_correlation_verify/ 的标注图片效果，判断 white-core local color correlation 是否能稳定区分红绿激光。若要修改，请继续保持隔离脚本优先，不要直接改 formal Vision Layer。优先使用：
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" ...
避免使用 conda run，以免触发临时文件冲突。
```

# 2026-07-07 White-Core Reacquire 调试交接文档

## 1. 当前项目位置

当前工作目录：

```text
2023e/
```

当前线程从“黑胶带区域上 red/green laser 稳定性调试”继续，已经从直接调 HSV/面积阈值，转向验证一个新思路：

```text
先稳定识别 white laser core，
再在 core 邻域内计算 red / green 颜色相关性，
用于判断这个白色中心属于哪种颜色的 laser。
```

## 2. 必须保持的边界

Vision Layer 仍然只输出：

- `ImagePoint`
- `ImageQuad`
- detection dataclass
- diagnostics

不得引入：

- `ScreenPoint`
- `ScreenQuad`
- `MotorCommand`
- Control Layer
- Kalman 强依赖

formal runtime 仍然不得重新引入：

```text
strict fail -> unconstrained full-frame relaxed fallback
```

white-core-first 当前只作为 DEBUG / evidence path 的诊断实验，不要直接替换 formal runtime 主链。

## 3. 本线程已完成的关键判断

### 3.1 原始黑胶带失效证据

通过直接跑视频和 detector diagnostics，观察到：

- `green` 进入黑胶带后，strict 主要因为 `low_brightness` / `too_small` 失败，但 relaxed 通常还能找到单个稳定候选。
- `red` 是更大的问题：
  - strict 经常变成 `too_small` / `low_brightness` / `low_confidence`
  - relaxed 会膨胀到 `40..60` 个候选
  - 很多 high-rank relaxed red candidates 是“白 core 很强，但 red support 极弱”

典型例子 `frame 42`：

```text
previous red center: (692.6, 346.2)
strict red: too_small

temporally-near relaxed candidate:
  center=(693.0, 358.5)
  distance=12.3 px
  color_area_px=96.5
  bright_core_area_px=6.0
  score=0.565

top relaxed candidate:
  center=(634.5, 364.5)
  distance=60.9 px
  color_area_px=1.0
  bright_core_area_px=48.0
  score=0.612
```

因此当前根因分为两块：

- runtime 风险：strict red 在黑胶带上敏感度不足
- debug 歧义：white-core-heavy 但目标颜色支撑很弱的候选会干扰排序

### 3.2 新方案方向

用户提出换思路：

```text
能否在所有帧中先稳定识别两个白色 laser 中心，
然后在中心点邻域中计算像素和目标颜色的相关性，
判断这是 red 还是 green laser。
```

已确认采用的第一阶段边界：

- 优先验证 `REACQUIRING` 扩大 ROI 的情况
- 搜索范围允许扩大到 `screen_roi / A4 ROI` 这种 bounded task region
- 不允许回到无界全图 relaxed fallback
- 第一阶段只做 DEBUG 采证，不立即改 formal runtime 主行为

## 4. 已新增文档

### 4.1 设计文档

已新增：

```text
2023e/docs/superpowers/specs/2026-07-07-white-core-reacquire-design.md
```

核心内容：

```text
bounded reacquire ROI
-> white core candidate extraction
-> candidate pruning
-> local red/green color correlation around each core
-> candidate diagnostics and ranking evidence
```

### 4.2 实施计划

已新增：

```text
2023e/docs/superpowers/plans/2026-07-07-white-core-reacquire.md
```

计划任务：

1. Task 1: White-core local red-correlation diagnostics
2. Task 2: Green correlation and pure-white guard tests
3. Task 3: Preview debug summary for white-core evidence
4. Task 4: Bounded reacquire evidence collection command
5. Task 5: Final verification

注意：计划中虽然按 Superpowers 模板写了 task/checkpoint，但本仓库规则是不经用户明确要求不 commit。

## 5. 当前代码实现状态

### 5.1 Task 1 已完成并通过 review

Task 1 已由 subagent 实现，并由另一个 subagent review 通过。

已修改：

```text
2023e/src/vision/laser_detector.py
2023e/tests/vision/test_laser_detector.py
```

新增内容包括：

- `_WhiteCoreCandidate`
- diagnostics keys:
  - `white_core_candidate_count`
  - `white_core_candidates`
- `_analyze_white_core_candidates(...)`
- `_local_color_score(...)`
- `_white_core_candidate_diagnostics(...)`
- synthetic red support test:
  - `test_white_core_diagnostics_score_red_support_above_green`

Task 1 TDD 证据：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py::test_white_core_diagnostics_score_red_support_above_green" -q
```

实现前结果：

```text
FAIL: KeyError: 'white_core_candidates'
1 failed in 0.19s
```

实现后结果：

```text
1 passed in 0.15s
```

Task 1 review 结论：

```text
Spec verdict: PASS
Quality verdict: APPROVED
Findings: None
```

reviewer 非阻塞备注：当前 white-core diagnostics 对所有非空 crop 都会跑，包括没有 ROI / previous detection 的调用。它是 diagnostic-only，不改变 measured detection，也没有重新引入 runtime fallback，因此 Task 1 不阻塞。

### 5.2 Task 2 尚未开始

用户在准备执行 Task 2 时要求写交接文档，因此已暂停实现。

下一步应从 Task 2 开始：

```text
Green correlation and pure-white guard tests
```

## 6. 当前重要代码位置

### 6.1 `laser_detector.py`

当前新增白 core diagnostics 大致位于：

- `_WhiteCoreCandidate`: `src/vision/laser_detector.py` 顶部 `_Candidate` 后
- diagnostics 初始化：`LaserDetector.detect(...)`
- white-core 分析调用：`detect(...)` 中 `_build_mask(...)` 后
- `_analyze_white_core_candidates(...)`
- `_local_color_score(...)`
- `_white_core_candidate_diagnostics(...)`

当前 white-core candidate diagnostics 中每个 candidate 包括：

```text
center
core_area_px
brightness
local_red_score
local_green_score
rank_score
```

### 6.2 `test_laser_detector.py`

当前新增测试在文件末尾：

```python
def test_white_core_diagnostics_score_red_support_above_green():
    ...
```

Task 2 应继续追加：

- `test_white_core_diagnostics_score_green_support_above_red`
- `test_white_core_diagnostics_keep_pure_white_color_scores_low`

计划文档里已有具体测试代码。

## 7. 工作区状态注意事项

当前 git worktree 很脏，且包含大量本任务之前已存在的改动、未跟踪文件、pycache 和一些删除项。

已观察到的情况包括：

- 多个 `2023e/src/vision/*.py` 已修改
- 多个 `2023e/tests/vision/*.py` 已修改
- 多个 handoff / plan / spec 文档未跟踪
- 有 pycache 变更
- 有仓库其它位置的删除项，例如：
  - `docs/superpowers/plans/2026-07-05-a4-target-png.md`
  - `docs/superpowers/specs/2026-07-05-a4-target-png-design.md`
  - `2023e/docs/plan/2023e/handoff.md`
  - `2023e/docs/plan/2023e/plan_v0.md`
  - `2023e/docs/E_运动目标控制与自动追踪系统.pdf`

不要清理、回退或重置这些无关改动。继续工作时只改当前任务需要的文件。

## 8. 推荐下一步

下一线程建议按以下顺序继续：

1. 读取本交接文档和 white-core design / plan。
2. 检查工作区状态，但不要回退无关改动。
3. 从 `Task 2` 开始执行：补 green correlation 和 pure-white guard tests。
4. 用 TDD 执行 Task 2：先补测试，再运行确认结果，再做最小修正。
5. Task 2 后继续 Task 3：在 `preview_vision_video.py` 中输出 compact white-core summary。
6. Task 4 跑 debug preview 采证，重点观察 `frame 41..120`。
7. Task 5 跑最终验证。

建议使用直接 Python 可执行文件，避免 `conda run` 临时文件冲突：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py" -q
```

debug preview 命令：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/preview_vision_video.py" --path-mode debug --no-window --max-frames 120
```

## 9. 新线程提示词

请在新线程中直接使用下面这段：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案。当前正在验证 white-core-first reacquire 方案：先在 bounded task ROI / reacquire ROI 中找白色 laser core，再通过 core 邻域 red/green 颜色相关性判断它属于 red 还是 green laser。

请先读取以下文件：
1. 2023e/README.md
2. 2023e/docs/hands-off/2026-07-07-laser-black-tape-stability-handoff.md
3. 2023e/docs/hands-off/2026-07-07-white-core-reacquire-handoff.md
4. 2023e/docs/superpowers/specs/2026-07-07-white-core-reacquire-design.md
5. 2023e/docs/superpowers/plans/2026-07-07-white-core-reacquire.md
6. 2023e/src/vision/laser_detector.py
7. 2023e/scripts/test/preview_vision_video.py
8. 2023e/tests/vision/test_laser_detector.py
9. 2023e/src/vision/pipeline.py
10. 2023e/src/vision/state_tracker.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. formal runtime mode 已改成 4 个题目驱动模式：RUNTIME_SCREEN_RED、RUNTIME_A4_RED、RUNTIME_TRACK_RED_GREEN、RUNTIME_A4_TRACK_RED_GREEN。
3. DEBUG / preview 不属于 <= 30 ms/frame 的正式 runtime 目标。
4. formal runtime 中不允许重新引入 strict 失败后全图 relaxed fallback。
5. white-core-first 当前只做 DEBUG 采证，不直接替换 formal runtime 主链。
6. 当前 white-core reacquire design / plan 已写好。
7. Task 1 已完成并通过 review：新增 white_core_candidate_count、white_core_candidates、_WhiteCoreCandidate、_analyze_white_core_candidates、_local_color_score、_white_core_candidate_diagnostics，以及 red local-correlation 测试。
8. Task 1 focused test 已按 TDD 跑过：实现前 KeyError 失败，实现后 1 passed。
9. 当前应从 plan 的 Task 2 开始：补 green correlation 和 pure-white guard tests，再继续 Task 3/4/5。
10. 当前 worktree 很脏，包含很多无关改动和 pycache，不要清理、回退或重置无关文件。

请先检查当前工作区状态，然后继续执行 2023e/docs/superpowers/plans/2026-07-07-white-core-reacquire.md，从 Task 2 开始。按 TDD 推进：先补失败测试或确认测试覆盖，再做最小实现修改。优先使用：
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" ...
避免使用 conda run，以免触发临时文件冲突。
```

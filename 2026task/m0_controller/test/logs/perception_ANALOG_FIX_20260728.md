# perception 模拟循迹感知层改进 — 修复日志

**日期**：2026-07-28
**模块**：`modules/Sens-Decision/src/perception.c` / `inc/perception.h`
**状态**：✅ 代码完成；`gcc -fsyntax-only -std=c99 -Wall -Wextra -Wpedantic` exit 0；待上板调参验证

---

## 背景

上一轮（`ir_tracker_WORK_SUMMARY_20260728.md`）完成了从 MCP23017 数字循迹到 UART 模拟循迹的硬件对接。
本轮发现 `perception.c` 的核心算法**仍按数字逻辑运行**——循环内 `weighted_sum += weight`（固定权重累加，不乘传感器值），ADC 模拟量未被真正利用。
同时识别出两个上板风险：激活阈值过高导致环境光噪声参与重心计算；丢线时 `heading_error` 冻结旧值，恢复后易过冲。

---

## 根因分析

| 问题 | 代码位置 | 说明 |
|---|---|---|
| 模拟量未参与重心 | `perception.c` 原第 51–54 行 | `weighted_sum += weight`，仅在 `v > 0.5` 时累加固定权重，等效数字逻辑 |
| 分母固定不自适应 | 原第 70–73 行 | `/ max_abs_weight`，多路同时激活时误差幅值虚增 |
| 丢线时误差冻结 | 原第 64–65 行 | `heading_error` 直接透传旧值，不衰减 |
| 无偏向记忆 | `perception_t` 结构体 | 丢线后上层无从得知最后偏向方向 |

---

## 改动内容

### ① 激活阈值：0.5 → 0.2（抑制环境光噪声）

```c
// 旧：硬阈值 0.5，等效数字二值化
if (ir_data->values[i] > 0.5f) {
    weighted_sum += weight;
    ++active_count;
}

// 新：阈值降为 0.2，低于阈值通道不参与重心，抑制噪声
#define PERCEPTION_ACTIVATION_THRESHOLD 0.2f

if (v > PERCEPTION_ACTIVATION_THRESHOLD) {
    weighted_sum     += weight * v;   // ② 同时改为乘以 v
    total_activation += v;
    ++active_count;
}
```

### ② 重心公式：固定权重累加 → 标准模拟量加权重心 Σ(w·v)/Σv

```c
// 旧：Σ(weight_i) / max_abs_weight —— 只在超阈值时累加固定权重
result->lateral_error = weighted_sum / max_abs_weight;

// 新：Σ(w_i·v_i) / Σ(v_i) —— 标准加权重心，分母随激活强度自适应
result->lateral_error = (total_activation > 0.0f)
                            ? weighted_sum / total_activation
                            : 0.0f;
```

**公式对比**：

| 方案 | 分子 | 分母 | 多路激活时的行为 |
|---|---|---|---|
| 旧（固定权重） | Σ weight_i（超阈值） | max_abs_weight | 多路激活误差幅值叠加放大 |
| 新（标准重心） | Σ(w_i · v_i) | Σ v_i | 多路激活自动归一化，线性响应 |

### ③ 丢线恢复：heading_error 指数衰减 + last_lateral_sign 偏向记忆

**`perception.h` — 新增字段**：

```c
typedef struct {
    float prev_lateral_error;
    uint64_t prev_timestamp_us;
    float heading_error;
    float last_lateral_sign;   /* 新增：记录丢线前的偏向符号（+1/-1/0），供上层恢复策略使用 */
    uint16_t lost_count;
    bool initialized;
} perception_t;
```

**`perception.c` — 丢线分支**：

```c
// 旧：冻结旧值
result->heading_error = perception->heading_error;

// 新：指数衰减，避免恢复后过冲
decay_alpha = g_sens_decision_config.perception.heading_filter_alpha;
if (decay_alpha <= 0.0f || decay_alpha >= 1.0f) {
    decay_alpha = PERCEPTION_HEADING_DECAY_FALLBACK;  // 0.85f
}
perception->heading_error *= decay_alpha;
result->heading_error = perception->heading_error;
```

**有线分支新增偏向符号记录**：

```c
if (result->lateral_error > 0.0f)      perception->last_lateral_sign =  1.0f;
else if (result->lateral_error < 0.0f) perception->last_lateral_sign = -1.0f;
// 若误差为 0（正中），保持上次符号不变
```

---

## 文件变更

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `modules/Sens-Decision/inc/perception.h` | 修改 | `perception_t` 新增 `last_lateral_sign` 字段 |
| `modules/Sens-Decision/src/perception.c` | 修改 | 三项改进（阈值/重心公式/丢线衰减）；移除无用的 `max_abs_weight` 变量；新增 `decay_alpha` 局部变量 |
| `logs/perception_ANALOG_FIX_20260728.md` | 新建 | 本文件 |

---

## 验收

### 静态语法检查

```
gcc -fsyntax-only -std=c99 -Wall -Wextra -Wpedantic \
    -I"modules/Sens-Decision/inc" \
    "modules/Sens-Decision/src/perception.c"

Exit code: 0  —— 零警告，零错误
```

### Git 提交

```
commit bb4dd36
refactor(perception): 模拟循迹感知层三项改进
2 files changed, 68 insertions(+), 46 deletions(-)
```

合并方式：隔离 worktree（`worktree-analog-perception-fix`）→ 语法验收通过 → `--no-ff` 合并回 main → worktree 删除。

---

## 遗留工作（上板后）

### 调参待办

1. **`PERCEPTION_ACTIVATION_THRESHOLD 0.2f`**：当前为经验值，上板后实测白底各路 `values[]` 基线，将阈值设在基线上方 10–20%。

2. **weights 数组重整定**：改为标准重心公式后，增益语义发生变化——原来 weights 等效传感器位置坐标，现在分母为信号总量，PD 增益 `k_lat` / `k_hdg` 需重新标定。建议先从原始增益的 50% 开始调。

3. **`last_lateral_sign` 接入行为层**：该字段目前仅写入 `perception_t`，`behavior_planner` 和上层控制尚未读取。丢线搜索转速策略可基于此字段选择偏转方向，避免往反向盲搜。

4. **heading_error 衰减速率**：当前复用 `heading_filter_alpha`（低通滤波系数），丢线高速场景可能衰减过慢。可考虑在 `sd_perception_config_t` 中独立增加 `heading_lost_decay_alpha` 字段，与滤波系数解耦。

### 主机测试覆盖缺口

`test_control_app` 仍处于跳过状态（`SOFTWARE_TEST_MODE` 桩漂移，见 `test_control_app_FIX_LOG_20260728.md`），`perception_update` 的新逻辑无主机测试覆盖，需上板验证。

---

## 关键学习点

1. **模拟接口对接后算法层必须同步验证**：硬件接口改为模拟量不代表算法层自动利用了模拟量；`perception.c` 的循环内 `+= weight`（不乘 `v`）是典型的"接口改了、算法忘改"的漂移。

2. **标准加权重心 vs 固定权重累加**：`Σ(w·v)/Σv` 的分母自适应使误差在多路激活时保持线性，`Σ(w)/max_w` 会在激活通道多时幅值溢出，不适合 PD 控制。

3. **冻结旧值的隐患**：丢线帧较短时冻结 `heading_error` 看似无害，但高速场景下连续多帧丢线后残留的大 `heading_error` 在视线恢复的第一帧直接作用于运动控制，产生的过冲会导致二次丢线。指数衰减是最低成本的修复。

---

**完成时间**：2026-07-28
**状态**：✅ 代码/语法验收完成；⏳ 待上板调参

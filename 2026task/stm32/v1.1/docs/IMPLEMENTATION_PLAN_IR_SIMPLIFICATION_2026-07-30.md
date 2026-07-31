# IR循迹算法简化 - 实施方案

**文档版本**: 1.0
**日期**: 2026-07-30
**设计师**: Claude Opus 4.8
**审核状态**: ⏳ 待用户审核

---

## 1. Executive Summary

当前IR循迹算法包含**线型事件检测**能力（交叉路口检测、弯道检测），但对于操场型轨迹（只有直线+弧线），这些检测要么物理上不可能触发（交叉路口），要么原理上无法可靠区分（弯道vs偏差）。本次简化将**移除所有事件检测逻辑**，将算法简化为纯粹的偏差计算+连续速度控制。

**核心改动**: 删除 `road_event_t` 枚举 → 简化 behavior FSM → 移除6个冗余参数 → 清理所有调用者。

---

## 2. 依赖关系分析

### 2.1 road_event_t 使用分布

| 文件 | 行号 | 用途 | 修改方式 |
|------|------|------|---------|
| `perception.h:18-23` | 定义 | 枚举定义 | **删除** |
| `perception.h:30` | 定义 | result中event字段 | **删除** |
| `perception.c:105` | 写入 | LINE_LOST时设event | **删除** |
| `perception.c:145-146` | 写入 | INTERSECTION检测 | **删除** |
| `perception.c:147-149` | 写入 | CURVE_ENTRY检测 | **删除** |
| `perception.c:151` | 写入 | NONE默认值 | **删除** |
| `behavior_planner.c:111` | 读取 | 触发APPROACH_CURVE | **删除** |
| `perception_debug.c:89-105` | 读取 | 详细调试打印 | **改为打印简化信息** |
| `perception_debug.c:143-149` | 读取 | 紧凑调试打印 | **改为打印简化信息** |

### 2.2 6个冗余参数使用分布

| 参数 | 定义位置 | 初始化 | 验证 | 使用点 |
|------|---------|--------|------|--------|
| `curve_error_threshold` | config.h:68 | config.c:332 | config.c:622 | perception.c:147 |
| `curve_derivative_threshold` | config.h:69 | config.c:333 | config.c:623 | perception.c:148 |
| `intersection_active_channels` | config.h:70 | config.c:334 | config.c:624-625 | perception.c:145, perception_debug.c:216, ir_calibration.c:242 |
| `curve_exit_stable_frames` | config.h:89 | config.c:532 | config.c:656 | behavior_planner.c:122 |
| `approach_curve_speed_mps` | config.h:92 | config.c:535 | config.c:660 | behavior_planner.c:149, speed_mode.c:21/29/37/45 |
| `curve_speed_mps` | config.h:93 | config.c:536 | config.c:661 | behavior_planner.c:152, speed_mode.c:22/30/38/46 |

### 2.3 受影响的调用者文件

| 文件 | 影响程度 | 修改内容 |
|------|---------|---------|
| `perception_debug.c` | 中等 | 移除event打印、intersection自检 |
| `speed_mode.c` | 中等 | 移除approach/curve速度设置 |
| `ir_calibration.c` | 轻微 | 移除intersection标记 |

---

## 3. 设计决策选项

### 选项对比

| 维度 | Option A (激进) | Option B (温和) | Option C (保守) |
|------|----------------|----------------|----------------|
| 删除road_event_t | ✅ 完全删除 | ✅ 完全删除 | ❌ 保留 |
| 删除INTERSECTION | ✅ | ✅ | ✅ |
| 删除CURVE_ENTRY | ✅ | ✅ | ❌ (加hysteresis) |
| 简化FSM | ✅ 4状态 | ⚠️ 5状态 | ❌ 7状态 |
| 删除6参数 | ✅ 全部 | ⚠️ 删3个(INTERSECTION相关) | ⚠️ 删1个 |
| 速度控制策略 | 连续偏差调速 | 两级调速 | 三级调速(保留) |
| 代码减少 | ~150行 | ~80行 | ~15行 |
| 风险 | 低 | 低 | 极低 |
| 回滚难度 | 简单(git revert) | 简单 | 简单 |

### ⭐ Option A (推荐): 激进简化

**哲学**: IR传感器提供的是**连续偏差信号**，不是事件。让控制系统直接使用偏差信号，不必通过事件分类转一道。

**核心变化**:
1. 删除 `road_event_t` 枚举和 `perception_result_t.event` 字段
2. 感知层输出：`lateral_error` + `heading_error` + `line_valid`
3. 行为FSM简化为4状态：`IDLE → RUNNING → STOPPED → FAULT`
4. 速度控制变为：`speed = line_speed * clamp(1.0f - k * |lateral_error|, 0.4f, 1.0f)`
5. 删除全部6个冗余参数

**新增参数**: `speed_error_gain`（基于偏差的速度调节系数，默认0.3）

**优势**:
- 代码最简洁（-150行）
- 消除所有误判风险（没有事件检测就不会误判）
- 控制逻辑直观（偏差大→减速+转向）
- 无需维护无意义的阈值

**劣势**:
- 如果有特别急的弯道，纯PD可能不够（需加curvature-based限速作为备选）

### Option B: 温和简化

**核心变化**:
1. 删除 `road_event_t` 枚举
2. 删除 INTERSECTION检测
3. behavior FSM简化但保留 CURVE 状态作为"高偏差"状态
4. 删除3个参数（intersection_active_channels, curve_error_threshold, curve_derivative_threshold）
5. 保留3个速度参数（approach_curve_speed_mps, curve_speed_mps, curve_exit_stable_frames）
6. 基于 `|lateral_error| > 1.0` 和 `|heading_error| > 3.0` 进入CURVE状态（无hysteresis→改有条件直接判定）

**优势**: 过渡方案，改动较小
**劣势**: 仍保留了复杂度，且阈值仍是经验值

### Option C: 保守方案

只移除INTERSECTION检测，其他一切不变。

**不推荐原因**: 没有解决伪曲线检测的核心问题。

---

## 4. Phase 1: P0 快速清理 (仅INTERSECTION检测)

**目标**: 移除物理上不可能触发的交叉路口检测

**文件清单**:

### 文件1: `modules/Sens-Decision/src/perception.c`

**删除 lines 145-146**:
```c
// 删除前:
        } else if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
            result->event = ROAD_EVENT_INTERSECTION;
        } else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
```

// 删除后:
        } else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
```

### 文件2: `modules/Sens-Decision/inc/config.h`

**删除 line 70**:
```c
// 删除:
    uint8_t intersection_active_channels;
```

### 文件3: `modules/Sens-Decision/src/config.c`

**删除 line 334** (初始化):
```c
// 删除:
    g_sens_decision_config.perception.intersection_active_channels = 4U;
```

**删除 lines 624-625** (验证):
```c
// 删除前:
        config->perception.intersection_active_channels == 0U ||
        config->perception.intersection_active_channels > SD_IR_CHANNEL_COUNT ||

// 变为:
// (删除这两行，下行直接接白名单验证)
```

**修改 sd_config_validate 中 perception 验证块**: 将 lines 622-625 合并:
```c
// 修改后 (合并逻辑):
    if (!position_is_finite(&config->perception.position) ||
        !isfinite(config->perception.heading_filter_alpha) ||
        config->perception.heading_filter_alpha < 0.0f ||
        config->perception.heading_filter_alpha > 1.0f ||
        !positive_finite(config->perception.curve_error_threshold) ||
        !positive_finite(config->perception.curve_derivative_threshold) ||
        !positive_finite(config->perception.black_strength_threshold)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
```

### 文件4: `Core/Src/app/ir_calibration.c`

**修改 line 242**: 移除 INTERSECTION 标记，改为直接显示active_count:
```c
// 修改前:
            } else if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
                printf(" CROSS");
            }

// 修改后:
// (直接删除此else if块)
```

### Phase 1 验证
- [ ] 编译通过
- [ ] `sd_config_validate()` 不再检查 intersection_active_channels
- [ ] ir_calibration.c 不再引用该参数

---

## 5. Phase 2: P1 核心简化 (Option A 完全方案)

### 5.1 感知模块

#### 文件5: `modules/Sens-Decision/inc/perception.h`

**删除 lines 18-23** (road_event_t 枚举):
```c
// 删除前:
typedef enum {
    ROAD_EVENT_NONE,
    ROAD_EVENT_CURVE_ENTRY,
    ROAD_EVENT_INTERSECTION,
    ROAD_EVENT_LINE_LOST
} road_event_t;

typedef struct {
    float lateral_error;
    float heading_error;
    uint16_t active_mask;
    uint16_t lost_count;
    road_event_t event;
    bool line_valid;
} perception_result_t;

// 删除后:
typedef struct {
    float lateral_error;
    float heading_error;
    uint16_t active_mask;
    uint16_t lost_count;
    bool line_valid;
} perception_result_t;
```

#### 文件6: `modules/Sens-Decision/src/perception.c`

**完整修改** - 删除事件检测逻辑 (lines 144-152) 并调整 line 105:

```c
// ===== Line 105: 删除 event 赋值 =====
// 修改前:
        result->event = ROAD_EVENT_LINE_LOST;
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;

// 修改后:
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;

// ===== Lines 144-152: 删除整个事件检测块 =====
// 删除以下全部:
        /* 步骤5: 道路事件检测 */
        if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
            result->event = ROAD_EVENT_INTERSECTION;
        } else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
                   fabsf(result->heading_error) >= g_sens_decision_config.perception.curve_derivative_threshold) {
            result->event = ROAD_EVENT_CURVE_ENTRY;
        } else {
            result->event = ROAD_EVENT_NONE;
        }
```

**修改后完整文件** (核心逻辑部分):
```c
    result->line_valid = (active_count > 0);

    /* 步骤3: 横向偏差计算 */
    if (active_count == 0) {
        ++perception->lost_count;
        result->lost_count = perception->lost_count;
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;
    } else {
        perception->lost_count = 0;
        result->lost_count = 0;

        /* 使用黑线强度总和归一化 */
        if (strength_sum > 1e-6f) {
            result->lateral_error = weighted_sum / strength_sum;
        } else {
            result->lateral_error = 0.0f;
        }

        /* 步骤4: 航向误差估计（时间导数） */
        if (perception->initialized) {
            dt_s = (timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
            if (dt_s > 0.0f) {
                derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                perception->heading_error =
                    g_sens_decision_config.perception.heading_filter_alpha * perception->heading_error +
                    (1.0f - g_sens_decision_config.perception.heading_filter_alpha) * derivative;
            }
        } else {
            if (timestamp_us > perception->prev_timestamp_us) {
                dt_s = (timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
                if (dt_s > 0.0f) {
                    derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                    perception->heading_error = derivative;
                } else {
                    perception->heading_error = 0.0f;
                }
            } else {
                perception->heading_error = 0.0f;
            }
        }
        result->heading_error = perception->heading_error;
    }
```

### 5.2 行为规划模块

#### 文件7: `modules/Sens-Decision/inc/behavior_planner.h`

**修改 behavior_state_t 枚举**:
```c
// 修改前:
typedef enum {
    BEHAVIOR_STATE_IDLE,
    BEHAVIOR_STATE_LINE_FOLLOW,
    BEHAVIOR_STATE_APPROACH_CURVE,
    BEHAVIOR_STATE_CURVE,
    BEHAVIOR_STATE_LINE_LOST_DEGRADED,
    BEHAVIOR_STATE_STOPPED,
    BEHAVIOR_STATE_FAULT
} behavior_state_t;

// 修改后:
typedef enum {
    BEHAVIOR_STATE_IDLE,
    BEHAVIOR_STATE_RUNNING,           // 重命名: LINE_FOLLOW → RUNNING
    BEHAVIOR_STATE_LINE_LOST_DEGRADED,
    BEHAVIOR_STATE_STOPPED,
    BEHAVIOR_STATE_FAULT
} behavior_state_t;
```

**删除 behavior_planner_t 中的 unused 字段**:
```c
// 修改前:
typedef struct {
    behavior_state_t current_state;
    behavior_state_t previous_running_state;
    uint16_t line_lost_frames;
    uint16_t critical_failure_count;
    uint16_t stable_straight_frames;     // 删除 - 不再需要
    float last_valid_lateral_error;
    bool initialized;
} behavior_planner_t;

// 修改后:
typedef struct {
    behavior_state_t current_state;
    behavior_state_t previous_running_state;
    uint16_t line_lost_frames;
    uint16_t critical_failure_count;
    float last_valid_lateral_error;
    bool initialized;
} behavior_planner_t;
```

#### 文件8: `modules/Sens-Decision/src/behavior_planner.c`

**修改1: 状态名数组**:
```c
// 修改前:
static const char *behavior_state_names[] = {
    "IDLE",
    "LINE_FOLLOW",
    "APPROACH_CURVE",
    "CURVE",
    "LINE_LOST_DEGRADED",
    "STOPPED",
    "FAULT"
};

// 修改后:
static const char *behavior_state_names[] = {
    "IDLE",
    "RUNNING",
    "LINE_LOST_DEGRADED",
    "STOPPED",
    "FAULT"
};
```

**修改2: behavior_planner_init**:
```c
// 删除 line 41: planner->stable_straight_frames = 0U;
// 删除 line 42 并调整
```

**修改3: behavior_planner_update - 删除曲线相关转换 (lines 110-129)**:
```c
// 删除以下全部代码:
        } else if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW &&
                   input->perception->event == ROAD_EVENT_CURVE_ENTRY) {
            new_state = BEHAVIOR_STATE_APPROACH_CURVE;
        } else if (planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE &&
                   (fabsf(input->perception->heading_error) >= 0.2f ||
                    fabsf(input->path_curvature) >= 0.2f)) {
            new_state = BEHAVIOR_STATE_CURVE;
            planner->stable_straight_frames = 0U;
        } else if (planner->current_state == BEHAVIOR_STATE_CURVE) {
            if (fabsf(input->perception->heading_error) < 0.1f &&
                fabsf(input->path_curvature) < 0.1f) {
                planner->stable_straight_frames++;
                if (planner->stable_straight_frames >= g_sens_decision_config.behavior.curve_exit_stable_frames) {
                    new_state = BEHAVIOR_STATE_LINE_FOLLOW;
                    planner->stable_straight_frames = 0U;
                }
            } else {
                planner->stable_straight_frames = 0U;
            }
```

同时将 LINE_FOLLOW 引用改为 RUNNING。

**修改4: was_running_state 检查**:
```c
// 修改前:
    was_running_state = (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW ||
                        planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE ||
                        planner->current_state == BEHAVIOR_STATE_CURVE);

// 修改后:
    was_running_state = (planner->current_state == BEHAVIOR_STATE_RUNNING);
```

**修改5: line_lost → running 转换**:
```c
// 修改前 (lines 87-89):
        if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW ||
            planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE ||
            planner->current_state == BEHAVIOR_STATE_CURVE) {

// 修改后:
        if (planner->current_state == BEHAVIOR_STATE_RUNNING) {
```

**修改6: 速度输出 (lines 141-169)**:
```c
// 修改后 (完整的 speed switch):
    switch (planner->current_state) {
        case BEHAVIOR_STATE_IDLE:
            output->speed_limit_mps = g_sens_decision_config.behavior.idle_speed_mps;
            break;
        case BEHAVIOR_STATE_RUNNING:
            // ★ 基于偏差的连续速度调节
            // 偏差大 → 适当减速，偏差小 → 全速
            {
                float error_mag = fabsf(input->perception->lateral_error);
                float speed_factor = 1.0f - g_sens_decision_config.behavior.speed_error_gain * error_mag;
                if (speed_factor < 0.4f) speed_factor = 0.4f;  // 最低40%速度
                output->speed_limit_mps = g_sens_decision_config.behavior.line_speed_mps * speed_factor;
            }
            break;
        case BEHAVIOR_STATE_LINE_LOST_DEGRADED:
            output->speed_limit_mps = g_sens_decision_config.behavior.degraded_speed_mps *
                                     (1.0f - (float)planner->line_lost_frames /
                                     (float)g_sens_decision_config.behavior.line_lost_stop_frames);
            if (output->speed_limit_mps < 0.0f) {
                output->speed_limit_mps = 0.0f;
            }
            break;
        case BEHAVIOR_STATE_STOPPED:
        case BEHAVIOR_STATE_FAULT:
            output->speed_limit_mps = 0.0f;
            break;
        default:
            output->speed_limit_mps = 0.0f;
            break;
    }
```

### 5.3 配置模块

#### 文件9: `modules/Sens-Decision/inc/config.h`

**修改 sd_perception_config_t**:
```c
// 修改前:
typedef struct {
    bool active_high;
    float weights[SD_IR_CHANNEL_COUNT];
    sd_position_t position;
    float heading_filter_alpha;
    float curve_error_threshold;         // 删除
    float curve_derivative_threshold;    // 删除
    uint8_t intersection_active_channels; // 删除 (Phase 1已删)
    float white_reference[SD_IR_CHANNEL_COUNT];
    float black_strength_threshold;
} sd_perception_config_t;

// 修改后:
typedef struct {
    bool active_high;
    float weights[SD_IR_CHANNEL_COUNT];
    sd_position_t position;
    float heading_filter_alpha;
    float white_reference[SD_IR_CHANNEL_COUNT];
    float black_strength_threshold;
} sd_perception_config_t;
```

**修改 sd_behavior_config_t**:
```c
// 修改前:
typedef struct {
    uint16_t localization_valid_frames;
    uint16_t localization_failure_frames;
    uint16_t line_recovery_frames;
    uint16_t line_lost_stop_frames;
    uint16_t critical_failure_frames;
    uint16_t curve_exit_stable_frames;      // 删除
    float idle_speed_mps;
    float line_speed_mps;
    float approach_curve_speed_mps;         // 删除
    float curve_speed_mps;                  // 删除
    float degraded_speed_mps;
} sd_behavior_config_t;

// 修改后:
typedef struct {
    uint16_t localization_valid_frames;
    uint16_t localization_failure_frames;
    uint16_t line_recovery_frames;
    uint16_t line_lost_stop_frames;
    uint16_t critical_failure_frames;
    float idle_speed_mps;
    float line_speed_mps;
    float degraded_speed_mps;
    float speed_error_gain;                 // ★ 新增：偏差→速度增益
} sd_behavior_config_t;
```

#### 文件10: `modules/Sens-Decision/src/config.c`

**删除**: lines 332-333 (curve参数的初始化)
```c
// 删除:
    g_sens_decision_config.perception.curve_error_threshold = 0.45f;
    g_sens_decision_config.perception.curve_derivative_threshold = 1.5f;
```

**删除**: lines 532, 535-536 (behavior参数的初始化)
```c
// 删除:
    g_sens_decision_config.behavior.curve_exit_stable_frames = 5U;
    // ...
    g_sens_decision_config.behavior.approach_curve_speed_mps = 0.7f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.5f;
```

**新增**: speed_error_gain 初始化
```c
// 新增 (放在 behavior 配置区):
    /**
     * 基于横向偏差的速度调节增益
     * speed = line_speed * (1 - gain * |lateral_error|), 最小40%
     * 默认值 0.3: lateral_error=1.0 时速度降至70%, lateral_error=2.0 时速度降至40%
     */
    g_sens_decision_config.behavior.speed_error_gain = 0.3f;
```

**修改 sd_config_validate**:
```c
// 删除 curve_error_threshold/curve_derivative_threshold 验证:
// 删除前 lines 622-623:
        !positive_finite(config->perception.curve_error_threshold) ||
        !positive_finite(config->perception.curve_derivative_threshold) ||

// 删除 curve_exit_stable_frames 验证:
// 删除 line 656:
        config->behavior.curve_exit_stable_frames == 0U ||

// 删除 approach/curve speed 验证:
// 删除 lines 660-661:
        !positive_finite(config->behavior.approach_curve_speed_mps) ||
        !positive_finite(config->behavior.curve_speed_mps) ||

// 新增 speed_error_gain 验证:
// 添加:
        !isfinite(config->behavior.speed_error_gain) ||
        config->behavior.speed_error_gain < 0.0f ||
```

### 5.4 调用者文件修改

#### 文件11: `modules/Sens-Decision/src/perception_debug.c`

**修改1: 详细打印 - 删除 event switch (lines 87-105)**:
```c
// 修改前:
    printf("Event:         ");
    switch (result->event) {
        case ROAD_EVENT_NONE:         printf("NORMAL TRACKING\r\n"); break;
        case ROAD_EVENT_CURVE_ENTRY:  printf("CURVE ENTRY\r\n");    break;
        case ROAD_EVENT_INTERSECTION: printf("INTERSECTION\r\n");   break;
        case ROAD_EVENT_LINE_LOST:    printf("LINE LOST\r\n");      break;
        default:                      printf("UNKNOWN\r\n");        break;
    }

// 修改后:
    printf("Speed Factor:  %.2f\r\n",
           1.0f - g_sens_decision_config.behavior.speed_error_gain * fabsf(result->lateral_error));
```

**修改2: 紧凑打印 - 删除 event 标记 (lines 143-149)**:
```c
// 修改前:
    if (result->event == ROAD_EVENT_INTERSECTION) {
        printf(" | INTERSECTION");
    } else if (result->event == ROAD_EVENT_LINE_LOST) {
        printf(" | LINE_LOST");
    } else if (result->event == ROAD_EVENT_CURVE_ENTRY) {
        printf(" | CURVE");
    }

// 修改后:
    if (!result->line_valid) {
        printf(" | LINE_LOST");
    } else if (fabsf(result->heading_error) > 2.0f) {
        printf(" | HIGH_HDG");
    }
```

**修改3: 自检 - 删除 intersection 部分 (lines 214-222)**:
```c
// 删除整个 [4] 检查块 (lines 214-222)
// 或替换为 speed_error_gain 检查:
    printf("\n[4] Speed Error Gain:\n");
    float gain = config->speed_error_gain;
    if (gain < 0.0f || gain > 2.0f) {
        printf("  ❌ Speed error gain: %.3f (out of range 0-2)\n", gain);
        passed = false;
    } else {
        printf("  ✓ Speed error gain: %.3f\n", gain);
    }
```

#### 文件12: `Core/Src/app/speed_mode.c`

**修改所有4个速度模式**: 删除 approach/curve 速度设置:

```c
// ===== SPEED_MODE_DEBUG =====
// 修改前:
    g_sens_decision_config.behavior.approach_curve_speed_mps = 0.18f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.15f;

// 修改后:
// (删除这两行)

// ===== SPEED_MODE_SLOW =====
// 修改前:
    g_sens_decision_config.behavior.approach_curve_speed_mps = 0.4f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.3f;

// 修改后:
// (删除这两行)

// ===== SPEED_MODE_NORMAL =====
// 修改前:
    g_sens_decision_config.behavior.approach_curve_speed_mps = 0.7f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.5f;

// 修改后:
// (删除这两行)

// ===== SPEED_MODE_FAST =====
// 修改前:
    g_sens_decision_config.behavior.approach_curve_speed_mps = 1.0f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.8f;

// 修改后:
// (删除这两行)
```

同步更新printf输出字符串（移除approach/curve速度信息）。

---

## 6. 新增速度控制策略

### 推荐方案: 偏差比例速度调节 (A1)

```c
// 在behavior_planner.c的RUNNING状态下使用:
float error_mag = fabsf(input->perception->lateral_error);
float speed_factor = 1.0f - g_sens_decision_config.behavior.speed_error_gain * error_mag;
if (speed_factor < 0.4f) speed_factor = 0.4f;  // clamp到40%
output->speed_limit_mps = g_sens_decision_config.behavior.line_speed_mps * speed_factor;
```

**原理**: 
- |lateral_error| ≈ 0（居中）→ speed_factor ≈ 1.0 → 全速
- |lateral_error| ≈ 1.0（偏10mm）→ speed_factor ≈ 0.7 → 70%速度
- |lateral_error| ≈ 2.0（偏20mm）→ speed_factor ≈ 0.4 (clamped) → 40%速度

**调参指南**:
- `speed_error_gain = 0.3`: 默认值，适合大多数情况
- `speed_error_gain = 0.5`: 更激进减速（配合高KP）
- `speed_error_gain = 0.2`: 更保守（依赖PD转向，速度不轻易降）

### 备选方案: 曲率速度调节 (A2)

如果需要在转弯时主动降速（需要轨迹规划器提供`path_curvature`）:

```c
// 曲率越大 → 速度越低
float curvature_speed = g_sens_decision_config.behavior.line_speed_mps /
                        (1.0f + g_sens_decision_config.trajectory.curvature_speed_gain * fabsf(path_curvature));
float error_speed = g_sens_decision_config.behavior.line_speed_mps *
                    (1.0f - g_sens_decision_config.behavior.speed_error_gain * fabsf(lateral_error));
// 取较保守的速度
output->speed_limit_mps = fminf(curvature_speed, error_speed);
```

**如果当前轨迹规划器没实现curvature**: 直接用A1，后续可以在A2的基础上优化。

### 转向控制（不变）

```c
// motion_control.c 中的PD控制器保持不变
steering = kp * lateral_error + kd * heading_error;
```

---

## 7. 风险评估

### Phase 1 风险

| 风险类型 | 等级 | 描述 | 缓解措施 |
|---------|------|------|---------|
| 编译错误 | 🟢 极低 | 删除未使用参数 | 先搜索所有引用，确认无遗漏 |
| 运行时崩溃 | 🟢 极低 | 死代码，无运行时影响 | N/A |
| 控制质量 | 🟢 极低 | INTERSECTION检测本来就不会触发 | N/A |
| **回滚方案** | | `git revert` 单个commit | |

### Phase 2 风险

| 风险类型 | 等级 | 描述 | 缓解措施 |
|---------|------|------|---------|
| 编译错误 | 🟡 低 | 多处文件修改可能遗漏引用 | 编译前后 `grep` 验证所有符号已清理 |
| 连接错误 | 🟡 低 | 结构体大小变化可能影响序列化 | 检查是否有memcpy/EEPROM操作依赖于结构体大小 |
| 急弯冲出路 | 🟡 中 | 无主动降速可能导致过弯速度过快 | ① PD增益足够高 ② speed_error_gain调至0.5 ③ 后续可加A2方案 |
| 速度振荡 | 🟢 低 | 偏差变化导致速度抖动 | speed_factor有clamp(0.4, 1.0)，且有heading_filter平滑 |
| **回滚方案** | | `git revert` 整个Phase 2 commit，恢复原始代码 | |

### 通用回滚触发条件

以下任一情况发生则考虑回滚:
1. 编译失败且15分钟内无法修复
2. 实车测试时发现直线上速度抖动明显（>20%波动）
3. 急弯冲出赛道（A1方案无法处理）

---

## 8. 验证计划

### 8.1 Pre-Merge 检查

- [ ] 编译通过，0 error, 0 warning
- [ ] `grep -r "road_event_t"` 在源码中无匹配 (docs/目录除外)
- [ ] `grep -r "ROAD_EVENT_"` 在源码中无匹配
- [ ] `grep -r "intersection_active_channels"` 在源码中无匹配
- [ ] `grep -r "curve_error_threshold\|curve_derivative_threshold\|approach_curve_speed\|curve_speed_mps\|curve_exit_stable"` 在源码中无匹配
- [ ] `sd_config_validate()` 返回 SD_OK
- [ ] `behavior_planner_init()` 不访问已删除的字段

### 8.2 Post-Merge 实车测试

#### 测试1: 直线循迹 (P0 必须通过)
- **条件**: 1m长直线，黑线宽15mm
- **初始位置**: 车居中于黑线
- **速度模式**: DEBUG (0.2 m/s)
- **预期**: 稳定跟随，无明显S形摆动
- **通过标准**: lateral_error保持在 ±0.5 以内

#### 测试2: 直线+扰动 (P0 必须通过)
- **条件**: 同测试1，但有意将车偏置10mm
- **预期**: 车自动纠偏，2秒内回到居中位置
- **通过标准**: 稳定后 lateral_error < 0.2

#### 测试3: 缓弯循迹 (P1 重要)
- **条件**: R=500mm 弧线
- **速度模式**: SLOW (0.5 m/s)
- **预期**: 平滑过弯，无明显冲出
- **通过标准**: 不脱离黑线（8个传感器至少1个能看到黑线）

#### 测试4: 急弯循迹 (P1 重要)
- **条件**: R=300mm 弧线（最急剧弯）
- **速度模式**: DEBUG (0.2 m/s)
- **预期**: 转向及时，不过冲
- **通过标准**: 不脱离黑线

#### 测试5: 直曲过渡 (P2 可选)
- **条件**: 直线→R=500mm→直线 的完整弯道
- **速度模式**: SLOW (0.5 m/s)
- **预期**: 过渡平滑，无明显速度突跳
- **通过标准**: 全程不脱离黑线，速度变化 < 30%

#### 测试6: 速度响应验证 (P2 可选)
- **条件**: 慢速手动偏移小车，观察串口输出的 speed_factor
- **预期**: lateral_error增大 → speed_factor减小
- **通过标准**: 速度响应连续、无振荡

---

## 9. Agent 任务交接

### Agent-2 (代码实现) 的任务Prompt

```
你是代码实现Agent。你的任务是按照审核通过的实施方案，修改IR循迹算法的代码。

## 背景
用户已审核通过了"IR循迹算法简化"的实施方案（Option A - 激进简化）。
实施方案文档: docs/IMPLEMENTATION_PLAN_IR_SIMPLIFICATION_2026-07-30.md

## 你的任务

### Phase 1: P0快速清理
1. 读取并修改 perception.c (删除 intersection 检测分支)
2. 读取并修改 config.h (删除 intersection_active_channels 字段)
3. 读取并修改 config.c (删除初始化和验证逻辑)
4. 读取并修改 ir_calibration.c (删除 intersection 标记)

### Phase 2: P1核心简化
5. 修改 perception.h (删除 road_event_t 枚举和 event 字段)
6. 修改 perception.c (删除事件检测逻辑, 调整 line 105)
7. 修改 behavior_planner.h (简化状态枚举, 删除 stable_straight_frames)
8. 修改 behavior_planner.c (简化FSM, 实现偏差调速, 删除曲线转换)
9. 修改 config.h (删除6参数, 新增 speed_error_gain)
10. 修改 config.c (删除参数初始化验证, 新增 speed_error_gain)
11. 修改 perception_debug.c (移除event打印, 改为打印新信息)
12. 修改 speed_mode.c (移除 curve/approach 速度设置)

## 重要规则
1. 修改前先用 Read 工具读取文件，确认行号
2. 使用 Edit 工具进行精确修改
3. 每次修改后验证语法（不引入未闭合的括号）
4. 完成所有修改后，编译验证
5. 创建 docs/MODIFICATION_SUMMARY_2026-07-30.md，包含：
   - 每个文件的修改内容列表
   - 修改前后代码片段对比
   - 编译验证结果
   - 新增/删除的参数列表

## 输出
1. 所有修改后的代码文件
2. docs/MODIFICATION_SUMMARY_2026-07-30.md (供Agent-3使用)
```

### Agent-3 (文档更新) 的任务Prompt

```
你是文档更新Agent。你的任务是在代码修改完成后，同步更新所有受影响的文档。

## 背景
IR循迹算法已简化（移除事件检测、简化FSM、删除6个冗余参数、新增1个参数）。
修改总结文档: docs/MODIFICATION_SUMMARY_2026-07-30.md
分析报告: docs/IR_ALGORITHM_ANALYSIS_2026-07-30.md
实施方案: docs/IMPLEMENTATION_PLAN_IR_SIMPLIFICATION_2026-07-30.md

## 你的任务

### 1. 读取修改总结
先读取 docs/MODIFICATION_SUMMARY_2026-07-30.md，理解具体修改了什么。

### 2. 更新现有文档

#### CHANGELOG.md
添加 v1.3.0 条目:
```markdown
## [v1.3.0] - 2026-07-30
### Changed
- IR循迹算法简化：移除线型事件检测（curve/intersection）
- 感知层输出简化：lateral_error + heading_error，不再输出road_event_t
- 行为状态机简化：7状态→5状态（移除APPROACH_CURVE和CURVE）
- 速度控制改为基于偏差的连续调节
### Removed
- road_event_t枚举和perception_result_t.event字段
- 6个冗余参数：curve_error_threshold, curve_derivative_threshold,
  intersection_active_channels, approach_curve_speed_mps, curve_speed_mps,
  curve_exit_stable_frames
### Added
- speed_error_gain参数（偏差→速度调节增益）
### Fixed
- 消除伪曲线检测导致的直线上误降速问题
```

#### README.md
更新算法描述部分，移除关于线型检测和分级速度的说明。

#### API_PITFALLS_GUIDE.md
移除关于road_event_t, line type detection, curve threshold tuning的陷阱条目。

#### docs/PARAMETER_TRACEABILITY.md
- 删除6个已移除参数的追溯条目
- 添加speed_error_gain的追溯条目

### 3. 创建新文档

#### docs/SESSION_FIX_LOG_2026-07-30_PART3.md (会话总结)
包含:
- Executive Summary
- 会话时间线
- 修改文件清单（代码+文档）
- 参数对比表（Before/After）
- Agent统计
- 待验证项

#### docs/ALGORITHM_SIMPLIFICATION_2026-07-30.md (算法简化对比)
包含:
- Before/After 代码架构图
- Before/After 状态机对比
- Before/After 参数列表
- 新速度控制策略说明
- 迁移指南（如果有旧代码需要移植）

#### docs/VALIDATION_AFTER_ALGORITHM_SIMPLIFICATION.md (验证清单)
复制实施方案中的验证计划，整理为可执行的检查列表。

## 输出
1. 更新后的 CHANGELOG.md
2. 更新后的 README.md (如果涉及)
3. 更新后的 API_PITFALLS_GUIDE.md (如果涉及)
4. 更新后的 docs/PARAMETER_TRACEABILITY.md
5. 新建 docs/SESSION_FIX_LOG_2026-07-30_PART3.md
6. 新建 docs/ALGORITHM_SIMPLIFICATION_2026-07-30.md
7. 新建 docs/VALIDATION_AFTER_ALGORITHM_SIMPLIFICATION.md
```

---

## 10. 文件修改总览

| # | 文件 | Phase | 修改类型 | 行数变化 | 风险 |
|---|------|-------|---------|---------|------|
| 1 | `perception.c` | P1+P2 | 删除事件检测 | -10行 | 🟢 |
| 2 | `perception.h` | P2 | 删除枚举+字段 | -6行 | 🟢 |
| 3 | `behavior_planner.c` | P2 | 简化FSM+调速 | -40行 | 🟡 |
| 4 | `behavior_planner.h` | P2 | 简化枚举+字段 | -6行 | 🟢 |
| 5 | `config.h` | P1+P2 | 删6参数+加1参数 | -4行 | 🟢 |
| 6 | `config.c` | P1+P2 | 删初始化+验证 | -30行 | 🟢 |
| 7 | `perception_debug.c` | P2 | 更新调试输出 | -20行 | 🟢 |
| 8 | `speed_mode.c` | P2 | 删除曲线速度设置 | -16行 | 🟢 |
| 9 | `ir_calibration.c` | P1 | 删除交叉标记 | -3行 | 🟢 |
| **合计** | **9个文件** | | | **~-135行** | |

---

## 11. 审核确认

请在审核后选择:

- [ ] **Phase 1 批准**: 先执行P0快速清理（INTERSECTION检测移除）
- [ ] **Phase 2 批准**: 执行P1核心简化
- [ ] **设计选择**: Option ___ (A=激进, B=温和, C=保守)
- [ ] **速度策略**: A1 (偏差调速) / A2 (曲率调速) / A3 (固定速度)
- [ ] **合并策略**: 两个Phase合并为一个commit / 分为两个独立commit

---

**文档状态**: ⏳ 等待用户审核
**下一步**: 用户确认后 → 启动Agent-2执行代码修改 → 启动Agent-3更新文档

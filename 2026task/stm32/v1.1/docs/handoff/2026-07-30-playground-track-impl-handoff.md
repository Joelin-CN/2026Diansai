# 交接文档：操场型循迹实现 (playground_track)

**日期**: 2026-07-30
**交接原因**: 会话上下文已满，切换新会话继续编写代码
**状态**: 设计阶段完成，待实现

---

## 一、本次会话完成的工作

1. 深度阅读了整个工程代码库（IR传感器、perception、behavior_planner、MotionControl、freertos、track_control_app 等）
2. 与用户确认了比赛需求（第2题全圈 + 第4题A→B钢球稳定）
3. 提出3种方案，用户选择**方案B：分段自适应循迹**
4. 完成设计审批（用户已确认）
5. 设计文档已写入并提交：
   - **路径**: `docs/superpowers/specs/2026-07-30-playground-track-design.md`
   - **commit**: `46918e5`

---

## 二、待实现的文件

### 新建（核心工作）
| 文件 | 说明 |
|---|---|
| `Core/Inc/app/playground_track.h` | 公共 API 头文件 |
| `Core/Src/app/playground_track.c` | 完整实现 |

### 修改
| 文件 | 改动内容 |
|---|---|
| `Core/Src/freertos.c` | 在现有 `#define TEST_MODE_*` 旁添加 `TEST_MODE_PLAYGROUND_TRACK` 分支 |
| `CMakeLists.txt` | 在 `target_sources` 里添加 `Core/Src/app/playground_track.c` |

---

## 三、设计要点速查（详见规格文档）

### 赛道几何（顺时针）
```
A ─────────── B      A = 顶部左端（启停线所在）
│             │      B = 顶部右端
│   R=0.5m   │      C = 底部右端
│             │      D = 底部左端
D ─────────── C
```

| 分段 | 里程区间 |
|---|---|
| 直道 A→B | 0.000 – 1.500 m |
| 弯道 B→C | 1.500 – 3.071 m |
| 直道 C→D | 3.071 – 4.571 m |
| 弯道 D→A（前段） | 4.571 – 5.000 m |
| 弯道 D→A（接近段） | 5.000 m – A线检测 |

### 两个任务的状态机

**第2题 (PLAYGROUND_TASK_LAP)**:
```
PT_IDLE →(line_valid×3帧)→ PT_TASK2_RUN
PT_TASK2_RUN →(dist>5.5m AND横线≥6通道)→ PT_TASK2_APPROACH_A
PT_TASK2_APPROACH_A →(v_actual<0.05m/s)→ PT_STOPPED
PT_TASK2_RUN →(line_lost>10帧)→ PT_FAULT
```

**第4题 (PLAYGROUND_TASK_AB_STRAIGHT)**:
```
PT_IDLE → PT_TASK4_ACCEL → PT_TASK4_CRUISE → PT_TASK4_DECEL → PT_STOPPED
```

### 段速度与增益表

| 里程区间 | v_target | kp_lat | kd_head | ω_max |
|---|---|---|---|---|
| 0.000–1.500m（直道） | 1.00 m/s | 1.5 | 1.0 | ±3.0 |
| 1.500–3.071m（弯道） | 0.60 m/s | 2.5 | 1.5 | ±3.0 |
| 3.071–4.571m（直道） | 1.00 m/s | 1.5 | 1.0 | ±3.0 |
| 4.571–5.000m（弯道前段） | 0.60 m/s | 2.5 | 1.5 | ±3.0 |
| 5.000m–A线（接近） | 0.25 m/s | 2.0 | 1.2 | ±2.0 |

### 第4题梯形速度参数
- v_max = 0.50 m/s，a_ramp = 0.30 m/s²
- d_ramp = v²/(2a) = 0.417m，d_decel_start = 1.083m
- 钢球最大位移估算：L=15cm时约 0.46cm < 1cm ✓

### omega 计算（通用）
```c
float omega = -(kp * lateral_error + kd * heading_error);
omega = clamp(omega, -omega_max, +omega_max);
```
`lateral_error` 单位为 **cm**（来自 `perception.c` 加权质心）

### A线检测
```c
// 统计 active_mask 中置位数 >= 6 AND dist > 5.5m
uint8_t n = 0;
for (uint8_t i = 0; i < 8; i++) n += (result.active_mask >> i) & 1U;
bool transverse = (n >= 6) && (g_dist_m > 5.5f);
```

### 里程积分
```c
// 50Hz，dt = 0.020s
float vl, vr;
MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
g_dist_m += (vl + vr) * 0.5f * 0.020f;
```

---

## 四、关键 API 参考

### 必须调用的函数（按调用顺序）

**初始化链**（参考 `track_control_app.c`，去掉 EKF/trajectory 部分）：
```c
Motor_Init(); Motor_Stop();
Encoder_Init();
IrUartSensor_Init(); IrUartSensor_RequestAnalogMode();
sd_config_reset_defaults(); PlatformTime_Init();
icm42688_stm32_bind(&cfg); icm42688_init();    // 失败可 skip
sensors_configure_hal(hal); sensors_init_all();
perception_init(&g_perc);
MotionControl_Init(&g_mc, EncoderAdapter_GetInterface(), MotorAdapter_GetInterface());
MotionControl_Start(&g_mc);
```

**RunFastCycle 结构**（与现有代码完全一致）：
```c
void PlaygroundTrack_RunFastCycle(void) {
    Encoder_Poll();                                 // 500Hz
    if ((g_cyc % 5) == 0) MotionControl_Update(&g_mc);   // 100Hz
    if ((g_cyc % 10) == 0) pg_decide_50hz();              // 50Hz
    if (++g_cyc >= 10) g_cyc = 0;
}
```

**50Hz 决策层**：
```c
static void pg_decide_50hz(void) {
    preprocess_update(PlatformTime_GetUs64(), &g_sf);
    if (perception_update(&g_perc, &g_sf.ir,
                          g_sf.timestamp_us, &g_res) != SD_OK) {
        // handle line lost
        return;
    }
    // 里程积分
    float vl, vr;
    MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
    g_dist_m += (vl + vr) * 0.5f * 0.020f;
    // 状态机
    pg_state_machine();
}
```

**设置速度指令**：
```c
MotionControl_SetVelocityCommand(&g_mc, v_cmd, omega);
```

**停车**：
```c
MotionControl_Stop(&g_mc);
Motor_Stop();
```

### 头文件引用（参考 track_control_app.c includes）
```c
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include "encoder_adapter.h"
#include "motor_adapter.h"
#include "sensor_adapter.h"
#include "ir_uart_sensor.h"
#include "icm42688_hal.h"
#include "icm42688_stm32.h"
#include "motion_control.h"
#include "config.h"
#include "interface.h"
#include "preprocess.h"
#include "perception.h"
#include "playground_track.h"
```

---

## 五、freertos.c 修改位置

在第65行附近添加第三个 define 选项，并在 `StartDefaultTask()` 里添加 `#elif` 分支：

```c
// 第65行附近：
// #define TEST_MODE_IR_CALIBRATION
// #define TEST_MODE_TRACK_CONTROL
#define TEST_MODE_PLAYGROUND_TRACK   /* ← 操场型循迹（第2/4题） */

// StartDefaultTask() 里，在 #elif defined(TEST_MODE_TRACK_CONTROL) 之后添加：
#elif defined(TEST_MODE_PLAYGROUND_TRACK)
  #include "playground_track.h"
  if (!PlaygroundTrack_Init(PLAYGROUND_TASK_LAP)) {
      printf("[FATAL] PlaygroundTrack_Init failed\r\n");
      for (;;) { osDelay(1000); }
  }
  SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
  printf("[PlaygroundTrack] Running\r\n");
  for (;;) {
      PlaygroundTrack_RunFastCycle();
      osDelay(2);
  }
```

若要切换到第4题，将 `PLAYGROUND_TASK_LAP` 改为 `PLAYGROUND_TASK_AB_STRAIGHT`。

---

## 六、CMakeLists.txt 修改

在 `target_sources` 的用户应用层列表中，紧跟 `track_control_app.c` 后添加：
```cmake
Core/Src/app/playground_track.c
```

---

## 七、完成后文档任务（遵守 CLAUDE.md）

1. 更新 `CHANGELOG.md`：添加 v1.4.0 条目（新增操场型循迹模块）
2. 创建 `docs/SESSION_FIX_LOG_2026-07-30_PART4.md`：记录本次实现概要
3. 更新 `API_PITFALLS_GUIDE.md`：如有新坑记录

---

## 八、验证步骤（P0，上车前必过）

1. `cmake --build cmake-build-debug` 无编译错误
2. 切 `TEST_MODE_PLAYGROUND_TRACK`，先将 `v_straight` 改为 `0.5f`，上赛道跑一圈
3. 确认 A线检测可靠（横跨黑线时串口打印检测信息）
4. 确认停车偏差 ≤ 2cm
5. 第4题验证钢球偏移

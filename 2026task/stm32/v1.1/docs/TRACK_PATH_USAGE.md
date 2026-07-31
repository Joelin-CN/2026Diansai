# 操场路径循迹功能使用指南

## 概述

本模块实现了一个类似400米操场形状的循迹轨道，包括：
- **两个半圆周**：半径 0.5m
- **两段直线**：长度 1.5m
- **总周长**：约 6.14m (2πR + 2L)

## 文件说明

### 核心文件

| 文件 | 说明 |
|------|------|
| `Core/Inc/app/track_path.h` | 操场路径定义（头文件） |
| `Core/Src/app/track_path.c` | 操场路径生成实现 |
| `Core/Inc/app/track_control_app.h` | 控制应用头文件 |
| `Core/Src/app/track_control_app.c` | 控制应用实现 |
| `Core/Src/app/track_demo.c` | 示例主程序 |

### 路径几何说明

```
代码坐标系（X=前方, Y=左侧）:

            半圆2 (左侧)
                 ╭─────╮
           C ────╯     ╰──── D
                直线段2
           │                 │
      直线段1           直线段1
           │                 │
           A ────╮     ╭──── B
                 ╰─────╯
            半圆1 (右侧)

关键点坐标:
  A: (0.00, -0.50) - 起点（半圆1入口）
  B: (1.50, -0.50) - 半圆1出口/直线2入口
  C: (1.50, +0.50) - 直线2出口/半圆2入口
  D: (0.00, +0.50) - 半圆2出口/直线1入口

路径参数:
  - 离散点数: ~308个点
  - 点间距: ≤20mm
  - 半圆曲率: 2.0 m⁻¹ (κ = 1/R)
  - 直线曲率: 0.0 m⁻¹
```

## 使用方法

### 方法1: 使用示例代码（推荐）

1. **复制示例代码到 `freertos.c`**

   打开 `Core/Src/app/track_demo.c`，将 `StartDefaultTask()` 函数复制到 `freertos.c` 中，替换原有的任务函数。

2. **设置目标圈数**

   在 `StartDefaultTask()` 中修改：
   ```c
   const uint8_t TARGET_LAPS = 3;  // 改为你想要的圈数 (1-10)
   ```

3. **编译烧录**
   ```bash
   cd build
   cmake --build . --target v1.0_freeRTOS.elf
   # 然后使用 ST-Link 烧录
   ```

4. **运行测试**
   - 将小车放置在轨道起点（A点，半圆1的入口）
   - 确保红外传感器对准黑线
   - 上电后自动开始循迹

### 方法2: 直接调用API

在你的FreeRTOS任务中：

```c
#include "track_control_app.h"

void YourTask(void *argument) {
    // 初始化（目标3圈）
    if (!TrackControlApp_Init(3)) {
        printf("Init failed!\n");
        return;
    }

    // 防御性修复：重新使能USART2中断
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    // 500 Hz 控制循环
    for (;;) {
        TrackControlApp_RunFastCycle();

        // 检查是否完成
        if (TrackControlApp_IsComplete()) {
            printf("Mission complete!\n");
            break;
        }

        osDelay(2);  // 2ms = 500Hz
    }
}
```

## 参数调优

### 初始参数（保守设置）

在 `track_control_app.c` 的 `TrackControlApp_Init()` 中：

```c
g_track_config.lateral_gain = 1.5f;      // 横向偏差增益
g_track_config.heading_gain = 1.0f;      // 航向偏差增益
g_track_config.max_omega_radps = 3.0f;   // 最大角速度
g_track_config.line_speed_mps = 0.5f;    // 直线速度
g_track_config.curve_speed_mps = 0.3f;   // 过弯速度
```

### 调优策略

#### 阶段1：验证基本功能（低速）
```c
line_speed_mps = 0.3f;
curve_speed_mps = 0.2f;
lateral_gain = 1.0f;
heading_gain = 0.5f;
```
**目标**：稳定完成1圈，先关闭航向修正

#### 阶段2：提升性能（中速）
```c
line_speed_mps = 0.6f;
curve_speed_mps = 0.4f;
lateral_gain = 1.5f;
heading_gain = 1.0f;
```
**目标**：稳定完成3-5圈

#### 阶段3：冲击极限（高速）
```c
line_speed_mps = 1.0f;
curve_speed_mps = 0.7f;
lateral_gain = 2.0f;
heading_gain = 1.5f;
```
**目标**：最短时间完成10圈

### 参数说明

| 参数 | 说明 | 典型范围 |
|------|------|----------|
| `lateral_gain` | 横向偏差修正增益，越大转向越激进 | 0.5 - 3.0 |
| `heading_gain` | 航向偏差修正增益，越大对方向敏感 | 0.0 - 2.0 |
| `max_omega_radps` | 最大角速度限制 | 2.0 - 5.0 rad/s |
| `line_speed_mps` | 直线段速度 | 0.3 - 1.5 m/s |
| `curve_speed_mps` | 曲线段速度 | 0.2 - 1.0 m/s |

## CMakeLists.txt 配置

确保新文件已添加到编译列表中。在 `CMakeLists.txt` 中添加：

```cmake
# Track path application
set(APP_SOURCES
    ${APP_SOURCES}
    ${CMAKE_SOURCE_DIR}/Core/Src/app/track_path.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app/track_control_app.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app/track_demo.c
)
```

## 调试技巧

### 1. 查看路径点数据

在初始化后打印路径信息：
```c
size_t count = TrackPath_GetPointCount();
const path_point_t *path = TrackPath_GetPoints();

printf("Track path loaded: %u points\n", count);
printf("First point: x=%.3f, y=%.3f, heading=%.3f\n",
       path[0].x, path[0].y, path[0].heading);
```

### 2. 监控当前位置

在控制循环中添加：
```c
if (cycle_count % 500 == 0) {  // 每1秒
    printf("Pos: x=%.3f, y=%.3f, theta=%.3f, v=%.3f\n",
           g_state_evaluator.state.x,
           g_state_evaluator.state.y,
           g_state_evaluator.state.theta,
           g_state_evaluator.state.v);
}
```

### 3. 检查红外传感器

使用现有的诊断工具：
```c
#include "ir_uart_diagnostic.h"
IrUartDiag_PrintReport();  // 打印接收统计
```

### 4. 验证编码器

```c
#include "encoder_hw_bridge.h"
int32_t left = EncoderHwBridge_GetCount(0);
int32_t right = EncoderHwBridge_GetCount(1);
printf("Encoders: L=%ld, R=%ld\n", left, right);
```

## 常见问题

### Q1: 小车不动
**排查步骤**：
1. 检查电机使能引脚 (STBY = PE0)
2. 检查 `Motor_Init()` 是否调用
3. 检查 `__HAL_TIM_MOE_ENABLE(&htim1)` 是否执行
4. 用万用表测量 PWM 输出

### Q2: 小车偏离轨道
**可能原因**：
1. `lateral_gain` 太小 → 增大到 2.0
2. 速度过快 → 降低 `line_speed_mps`
3. 红外传感器未对准 → 调整安装位置
4. 编码器反馈错误 → 检查 `ENCODER_PPR = 60000`

### Q3: 转弯时冲出去
**解决方案**：
1. 降低 `curve_speed_mps` (建议 0.2 - 0.3 m/s)
2. 增大 `lateral_gain` 提高转向响应
3. 检查 `max_omega_radps` 是否足够（建议 ≥ 3.0）

### Q4: 红外传感器无数据
**排查步骤**：
1. 检查 USART2 接线 (PA2=TX, PA3=RX)
2. 确认波特率 115200
3. 执行防御性修复：
   ```c
   SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
   ```
4. 运行诊断工具查看统计信息

### Q5: 圈数统计不准确
**原因**：
- 起点保护区太小，导致误计数
- 路径点索引跳变异常

**解决方案**：
在 `track_path.c` 的 `TrackPath_UpdateLap()` 中调整保护区大小：
```c
size_t guard_threshold = path_count / 20;  // 5% 保护区
// 可以改为 path_count / 10 增大到 10%
```

## 性能预期

| 速度设置 | 单圈时间 | 10圈总时间 | 稳定性 |
|---------|---------|-----------|--------|
| 低速 (0.3/0.2 m/s) | ~25秒 | ~4.2分钟 | 高 |
| 中速 (0.6/0.4 m/s) | ~13秒 | ~2.2分钟 | 中 |
| 高速 (1.0/0.7 m/s) | ~8秒 | ~1.3分钟 | 低 |

**注意**：实际性能取决于：
- 电机扭矩
- 轮胎抓地力
- 传感器响应速度
- 控制器调优质量

## 与方形路径的区别

| 特性 | 方形路径 | 操场路径 |
|------|---------|---------|
| 形状 | 4条直线 + 4个直角 | 2条直线 + 2个半圆 |
| 周长 | 4m | ~6.14m |
| 曲率 | 直线=0, 拐角处无穷大 | 直线=0, 半圆=2.0 m⁻¹ |
| 难度 | 低（直角易识别） | 中（需平滑过弯） |
| 速度限制 | 拐角处需急刹 | 可保持较高速度 |
| 适用场景 | 快速验证 | 性能测试 |

## 参考资料

- **API调试手册**: `API_PITFALLS_GUIDE.md`
- **系统README**: `README.md`
- **开发日志**: `logs/` 目录
- **坐标系说明**: `API_PITFALLS_GUIDE.md` 第7节

## 后续优化方向

1. **动态速度规划**：根据曲率实时调整速度
2. **前瞻距离自适应**：低速时减小前瞻，高速时增大
3. **IMU融合**：使用陀螺仪辅助航向估计
4. **轨迹预测**：预判下一个弯道，提前减速
5. **多路径支持**：椭圆、8字形等复杂轨迹

---

**创建时间**: 2026-07-30  
**版本**: 1.0.0  
**作者**: Claude (Kiro)

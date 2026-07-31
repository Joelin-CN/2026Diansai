# STM32F407 H题任务 2/3/4/5/6 统一工程

本工程从已经实车验证效果良好的 `v1.1` 复制后独立开发，不会修改原工程。

## 上电与按键

1. 上电后先初始化电机、编码器、IR、ICM42688，并完成 100 点陀螺仪零偏校准。
2. OLED 在此期间显示 `CALIBRATING...`，串口最终输出实际初始化耗时。
3. 典型初始化耗时约 **1.2~1.5 秒**。OLED 显示任务选择界面后才接受启动按键。
4. 校准期间产生的按键中断会被清空，因此不会在校准结束后误启动。
5. KEY1/KEY2在题2～题6间循环选择，OLED显示当前任务；KEY3确认后立即启动。
6. 车辆运行期间选择键被锁定，KEY4和KEY5不具备任何功能。

| 按键 | MCU 引脚 | 功能 |
|---|---|---|
| KEY1 | PC0 | 向左/上一项：题2←题3←题4←题5←题6，循环选择 |
| KEY2 | PC1 | 向右/下一项：题2→题3→题4→题5→题6，循环选择 |
| KEY3 | PC2 | 确认当前OLED显示的任务并立即开始 |
| KEY4 | PC3 | 禁用，按下不会产生功能 |
| KEY5 | PC4 | 禁用，按下不会产生功能 |

| OLED选中任务 | 当前行为 |
|---|---|
| 题2 | 使用已验证的高速循迹、100 Hz yaw 内环、双轮独立 PI、编码器交换修正、A 点预减速精确停车 |
| 题3 | 预留；确认后显示 `NOT IMPLEMENTED`，电机不动作 |
| 题4 | A→B，0.30 m/s 最大速度与 0.30 m/s² 加减速，理论约6秒 |
| 题5 | 使用题2稳定控制器的慢速表，目标约25秒一圈 |
| 题6 | 预留；确认后显示 `NOT IMPLEMENTED`，电机不动作 |

题3、题6以及题4/5的钢球位置闭环需要后续接入 K230 位置数据和舵机控制器。目前题4/5完成的是小车运动与计时部分，不会伪装成已经完成钢球控制。

## 关键速度参数

- 题2：直线 0.52 m/s，弯道 0.40 m/s，接近段 0.34 m/s，精停段 0.12 m/s。
- 题4：最高 0.30 m/s，加/减速度均为 0.30 m/s²；1.5 m 理论梯形时间约 6.0 s。
- 题5：直线 0.28 m/s，弯道 0.235 m/s，接近段 0.18 m/s，精停段 0.10 m/s；理论与启动/精停损耗合计约 24~26 s。

题4和题5的时间是基于赛道尺寸与目标轮速的计算值，必须实车记录一次 OLED/串口时间后再做最后微调。

## 主要文件

- `Core/Src/app/competition_tasks.c`：任务选择、启动/完成、故障处理。
- `Core/Src/app/competition_timer.c`：500 Hz GPIO轮询、20 ms消抖、KEY1/KEY2选择、KEY3确认和计时状态机。
- `Core/Src/app/competition_display.c`：OLED 状态和时间显示。
- `Core/Src/app/playground_track.c`：题2/4/5的小车运动控制。
- `Core/Src/freertos.c`：500 Hz 控制线程和独立的低优先级 OLED 线程。

OLED 整屏 I2C 刷新没有放进 500 Hz 电机控制循环；显示线程以 10 Hz
独立刷新，避免约 1 KB 的阻塞式 I2C 传输破坏循迹控制周期。

KEY1～KEY3不再使用EXTI边沿中断，而是配置为内部上拉的普通输入并由
500 Hz控制线程轮询。UART5在READY时会输出PC0～PC2松开状态的原始电平，
正常应全部为1；每个有效按键也会输出对应的选择/确认日志。

## VS Code / CMake 编译

选择 CMake preset `CompetitionTasks`，然后编译同名 preset。该 preset 固定启用已经通过实车验证的：

- `GITHUB_MODE_SWITCH_PROFILE`
- `FAST_STEERING_TEST_PROFILE`
- `FIFTEEN_SECOND_LAP_PROFILE`
- `HIGH_SPEED_YAW_INNER_TEST_PROFILE`
- `INDEPENDENT_WHEEL_PI_TEST_PROFILE`

生成物位于 `firmware/`，可直接使用 ELF 烧录。

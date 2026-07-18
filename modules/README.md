# 模块驱动

- `MCP23017/`：12 路循迹输入使用的 I2C GPIO 扩展器驱动；与 OLED 共用 I2C0（PA0/PA1），地址 `0x20`。
- `ICM42688/`：ICM42688 六轴 IMU 驱动（SPI），含平台无关核心 HAL、Mahony AHRS 姿态解算与 MSPM0 适配层，附主机端单元测试（`temp/tests/run_tests.ps1`）。
- `Motion Control/`：运动控制执行层，接收上层 `(v, ω)` 速度指令，经平滑/限幅、差速逆运动学与前馈+PI 轮速闭环输出电机 PWM。
- `Sens-Decision/`：传感与决策模块，五层数据流：预处理 → EKF 状态估计 → 感知 → 行为规划（FSM）→ 轨迹生成，输出 `trajectory_point_t {v, omega}` 供 Motion Control 使用；附主机端测试（`temp/build_and_test.ps1`）。
- `oled/`：SSD1306 兼容 128×64 OLED 的 4 线 SPI 驱动（MSPM0 DriverLib），支持字符、汉字、数字、图形与位图显示。
- `motor/`：底盘控制理论与设计文档集（分层架构、前馈+反馈、频域分析等），无源码。
- `servo/`：ZDT X42S 闭环步进电机厂商资料（用户手册 + MODBUS-RTU 协议说明），无源码。

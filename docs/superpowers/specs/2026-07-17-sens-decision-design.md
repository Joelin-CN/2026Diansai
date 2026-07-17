# Sens-Decision 感知决策层设计

## 1. 目标

依据 `modules/Sens-Decision/READEME.md` 完善感知决策层，实现从传感器采集、状态估计、红外感知、行为决策到轨迹生成的完整逻辑链，并使用虚拟原始数据进行主机端测试。

本轮只实现 `modules/Sens-Decision` 内部逻辑和抽象接口，不适配现有 MSPM0 编码器、ICM42688、MCP23017 或运动控制模块，不修改 MCU 工程。未来硬件接入应通过独立适配层完成。

实现遵循 C99，不使用动态内存。所有可见输出必须带有统一模块和级别标识，不允许业务代码直接输出无前缀文本。

## 2. 系统分层

数据按以下顺序流动：

```text
真实硬件适配层（本轮不实现）/ 虚拟 HAL
        ↓
传感器 VTable：init/read/write/release
        ↓
传感器预处理层：统一时间戳、物理量转换、数据聚合
        ↓
状态估计层：编码器 + IMU 完整 EKF
        ↓
红外感知层：偏差、航向误差、道路事件、有效性
        ↓
行为决策层：有限状态机和安全降级
        ↓
轨迹层：路径跟踪、曲率限速、离散 S 曲线速度规划
        ↓
运动控制层（本轮范围外）
```

每一层只依赖前一层公开的数据结构，不直接访问其他层的内部状态。

## 3. 公共约定

### 3.1 坐标系与单位

- 车体中心为 `base_link` 原点。
- `x` 轴指向车头，`z` 轴垂直地面向上，`y` 轴遵循项目现有左手系约定。
- 距离使用米，时间使用秒，线速度使用米每秒，角度使用弧度，角速度使用弧度每秒。
- 传感器安装位置配置统一使用米，替换当前结构中不明确或以毫米表达的字段。
- 时间戳使用外部调度器提供的单调递增整数，单位固定为微秒。

### 3.2 错误码

模块统一使用如下错误语义：

```c
typedef enum {
    SD_OK = 0,
    SD_ERR_INVALID_ARGUMENT = -1,
    SD_ERR_NOT_INITIALIZED = -2,
    SD_ERR_READ = -3,
    SD_ERR_TIMEOUT = -4,
    SD_ERR_HW_FAULT = -5,
    SD_ERR_UNSUPPORTED = -6,
    SD_ERR_DATA_INVALID = -7,
    SD_ERR_NUMERIC = -8
} sd_status_t;
```

所有公开操作返回错误码，结果通过输出指针返回。失败时不得将未初始化或非有限数据传递给后续算法。

### 3.3 日志

业务代码使用统一日志接口，不直接调用裸 `printf`。输出格式固定为：

```text
[Sens-Decision] debug: ...
[Sens-Decision] info: ...
[Sens-Decision] warning: ...
[Sens-Decision] error: ...
```

级别名称使用标准拼写 `warning`。日志宏支持编译期级别裁剪。传感器名称、错误码、时间戳、状态转换等必要上下文应包含在消息中。

## 4. 传感器抽象

### 4.1 VTable

每个传感器对象都具有 `init`、`read`、`write`、`release` 四个操作：

```c
typedef struct sensor sensor_t;

typedef struct {
    sd_status_t (*init)(sensor_t *sensor);
    sd_status_t (*read)(sensor_t *sensor, void *output, uint64_t timestamp_us);
    sd_status_t (*write)(sensor_t *sensor, const void *input);
    sd_status_t (*release)(sensor_t *sensor);
} sensor_vtable_t;

struct sensor {
    const char *name;
    const sensor_vtable_t *vtable;
    void *private_data;
    bool initialized;
};
```

- `init` 从全局配置管理器读取该实例的配置。
- `read` 接收外部统一时间戳，并完成原始值到物理量的基础转换。
- 当前传感器均只读，`write` 必须返回 `SD_ERR_UNSUPPORTED`，不能伪装成功。
- `release` 清理运行状态且可重复调用。
- 所有实例和私有数据静态分配。

### 4.2 HAL 边界

HAL 只负责提供原始值，不包含预处理、感知或决策算法：

```c
typedef struct {
    sd_status_t (*read_encoder_count)(uint8_t index, int32_t *count);
    sd_status_t (*read_imu_raw)(imu_raw_data_t *data);
    sd_status_t (*read_ir_mask)(uint16_t *active_mask);
} sensor_hal_t;
```

测试使用虚拟 HAL。未来真实硬件适配器再将这些回调映射至项目驱动，避免 Sens-Decision 依赖具体 HAL、芯片寄存器或厂商头文件。

### 4.3 静态注册

固定注册以下传感器：

- 左前、左后、右前、右后四个编码器。
- 一个 IMU。
- 一个包含 12 路数字输入的红外阵列。

注册表通过 `sensor_id_t` 索引。批量初始化失败时，按逆序释放此前成功初始化的实例。初始化前读取返回 `SD_ERR_NOT_INITIALIZED`。

### 4.4 输出数据

编码器输出当前轮速和用于诊断的原始累计脉冲数。轮速根据相邻时间戳之间的脉冲差、每圈脉冲数和轮半径计算，不能依赖固定调用频率。

IMU输出去零偏并经过基础一阶低通滤波的三轴加速度、三轴角速度和温度。原始量程系数、零偏和滤波系数来自配置管理器。

红外阵列按现有数字硬件语义输出：

```c
typedef struct {
    uint16_t active_mask;
    float values[12];
} ir_array_data_t;
```

`active_mask` 仅低 12 位有效，`values[i]` 只能为 `0.0f` 或 `1.0f`。本轮不设计 ADC 模拟红外采集。

## 5. 配置管理

全局配置管理器静态保存以下配置：

- 四个编码器的轮半径、每圈脉冲数、计数方向和安装位置。
- 车辆轮距以及四轮到左右侧的映射。
- IMU量程换算、零偏、滤波系数和安装位置。
- 12路红外通道有效电平、位置权重和安装位置。
- EKF初始协方差、过程噪声 `Q`、观测噪声 `R`、最小和最大 `dt`。
- 感知层路口、弯道、丢线和滤波阈值。
- FSM恢复帧数、持续丢线阈值、故障阈值和各状态限速。
- 轨迹前视距离、曲率限速、最大速度、最大加速度、最大减速度和最大加加速度。

提供默认配置初始化函数和配置合法性校验函数。算法中不散落可调魔法数字。

## 6. 预处理与数据聚合

调度器通过统一入口采集一帧数据：

```c
sd_status_t preprocess_update(uint64_t timestamp_us, sensor_frame_t *frame);
```

`sensor_frame_t` 包含四路轮速、IMU物理量、红外阵列和本帧时间戳。一次调用向所有传感器传递同一个时间戳。

若某个传感器失败，帧中为各子数据维护有效标志，成功读取的数据仍可用于受控降级；函数同时返回本轮最严重错误。失败数据不会沿用为本轮有效数据。

## 7. 状态估计

### 7.1 运动模型

车辆采用四轮差速模型：

```text
v_left  = (v_left_front + v_left_rear) / 2
v_right = (v_right_front + v_right_rear) / 2
v_encoder     = (v_right + v_left) / 2
omega_encoder = (v_right - v_left) / wheel_track
```

EKF状态向量为：

```text
X = [x, y, theta, v, omega]^T
```

非线性预测模型为：

```text
x'     = x + v * cos(theta) * dt
y'     = y + v * sin(theta) * dt
theta' = normalize(theta + omega * dt)
v'     = v
omega' = omega
```

### 7.2 观测与更新

观测向量为：

```text
Z = [v_encoder, omega_encoder, gyro_z]^T
```

实现完整 EKF：

- 使用运动模型雅可比 `F` 预测状态和 `5 x 5` 协方差 `P`。
- 使用观测矩阵 `H`、观测噪声 `R` 计算创新和卡尔曼增益。
- 使用 Joseph 形式更新 `P`，降低浮点误差导致协方差失去半正定性的风险。
- 每次预测和更新后归一化 `theta`。
- 首帧仅建立时间基准，不执行位姿积分。
- 时间戳倒退、`dt` 超限、非有限输入、不可逆创新协方差均返回明确错误。

输出 `VehicleState`，包含 `(x, y, theta, v, omega)`、`P`、时间戳和 `localization_valid`。连续有效更新后才置位定位有效标志；连续失败超过配置阈值后清除。

## 8. 红外感知

12路默认对称权重为：

```text
[-11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11]
```

感知输出包括：

- 归一化横向偏差。
- 基于连续帧横向偏差变化率并经低通滤波得到的航向误差估计。
- 原始有效掩码。
- 连续丢线计数。
- `line_valid`。
- 道路事件：无事件、弯道入口、路口、断线。

判定规则：

- 有效通道权重平均值形成横向偏差。
- 没有有效通道时增加丢线计数并产生断线事件。
- 有效通道数达到配置阈值时产生路口事件。
- 横向偏差绝对值和变化率达到配置阈值时产生弯道入口事件。
- 重新找到线后清零丢线计数；事件只描述当前帧，不作为永久锁存状态。

## 9. 行为决策

FSM状态如下：

```text
IDLE
LINE_FOLLOW
APPROACH_CURVE
CURVE
LINE_LOST_DEGRADED
STOPPED
FAULT
```

关键转换规则：

```text
IDLE -> LINE_FOLLOW                 启动命令且定位和线路有效
LINE_FOLLOW -> APPROACH_CURVE       检测到弯道入口
APPROACH_CURVE -> CURVE             航向误差或路径曲率达到阈值
CURVE -> LINE_FOLLOW                弯道条件消失并稳定若干帧
任意运行态 -> LINE_LOST_DEGRADED   短时丢线
LINE_LOST_DEGRADED -> 原运行态      恢复窗口内重新找到线
LINE_LOST_DEGRADED -> STOPPED       持续丢线超过阈值
任意运行态 -> FAULT                关键输入或 EKF 连续失败
任意状态 -> STOPPED                收到停车命令
STOPPED -> IDLE                     收到复位命令且故障已清除
FAULT -> IDLE                       显式复位且输入恢复正常
```

进入丢线降级状态时保存原运行状态和最后有效方向，逐步降低速度。所有状态变化输出一条规范 `info` 或 `warning` 日志；重复保持同一状态不重复刷屏。

## 10. 轨迹生成

调用方提供预存路径点序列：

```c
typedef struct {
    float x;
    float y;
    float heading;
    float curvature;
} path_point_t;
```

轨迹层执行：

- 在上次索引附近查找当前位置的最近路径点，避免每周期全路径搜索。
- 根据前视距离选择目标点。
- 根据局部曲率限制最高速度。
- 根据FSM状态进一步限制或归零目标速度。
- 通过离散速度、加速度和加加速度约束实现平滑 S 曲线速度变化。
- 根据目标速度和曲率计算角速度，并输出位置、航向、速度、角速度、加速度、角加速度和曲率。

`LINE_LOST_DEGRADED` 沿最后有效目标方向降速。`STOPPED` 和 `FAULT` 始终输出零速度安全轨迹。空路径、路径点非有限或索引无效均返回错误，不产生未定义目标。

## 11. 文件边界

计划保持现有模块划分并补充必要文件：

- `inc/config.h`, `src/config.c`：公共错误码和全局配置。
- `inc/interface.h`, `src/interface.c`：传感器 HAL、VTable、静态实例和生命周期。
- `inc/preprocess.h`, `src/preprocess.c`：物理量转换和统一帧采集。
- `inc/EKF.h`, `src/EKF.c`：矩阵运算和完整 EKF。
- `inc/state_evaluate.h`, `src/state_evaluate.c`：差速观测构造、EKF调度和 `VehicleState`。
- `inc/perception.h`, `src/perception.c`：数字红外感知。
- `inc/behavior_planner.h`, `src/behavior_planner.c`：FSM。
- `inc/trajectory_generate.h`, `src/trajectory_generate.c`：路径跟踪和速度规划。
- `inc/utils.h`, `src/utils.c`：日志、角度归一化和有限值等通用工具。
- `temp/test_main.c`：虚拟 HAL、测试场景和测试入口。
- `temp/`：测试源码与构建脚本、中间文件、测试可执行文件和运行时临时输出。

各头文件保护宏使用 `SENS_DECISION_<FILE>_H` 前缀，避免与其他模块冲突。现有打印接口声明和实现不一致、空结构体等问题在实现时一并修正，但不进行与本模块无关的重构。

`temp/` 不存放正式模块源码或公共头文件，只存放测试源码、测试构建脚本及可再生成文件。测试构建脚本从 `inc/` 和 `src/` 读取正式源码，并将目标文件、依赖文件、测试程序及测试日志全部写入 `temp/`，避免污染模块根目录。需要由 Git 保留的测试源码和脚本可提交；可再生成的编译产物和日志通过 `temp/.gitignore` 排除。

## 12. 测试策略

测试必须从虚拟原始数据进入 HAL，再经过 VTable 和全部业务层，不允许直接向中间算法注入已经处理好的最终结果来代替端到端验证。

至少覆盖：

1. 传感器初始化、初始化前读取、重复初始化、只读 `write`、重复释放和初始化失败回滚。
2. 编码器脉冲到轮速的时间戳换算、计数方向和异常时间戳。
3. IMU换算、去零偏和滤波。
4. 红外掩码到12路二值数组的映射。
5. 直线匀速、加减速、转弯和原地转向的EKF估计。
6. EKF时间戳倒退、过大 `dt`、无效数值和创新矩阵异常。
7. 居中、左右偏线、弯道入口、路口和断线感知。
8. 短时丢线恢复、持续丢线停车、关键故障进入 `FAULT` 和显式复位。
9. 直线路径、圆弧路径、曲率限速和S曲线速度约束。
10. 从虚拟原始数据到最终轨迹的完整数据链。
11. HAL主动返回错误时的错误传播和安全输出。
12. 日志前缀与级别格式，不保留无前缀业务输出。

浮点结果按合理容差比较，不要求逐位相等。测试程序最后输出：

```text
[Sens-Decision] info: test summary: passed=N, failed=0
```

## 13. 验收标准

- 主机端能以 C99 编译整个 Sens-Decision 模块和测试程序。
- 测试脚本和所有可再生成的构建中间文件、测试程序及运行时临时输出均位于 `modules/Sens-Decision/temp/`。
- 所有虚拟数据测试通过，失败时进程返回非零状态。
- 完整EKF维护并更新五维状态和 `5 x 5` 协方差。
- 数字红外输入不会被错误描述为 ADC 连续强度。
- FSM能完成启动、巡线、弯道、短时丢线恢复、持续丢线停车和故障保护。
- 轨迹生成满足速度、加速度和加加速度限制，并在停车或故障状态输出安全零速度。
- 所有传感器通过同一 VTable 约定操作，且不使用动态内存。
- 模块不直接依赖当前仓库中的硬件驱动；真实适配留待后续任务。
- 所有运行输出符合 `[Sens-Decision] <level>: <message>` 格式。

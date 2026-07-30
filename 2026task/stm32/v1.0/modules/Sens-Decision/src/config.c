#include "config.h"

#include <math.h>
#include <stddef.h>

sens_decision_config_t g_sens_decision_config;

static bool position_is_finite(const sd_position_t *position) {
    return isfinite(position->x_m) && isfinite(position->y_m) &&
           isfinite(position->z_m);
}

static bool positive_finite(float value) {
    return isfinite(value) && value > 0.0f;
}

void sd_config_reset_defaults(void) {
    /**
     * IR传感器横向位置权重数组
     *
     * @category B: 理论计算参数（基于物理测量）
     *
     * @value [3.9861, 2.8472, 1.7083, 0.5694, -0.5694, -1.7083, -2.8472, -3.9861]
     *
     * @origin 基于传感器物理安装位置计算（2026-07-30更新）
     *   坐标系说明（代码坐标系）:
     *   - 前 = +X 方向
     *   - 左 = +Y 方向
     *   - 上 = +Z 方向
     *
     *   物理测量（传感器阵列横向分布，代码坐标系）:
     *   - 通道0: Y = +39.8606 mm（最左侧）
     *   - 通道1: Y = +28.4719 mm
     *   - 通道2: Y = +17.0831 mm
     *   - 通道3: Y = +5.6944 mm
     *   - 通道4: Y = -5.6944 mm
     *   - 通道5: Y = -17.0831 mm
     *   - 通道6: Y = -28.4719 mm
     *   - 通道7: Y = -39.8606 mm（最右侧）
     *
     *   传感器间距: 11.3887 mm
     *   阵列中心X: 183 mm
     *
     *   权重计算:
     *   - weight[i] = Y[i] / 10mm（归一化）
     *   - 左侧（+Y）为正值，右侧（-Y）为负值
     *   - 越靠外侧，绝对值越大
     *
     * @validation 符号验证方法
     *   使用ir_sensor_calibration.c工具:
     *   1. 将小车放在黑线上
     *   2. 手动向右移动 → lateral_error应为正值（线在左侧传感器）
     *   3. 手动向左移动 → lateral_error应为负值（线在右侧传感器）
     *   4. 如果符号相反，需要将所有权重取反
     *
     * @tuning_guide
     *   何时需要调整:
     *   - 更换IR传感器阵列
     *   - 传感器阵列位置改变
     *   - lateral_error符号验证失败
     *
     *   调整方法:
     *   - 重新测量传感器物理位置
     *   - 重新计算权重
     *   - 如果只是符号错误，将所有值取反
     *
     * @history
     *   - 2026-07-30: 更新传感器位置（间距11.39→11.3887mm，中心132.1→183mm）
     *
     * @references
     *   - docs/CALIBRATION_QUICK_GUIDE.md - IR传感器符号验证
     *   - docs/GEOMETRY_UPDATE_2026-07-30.md - 几何参数更新记录
     *
     * @warnings
     *   - 权重符号错误会导致循迹反向（越跑越偏）
     *   - 必须通过实车验证符号正确性
     */
    static const float ir_weights[SD_IR_CHANNEL_COUNT] = {
        3.9861f, 2.8472f, 1.7083f, 0.5694f, -0.5694f, -1.7083f, -2.8472f, -3.9861f
    };
    /**
     * 编码器方向配置
     *
     * @category A: 物理测量参数（需要实测验证）
     *
     * @value {1, -1} (左轮正向，右轮反向)
     *
     * @origin 基于电机安装方向
     *   - +1: 电机正转时编码器计数增加
     *   - -1: 电机正转时编码器计数减少
     *   - 差速底盘左右电机通常镜像安装，方向相反
     *
     * @validation 使用motor_direction_calibration.c验证
     *   测试方法:
     *   1. 给左电机正向PWM，观察编码器变化
     *   2. 给右电机正向PWM，观察编码器变化
     *   3. 预期: 正转时编码器应增加（考虑方向系数后）
     *
     *   如果验证失败:
     *   - 选项1: 交换电机接线（硬件修改，推荐）
     *   - 选项2: 修改此direction值（软件修改）
     *
     * @warnings
     *   - 方向配置错误会导致差速计算错误
     *   - 小车可能无法直线行驶或转向异常
     *   - 必须通过实车验证
     */
    static const int8_t encoder_directions[SD_ENCODER_COUNT] = {1, -1};
    static const float encoder_x[SD_ENCODER_COUNT] = {0.0935f, 0.0935f};
    static const float encoder_y[SD_ENCODER_COUNT] = {0.107f, -0.107f};
    size_t index;

    /**
     * 车辆轮距配置
     *
     * @category A: 物理测量参数
     *
     * @value 0.214f m (214 mm)
     *
     * @origin 实际测量
     *   - 测量方法: 卷尺测量左右轮中心距离
     *   - 测量日期: 2026-07-30
     *   - 之前配置: 115mm（已更新）
     *
     * @validation 原地旋转验证法
     *   理论依据: 原地旋转时，左右轮差速 ΔS = wheel_track × θ
     *   验证步骤:
     *   1. 让小车原地旋转N圈（例如10圈）
     *   2. 记录左右轮编码器计数差: Δcount
     *   3. 计算实际角度: θ = Δcount / ENCODER_PPR × 2π
     *   4. 反推轮距: wheel_track = ΔS / θ
     *   5. 验证偏差是否 < 5%
     *
     * @history
     *   - 2026-07-30: 115mm → 214mm (实测更新)
     *
     * @warnings
     *   - 必须与motion_config.h的WHEEL_BASE保持一致
     *   - 轮距误差会导致转向角度误差
     */
    g_sens_decision_config.vehicle.wheel_track_m = 0.214f;

    /**
     * 双轮差速底盘编码器索引配置（2026-07-30迁移）
     *
     * @category 双轮配置（从四轮迁移）
     *
     * @config 当前配置:
     *   - 左轮: 编码器0 (TIM3)
     *   - 右轮: 编码器1 (TIM4)
     *   - 索引[1]: 设为INVALID_ENCODER_INDEX（未使用）
     *
     * @note 数组保留2个元素是为了兼容可能的前后轮配置
     *       双轮配置下，每侧只使用索引[0]，索引[1]标记为无效
     *
     * @validation 验证逻辑（config.c:sd_config_validate）
     *   - 检查每个有效编码器索引只被引用1次
     *   - INVALID_ENCODER_INDEX不参与重复检查
     *
     * @hardware 物理编码器映射:
     *   - 编码器0 → 左轮电机（TIM3，4倍频）
     *   - 编码器1 → 右轮电机（TIM4，4倍频）
     */
    g_sens_decision_config.vehicle.left_encoder_indices[0] = 0U;  /* 左轮使用编码器0 */
    g_sens_decision_config.vehicle.left_encoder_indices[1] = INVALID_ENCODER_INDEX;  /* 双轮无第二编码器 */
    g_sens_decision_config.vehicle.right_encoder_indices[0] = 1U;  /* 右轮使用编码器1 */
    g_sens_decision_config.vehicle.right_encoder_indices[1] = INVALID_ENCODER_INDEX;  /* 双轮无第二编码器 */

    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        sd_encoder_config_t *encoder = &g_sens_decision_config.encoders[index];
        /**
         * 轮半径配置
         *
         * @category A: 物理测量参数
         *
         * @value 0.033f m (33 mm)
         *
         * @origin 游标卡尺测量
         *
         * @validation 滚动距离验证法（推荐）
         *   标定步骤:
         *   1. 让轮子滚动10圈
         *   2. 测量实际位移 S (m)
         *   3. 计算: 周长 C = S / 10
         *   4. 反推: R = C / (2π)
         *   5. 验证偏差 < 2%
         *
         * @warnings
         *   - 必须与motion_config.h的WHEEL_RADIUS保持一致
         *   - 轮半径误差直接导致速度和位移估计误差
         */
        encoder->wheel_radius_m = 0.033f;
        /**
         * 编码器分辨率（每转计数）
         *
         * @category B: 理论计算参数（已实测验证）
         *
         * @value 60000 counts/revolution
         *
         * @origin 理论计算 + 实测验证
         *   计算依据:
         *   - 电机编码器线数: 500 PPR（厂商规格书）
         *   - 4倍频（AB相正交解码）: 500 × 4 = 2,000 counts/电机转
         *   - 减速比: 30:1（齿轮箱规格书）
         *   - 轮子转1圈: 2,000 × 30 = 60,000 counts
         *
         * @validation 实测验证 (2026-07-30)
         *   测试条件: 10% PWM, 3秒运行
         *   测试结果:
         *   - 左轮: 128,950 counts / 2.2圈 = 58,614 counts/圈
         *   - 偏差: 2.3% (合格)
         *
         * @references
         *   - logs/2026-07-30_encoder_ppr_correction.md - 详细验证过程
         *
         * @warnings
         *   - 必须与motion_config.h的ENCODER_PPR保持一致
         *   - 这是最关键的参数之一
         */
        encoder->pulses_per_revolution = 60000U;
        encoder->direction = encoder_directions[index];
        encoder->position.x_m = encoder_x[index];
        encoder->position.y_m = encoder_y[index];
        encoder->position.z_m = 0.0f;
    }

    /**
     * IMU加速度计刻度因子
     *
     * @category B: 理论计算参数（基于数据手册）
     *
     * @value 9.80665 / 2048.0 ≈ 0.00478 m/s² per LSB
     *
     * @origin MPU6050数据手册
     *   - 量程配置: ±16g
     *   - ADC分辨率: 16位有符号整数 (-32768 ~ +32767)
     *   - 灵敏度: 2048 LSB/g (±16g量程)
     *   - 转换: 1 LSB = (1/2048) g = (9.80665/2048) m/s²
     *
     * @validation
     *   - 静止时Z轴加速度应约为 9.8 m/s² (重力加速度)
     *   - XY轴应接近0
     *
     * @warnings
     *   - 更改IMU量程配置后必须更新此值
     *   - 不同量程对应不同的灵敏度:
     *     * ±2g: 16384 LSB/g
     *     * ±4g: 8192 LSB/g
     *     * ±8g: 4096 LSB/g
     *     * ±16g: 2048 LSB/g (当前)
     */
    g_sens_decision_config.imu.accel_scale_mps2_per_lsb = 9.80665f / 2048.0f;

    /**
     * IMU陀螺仪刻度因子
     *
     * @category B: 理论计算参数（基于数据手册）
     *
     * @value 0.017453292519943295 / 16.4 ≈ 0.00106 rad/s per LSB
     *
     * @origin MPU6050数据手册
     *   - 量程配置: ±2000°/s
     *   - ADC分辨率: 16位有符号整数
     *   - 灵敏度: 16.4 LSB/(°/s) (±2000°/s量程)
     *   - 转换: 1 LSB = (1/16.4) °/s = (π/180/16.4) rad/s
     *
     * @validation
     *   - 静止时三轴角速度应接近0
     *   - 原地旋转测试与编码器差速计算的角速度对比
     *
     * @warnings
     *   - 更改IMU量程配置后必须更新此值
     *   - 不同量程对应不同的灵敏度:
     *     * ±250°/s: 131 LSB/(°/s)
     *     * ±500°/s: 65.5 LSB/(°/s)
     *     * ±1000°/s: 32.8 LSB/(°/s)
     *     * ±2000°/s: 16.4 LSB/(°/s) (当前)
     */
    g_sens_decision_config.imu.gyro_scale_radps_per_lsb =
        0.017453292519943295f / 16.4f;
    for (index = 0U; index < 3U; ++index) {
        g_sens_decision_config.imu.accel_bias_mps2[index] = 0.0f;
        g_sens_decision_config.imu.gyro_bias_radps[index] = 0.0f;
    }
    g_sens_decision_config.imu.filter_alpha = 0.25f;
    g_sens_decision_config.imu.position.x_m = 0.0f;
    g_sens_decision_config.imu.position.y_m = 0.0f;
    g_sens_decision_config.imu.position.z_m = 0.03f;

    g_sens_decision_config.perception.active_high = true;
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        g_sens_decision_config.perception.weights[index] = ir_weights[index];
    }
    /**
     * IR传感器阵列安装位置
     *
     * @category A: 物理测量参数
     *
     * @value (0.183, 0.0, -0.02) m
     *
     * @origin 实际测量（2026-07-30更新）
     *   坐标系（代码坐标系）:
     *   - X轴: 前方为正，后方为负
     *   - Y轴: 左侧为正，右侧为负
     *   - Z轴: 上方为正，下方为负
     *
     *   测量值:
     *   - X = 0.183 m (183 mm): 阵列位于车头前方183mm
     *   - Y = 0.0 m: 阵列横向居中于车辆中心线
     *   - Z = -0.02 m (-20 mm): 阵列低于车辆坐标系原点20mm
     *
     * @history
     *   - 2026-07-30: 132.1mm → 183mm (向前移动50.9mm)
     *
     * @validation
     *   - 使用卷尺测量IR阵列中心到车轮轴中心的距离
     *   - 验证Y坐标是否居中（左右对称）
     *
     * @tuning_guide
     *   影响:
     *   - X坐标影响前瞻距离和转向预判
     *   - Y坐标偏移会导致左右偏向
     *   - Z坐标影响传感器到地面距离
     *
     * @warnings
     *   - 更改传感器安装位置后必须重新测量
     */
    g_sens_decision_config.perception.position.x_m = 0.183f;
    g_sens_decision_config.perception.position.y_m = 0.0f;
    g_sens_decision_config.perception.position.z_m = -0.02f;
    g_sens_decision_config.perception.heading_filter_alpha = 0.3f;

    /**
     * 白色背景参考值（黑线检测算法核心参数）
     *
     * @category A: 物理测量参数（需要校准）
     *
     * @value 270.0f (初始估计值，基于典型白色背景ADC读数)
     *
     * @origin 实测数据（白色背景约250-270）
     *   - 测量方法: 将传感器置于白色背景上，记录各通道ADC值
     *   - 这些值代表"没有黑线"时的基准反射率
     *
     * @calibration 白平衡校准（强烈推荐）
     *   使用ir_calibration工具:
     *   1. 将小车放在纯白色表面
     *   2. 调用 ir_calibrate_white_balance()
     *   3. 自动采样100次并更新此数组
     *
     * @algorithm 黑线强度反转算法:
     *   black_strength[i] = white_reference[i] - current_value[i]
     *   - 白色区域: black_strength ≈ 0 (270 - 270 = 0)
     *   - 黑线区域: black_strength ≈ 170 (270 - 100 = 170)
     *
     * @warnings
     *   - 光照条件变化时需要重新校准
     *   - 传感器老化或更换后需要重新校准
     *   - 不同传感器通道可能有差异（个体差异）
     */
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        g_sens_decision_config.perception.white_reference[index] = 1136.0f;  // 2026-07-30校准：反向传感器
    }

    /**
     * 黑线强度阈值（检测灵敏度）
     *
     * @category C: 经验调参（需要实验验证）
     *
     * @value 50.0f (初始保守值)
     *
     * @origin 理论估算
     *   - 白色背景: ~270
     *   - 黑线: ~100
     *   - 差值: ~170
     *   - 阈值设为差值的30%: 170 * 0.3 ≈ 50
     *
     * @calibration 自动校准（推荐）
     *   使用ir_calibration工具:
     *   1. 将小车居中对齐黑线
     *   2. 调用 ir_calibrate_black_threshold()
     *   3. 自动设置为最大黑线强度的50%
     *
     * @tuning_guide
     *   如何判断阈值是否合适:
     *   - 阈值过低: 白色也被误判为黑线（噪声敏感）
     *   - 阈值过高: 黑线检测不到（灵敏度不足）
     *
     *   建议调整范围: 30.0 ~ 100.0
     *   - 强光环境: 可适当提高（抗干扰）
     *   - 弱光环境: 可适当降低（提高灵敏度）
     *
     * @warnings
     *   - 必须在校准白色参考值后再校准此阈值
     *   - 不同赛道表面反射率不同，可能需要微调
     */
    g_sens_decision_config.perception.black_strength_threshold = 427.0f;  // 2026-07-30校准：反向传感器 50%

    /**
     * EKF初始协方差对角线元素
     *
     * @category C: 经验调参（需要实验）
     *
     * @value 0.1f (所有5个状态)
     *
     * @origin 经验初值
     *   - 表示对初始状态估计的不确定度
     *   - 5个状态: [x, y, θ, v, ω]
     *   - 较大的初始协方差允许滤波器快速调整
     *
     * @tuning_guide
     *   物理意义:
     *   - 初始协方差越大，滤波器初期越信任观测值
     *   - 初始协方差越小，滤波器初期越信任模型预测
     *
     *   建议范围: 0.01 ~ 1.0
     *
     *   如何调整:
     *   - 如果初始状态已知较准确，可减小此值
     *   - 如果初始状态不确定，可增大此值
     *   - 通常启动后几秒会快速收敛，初值影响不大
     *
     * @warnings
     *   - 过小可能导致滤波器收敛慢
     *   - 过大可能导致初期估计波动大
     */
    for (index = 0U; index < SD_EKF_STATE_COUNT; ++index) {
        g_sens_decision_config.ekf.initial_covariance_diag[index] = 0.1f;
        /**
         * EKF过程噪声对角线元素
         *
         * @category C: 经验调参（需要实验，关键参数）
         *
         * @value 0.01f (所有5个状态)
         *
         * @origin 经验初值（待实车调优）
         *   - 表示模型预测的不确定度
         *   - 5个状态: [x, y, θ, v, ω]
         *   - 反映运动模型与实际的偏差
         *
         * @tuning_guide
         *   物理意义:
         *   - 过程噪声越大，滤波器越信任观测值
         *   - 过程噪声越小，滤波器越信任模型预测
         *
         *   建议范围: 0.001 ~ 0.1
         *
         *   如何判断需要调整:
         *   1. 观察状态估计曲线
         *   2. 如果估计值对观测响应太慢 → 增大过程噪声
         *   3. 如果估计值跟随观测波动太大 → 减小过程噪声
         *   4. 最佳状态: 平滑跟踪，无明显延迟
         *
         *   分状态调整（高级）:
         *   - position (x,y): 0.01 ~ 0.05
         *   - heading (θ): 0.005 ~ 0.02
         *   - velocity (v): 0.01 ~ 0.05
         *   - angular velocity (ω): 0.01 ~ 0.05
         *
         * @validation
         *   - 运行小车，记录状态估计曲线
         *   - 检查是否平滑且无延迟
         *   - 对比编码器原始数据和滤波后结果
         *
         * @warnings
         *   - 这是EKF最重要的调参参数之一
         *   - 需要根据实际运行情况迭代调整
         */
        g_sens_decision_config.ekf.process_noise_diag[index] = 0.01f;
    }

    /**
     * EKF观测噪声配置（2个观测量）
     *
     * @category C: 经验调参（需要实验）
     *
     * @value [0.03, 0.08] (m/s)² 和 (rad/s)²
     *
     * @origin 基于编码器特性估计
     *   观测量:
     *   - observation[0]: 线速度 v (从编码器计算)
     *   - observation[1]: 角速度 ω (从编码器差速计算)
     *
     *   噪声来源:
     *   - 编码器量化误差
     *   - 轮胎打滑
     *   - 地面不平
     *   - 差速计算会放大误差（ω噪声 > v噪声）
     *
     * @tuning_guide
     *   物理意义:
     *   - 观测噪声越大，滤波器越不信任观测值
     *   - 观测噪声越小，滤波器越信任观测值
     *
     *   典型范围:
     *   - 线速度v噪声: 0.02 ~ 0.05 m/s
     *   - 角速度ω噪声: 0.05 ~ 0.10 rad/s
     *
     *   如何判断需要调整:
     *   1. 记录编码器速度测量值（多次采样）
     *   2. 计算标准差，即为观测噪声的估计
     *   3. ω的噪声通常是v的2-3倍（差速放大）
     *
     *   当前配置说明:
     *   - v噪声 = 0.03 m/s (约3%误差 @ 1m/s)
     *   - ω噪声 = 0.08 rad/s (约2.7倍于v)
     *
     * @validation
     *   - 让小车匀速直线行驶
     *   - 记录速度观测值的标准差
     *   - 与配置值对比，偏差应 < 50%
     *
     * @history
     *   - 2026-07-30: 从3维观测改为2维观测（移除IMU航向）
     *
     * @warnings
     *   - 观测维度必须与SD_EKF_OBSERVATION_COUNT一致（当前=2）
     *   - 噪声值设置不当会导致滤波效果差
     */
    g_sens_decision_config.ekf.observation_noise_diag[0] = 0.03f;  // v noise (m/s)²
    g_sens_decision_config.ekf.observation_noise_diag[1] = 0.08f;  // ω noise (rad/s)²
    g_sens_decision_config.ekf.dt_min_s = 0.0001f;
    g_sens_decision_config.ekf.dt_max_s = 0.1f;

    g_sens_decision_config.behavior.localization_valid_frames = 3U;
    g_sens_decision_config.behavior.localization_failure_frames = 3U;
    g_sens_decision_config.behavior.line_recovery_frames = 3U;
    g_sens_decision_config.behavior.line_lost_stop_frames = 20U;
    g_sens_decision_config.behavior.critical_failure_frames = 5U;
    g_sens_decision_config.behavior.idle_speed_mps = 0.0f;
    g_sens_decision_config.behavior.line_speed_mps = 1.0f;
    g_sens_decision_config.behavior.degraded_speed_mps = 0.25f;

    /**
     * 基于横向偏差的速度调节增益
     * speed = line_speed * (1 - gain * |lateral_error|), 最小40%
     * 默认值 0.3: lateral_error=1.0 时速度降至70%, lateral_error=2.0 时速度降至40%
     */
    g_sens_decision_config.behavior.speed_error_gain = 0.3f;

    g_sens_decision_config.trajectory.lookahead_distance_m = 0.25f;
    g_sens_decision_config.trajectory.curvature_speed_gain = 1.0f;
    g_sens_decision_config.trajectory.max_speed_mps = 1.2f;
    g_sens_decision_config.trajectory.max_accel_mps2 = 1.5f;
    g_sens_decision_config.trajectory.max_decel_mps2 = 2.0f;
    g_sens_decision_config.trajectory.max_jerk_mps3 = 5.0f;
    g_sens_decision_config.trajectory.forward_search_points = 32U;
}

sd_status_t sd_config_validate(const sens_decision_config_t *config) {
    uint8_t encoder_index_counts[SD_ENCODER_COUNT] = {0U, 0U};
    size_t index;

    if (config == NULL || !positive_finite(config->vehicle.wheel_track_m)) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    /**
     * 编码器索引验证（支持双轮配置）
     *
     * @note 双轮差速底盘特殊处理:
     *       - INVALID_ENCODER_INDEX (0xFF) 表示该位置未使用
     *       - 只统计有效编码器索引的引用次数
     *       - 每个有效编码器必须被引用恰好1次
     *
     * @validation 检查项:
     *   1. 索引值必须 < SD_ENCODER_COUNT 或等于 INVALID_ENCODER_INDEX
     *   2. 每个有效编码器索引被引用次数 == 1（不重复、不遗漏）
     */
    for (index = 0U; index < 2U; ++index) {
        uint8_t left_idx = config->vehicle.left_encoder_indices[index];
        uint8_t right_idx = config->vehicle.right_encoder_indices[index];

        /* 检查左侧编码器索引合法性 */
        if (left_idx != INVALID_ENCODER_INDEX) {
            if (left_idx >= SD_ENCODER_COUNT) {
                return SD_ERR_INVALID_ARGUMENT;
            }
            ++encoder_index_counts[left_idx];
        }

        /* 检查右侧编码器索引合法性 */
        if (right_idx != INVALID_ENCODER_INDEX) {
            if (right_idx >= SD_ENCODER_COUNT) {
                return SD_ERR_INVALID_ARGUMENT;
            }
            ++encoder_index_counts[right_idx];
        }
    }

    /* 验证每个编码器被引用恰好1次（双轮配置：2个编码器各用1次） */
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        if (encoder_index_counts[index] != 1U) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }

    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        const sd_encoder_config_t *encoder = &config->encoders[index];
        if (!positive_finite(encoder->wheel_radius_m) ||
            encoder->pulses_per_revolution == 0U ||
            (encoder->direction != -1 && encoder->direction != 1) ||
            !position_is_finite(&encoder->position)) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!positive_finite(config->imu.accel_scale_mps2_per_lsb) ||
        !positive_finite(config->imu.gyro_scale_radps_per_lsb) ||
        !isfinite(config->imu.filter_alpha) || config->imu.filter_alpha < 0.0f ||
        config->imu.filter_alpha > 1.0f ||
        !position_is_finite(&config->imu.position)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 3U; ++index) {
        if (!isfinite(config->imu.accel_bias_mps2[index]) ||
            !isfinite(config->imu.gyro_bias_radps[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!position_is_finite(&config->perception.position) ||
        !isfinite(config->perception.heading_filter_alpha) ||
        config->perception.heading_filter_alpha < 0.0f ||
        config->perception.heading_filter_alpha > 1.0f ||
        !positive_finite(config->perception.black_strength_threshold)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        if (!isfinite(config->perception.weights[index]) ||
            !positive_finite(config->perception.white_reference[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!positive_finite(config->ekf.dt_min_s) ||
        !positive_finite(config->ekf.dt_max_s) ||
        config->ekf.dt_min_s >= config->ekf.dt_max_s) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < SD_EKF_STATE_COUNT; ++index) {
        if (!positive_finite(config->ekf.initial_covariance_diag[index]) ||
            !positive_finite(config->ekf.process_noise_diag[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < SD_EKF_OBSERVATION_COUNT; ++index) {
        if (!positive_finite(config->ekf.observation_noise_diag[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (config->behavior.localization_valid_frames == 0U ||
        config->behavior.localization_failure_frames == 0U ||
        config->behavior.line_recovery_frames == 0U ||
        config->behavior.line_lost_stop_frames == 0U ||
        config->behavior.critical_failure_frames == 0U ||
        !isfinite(config->behavior.idle_speed_mps) ||
        config->behavior.idle_speed_mps < 0.0f ||
        !positive_finite(config->behavior.line_speed_mps) ||
        !positive_finite(config->behavior.degraded_speed_mps) ||
        !isfinite(config->behavior.speed_error_gain) ||
        config->behavior.speed_error_gain < 0.0f) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (!positive_finite(config->trajectory.lookahead_distance_m) ||
        !positive_finite(config->trajectory.curvature_speed_gain) ||
        !positive_finite(config->trajectory.max_speed_mps) ||
        !positive_finite(config->trajectory.max_accel_mps2) ||
        !positive_finite(config->trajectory.max_decel_mps2) ||
        !positive_finite(config->trajectory.max_jerk_mps3) ||
        config->trajectory.forward_search_points == 0U) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    return SD_OK;
}

/**
 * @file parameter_verification.h
 * @brief 物理参数验证测试套件
 * @date 2026-07-30
 * 
 * 用于验证所有物理参数配置是否正确：
 * 1. 编码器分辨率和方向
 * 2. IR传感器符号和权重
 * 3. 轮半径标定
 * 4. 坐标系一致性检查
 */

#ifndef PARAMETER_VERIFICATION_H
#define PARAMETER_VERIFICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 测试1：编码器分辨率验证
 * 
 * 手动转动轮子1圈，验证编码器计数是否为1560
 * 
 * 步骤：
 * 1. 调用此函数
 * 2. 按提示手动转动左轮1圈
 * 3. 记录计数值
 * 4. 按提示手动转动右轮1圈
 * 5. 记录计数值
 * 6. 对比预期值1560
 * 
 * 预期结果：
 * - 左轮：1560 ± 20 counts
 * - 右轮：-1560 ± 20 counts（方向相反）
 */
void ParamVerify_EncoderResolution(void);

/**
 * @brief 测试2：编码器方向验证
 * 
 * 施加正向PWM，验证编码器计数方向
 * 
 * 步骤：
 * 1. 调用此函数
 * 2. 电机会以低PWM正向转动2秒
 * 3. 观察打印的计数变化
 * 
 * 预期结果：
 * - 左右轮计数都应该增加（正值）
 * - 如果某个轮子是负值，说明direction配置错误
 */
void ParamVerify_EncoderDirection(void);

/**
 * @brief 测试3：轮半径标定
 * 
 * 让轮子转动N圈，测量实际行进距离，计算有效轮半径
 * 
 * 步骤：
 * 1. 在轮子上做标记
 * 2. 在地面上做起点标记
 * 3. 调用此函数（默认转5圈）
 * 4. 用卷尺测量行进距离
 * 5. 输入实测距离，程序计算实际半径
 * 
 * @param rotations 转动圈数（建议5-10圈）
 */
void ParamVerify_WheelRadius(uint32_t rotations);

/**
 * @brief 测试4：IR传感器符号验证
 * 
 * 手动偏移小车，验证lateral_error符号是否正确
 * 
 * 步骤：
 * 1. 将小车放在黑线上（正中）
 * 2. 调用此函数开始监控
 * 3. 手动向右移动小车 → 观察lateral_error（应为负）
 * 4. 手动向左移动小车 → 观察lateral_error（应为正）
 * 5. 按任意键结束测试
 * 
 * 预期结果：
 * - 小车右偏 → lateral_error < 0
 * - 小车左偏 → lateral_error > 0
 * - 如果符号反了，说明IR权重配置错误
 */
void ParamVerify_IRSensorSign(void);

/**
 * @brief 测试5：IR传感器原始数据查看
 * 
 * 实时显示8路IR传感器的原始值
 * 
 * 步骤：
 * 1. 将小车放在黑线上
 * 2. 调用此函数
 * 3. 观察哪些传感器被激活
 * 4. 手动移动小车，观察数值变化
 * 
 * 用途：
 * - 检查传感器是否正常工作
 * - 确认传感器编号顺序
 * - 调试传感器阈值
 */
void ParamVerify_IRRawData(void);

/**
 * @brief 测试6：速度估计验证
 * 
 * 以恒定PWM运行，验证速度估计是否准确
 * 
 * 步骤：
 * 1. 调用此函数（指定PWM和运行时间）
 * 2. 电机以指定PWM运行
 * 3. 用秒表和卷尺测量实际速度
 * 4. 对比程序计算的速度
 * 
 * @param pwm 测试PWM值（建议200-400）
 * @param duration_ms 运行时间（毫秒）
 */
void ParamVerify_SpeedEstimation(int16_t pwm, uint32_t duration_ms);

/**
 * @brief 测试7：坐标系一致性检查
 * 
 * 打印各模块使用的坐标系定义，供人工检查
 * 
 * 输出内容：
 * - 配置文件中的坐标系定义
 * - 传感器安装位置
 * - EKF运动模型的坐标轴
 * - 路径点的坐标系
 */
void ParamVerify_CoordinateSystem(void);

/**
 * @brief 完整测试套件（交互式）
 * 
 * 按顺序运行所有测试，带用户提示
 */
void ParamVerify_RunAllTests(void);

#ifdef __cplusplus
}
#endif

#endif /* PARAMETER_VERIFICATION_H */

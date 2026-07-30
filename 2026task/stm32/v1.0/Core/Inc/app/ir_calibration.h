/**
 * @file      ir_calibration.h
 * @brief     红外传感器校准工具（白平衡和阈值校准）
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-30
 * @note      黑线检测算法修复配套工具
 */

#ifndef IR_CALIBRATION_H
#define IR_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 红外传感器白平衡校准
 *
 * @details 校准步骤:
 *   1. 将小车放置在纯白色表面上（无黑线）
 *   2. 调用此函数
 *   3. 等待1秒稳定后自动采集100次样本
 *   4. 计算平均值并更新到 g_sens_decision_config.perception.white_reference[]
 *
 * @note 何时需要校准:
 *   - 首次使用
 *   - 光照条件变化（室内/室外切换）
 *   - 更换传感器或赛道表面
 *
 * @warning 校准期间请勿移动小车
 */
void IrCalibration_WhiteBalance(void);

/**
 * @brief 黑线强度阈值校准
 *
 * @details 校准步骤:
 *   1. 确保已完成白平衡校准（IrCalibration_WhiteBalance）
 *   2. 将小车居中对准黑线（中间传感器应检测到黑线）
 *   3. 调用此函数
 *   4. 自动读取黑线强度并设置阈值为最大强度的50%
 *
 * @note 必须在白平衡校准后调用
 *
 * @warning 校准时小车必须对准黑线中心
 */
void IrCalibration_BlackThreshold(void);

/**
 * @brief 打印当前红外传感器校准参数
 *
 * @details 输出内容:
 *   - 白色参考值（8通道）
 *   - 黑线强度阈值
 *   - 当前传感器原始读数
 *   - 计算的黑线强度
 */
void IrCalibration_PrintConfig(void);

/**
 * @brief 实时监控红外传感器状态（调试用）
 *
 * @param duration_ms 监控持续时间（毫秒）
 * @param interval_ms 采样间隔（毫秒）
 *
 * @details 实时打印:
 *   - 原始ADC值（8通道）
 *   - 黑线强度（8通道）
 *   - 激活通道数
 *   - 横向偏差
 *
 * @note 用于验证校准效果和调试感知算法
 */
void IrCalibration_Monitor(uint32_t duration_ms, uint32_t interval_ms);

#endif // IR_CALIBRATION_H

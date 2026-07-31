/**
 * @file encoder_resolution_calibration.h
 * @brief 编码器分辨率校准工具接口
 * @date 2026-07-30
 */

#ifndef ENCODER_RESOLUTION_CALIBRATION_H
#define ENCODER_RESOLUTION_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行编码器分辨率校准程序
 *
 * 通过手动旋转轮子1圈，测量实际编码器计数值
 * 验证 ENCODER_PPR 配置是否正确
 *
 * 预期值: 1560 counts/revolution
 * 容差: ±5%
 */
void EncoderResolutionCalibration_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_RESOLUTION_CALIBRATION_H */

/**
 * @file encoder_diagnostic.h
 * @brief 编码器硬件诊断工具接口
 * @date 2026-07-30
 */

#ifndef ENCODER_DIAGNOSTIC_H
#define ENCODER_DIAGNOSTIC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行编码器硬件诊断
 *
 * 直接读取TIM寄存器，检查编码器硬件连接
 */
void EncoderDiagnostic_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_DIAGNOSTIC_H */

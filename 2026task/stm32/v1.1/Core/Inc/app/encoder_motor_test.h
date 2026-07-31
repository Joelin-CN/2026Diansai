/**
 * @file encoder_motor_test.h
 * @brief 电机编码器联合测试接口
 * @date 2026-07-30
 */

#ifndef ENCODER_MOTOR_TEST_H
#define ENCODER_MOTOR_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行电机编码器联合测试
 *
 * 通过电机转动来测量编码器分辨率
 * - 左电机 15% PWM 运行 2 秒
 * - 右电机 15% PWM 运行 2 秒
 * - 用户手动记录轮子转了几圈
 */
void EncoderMotorTest_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_MOTOR_TEST_H */

/**
 * @file encoder_motor_test.c
 * @brief 电机编码器联合测试 - 通过电机转动来测量编码器
 * @date 2026-07-30
 */

#include "encoder_motor_test.h"
#include "encoder.h"
#include "motor.h"
#include "platform_time.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_PWM 15
#define TEST_DURATION_MS 2000

// Helper to get milliseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

/**
 * @brief 测试单个电机和编码器
 */
static void test_motor_encoder(uint8_t motor_id, const char* motor_name) {
    printf("\n========================================\n");
    printf("测试 %s (Motor %d)\n", motor_name, motor_id);
    printf("========================================\n\n");

    printf("测试参数:\n");
    printf("  PWM: %d%%\n", TEST_PWM);
    printf("  时长: %d ms (%.1f 秒)\n\n", TEST_DURATION_MS, TEST_DURATION_MS / 1000.0f);

    // 复位编码器
    Encoder_ResetCount(motor_id);
    HAL_Delay(10);
    Encoder_Poll();
    HAL_Delay(10);
    Encoder_Poll();

    int32_t start_count = Encoder_GetCount(motor_id);
    printf("初始编码器计数: %ld\n", start_count);

    printf("\n电机启动...\n");

    // 启动电机
    if (motor_id == 0) {
        Motor_SetSpeed(TEST_PWM, 0);  // 左电机
    } else {
        Motor_SetSpeed(0, TEST_PWM);  // 右电机
    }

    // 运行指定时间，持续轮询编码器
    uint32_t start_time = PlatformTime_GetMillis();
    uint32_t last_print = start_time;
    uint32_t last_poll = start_time;

    while (PlatformTime_GetMillis() - start_time < TEST_DURATION_MS) {
        uint32_t now = PlatformTime_GetMillis();

        // 每1ms轮询一次编码器（更频繁）
        if (now - last_poll >= 1) {
            Encoder_Poll();
            last_poll = now;
        }

        // 每100ms打印一次当前计数
        if (now - last_print >= 100) {
            int32_t current = Encoder_GetCount(motor_id);
            printf("  [%4lu ms] 计数: %ld\n", now - start_time, current);
            last_print = now;
        }
    }

    // 停止电机
    Motor_SetSpeed(0, 0);
    printf("\n电机停止\n");

    // 等待轮子完全停止
    HAL_Delay(200);

    // 最后轮询几次确保计数稳定
    for (int i = 0; i < 20; i++) {
        Encoder_Poll();
        HAL_Delay(10);
    }

    int32_t end_count = Encoder_GetCount(motor_id);
    int32_t delta = end_count - start_count;

    printf("\n========================================\n");
    printf("测量结果\n");
    printf("========================================\n");
    printf("初始计数: %ld\n", start_count);
    printf("最终计数: %ld\n", end_count);
    printf("变化量:   %ld counts\n\n", delta);

    printf("📋 请观察并记录:\n");
    printf("  轮子旋转了几圈? _______ 圈\n\n");

    if (delta == 0) {
        printf("❌ 编码器无变化！\n");
        printf("   可能原因:\n");
        printf("   - 编码器未连接\n");
        printf("   - TIM未启动\n");
        printf("   - 编码器损坏\n");
    } else if (abs(delta) < 10) {
        printf("⚠️  编码器计数非常少！\n");
        printf("   可能原因:\n");
        printf("   - 编码器连接不良\n");
        printf("   - 轮询频率不够\n");
        printf("   - 编码器配置错误\n");
    } else {
        printf("✅ 编码器有响应\n");
        if (delta > 0) {
            printf("   方向: 正向 (计数增加)\n");
        } else {
            printf("   方向: 反向 (计数减少)\n");
        }
        printf("\n   根据你观察的圈数，计算编码器分辨率:\n");
        printf("   ENCODER_PPR = %ld / 圈数\n", abs(delta));
    }

    printf("\n等待2秒...\n");
    HAL_Delay(2000);
}

/**
 * @brief 运行电机编码器测试
 */
void EncoderMotorTest_Run(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  电机编码器联合测试 v1.0               ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("🎯 测试目的:\n");
    printf("  通过电机转动来测量编码器分辨率\n\n");

    printf("📐 测试方法:\n");
    printf("  1. 以 15%% PWM 驱动左电机 2 秒\n");
    printf("  2. 记录编码器计数变化\n");
    printf("  3. 你手动数轮子转了几圈\n");
    printf("  4. 重复测试右电机\n\n");

    printf("📋 准备:\n");
    printf("  - 请盯着轮子，准备数圈数\n");
    printf("  - 确保轮子能自由转动\n\n");

    printf("准备开始...\n");
    printf("倒计时: ");
    for (int i = 3; i > 0; i--) {
        printf("%d ", i);
        fflush(stdout);
        HAL_Delay(1000);
    }
    printf("\n\n");

    // 确保电机停止
    Motor_SetSpeed(0, 0);
    HAL_Delay(100);

    // 测试左电机
    test_motor_encoder(0, "左电机");

    // 测试右电机
    test_motor_encoder(1, "右电机");

    // 总结
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  测试完成！                            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("📊 请告诉我:\n\n");
    printf("左电机:\n");
    printf("  - 编码器计数变化: _______ counts\n");
    printf("  - 轮子转了几圈: _______ 圈\n\n");

    printf("右电机:\n");
    printf("  - 编码器计数变化: _______ counts\n");
    printf("  - 轮子转了几圈: _______ 圈\n\n");

    printf("然后我会计算出正确的 ENCODER_PPR 值\n\n");
}

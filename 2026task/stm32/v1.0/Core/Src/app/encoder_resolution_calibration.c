/**
 * @file encoder_resolution_calibration.c
 * @brief 编码器分辨率校准工具 - 验证ENCODER_PPR配置
 * @date 2026-07-30
 *
 * 用途：通过手动旋转轮子1圈，测量实际编码器计数，验证配置是否正确
 *
 * 预期值：1560 counts/revolution
 * 计算依据：
 *   - 电机编码器: 13 PPR
 *   - 4倍频: 13 × 4 = 52 counts/电机转
 *   - 减速比: 30:1
 *   - 轮子1圈 = 52 × 30 = 1560 counts
 */

#include "encoder_resolution_calibration.h"
#include "encoder.h"
#include "motor.h"
#include "platform_time.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>

#define EXPECTED_PPR 1560
#define TOLERANCE_PERCENT 5.0f

// Helper to get milliseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

/**
 * @brief 测量单个轮子的编码器分辨率
 * @param wheel_id 轮子ID (0=左, 1=右)
 * @param wheel_name 轮子名称 ("左轮" 或 "右轮")
 */
static void measure_wheel_ppr(uint8_t wheel_id, const char* wheel_name) {
    printf("\n========================================\n");
    printf("测量 %s 编码器分辨率\n", wheel_name);
    printf("========================================\n\n");

    // 复位编码器
    Encoder_ResetCount(wheel_id);
    Encoder_Poll();
    HAL_Delay(10);  // 短暂延时确保复位生效
    Encoder_Poll();

    int32_t initial = Encoder_GetCount(wheel_id);
    printf("初始计数: %ld\n\n", initial);

    printf("📋 操作步骤:\n");
    printf("  1. 在轮子上做一个标记\n");
    printf("  2. 记住标记的起始位置\n");
    printf("  3. 手动【正向】旋转轮子\n");
    printf("  4. 旋转【完整1圈】回到起始位置\n");
    printf("  5. 按下蓝色用户按钮 (USER_BTN)\n\n");

    printf("⚠️  注意事项:\n");
    printf("  - 确保旋转完整1圈（360度）\n");
    printf("  - 尽量保持匀速旋转\n");
    printf("  - 不要反转或停顿\n\n");

    printf("等待操作...\n");
    printf("（旋转轮子1圈后按蓝色按钮）\n\n");

    // 等待用户按钮（假设按钮连接到某个GPIO）
    // 这里用简单的延时代替，实际使用时可以读取按钮状态
    // 或者用串口输入触发
    printf("提示: 如果没有按钮，请在完成旋转后等待5秒\n");
    printf("倒计时: ");

    for (int i = 30; i > 0; i--) {
        printf("%d ", i);
        fflush(stdout);
        uint32_t start = PlatformTime_GetMillis();
        while (PlatformTime_GetMillis() - start < 1000) {
            Encoder_Poll();  // 持续轮询编码器，确保不丢失计数
            HAL_Delay(1);    // 短暂延时避免CPU占用过高
        }
    }
    printf("\n\n");

    // 最后再轮询几次确保计数稳定
    for (int i = 0; i < 10; i++) {
        Encoder_Poll();
        HAL_Delay(1);
    }

    // 读取最终计数
    Encoder_Poll();
    int32_t final = Encoder_GetCount(wheel_id);
    int32_t delta = final - initial;

    printf("========================================\n");
    printf("测量结果\n");
    printf("========================================\n");
    printf("初始计数: %ld\n", initial);
    printf("最终计数: %ld\n", final);
    printf("变化量:   %ld counts\n\n", delta);

    // 计算偏差
    int32_t abs_delta = abs(delta);
    float error_percent = (float)(abs_delta - EXPECTED_PPR) / EXPECTED_PPR * 100.0f;

    printf("预期值:   %d counts/revolution\n", EXPECTED_PPR);
    printf("实测值:   %ld counts/revolution\n", abs_delta);
    printf("偏差:     %.2f%%\n\n", error_percent);

    // 评估结果
    if (abs(error_percent) <= TOLERANCE_PERCENT) {
        printf("✅ 测试通过！\n");
        printf("   编码器配置正确，偏差在 ±%.0f%% 范围内\n", TOLERANCE_PERCENT);
    } else {
        printf("❌ 测试失败！\n");
        printf("   编码器配置可能有误\n\n");
        printf("🔧 建议的修正值:\n");
        printf("   ENCODER_PPR = %ld\n\n", abs_delta);
        printf("📝 需要更新的文件:\n");
        printf("   1. modules/MotionControl/inc/motion_config.h:44\n");
        printf("      #define ENCODER_PPR %ld\n", abs_delta);
        printf("   2. modules/Sens-Decision/src/config.c:35\n");
        printf("      .pulses_per_revolution = %ldU\n", abs_delta);
    }

    // 方向检查
    printf("\n📊 方向检查:\n");
    if (delta > 0) {
        printf("   正向旋转 → 编码器增加 ✅\n");
    } else if (delta < 0) {
        printf("   正向旋转 → 编码器减少 ⚠️\n");
        printf("   （可能是编码器接线反了或配置反了）\n");
    } else {
        printf("   ❌ 错误: 编码器无变化！\n");
        printf("   请检查:\n");
        printf("     - 编码器是否正常工作\n");
        printf("     - 编码器线是否连接正确\n");
        printf("     - TIM定时器是否配置为编码器模式\n");
    }

    printf("\n等待3秒继续...\n");
    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        Encoder_Poll();
        HAL_Delay(10);
    }
}

/**
 * @brief 运行编码器分辨率校准
 */
void EncoderResolutionCalibration_Run(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  编码器分辨率校准工具 v1.0             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    printf("目的: 验证 ENCODER_PPR 配置是否正确\n\n");

    printf("📐 理论计算:\n");
    printf("  电机编码器线数: 13 PPR\n");
    printf("  4倍频:         13 × 4 = 52 counts/电机转\n");
    printf("  减速比:        30:1\n");
    printf("  理论分辨率:     52 × 30 = 1560 counts/轮转\n\n");

    printf("⚙️  当前配置:\n");
    printf("  ENCODER_PPR = %d\n\n", EXPECTED_PPR);

    printf("🎯 测试计划:\n");
    printf("  1. 测量左轮编码器分辨率\n");
    printf("  2. 测量右轮编码器分辨率\n");
    printf("  3. 对比理论值与实测值\n\n");

    printf("准备开始...\n");
    printf("倒计时: ");
    for (int i = 5; i > 0; i--) {
        printf("%d ", i);
        fflush(stdout);
        uint32_t start = PlatformTime_GetMillis();
        while (PlatformTime_GetMillis() - start < 1000) {
            // Wait
        }
    }
    printf("\n\n");

    // 确保电机停止
    Motor_SetSpeed(0, 0);

    // 测量左轮
    measure_wheel_ppr(0, "左轮");

    // 测量右轮
    measure_wheel_ppr(1, "右轮");

    // 总结
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  校准完成！                            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    printf("📋 后续步骤:\n\n");

    printf("1️⃣  如果两个轮子的偏差都在 ±5%% 以内:\n");
    printf("   ✅ 编码器配置正确，无需修改\n");
    printf("   继续进行 IR 传感器校准\n\n");

    printf("2️⃣  如果偏差超过 5%%:\n");
    printf("   ❌ 需要更新 ENCODER_PPR 配置\n");
    printf("   使用上面建议的修正值更新代码\n");
    printf("   重新编译和下载程序\n\n");

    printf("3️⃣  如果编码器计数为0:\n");
    printf("   ⚠️  硬件连接问题\n");
    printf("   检查编码器接线和定时器配置\n\n");

    printf("4️⃣  如果方向相反:\n");
    printf("   ⚠️  需要调整编码器方向配置\n");
    printf("   在 config.c 中修改 encoder_directions[]\n\n");

    printf("📝 下一个校准项目:\n");
    printf("   IR 传感器符号验证\n");
    printf("   （验证 lateral_error 的符号是否正确）\n\n");
}

/**
 * @file encoder_diagnostic.c
 * @brief 编码器硬件诊断工具 - 直接读取TIM寄存器
 * @date 2026-07-30
 */

#include "encoder_diagnostic.h"
#include "encoder.h"
#include "motor.h"
#include "tim.h"
#include "main.h"
#include <stdio.h>

/**
 * @brief 打印TIM寄存器状态
 */
static void print_tim_registers(TIM_TypeDef *TIMx, const char* name) {
    printf("\n%s 寄存器状态:\n", name);
    printf("  CNT (计数器):  0x%04X (%u)\n", TIMx->CNT, TIMx->CNT);
    printf("  CR1 (控制1):   0x%04X\n", TIMx->CR1);
    printf("  CR2 (控制2):   0x%04X\n", TIMx->CR2);
    printf("  SMCR (从模式): 0x%04X\n", TIMx->SMCR);
    printf("  CCER (捕获使能):0x%04X\n", TIMx->CCER);
    printf("  ARR (自动重装):0x%04X\n", TIMx->ARR);

    // 检查定时器是否使能
    if (TIMx->CR1 & TIM_CR1_CEN) {
        printf("  状态: ✅ 定时器已使能\n");
    } else {
        printf("  状态: ❌ 定时器未使能\n");
    }

    // 检查编码器模式
    uint16_t sms = TIMx->SMCR & 0x0007;
    if (sms == 0x01 || sms == 0x02 || sms == 0x03) {
        printf("  模式: ✅ 编码器模式 (SMS=%u)\n", sms);
    } else {
        printf("  模式: ❌ 非编码器模式 (SMS=%u)\n", sms);
    }
}

/**
 * @brief 测试编码器硬件连接
 */
static void test_encoder_hw(uint8_t id, const char* name, TIM_TypeDef *TIMx) {
    printf("\n========================================\n");
    printf("测试 %s (Encoder %d)\n", name, id);
    printf("========================================\n");

    // 打印初始状态
    print_tim_registers(TIMx, name);

    printf("\n📋 测试方法:\n");
    printf("  请**手动慢速**旋转 %s\n", name);
    printf("  观察CNT计数器是否变化\n\n");

    printf("开始监测 (10秒)...\n\n");
    printf("时间  |  CNT值  | 变化\n");
    printf("------|---------|------\n");

    uint16_t last_cnt = TIMx->CNT;
    printf(" 0.0s | %5u   | --\n", last_cnt);

    for (int i = 1; i <= 20; i++) {
        HAL_Delay(500);

        uint16_t now_cnt = TIMx->CNT;
        int16_t delta = (int16_t)(now_cnt - last_cnt);

        printf(" %2.1fs | %5u   | %+6d\n", i * 0.5f, now_cnt, delta);
        last_cnt = now_cnt;
    }

    printf("\n");
}

/**
 * @brief 运行编码器硬件诊断
 */
void EncoderDiagnostic_Run(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  编码器硬件诊断工具 v1.0               ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("🎯 诊断目的:\n");
    printf("  检查编码器硬件是否正常工作\n");
    printf("  直接读取TIM寄存器，不依赖软件层\n\n");

    printf("📋 测试内容:\n");
    printf("  1. 检查TIM3/TIM4寄存器配置\n");
    printf("  2. 手动旋转轮子，观察CNT变化\n");
    printf("  3. 判断编码器是否连接正常\n\n");

    // 确保编码器已初始化
    printf("[INIT] 初始化编码器...\n");
    Encoder_Init();
    printf("[INIT] 编码器已初始化\n\n");

    HAL_Delay(1000);

    // 测试TIM3 (左编码器)
    test_encoder_hw(0, "左编码器 (TIM3)", TIM3);

    printf("\n等待2秒...\n");
    HAL_Delay(2000);

    // 测试TIM4 (右编码器)
    test_encoder_hw(1, "右编码器 (TIM4)", TIM4);

    // 总结
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  诊断完成！                            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("📊 分析结果:\n\n");

    printf("如果CNT计数器有变化:\n");
    printf("  ✅ 编码器硬件连接正常\n");
    printf("  ✅ TIM配置正确\n");
    printf("  → 问题在软件轮询层\n\n");

    printf("如果CNT计数器始终不变:\n");
    printf("  ❌ 可能的原因:\n");
    printf("     1. 编码器未连接或连接错误\n");
    printf("     2. 编码器线序错误\n");
    printf("     3. 编码器损坏\n");
    printf("     4. GPIO引脚定义错误\n\n");

    printf("硬件信息:\n");
    printf("  左编码器: PB4(A), PB5(B) → TIM3_CH1/CH2\n");
    printf("  右编码器: PD12(A), PD13(B) → TIM4_CH1/CH2\n\n");
}

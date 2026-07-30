/**
 * @file ir_sensor_calibration.c
 * @brief IR传感器符号和配置验证工具
 * @date 2026-07-30
 *
 * 用途：验证IR传感器的符号是否正确
 *   - 小车向右偏 → lateral_error 应为负值
 *   - 小车向左偏 → lateral_error 应为正值
 */

#include "ir_sensor_calibration.h"
#include "ir_uart_sensor.h"
#include "platform_time.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>

#define TEST_DURATION_MS 10000  // 10秒测试时间
#define SAMPLE_INTERVAL_MS 100  // 100ms采样间隔

// Helper to get milliseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

/**
 * @brief 显示传感器原始值
 */
static void display_raw_sensors(void) {
    printf("\n========================================\n");
    printf("IR传感器原始值监测\n");
    printf("========================================\n\n");

    printf("传感器布局（从车头看）:\n");
    printf("左侧 ←                    中心                     → 右侧\n");
    printf("-39.8606  -28.4719  -17.0831  -5.6944   5.6944   17.0831   28.4719   39.8606 (mm)\n");
    printf("  [7]     [6]     [5]     [4]     [3]    [2]     [1]     [0]  (索引)\n");
    printf("\n");
    printf("⚠️  注意: 通道0在最右侧(+39.8606mm)，通道7在最左侧(-39.8606mm)\n\n");

    printf("测试时长: %d 秒\n\n", TEST_DURATION_MS / 1000);
    printf("开始监测...\n\n");

    uint32_t start_time = PlatformTime_GetMillis();
    uint32_t last_sample = start_time;

    while (PlatformTime_GetMillis() - start_time < TEST_DURATION_MS) {
        uint32_t now = PlatformTime_GetMillis();

        if (now - last_sample >= SAMPLE_INTERVAL_MS) {
            last_sample = now;

            uint16_t ir_raw[8];
            if (IrUartSensor_GetAnalog(ir_raw)) {
                // 显示原始值
                printf("\r[%5lu ms] ", now - start_time);
                for (int i = 0; i < 8; i++) {
                    printf("%4d ", ir_raw[i]);
                }
                printf("   ");
                fflush(stdout);
            }
        }
    }

    printf("\n\n原始值监测完成\n");
}

/**
 * @brief 验证lateral_error符号
 *
 * 这个函数需要根据你的实际perception模块接口调整
 */
static void verify_lateral_error_sign(void) {
    printf("\n========================================\n");
    printf("Lateral Error 符号验证\n");
    printf("========================================\n\n");

    printf("📋 测试说明:\n");
    printf("  将小车放在黑线上，然后手动移动小车\n");
    printf("  观察 lateral_error 的符号是否正确\n\n");

    printf("✅ 预期行为:\n");
    printf("  小车向右偏移 → lateral_error < 0 (负值)\n");
    printf("  小车向左偏移 → lateral_error > 0 (正值)\n");
    printf("  小车在中心   → lateral_error ≈ 0\n\n");

    printf("⚠️  如果符号相反:\n");
    printf("  需要调整 IR 权重数组的符号\n");
    printf("  在 config.c 的 ir_weights[] 中全部取反\n\n");

    printf("测试时长: %d 秒\n\n", TEST_DURATION_MS / 1000);
    printf("开始测试...\n\n");

    // 这里需要初始化perception模块
    // perception_t perception;
    // perception_init(&perception);

    uint32_t start_time = PlatformTime_GetMillis();
    uint32_t last_sample = start_time;

    printf("Time    |  Lateral Error  | 状态\n");
    printf("--------|-----------------|------------------\n");

    while (PlatformTime_GetMillis() - start_time < TEST_DURATION_MS) {
        uint32_t now = PlatformTime_GetMillis();

        if (now - last_sample >= SAMPLE_INTERVAL_MS) {
            last_sample = now;

            uint16_t ir_raw[8];
            if (IrUartSensor_GetAnalog(ir_raw)) {
                // 简单的加权平均计算（临时实现）
                // 实际应该调用perception模块
                float weighted_sum = 0.0f;
                float total_weight = 0.0f;

                // IR权重（根据config.c中的配置）
                // 注意: 通道0在最右侧(+39.8606mm), 通道7在最左侧(-39.8606mm)
                const float ir_weights[8] = {
                    3.9861f,   // 通道0: Y=+39.8606mm (最右)
                    2.8472f,   // 通道1: Y=+28.4719mm
                    1.7083f,   // 通道2: Y=+17.0831mm
                    0.5694f,   // 通道3: Y=+5.6944mm
                    -0.5694f,  // 通道4: Y=-5.6944mm
                    -1.7083f,  // 通道5: Y=-17.0831mm
                    -2.8472f,  // 通道6: Y=-28.4719mm
                    -3.9861f   // 通道7: Y=-39.8606mm (最左)
                };

                for (int i = 0; i < 8; i++) {
                    float normalized = (float)ir_raw[i] / 4095.0f;  // 假设12位ADC
                    weighted_sum += normalized * ir_weights[i];
                    total_weight += normalized;
                }

                float lateral_error = 0.0f;
                if (total_weight > 0.01f) {
                    lateral_error = weighted_sum / total_weight;
                }

                // 显示结果
                printf("%5lu s |  %7.3f      | ",
                       (now - start_time) / 1000,
                       lateral_error);

                if (lateral_error < -0.5f) {
                    printf(">>> 右偏 (应为负)\n");
                } else if (lateral_error > 0.5f) {
                    printf("<<< 左偏 (应为正)\n");
                } else {
                    printf("    中心\n");
                }
            }
        }
    }

    printf("\n========================================\n");
    printf("测试完成\n");
    printf("========================================\n\n");

    printf("📊 评估结果:\n\n");
    printf("请根据上面的输出判断:\n");
    printf("  1. 当你把小车向右移动时，lateral_error 是否变为负值？\n");
    printf("     □ 是 (正确)  □ 否 (需要调整)\n\n");
    printf("  2. 当你把小车向左移动时，lateral_error 是否变为正值？\n");
    printf("     □ 是 (正确)  □ 否 (需要调整)\n\n");

    printf("🔧 如果符号错误，修改方法:\n");
    printf("   在 modules/Sens-Decision/src/config.c 中\n");
    printf("   将 ir_weights[] 数组的所有值取反\n");
    printf("   例如: 3.9861f → -3.9861f\n\n");
}

/**
 * @brief 传感器覆盖范围测试
 */
static void test_sensor_coverage(void) {
    printf("\n========================================\n");
    printf("传感器覆盖范围测试\n");
    printf("========================================\n\n");

    printf("📋 测试说明:\n");
    printf("  将小车放在黑线上，观察哪些传感器被激活\n\n");

    printf("测试时长: 5秒\n\n");

    uint32_t start_time = PlatformTime_GetMillis();
    uint32_t last_sample = start_time;

    printf("索引:  [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]\n");
    printf("-------|----|----|----|----|----|----|----|----|----\n");

    while (PlatformTime_GetMillis() - start_time < 5000) {
        uint32_t now = PlatformTime_GetMillis();

        if (now - last_sample >= 200) {
            last_sample = now;

            uint16_t ir_raw[8];
            if (IrUartSensor_GetAnalog(ir_raw)) {
                printf("值:   ");
                for (int i = 0; i < 8; i++) {
                    printf("%4d ", ir_raw[i]);
                }
                printf("\n状态: ");
                for (int i = 0; i < 8; i++) {
                    // 假设阈值是2000（需要根据实际情况调整）
                    if (ir_raw[i] > 2000) {
                        printf(" ON  ");
                    } else {
                        printf(" off ");
                    }
                }
                printf("\n\n");
            }
        }
    }

    printf("覆盖范围测试完成\n");
}

/**
 * @brief 运行IR传感器校准程序
 */
void IrSensorCalibration_Run(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  IR传感器校准工具 v1.0                 ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("🎯 校准目标:\n");
    printf("  1. 验证传感器原始值是否正常\n");
    printf("  2. 验证 lateral_error 符号是否正确\n");
    printf("  3. 测试传感器覆盖范围\n\n");

    printf("📐 传感器配置信息:\n");
    printf("  传感器数量: 8\n");
    printf("  Y坐标: 183 mm (车头前方)\n");
    printf("  X范围: -39.8606 到 +39.8606 mm\n");
    printf("  总宽度: 79.72 mm\n\n");

    // 初始化IR传感器
    printf("[INIT] Initializing IR sensor...\n");
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();
    printf("[INIT] IR sensor initialized\n\n");

    printf("准备开始...\n");
    printf("倒计时: ");
    for (int i = 3; i > 0; i--) {
        printf("%d ", i);
        fflush(stdout);
        uint32_t start = PlatformTime_GetMillis();
        while (PlatformTime_GetMillis() - start < 1000) {
            // Wait
        }
    }
    printf("\n\n");

    // 测试1: 显示原始传感器值
    display_raw_sensors();

    printf("\n准备下一个测试...\n");
    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 2000) {
        // Wait
    }

    // 测试2: 验证lateral_error符号
    verify_lateral_error_sign();

    printf("\n准备下一个测试...\n");
    wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 2000) {
        // Wait
    }

    // 测试3: 传感器覆盖范围
    test_sensor_coverage();

    // 总结
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  IR传感器校准完成！                    ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    printf("📋 后续步骤:\n\n");

    printf("1️⃣  如果 lateral_error 符号正确:\n");
    printf("   ✅ 传感器配置正确，可以进行循迹测试\n\n");

    printf("2️⃣  如果符号相反:\n");
    printf("   🔧 修改 config.c 中的 ir_weights[] 数组\n");
    printf("   将所有权重值取反\n");
    printf("   重新编译和测试\n\n");

    printf("3️⃣  如果传感器值异常:\n");
    printf("   ⚠️  检查硬件连接\n");
    printf("   - UART连接是否正常\n");
    printf("   - 传感器供电是否正常\n");
    printf("   - 传感器高度和角度是否合适\n\n");

    printf("📝 下一个校准项目:\n");
    printf("   低速循迹测试\n");
    printf("   （在实际赛道上测试控制效果）\n\n");
}

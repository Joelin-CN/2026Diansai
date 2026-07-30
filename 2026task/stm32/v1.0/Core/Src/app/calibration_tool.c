/**
 * @file calibration_tool.c
 * @brief 统一校准工具 - 集成所有校准功能
 * @date 2026-07-30
 */

#include "calibration_tool.h"
#include "encoder_resolution_calibration.h"
#include "ir_sensor_calibration.h"
#include "motor_direction_calibration.h"
#include "platform_time.h"
#include <stdio.h>

// Helper to get milliseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

/**
 * @brief 显示校准菜单
 */
static void show_menu(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         小车参数校准工具集 v1.0                  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("📋 可用的校准工具:\n\n");

    printf("  [1] 编码器分辨率校准 🔴 (优先级最高)\n");
    printf("      验证 ENCODER_PPR = 1560 是否正确\n");
    printf("      方法: 手动旋转轮子1圈，测量计数值\n");
    printf("      时间: 约5分钟\n\n");

    printf("  [2] IR传感器符号验证 🟡\n");
    printf("      验证 lateral_error 符号是否正确\n");
    printf("      方法: 移动小车观察符号变化\n");
    printf("      时间: 约3分钟\n\n");

    printf("  [3] 电机方向校准 🟢\n");
    printf("      验证电机正反转方向是否正确\n");
    printf("      方法: 自动测试电机和编码器方向\n");
    printf("      时间: 约2分钟\n\n");

    printf("  [4] 完整校准流程 (推荐) ⭐\n");
    printf("      按优先级依次执行所有校准\n");
    printf("      时间: 约10分钟\n\n");

    printf("  [0] 退出校准工具\n\n");

    printf("══════════════════════════════════════════════════\n\n");
}

/**
 * @brief 显示校准状态总结
 */
static void show_calibration_status(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         当前配置状态                             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("✅ 已更新的参数:\n");
    printf("  • 轮距: 115 mm (旧值: 150 mm)\n");
    printf("  • 减速比: 30:1 (旧值: 28:1)\n");
    printf("  • 编码器PPR: 1560 (旧值: 334)\n");
    printf("  • IR传感器Y坐标: 132.1 mm (旧值: 100 mm)\n");
    printf("  • IR权重: 已更新为实际分布\n\n");

    printf("⚠️  待验证的参数:\n");
    printf("  • 编码器分辨率 (1560) - 需要实测验证 🔴\n");
    printf("  • IR传感器符号 - 需要符号验证 🟡\n");
    printf("  • 电机方向 - 需要方向确认 🟢\n");
    printf("  • 轮半径 (33 mm) - 需要滚动测量\n\n");

    printf("📊 参数影响程度:\n");
    printf("  🔴 严重: 编码器PPR错误会导致速度估计错误4.67倍\n");
    printf("  🟡 中等: IR符号错误会导致循迹反向\n");
    printf("  🟢 轻微: 电机方向错误可以通过软件修正\n\n");
}

/**
 * @brief 等待用户输入（简化版）
 * @return 用户选择 (0-4)
 */
static int wait_for_input(void) {
    printf("请输入选项 (0-4): ");
    fflush(stdout);

    // 简化实现：等待一段时间，实际使用时需要实现UART输入
    // 这里返回一个默认值，实际应该从串口读取
    printf("\n⚠️  简化版本: 自动选择完整校准流程 [4]\n");
    printf("(实际使用时请通过串口输入选项)\n\n");

    uint32_t start = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - start < 2000) {
        // 在实际实现中，这里应该检查UART输入
        // 例如: if (uart_has_data()) { return uart_read_char() - '0'; }
    }

    // 返回默认选项：完整校准流程
    return 4;
}

/**
 * @brief 执行完整校准流程
 */
static void run_full_calibration(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         开始完整校准流程                         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("校准顺序:\n");
    printf("  第1步: 编码器分辨率校准 (5分钟)\n");
    printf("  第2步: IR传感器符号验证 (3分钟)\n");
    printf("  第3步: 电机方向校准 (2分钟)\n\n");

    printf("准备开始...\n");
    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        // Wait
    }

    // 步骤1: 编码器分辨率校准
    printf("\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("  第1步: 编码器分辨率校准\n");
    printf("═══════════════════════════════════════════════════\n");
    EncoderResolutionCalibration_Run();

    printf("\n按任意键继续到下一步...\n");
    wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        // Wait
    }

    // 步骤2: IR传感器符号验证
    printf("\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("  第2步: IR传感器符号验证\n");
    printf("═══════════════════════════════════════════════════\n");
    IrSensorCalibration_Run();

    printf("\n按任意键继续到下一步...\n");
    wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        // Wait
    }

    // 步骤3: 电机方向校准
    printf("\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("  第3步: 电机方向校准\n");
    printf("═══════════════════════════════════════════════════\n");
    MotorDirectionCalibration_Run();

    // 完成总结
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║         完整校准流程完成！                       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("📋 校准结果检查清单:\n\n");
    printf("□ 左轮编码器: 1560 ± 5%%\n");
    printf("□ 右轮编码器: 1560 ± 5%%\n");
    printf("□ 小车向右偏 → lateral_error < 0\n");
    printf("□ 小车向左偏 → lateral_error > 0\n");
    printf("□ 左电机正转 → 编码器增加\n");
    printf("□ 右电机正转 → 编码器增加\n\n");

    printf("✅ 如果所有项目都通过:\n");
    printf("   恭喜！参数配置正确，可以开始循迹测试\n\n");

    printf("❌ 如果有项目未通过:\n");
    printf("   请根据各校准工具的建议修改配置\n");
    printf("   修改后重新编译、下载和测试\n\n");

    printf("📝 下一步:\n");
    printf("   1. 记录所有测量值\n");
    printf("   2. 更新必要的配置文件\n");
    printf("   3. 开始低速循迹测试 (0.3 m/s)\n\n");
}

/**
 * @brief 运行校准工具主程序
 */
void CalibrationTool_Run(void) {
    // 显示配置状态
    show_calibration_status();

    // 显示菜单
    show_menu();

    // 获取用户选择
    int choice = wait_for_input();

    switch (choice) {
        case 1:
            EncoderResolutionCalibration_Run();
            break;

        case 2:
            IrSensorCalibration_Run();
            break;

        case 3:
            MotorDirectionCalibration_Run();
            break;

        case 4:
            run_full_calibration();
            break;

        case 0:
            printf("退出校准工具\n");
            break;

        default:
            printf("无效选项，执行完整校准流程\n");
            run_full_calibration();
            break;
    }

    printf("\n校准工具已结束\n\n");
}

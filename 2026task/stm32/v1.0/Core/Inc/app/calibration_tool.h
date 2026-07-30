/**
 * @file calibration_tool.h
 * @brief 统一校准工具接口
 * @date 2026-07-30
 */

#ifndef CALIBRATION_TOOL_H
#define CALIBRATION_TOOL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行校准工具主程序
 *
 * 提供交互式菜单，让用户选择需要执行的校准任务:
 *   1. 编码器分辨率校准 (优先级最高)
 *   2. IR传感器符号验证
 *   3. 电机方向校准
 *   4. 完整校准流程 (推荐)
 */
void CalibrationTool_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_TOOL_H */

# Task 1: Platform-Independent ICM42688 Core — Task Reviewer Brief

## Global Constraints (verbatim from plan)

- 目标器件固定为 MSPM0G3507 LQFP-48。
- SPI1 引脚固定为 POCI PA16、SCLK PA17、PICO PA18；ICM42688_CS 固定为 PB20，低有效。
- SPI 固定为 Mode 0、8 位、MSB first，初始时钟 1 MHz。
- 不修改 `controller/`、其 `.syscfg` 文件或自动生成文件。
- 中间产物和主机测试代码只放在 `modules/ICM42688/temp/`。
- 最终模块文件只放在 `modules/ICM42688/inc/`、`modules/ICM42688/src/` 和 `modules/ICM42688/README.md`。
- 不修改 `pictures/`；不添加 DMA、FIFO、数据就绪中断、I2C、磁力计或九轴融合。
- TIMG0 配置为 1 MHz 自由运行计数器；FreeRTOS 保留 SysTick 作为系统节拍。
- 不执行 git commit。

## Task Requirements (from task-1-brief.md and plan)

The task must produce three files:

1. `inc/icm42688_hal.h` — Registers, enums (icm42688_interface_type_t, icm42688_acc_sample_t, icm42688_gyro_sample_t, icm42688_sample_rate_t, icm42688_axis_data_t, icm42688_config_t), callback structs (icm42688_comm_t, icm42688_system_t), status enum, data struct, public API declarations.

2. `src/icm42688_hal.c` — Platform-independent. No STM32 or MSPM0 includes. Init sequence: check ID (0x47), soft reset (DEVICE_CONFIG=0x01, 50ms wait), wake (PWR_MGMT0=0x0F), set range/ODR/filters. Read 12 bytes from ACCEL_DATA_X1 burst. Big-endian decode. Scale factors: 8.0f/32768.0f (+-8g), 1000.0f/32768.0f (+-1000dps). Calibration averages gyro readings and stores bias in dps.

3. `temp/tests/test_icm42688.c` — 10 tests: null comm, null fn pointers, bad ID, reset register check, wake register check, scale conversion, null data pointer, unbound interface, zero samples calibration, calibration zero output. Prints `test_icm42688: PASS` or `test_icm42688: FAIL` with message.

## Reviewer Instructions

Read the following files in order:
1. `E:\B306\2026\电赛\modules\ICM42688\temp\task-1-brief.md` — the requirements the implementer must meet
2. `E:\B306\2026\电赛\modules\ICM42688\temp\task-1-report.md` — the implementer's self-report

Then read the actual implementation files:
- `E:\B306\2026\电赛\modules\ICM42688\inc\icm42688_hal.h`
- `E:\B306\2026\电赛\modules\ICM42688\src\icm42688_hal.c`
- `E:\B306\2026\电赛\modules\ICM42688\temp\tests\test_icm42688.c`

Run the tests to confirm they pass:
```
cd E:\B306\2026\电赛\modules\ICM42688
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe
.\temp\tests\test_icm42688.exe
```

Provide TWO verdicts:
1. **Spec compliance**: Does every requirement from the brief exist in the implementation? Nothing missing, nothing extra?
2. **Code quality**: Is the code correct, clean, and free of issues? Rate findings as Critical/Important/Minor.

Return your full review result.

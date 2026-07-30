cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME v1.0_freeRTOS)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    # ── 用户应用层 ──────────────────────────────────────────────────────────────
    Core/Src/app/motor.c
    Core/Src/app/encoder.c
    Core/Src/app/encoder_hw_bridge.c
    Core/Src/app/encoder_adapter.c
    Core/Src/app/motor_adapter.c
    Core/Src/app/sensor_adapter.c
    Core/Src/app/uart_debug.c
    Core/Src/app/platform_time.c
    Core/Src/app/control_app.c
    Core/Src/app/square_path.c
    Core/Src/app/track_path.c
    Core/Src/app/track_control_app.c
    Core/Src/app/playground_track.c
    Core/Src/app/speed_mode.c
    # Core/Src/app/track_demo.c  # 示例代码，不参与编译
    Core/Src/app/imu_debug.c
    Core/Src/app/spi_diagnostic.c
    Core/Src/app/motor_debug.c
    Core/Src/app/motor_hw_diagnostic.c
    Core/Src/app/tim1_register_dump.c
    Core/Src/app/motor_speed_test.c
    Core/Src/app/motor_direction_calibration.c
    Core/Src/app/motor_interactive_test.c
    Core/Src/app/ir_sensor_test.c
    Core/Src/app/ir_uart_diagnostic.c
    Core/Src/app/ir_raw_capture.c
    Core/Src/app/calibration_tool.c
    Core/Src/app/encoder_resolution_calibration.c
    Core/Src/app/ir_sensor_calibration.c
    Core/Src/app/ir_calibration.c
    Core/Src/app/encoder_motor_test.c
    Core/Src/app/encoder_diagnostic.c

    # Motion Control 模块
    modules/MotionControl/src/motion_control.c
    modules/MotionControl/src/motion_feedback.c
    modules/MotionControl/src/motion_feedforward.c
    modules/MotionControl/src/motion_kinematics.c

    # Sens-Decision 模块
    modules/Sens-Decision/src/config.c
    modules/Sens-Decision/src/interface.c
    modules/Sens-Decision/src/utils.c
    modules/Sens-Decision/src/preprocess.c
    modules/Sens-Decision/src/EKF.c
    modules/Sens-Decision/src/state_evaluate.c
    modules/Sens-Decision/src/perception.c
    modules/Sens-Decision/src/behavior_planner.c
    modules/Sens-Decision/src/trajectory_generate.c

    # ICM42688 模块
    modules/ICM42688/src/icm42688_hal.c
    modules/ICM42688/src/icm42688_stm32.c
    modules/ICM42688/src/ahrs_hal.c

    # IR 循迹模块
    modules/IR-tracker/src/ir_uart_sensor.c

    # MCP23017 模块（暂时禁用，需要适配STM32 HAL I2C）
    # modules/MCP23017/src/mcp23017.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc/app
    modules/MotionControl/inc
    modules/Sens-Decision/inc
    modules/ICM42688/inc
    modules/IR-tracker/inc
    modules/MCP23017/inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

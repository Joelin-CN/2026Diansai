/**
 * @file control_app.c
 * @brief Application coordinator implementation
 * @date 2026-07-18
 */

/* Software-only test mode: skip all hardware initialization */
#define SOFTWARE_TEST_MODE 1

#include "control_app.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include "square_path.h"
#include "encoder_adapter.h"
#include "motor_adapter.h"
#include "sensor_adapter.h"

#include "../modules/IR-tracker/inc/ir_uart_sensor.h"
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/ICM42688/inc/icm42688_mspm0.h"
#include "../modules/Motion Control/inc/motion_control.h"
#include "../modules/Sens-Decision/inc/config.h"
#include "../modules/Sens-Decision/inc/interface.h"
#include "../modules/Sens-Decision/inc/preprocess.h"
#include "../modules/Sens-Decision/inc/state_evaluate.h"
#include "../modules/Sens-Decision/inc/perception.h"
#include "../modules/Sens-Decision/inc/behavior_planner.h"
#include "../modules/Sens-Decision/inc/trajectory_generate.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Static Application State
 * ============================================================================ */

static MotionControl_t g_motion_control;
static state_evaluator_t g_state_evaluator;
static perception_t g_perception;
static behavior_planner_t g_behavior_planner;
static trajectory_generator_t g_trajectory_generator;
static square_path_config_t g_square_config;
static lap_counter_t g_lap_counter;

static sensor_frame_t g_sensor_frame;
static perception_result_t g_perception_result;
static behavior_input_t g_behavior_input;
static behavior_output_t g_behavior_output;
static trajectory_point_t g_trajectory;

static uint8_t g_target_laps = 0;
static unsigned g_cycle_counter = 0;
static unsigned g_critical_failure_count = 0;

#if !SOFTWARE_TEST_MODE
static const icm42688_config_t g_icm_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float DEGREES_TO_RADIANS = 0.017453292519943295f;
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

bool ControlApp_Init(uint8_t target_laps) {
#if SOFTWARE_TEST_MODE
    printf("[ControlApp] *** SOFTWARE TEST MODE - NO HARDWARE ***\n");
    printf("[ControlApp] Initializing algorithm modules only...\n");

    /* Clear all state */
    memset(&g_motion_control, 0, sizeof(g_motion_control));
    memset(&g_state_evaluator, 0, sizeof(g_state_evaluator));
    memset(&g_perception, 0, sizeof(g_perception));
    memset(&g_behavior_planner, 0, sizeof(g_behavior_planner));
    memset(&g_trajectory_generator, 0, sizeof(g_trajectory_generator));
    memset(&g_square_config, 0, sizeof(g_square_config));
    memset(&g_lap_counter, 0, sizeof(g_lap_counter));
    memset(&g_sensor_frame, 0, sizeof(g_sensor_frame));
    memset(&g_perception_result, 0, sizeof(g_perception_result));
    memset(&g_behavior_input, 0, sizeof(g_behavior_input));
    memset(&g_behavior_output, 0, sizeof(g_behavior_output));
    memset(&g_trajectory, 0, sizeof(g_trajectory));
    g_target_laps = 0U;
    g_cycle_counter = 0U;
    g_critical_failure_count = 0U;

    /* Validate target laps */
    printf("[ControlApp] Validating target laps: %u\n", (unsigned int)target_laps);
    if (target_laps < 1 || target_laps > 5) {
        printf("[ERROR] Invalid target_laps: %u\n", (unsigned int)target_laps);
        return false;
    }
    g_target_laps = target_laps;

    /* Load Sens-Decision defaults */
    printf("[ControlApp] Loading Sens-Decision config defaults...\n");
    sd_config_reset_defaults();

    /* Initialize Sens-Decision objects */
    printf("[ControlApp] Initializing Sens-Decision modules...\n");
    state_evaluator_init(&g_state_evaluator, &g_sens_decision_config.ekf);
    perception_init(&g_perception);
    behavior_planner_init(&g_behavior_planner);

    /* Initialize trajectory generator with square path */
    printf("[ControlApp] Setting up trajectory generator...\n");
    trajectory_generator_init(&g_trajectory_generator, &g_sens_decision_config.trajectory);
    if (trajectory_set_path(&g_trajectory_generator, SquarePath_GetPoints(),
                           SquarePath_GetPointCount()) != SD_OK) {
        printf("[ERROR] trajectory_set_path failed\n");
        return false;
    }

    /* Initialize square path config */
    printf("[ControlApp] Configuring square path parameters...\n");
    g_square_config.lateral_gain = 2.0f;
    g_square_config.heading_gain = 1.5f;
    g_square_config.max_omega_radps = 2.0f;
    g_square_config.target_laps = target_laps;

    printf("[ControlApp] *** Algorithm initialization complete! ***\n");
    printf("[ControlApp] Note: Motion Control and hardware are NOT initialized\n");
    return true;

#else
    /* Original hardware initialization code */
    printf("[ControlApp] Step 1: Motor init...\n");
    /* Step 1: Establish a safe motor state before validating configuration */
    Motor_Init();
    Motor_Stop();

    printf("[ControlApp] Step 1b: Sensor release...\n");
    if (sensors_release_all() != SD_OK) {
        printf("[ERROR] sensors_release_all failed\n");
        Motor_Stop();
        return false;
    }

    printf("[ControlApp] Step 1c: Memory init...\n");
    memset(&g_motion_control, 0, sizeof(g_motion_control));
    memset(&g_state_evaluator, 0, sizeof(g_state_evaluator));
    memset(&g_perception, 0, sizeof(g_perception));
    memset(&g_behavior_planner, 0, sizeof(g_behavior_planner));
    memset(&g_trajectory_generator, 0, sizeof(g_trajectory_generator));
    memset(&g_square_config, 0, sizeof(g_square_config));
    memset(&g_lap_counter, 0, sizeof(g_lap_counter));
    memset(&g_sensor_frame, 0, sizeof(g_sensor_frame));
    memset(&g_perception_result, 0, sizeof(g_perception_result));
    memset(&g_behavior_input, 0, sizeof(g_behavior_input));
    memset(&g_behavior_output, 0, sizeof(g_behavior_output));
    memset(&g_trajectory, 0, sizeof(g_trajectory));
    g_target_laps = 0U;
    g_cycle_counter = 0U;
    g_critical_failure_count = 0U;

    /* Validate target laps */
    printf("[ControlApp] Step 1d: Validate target laps (%u)...\n", (unsigned int)target_laps);
    if (target_laps < 1 || target_laps > 5) {
        printf("[ERROR] Invalid target_laps: %u\n", (unsigned int)target_laps);
        return false;
    }
    g_target_laps = target_laps;

    printf("[ControlApp] Step 2: Encoder init...\n");
    /* Step 2: Encoder_Init */
    Encoder_Init();

    printf("[ControlApp] Step 3: IR-tracker init (UART1, ~20s warm-up)...\n");
    /* Step 3: Initialize 8-way IR-tracker module and request analog stream.
     * The on-board MCU needs ~20s to stabilize after power-on; read_ir()
     * returns SD_ERR_READ until the first valid frame arrives. */
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();

    printf("[ControlApp] Step 4: Config defaults and time init...\n");
    /* Step 4: Load Sens-Decision defaults and initialize platform time */
    sd_config_reset_defaults();
    PlatformTime_Init();

    printf("[ControlApp] Step 5: ICM42688 bind and init...\n");
    /* Step 5: Bind and initialize ICM - init checks WHO_AM_I internally */
    icm42688_mspm0_bind(&g_icm_config);
    if (icm42688_init() != ICM42688_STATUS_OK) {
        printf("[WARNING] icm42688_init failed (skipping for now)\n");
        /* Skip ICM42688 calibration and continue */
        goto skip_icm42688;
    }

    printf("[ControlApp] Step 6: ICM42688 gyro calibration (this takes ~1 second)...\n");
    /* Step 6: Calibrate gyro bias using HAL built-in calibration */
    if (icm42688_calibrate_gyro(100U, 10U) != ICM42688_STATUS_OK) {
        printf("[WARNING] icm42688_calibrate_gyro failed\n");
        goto skip_icm42688;
    }

    printf("[ControlApp] Step 7: ICM42688 metadata sync...\n");
    /* Step 7: Synchronize calibrated ICM metadata into Sens-Decision SI units */
    float accel_g_per_lsb;
    float gyro_dps_per_lsb;
    icm42688_vector3f_t gyro_bias_dps;

    if (icm42688_get_scale_factors(&accel_g_per_lsb, &gyro_dps_per_lsb) !=
            ICM42688_STATUS_OK ||
        icm42688_get_gyro_bias(&gyro_bias_dps) != ICM42688_STATUS_OK) {
        printf("[WARNING] icm42688 metadata retrieval failed\n");
        goto skip_icm42688;
    }

    g_sens_decision_config.imu.accel_scale_mps2_per_lsb =
        accel_g_per_lsb * STANDARD_GRAVITY_MPS2;
    g_sens_decision_config.imu.gyro_scale_radps_per_lsb =
        gyro_dps_per_lsb * DEGREES_TO_RADIANS;
    g_sens_decision_config.imu.gyro_bias_radps[0] =
        gyro_bias_dps.x * DEGREES_TO_RADIANS;
    g_sens_decision_config.imu.gyro_bias_radps[1] =
        gyro_bias_dps.y * DEGREES_TO_RADIANS;
    g_sens_decision_config.imu.gyro_bias_radps[2] =
        gyro_bias_dps.z * DEGREES_TO_RADIANS;

skip_icm42688:
    printf("[ControlApp] Step 8: Sensor HAL configure/init...\n");
    /* Step 8: Sensor HAL configure/init */
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    if (sensors_configure_hal(hal) != SD_OK) {
        printf("[WARNING] sensors_configure_hal failed\n");
        /* Continue anyway for testing */
    }

    if (sensors_init_all() != SD_OK) {
        printf("[WARNING] sensors_init_all failed\n");
        /* Continue anyway for testing */
    }

    printf("[ControlApp] Step 9: Sens-Decision objects/path...\n");
    /* Step 9: Sens-Decision objects/path */
    state_evaluator_init(&g_state_evaluator, &g_sens_decision_config.ekf);
    perception_init(&g_perception);
    behavior_planner_init(&g_behavior_planner);

    trajectory_generator_init(&g_trajectory_generator, &g_sens_decision_config.trajectory);
    if (trajectory_set_path(&g_trajectory_generator, SquarePath_GetPoints(),
                           SquarePath_GetPointCount()) != SD_OK) {
        printf("[ERROR] trajectory_set_path failed\n");
        Motor_Stop();
        return false;
    }

    printf("[ControlApp] Step 10: Square path config...\n");
    /* Initialize square path config */
    g_square_config.lateral_gain = 2.0f;
    g_square_config.heading_gain = 1.5f;
    g_square_config.max_omega_radps = 2.0f;
    g_square_config.target_laps = target_laps;

    printf("[ControlApp] Step 11: Motion Control init/start...\n");
    /* Step 10: Motion Control init/start */
    if (!MotionControl_Init(&g_motion_control, EncoderAdapter_GetInterface(),
                           MotorAdapter_GetInterface())) {
        printf("[ERROR] MotionControl_Init failed\n");
        Motor_Stop();
        return false;
    }

    MotionControl_Start(&g_motion_control);

    printf("[ControlApp] Initialization complete!\n");
    return true;
#endif  /* SOFTWARE_TEST_MODE */
}

/* ============================================================================
 * Fast Cycle: 500 Hz Motion Control, 50 Hz Sens-Decision
 * ============================================================================ */

void ControlApp_RunFastCycle(void) {
#if SOFTWARE_TEST_MODE
    /* Software test mode: Log algorithm execution without hardware */
    static uint32_t log_counter = 0;

    /* Every 10th cycle: Run Sens-Decision pipeline at 50 Hz */
    if ((g_cycle_counter % 10U) == 0U) {
        const float dt = 0.020f;  /* 50 Hz */

        /* Generate simulated sensor data */
        g_sensor_frame.timestamp_us = g_cycle_counter * 2000ULL;  /* 2ms per cycle */

        /* Encoder data - simulate zero speed */
        for (size_t i = 0; i < SD_ENCODER_COUNT; i++) {
            g_sensor_frame.encoders[i].count = 0;
            g_sensor_frame.encoders[i].speed_mps = 0.0f;
            g_sensor_frame.encoders[i].timestamp_us = g_sensor_frame.timestamp_us;
            g_sensor_frame.encoder_valid[i] = true;  /* Mark as valid */
        }

        /* IMU data - simulate stationary robot */
        g_sensor_frame.imu.accel_mps2[0] = 0.0f;
        g_sensor_frame.imu.accel_mps2[1] = 0.0f;
        g_sensor_frame.imu.accel_mps2[2] = 9.81f;
        g_sensor_frame.imu.gyro_radps[0] = 0.0f;
        g_sensor_frame.imu.gyro_radps[1] = 0.0f;
        g_sensor_frame.imu.gyro_radps[2] = 0.0f;
        g_sensor_frame.imu.temperature_c = 25.0f;
        g_sensor_frame.imu.timestamp_us = g_sensor_frame.timestamp_us;
        g_sensor_frame.imu_valid = true;  /* Mark as valid */

        /* IR array data - simulate centered on track */
        g_sensor_frame.ir.active_mask = 0xFFFF;
        for (size_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
            g_sensor_frame.ir.values[i] = 100.0f;  /* All sensors see track */
        }
        g_sensor_frame.ir.timestamp_us = g_sensor_frame.timestamp_us;
        g_sensor_frame.ir_valid = true;  /* Mark as valid */

        /* Run algorithm pipeline */
        sd_status_t status = state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
        if (status != SD_OK) {
            printf("[Cycle %u] state_evaluator_update failed: %d\n",
                   (unsigned int)g_cycle_counter, (int)status);
        }

        if (status == SD_OK) {
            status = perception_update(&g_perception, &g_sensor_frame.ir,
                                       g_sensor_frame.timestamp_us,
                                       &g_perception_result);
            if (status != SD_OK) {
                printf("[Cycle %u] perception_update failed: %d\n",
                       (unsigned int)g_cycle_counter, (int)status);
            }
        }
        if (status == SD_OK) {
            g_behavior_input.vehicle = &g_state_evaluator.state;
            g_behavior_input.perception = &g_perception_result;
            g_behavior_input.path_curvature = 0.0f;
            g_behavior_input.command =
                g_behavior_output.state == BEHAVIOR_STATE_IDLE
                    ? BEHAVIOR_CMD_START
                    : BEHAVIOR_CMD_NONE;
            status = behavior_planner_update(&g_behavior_planner, &g_behavior_input,
                                             &g_behavior_output);
            if (status != SD_OK) {
                printf("[Cycle %u] behavior_planner_update failed: %d\n",
                       (unsigned int)g_cycle_counter, (int)status);
            }
        }
        if (status == SD_OK) {
            status = trajectory_generate(&g_trajectory_generator,
                                         &g_state_evaluator.state,
                                         &g_behavior_output, dt, &g_trajectory);
            if (status != SD_OK) {
                printf("[Cycle %u] trajectory_generate failed: %d\n",
                       (unsigned int)g_cycle_counter, (int)status);
            }
        }

        if (status == SD_OK) {
            float corrected_omega = SquarePath_CorrectOmega(
                g_trajectory.omega, g_perception_result.lateral_error,
                g_perception_result.heading_error, &g_square_config);

            /* Log every second (50 cycles = 1 second at 50Hz) */
            if ((log_counter % 50) == 0) {
                printf("[Cycle %u] Algorithm OK - v=%.2f, omega=%.2f\n",
                       (unsigned int)g_cycle_counter,
                       g_trajectory.v, corrected_omega);
                printf("  State: x=%.2f, y=%.2f, theta=%.2f\n",
                       g_state_evaluator.state.x,
                       g_state_evaluator.state.y,
                       g_state_evaluator.state.theta);
                printf("  Perception: lateral_err=%.2f, heading_err=%.2f\n",
                       g_perception_result.lateral_error,
                       g_perception_result.heading_error);
            }

            SquarePath_UpdateLap(&g_lap_counter,
                                 g_trajectory_generator.last_nearest_index,
                                 SquarePath_GetPointCount(), g_target_laps);

            if (g_lap_counter.target_reached) {
                printf("[Cycle %u] *** Target reached! ***\n",
                       (unsigned int)g_cycle_counter);
            }
            g_critical_failure_count = 0;
        } else {
            if (g_critical_failure_count < 3) {
                g_critical_failure_count++;
                printf("[Cycle %u] [WARNING] Algorithm failed (count: %u)\n",
                       (unsigned int)g_cycle_counter,
                       (unsigned int)g_critical_failure_count);
                if (g_critical_failure_count == 3) {
                    printf("[Cycle %u] [ERROR] Critical failure threshold reached!\n",
                           (unsigned int)g_cycle_counter);
                }
            }
        }
        log_counter++;
    }

    g_cycle_counter++;

#else
    /* Original hardware code */
    /* Every 10th cycle: Run Sens-Decision pipeline at 50 Hz */
    if ((g_cycle_counter % 10U) == 0U) {
        const float dt = 0.020f;  /* 50 Hz */
        
        /* Preprocess sensor data */
        sd_status_t status = preprocess_update(PlatformTime_GetUs64(), &g_sensor_frame);

        if (status == SD_OK) {
            status = state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
        }
        if (status == SD_OK) {
            status = perception_update(&g_perception, &g_sensor_frame.ir,
                                       g_sensor_frame.timestamp_us,
                                       &g_perception_result);
        }
        if (status == SD_OK) {
            g_behavior_input.vehicle = &g_state_evaluator.state;
            g_behavior_input.perception = &g_perception_result;
            g_behavior_input.path_curvature = 0.0f;
            g_behavior_input.command =
                g_behavior_output.state == BEHAVIOR_STATE_IDLE
                    ? BEHAVIOR_CMD_START
                    : BEHAVIOR_CMD_NONE;
            status = behavior_planner_update(&g_behavior_planner, &g_behavior_input,
                                             &g_behavior_output);
        }
        if (status == SD_OK) {
            status = trajectory_generate(&g_trajectory_generator,
                                         &g_state_evaluator.state,
                                         &g_behavior_output, dt, &g_trajectory);
        }

        if (status == SD_OK) {
            float corrected_omega = SquarePath_CorrectOmega(
                g_trajectory.omega, g_perception_result.lateral_error,
                g_perception_result.heading_error, &g_square_config);

            MotionControl_SetVelocityCommand(&g_motion_control, g_trajectory.v,
                                             corrected_omega);
            SquarePath_UpdateLap(&g_lap_counter,
                                 g_trajectory_generator.last_nearest_index,
                                 SquarePath_GetPointCount(), g_target_laps);

            if (g_lap_counter.target_reached) {
                MotionControl_Stop(&g_motion_control);
            }
            g_critical_failure_count = 0;
        } else {
            if (g_critical_failure_count < 3) {
                g_critical_failure_count++;
                if (g_critical_failure_count == 3) {
                    MotionControl_EmergencyStop(&g_motion_control);
                }
            }
        }
    }
    
    /* Every cycle: Update Motion Control at 500 Hz */
    MotionControl_Update(&g_motion_control);

    g_cycle_counter++;
#endif  /* SOFTWARE_TEST_MODE */
}

/* ============================================================================
 * Emergency Stop
 * ============================================================================ */

void ControlApp_EmergencyStop(void) {
    MotionControl_EmergencyStop(&g_motion_control);
}

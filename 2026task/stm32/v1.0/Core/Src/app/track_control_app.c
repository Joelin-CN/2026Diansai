/**
 * @file      track_control_app.c
 * @brief     Track path control application implementation
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Control application for 400m track-shaped path (2 semicircles + 2 straights)
 */

#include "track_control_app.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include "uart_debug.h"
#include "track_path.h"
#include "encoder_adapter.h"
#include "motor_adapter.h"
#include "sensor_adapter.h"

#include "ir_uart_sensor.h"
#include "icm42688_hal.h"
#include "icm42688_stm32.h"
#include "motion_control.h"
#include "config.h"
#include "interface.h"
#include "preprocess.h"
#include "state_evaluate.h"
#include "perception.h"
#include "behavior_planner.h"
#include "trajectory_generate.h"

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
static track_path_config_t g_track_config;
static track_lap_counter_t g_lap_counter;

static sensor_frame_t g_sensor_frame;
static perception_result_t g_perception_result;
static behavior_input_t g_behavior_input;
static behavior_output_t g_behavior_output;
static trajectory_point_t g_trajectory;

static uint8_t g_target_laps = 0;
static unsigned g_cycle_counter = 0;
static unsigned g_critical_failure_count = 0;

static const icm42688_config_t g_icm_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float DEGREES_TO_RADIANS = 0.017453292519943295f;

/* ============================================================================
 * Initialization
 * ============================================================================ */

bool TrackControlApp_Init(uint8_t target_laps) {
    printf("[TrackControlApp] ========== Track Path Initialization ==========\n");
    printf("[TrackControlApp] Track geometry: R=0.5m, L=1.5m, perimeter≈6.14m\n");

    printf("[TrackControlApp] Step 1: Motor init...\n");
    /* Step 1: Establish a safe motor state before validating configuration */
    Motor_Init();
    Motor_Stop();

    printf("[TrackControlApp] Step 1b: Sensor release...\n");
    if (sensors_release_all() != SD_OK) {
        printf("[ERROR] sensors_release_all failed\n");
        Motor_Stop();
        return false;
    }

    printf("[TrackControlApp] Step 1c: Memory init...\n");
    memset(&g_motion_control, 0, sizeof(g_motion_control));
    memset(&g_state_evaluator, 0, sizeof(g_state_evaluator));
    memset(&g_perception, 0, sizeof(g_perception));
    memset(&g_behavior_planner, 0, sizeof(g_behavior_planner));
    memset(&g_trajectory_generator, 0, sizeof(g_trajectory_generator));
    memset(&g_track_config, 0, sizeof(g_track_config));
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
    printf("[TrackControlApp] Step 1d: Validate target laps (%u)...\n", (unsigned int)target_laps);
    if (target_laps < 1 || target_laps > 10) {
        printf("[ERROR] Invalid target_laps: %u (must be 1-10)\n", (unsigned int)target_laps);
        return false;
    }
    g_target_laps = target_laps;

    printf("[TrackControlApp] Step 2: Encoder init...\n");
    /* Step 2: Encoder_Init */
    Encoder_Init();

    printf("[TrackControlApp] Step 3: IR-tracker init (USART2, ~2s warm-up)...\n");
    /* Step 3: Initialize 8-way IR-tracker module and request analog stream */
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();

    printf("[TrackControlApp] Step 4: Config defaults and time init...\n");
    /* Step 4: Load Sens-Decision defaults and initialize platform time */
    sd_config_reset_defaults();
    PlatformTime_Init();

    printf("[TrackControlApp] Step 5: ICM42688 bind and init...\n");
    /* Step 5: Bind and initialize ICM - init checks WHO_AM_I internally */
    icm42688_stm32_bind(&g_icm_config);
    if (icm42688_init() != ICM42688_STATUS_OK) {
        printf("[WARNING] icm42688_init failed (skipping for now)\n");
        goto skip_icm42688;
    }

    printf("[TrackControlApp] Step 6: ICM42688 gyro calibration (this takes ~1 second)...\n");
    /* Step 6: Calibrate gyro bias using HAL built-in calibration */
    if (icm42688_calibrate_gyro(100U, 10U) != ICM42688_STATUS_OK) {
        printf("[WARNING] icm42688_calibrate_gyro failed\n");
        goto skip_icm42688;
    }

    printf("[TrackControlApp] Step 7: ICM42688 metadata sync...\n");
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
    printf("[TrackControlApp] Step 8: Sensor HAL configure/init...\n");
    /* Step 8: Sensor HAL configure/init */
    const sensor_hal_t *hal = SensorAdapter_GetInterface();
    if (sensors_configure_hal(hal) != SD_OK) {
        printf("[WARNING] sensors_configure_hal failed\n");
    }

    if (sensors_init_all() != SD_OK) {
        printf("[WARNING] sensors_init_all failed\n");
    }

    printf("[TrackControlApp] Step 9: Sens-Decision objects/path...\n");
    /* Step 9: Sens-Decision objects/path */
    state_evaluator_init(&g_state_evaluator, &g_sens_decision_config.ekf);
    perception_init(&g_perception);
    behavior_planner_init(&g_behavior_planner);

    trajectory_generator_init(&g_trajectory_generator, &g_sens_decision_config.trajectory);

    /* Load track path instead of square path */
    size_t track_point_count = TrackPath_GetPointCount();
    printf("[TrackControlApp] Loading track path with %u points...\n", (unsigned int)track_point_count);

    if (trajectory_set_path(&g_trajectory_generator, TrackPath_GetPoints(),
                           track_point_count) != SD_OK) {
        printf("[ERROR] trajectory_set_path failed\n");
        Motor_Stop();
        return false;
    }

    printf("[TrackControlApp] Step 10: Track path config...\n");
    /* Initialize track path config with conservative values for first run */
    g_track_config.lateral_gain = 1.5f;      /* Conservative lateral correction */
    g_track_config.heading_gain = 1.0f;      /* Conservative heading correction */
    g_track_config.max_omega_radps = 3.0f;   /* Max turn rate (rad/s) */
    g_track_config.target_laps = target_laps;
    g_track_config.line_speed_mps = 0.5f;    /* Conservative straight speed */
    g_track_config.curve_speed_mps = 0.3f;   /* Conservative curve speed */

    printf("[TrackControlApp] Config: lateral_gain=%.2f, heading_gain=%.2f\n",
           g_track_config.lateral_gain, g_track_config.heading_gain);
    printf("[TrackControlApp] Config: line_speed=%.2f m/s, curve_speed=%.2f m/s\n",
           g_track_config.line_speed_mps, g_track_config.curve_speed_mps);

    printf("[TrackControlApp] Step 11: Motion Control init/start...\n");
    /* Step 11: Motion Control init/start */
    if (!MotionControl_Init(&g_motion_control, EncoderAdapter_GetInterface(),
                           MotorAdapter_GetInterface())) {
        printf("[ERROR] MotionControl_Init failed\n");
        Motor_Stop();
        return false;
    }

    MotionControl_Start(&g_motion_control);

    printf("[TrackControlApp] ========== Initialization complete! ==========\n");
    printf("[TrackControlApp] Target: %u laps on track path\n", (unsigned int)target_laps);
    printf("[TrackControlApp] Ready to start - beginning control loop...\n");
    return true;
}

/* ============================================================================
 * Fast Cycle: Layered Frequency Architecture (Optimized 2026-07-30)
 * ============================================================================
 *
 * Frequency Layer Design:
 *   500 Hz (2ms) - Main loop, encoder sampling
 *   100 Hz (10ms) - PID controller (every 5 cycles)
 *   50 Hz (20ms) - EKF/perception/behavior (every 10 cycles)
 *
 * Rationale:
 *   - Encoder @ 500Hz: High-frequency sampling ensures accurate velocity estimation
 *   - PID @ 100Hz: Matches motor PWM response time (~10ms), saves 80% CPU vs 500Hz
 *   - EKF @ 50Hz: Computationally intensive, lower frequency sufficient for perception
 *
 * Performance Impact:
 *   - Original: MotionControl_Update() 500 times/sec
 *   - Optimized: MotionControl_Update() 100 times/sec
 *   - CPU savings: ~80% reduction in PID computation overhead
 *
 * ============================================================================ */

/**
 * @brief Main control loop - 500Hz execution
 *
 * Implements layered frequency architecture (2026-07-30 optimization):
 *
 * FREQUENCY LAYERS:
 * - Every cycle (500Hz, 2ms): Encoder sampling
 * - Every 5 cycles (100Hz, 10ms): PID control execution
 * - Every 10 cycles (50Hz, 20ms): EKF/Perception/Planning
 *
 * RATIONALE:
 * - Encoder sampling at 500Hz ensures accurate velocity estimation
 * - PID at 100Hz matches motor PWM response time (~10ms)
 * - EKF/Planning at 50Hz reduces computational load for heavy algorithms
 * - Original 500Hz PID wasted 80% CPU on unnecessary updates
 *
 * TIMING ALIGNMENT:
 * Cycle 0:  Encoder + PID + EKF (all layers)
 * Cycle 1-4: Encoder only
 * Cycle 5:  Encoder + PID
 * Cycle 6-9: Encoder only
 * Cycle 10: Wraps to 0 (all layers again)
 *
 * @note Call this function every 2ms from FreeRTOS task
 * @note Modified 2026-07-30: Added frequency layering for performance
 *
 * @see docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt for detailed analysis
 */
void TrackControlApp_RunFastCycle(void) {
    /* -----------------------------------------------------------------------
     * Layer 1: 500Hz - Encoder Sampling (every cycle)
     * High-frequency encoder polling is critical for accurate velocity estimation
     * ----------------------------------------------------------------------- */
    Encoder_Poll();

    /* -----------------------------------------------------------------------
     * Layer 2: 100Hz - PID Control (every 5 cycles = 10ms)
     * Motor PWM response time is ~10ms, so 100Hz control frequency is sufficient
     * ----------------------------------------------------------------------- */
    if ((g_cycle_counter % 5U) == 0U) {
        MotionControl_Update(&g_motion_control);
    }

    /* -----------------------------------------------------------------------
     * Layer 3: 50Hz - EKF/Perception/Decision (every 10 cycles = 20ms)
     * Computationally intensive pipeline runs at lower frequency
     * ----------------------------------------------------------------------- */
    if ((g_cycle_counter % 10U) == 0U) {
        const float dt = 0.020f;  /* 50 Hz = 20ms */

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
            /* Apply track-specific correction to omega */
            float corrected_omega = TrackPath_CorrectOmega(
                g_trajectory.omega, g_perception_result.lateral_error,
                g_perception_result.heading_error, &g_track_config);

            MotionControl_SetVelocityCommand(&g_motion_control, g_trajectory.v,
                                             corrected_omega);

            /* Update lap counter */
            size_t nearest_index = g_trajectory_generator.last_nearest_index;
            if (TrackPath_UpdateLap(&g_lap_counter, nearest_index,
                                    TrackPath_GetPointCount(), g_target_laps)) {
                printf("[TrackControlApp] Lap %u completed!\n",
                       (unsigned int)g_lap_counter.completed_laps);
            }

            /* Check if target reached */
            if (g_lap_counter.target_reached) {
                printf("[TrackControlApp] *** Target %u laps completed! Stopping... ***\n",
                       (unsigned int)g_target_laps);
                MotionControl_Stop(&g_motion_control);
                Motor_Stop();
            }

            /* Reset failure counter on success */
            g_critical_failure_count = 0;
        } else {
            g_critical_failure_count++;

            /* Print detailed error info every 10 failures */
            if (g_critical_failure_count % 10 == 1) {
                printf("[DEBUG] Failure #%u: preprocess=%d, ir_valid=%d, imu_valid=%d\n",
                       g_critical_failure_count,
                       status,
                       g_sensor_frame.ir_valid,
                       g_sensor_frame.imu_valid);
            }

            if (g_critical_failure_count > 50U) {  /* 1 second at 50Hz */
                printf("[ERROR] Critical failure - stopping motor\n");
                MotionControl_Stop(&g_motion_control);
                Motor_Stop();
            }
        }
    }

    /* Increment cycle counter (wraps at 10 to align with 50Hz layer) */
    g_cycle_counter++;
    if (g_cycle_counter >= 10U) {
        g_cycle_counter = 0U;
    }
}

/* ============================================================================
 * Status Query Functions
 * ============================================================================ */

bool TrackControlApp_IsComplete(void) {
    return g_lap_counter.target_reached;
}

uint8_t TrackControlApp_GetCompletedLaps(void) {
    return g_lap_counter.completed_laps;
}

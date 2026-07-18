/**
 * @file control_app.c
 * @brief Application coordinator implementation
 * @date 2026-07-18
 */

#include "control_app.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include "square_path.h"
#include "encoder_adapter.h"
#include "motor_adapter.h"
#include "sensor_adapter.h"

#include "../modules/MCP23017/inc/mcp23017.h"
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

bool ControlApp_Init(uint8_t target_laps) {
    /* Step 1: Establish a safe motor state before validating configuration */
    Motor_Init();
    Motor_Stop();

    /* Validate target laps */
    if (target_laps < 1 || target_laps > 5) {
        return false;
    }
    g_target_laps = target_laps;
    
    /* Step 2: Encoder_Init */
    Encoder_Init();
    
    /* Step 3: MCP23017_Init and read */
    if (MCP23017_Init() != MCP23017_STATUS_OK) {
        Motor_Stop();
        return false;
    }
    
    uint16_t ir_mask = 0;
    if (MCP23017_ReadInputs(&ir_mask) != MCP23017_STATUS_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 4: Load Sens-Decision defaults and initialize platform time */
    sd_config_reset_defaults();
    PlatformTime_Init();
    
    /* Step 5: Bind and initialize ICM - init checks WHO_AM_I internally */
    icm42688_mspm0_bind(&g_icm_config);
    if (icm42688_init() != ICM42688_STATUS_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 6: Calibrate gyro bias using HAL built-in calibration */
    if (icm42688_calibrate_gyro(100U, 10U) != ICM42688_STATUS_OK) {
        Motor_Stop();
        return false;
    }

    /* Step 7: Synchronize calibrated ICM metadata into Sens-Decision SI units */
    float accel_g_per_lsb;
    float gyro_dps_per_lsb;
    icm42688_vector3f_t gyro_bias_dps;

    if (icm42688_get_scale_factors(&accel_g_per_lsb, &gyro_dps_per_lsb) !=
            ICM42688_STATUS_OK ||
        icm42688_get_gyro_bias(&gyro_bias_dps) != ICM42688_STATUS_OK) {
        Motor_Stop();
        return false;
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

    /* Step 8: Sensor HAL configure/init */
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    if (sensors_configure_hal(hal) != SD_OK) {
        Motor_Stop();
        return false;
    }
    
    if (sensors_init_all() != SD_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 9: Sens-Decision objects/path */
    state_evaluator_init(&g_state_evaluator, &g_sens_decision_config.ekf);
    perception_init(&g_perception);
    behavior_planner_init(&g_behavior_planner);
    
    trajectory_generator_init(&g_trajectory_generator, &g_sens_decision_config.trajectory);
    if (trajectory_set_path(&g_trajectory_generator, SquarePath_GetPoints(), 
                           SquarePath_GetPointCount()) != SD_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Initialize square path config */
    g_square_config.lateral_gain = 2.0f;
    g_square_config.heading_gain = 1.5f;
    g_square_config.max_omega_radps = 2.0f;
    g_square_config.target_laps = target_laps;
    
    /* Initialize lap counter */
    memset(&g_lap_counter, 0, sizeof(g_lap_counter));
    
    /* Step 10: Motion Control init/start */
    if (!MotionControl_Init(&g_motion_control, EncoderAdapter_GetInterface(), 
                           MotorAdapter_GetInterface())) {
        Motor_Stop();
        return false;
    }
    
    MotionControl_Start(&g_motion_control);
    
    /* Reset cycle counter */
    g_cycle_counter = 0;
    g_critical_failure_count = 0;
    
    return true;
}

/* ============================================================================
 * Fast Cycle: 500 Hz Motion Control, 50 Hz Sens-Decision
 * ============================================================================ */

void ControlApp_RunFastCycle(void) {
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
            g_critical_failure_count++;
            if (g_critical_failure_count >= 3) {
                MotionControl_EmergencyStop(&g_motion_control);
            }
        }
    }
    
    /* Every cycle: Update Motion Control at 500 Hz */
    MotionControl_Update(&g_motion_control);
    
    g_cycle_counter++;
}

/* ============================================================================
 * Emergency Stop
 * ============================================================================ */

void ControlApp_EmergencyStop(void) {
    MotionControl_EmergencyStop(&g_motion_control);
}

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

/* ============================================================================
 * Initialization
 * ============================================================================ */

bool ControlApp_Init(uint8_t target_laps) {
    /* Validate target laps */
    if (target_laps < 1 || target_laps > 5) {
        return false;
    }
    g_target_laps = target_laps;
    
    /* Step 1: Motor_Init -> Motor_Stop */
    Motor_Init();
    Motor_Stop();
    
    /* Step 2: Encoder_Init */
    Encoder_Init();
    
    /* Step 3: MCP23017_Init and read */
    if (!MCP23017_Init()) {
        Motor_Stop();
        return false;
    }
    
    uint16_t ir_mask = 0;
    if (!MCP23017_ReadInputs(&ir_mask)) {
        Motor_Stop();
        return false;
    }
    
    /* Step 4: PlatformTime_Init */
    PlatformTime_Init();
    
    /* Step 5: ICM bind/init - icm42688_init checks WHO_AM_I internally */
    if (icm42688_init() != ICM42688_STATUS_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 6: Calibrate gyro bias using HAL built-in calibration */
    if (icm42688_calibrate_gyro(100, 10) != ICM42688_STATUS_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 7: Sensor HAL configure/init */
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    if (sensors_configure_hal(hal) != SD_OK) {
        Motor_Stop();
        return false;
    }
    
    if (sensors_init_all() != SD_OK) {
        Motor_Stop();
        return false;
    }
    
    /* Step 8: Sens-Decision objects/path */
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
    
    /* Step 9: Motion Control init/start */
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
        if (status != SD_OK) {
            g_critical_failure_count++;
            if (g_critical_failure_count >= 3) {
                MotionControl_EmergencyStop(&g_motion_control);
            }
        } else {
            g_critical_failure_count = 0;
        }
        
        /* State evaluation */
        status = state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
        
        /* Perception */
        status = perception_update(&g_perception, &g_sensor_frame.ir,
                                  g_sensor_frame.timestamp_us, &g_perception_result);
        
        /* Behavior planning with persistent start command */
        g_behavior_input.vehicle = &g_state_evaluator.state;
        g_behavior_input.perception = &g_perception_result;
        g_behavior_input.path_curvature = 0.0f;  /* Filled by trajectory generator */
        
        /* Keep sending BEHAVIOR_CMD_START until planner exits IDLE state */
        if (g_behavior_output.state == BEHAVIOR_STATE_IDLE) {
            g_behavior_input.command = BEHAVIOR_CMD_START;
        } else {
            g_behavior_input.command = BEHAVIOR_CMD_NONE;
        }
        
        status = behavior_planner_update(&g_behavior_planner, &g_behavior_input, 
                                        &g_behavior_output);
        
        /* Trajectory generation */
        status = trajectory_generate(&g_trajectory_generator, &g_state_evaluator.state,
                                    &g_behavior_output, dt, &g_trajectory);
        
        /* Apply IR corrections to omega */
        float corrected_omega = SquarePath_CorrectOmega(g_trajectory.omega,
                                                       g_perception_result.lateral_error,
                                                       g_perception_result.heading_error,
                                                       &g_square_config);
        
        /* Set velocity command for Motion Control */
        MotionControl_SetVelocityCommand(&g_motion_control, g_trajectory.v, corrected_omega);
        
        /* Update lap counter */
        if (SquarePath_UpdateLap(&g_lap_counter, g_trajectory_generator.last_nearest_index,
                                SquarePath_GetPointCount(), g_target_laps)) {
            /* Lap incremented */
        }
        
        /* Check if target laps reached */
        if (g_lap_counter.target_reached) {
            MotionControl_Stop(&g_motion_control);
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

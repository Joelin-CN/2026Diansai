/**
 * @file      playground_track.c
 * @brief     Playground track (操场型循迹) - Implementation
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Segment-aware adaptive line-following for competition tasks 2 & 4
 */

#include "playground_track.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
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
#include "perception.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Private Types
 * ============================================================================ */

/**
 * @brief Internal state machine states
 */
typedef enum {
    PT_IDLE,                    /**< Initial state, waiting for line detection */
    PT_TASK2_RUN,              /**< Task 2: Running lap with segment-aware control */
    PT_TASK2_APPROACH_A,       /**< Task 2: Final approach to A-line, decelerating */
    PT_TASK4_ACCEL,            /**< Task 4: Acceleration phase */
    PT_TASK4_CRUISE,           /**< Task 4: Constant speed cruise */
    PT_TASK4_DECEL,            /**< Task 4: Deceleration phase */
    PT_STOPPED,                /**< Task complete, motors stopped */
    PT_FAULT,                  /**< Error state, line lost too long */
} pt_state_t;

/**
 * @brief Configuration parameters for playground track
 */
typedef struct {
    /* Task 2 segment speeds */
    float v_straight;          /**< Speed on straight segments (m/s) */
    float v_curve;             /**< Speed on curve segments (m/s) */
    float v_approach;          /**< Speed during final approach (m/s) */

    /* Task 2 PD gains for straights */
    float kp_straight;         /**< Proportional gain for lateral error */
    float kd_straight;         /**< Derivative gain for heading error */
    float omega_max_straight;  /**< Max angular velocity (rad/s) */

    /* Task 2 PD gains for curves */
    float kp_curve;
    float kd_curve;
    float omega_max_curve;

    /* Task 2 PD gains for approach */
    float kp_approach;
    float kd_approach;
    float omega_max_approach;

    /* Task 4 trapezoid profile */
    float v_task4_max;         /**< Maximum speed for task 4 (m/s) */
    float a_task4;             /**< Acceleration/deceleration (m/s²) */
    float kp_task4;            /**< Lateral correction gain */
    float kd_task4;            /**< Heading correction gain */
    float omega_max_task4;     /**< Max angular velocity (rad/s) */

    /* Segment boundaries (cumulative distance from A) */
    float dist_ab_end;         /**< End of A→B straight (m) */
    float dist_bc_end;         /**< End of B→C curve (m) */
    float dist_cd_end;         /**< End of C→D straight (m) */
    float dist_da_early;       /**< End of D→A early curve (m) */
    float approach_start_dist; /**< Distance to switch to approach speed (m) */

    /* A-line detection */
    uint8_t transverse_min_ch; /**< Min channels to detect A-line */
    float a_detect_min_dist;   /**< Min distance before detecting A-line (m) */

    /* Task 4 distance parameters */
    float d_decel_start;       /**< Distance to start deceleration (m) */

    /* Fault detection */
    uint16_t line_lost_fault_lap;  /**< Frames before fault in lap mode */
    uint16_t line_lost_fault_ab;   /**< Frames before fault in AB mode */
} pg_config_t;

/* ============================================================================
 * Static Variables
 * ============================================================================ */

static MotionControl_t g_mc;
static perception_t g_perc;
static sensor_frame_t g_sf;
static perception_result_t g_res;
static pg_config_t g_cfg;

static playground_task_t g_task;
static pt_state_t g_state;
static float g_dist_m;
static float g_v_cmd;
static unsigned g_cyc;
static uint16_t g_line_valid_count;
static uint16_t g_line_lost_count;

static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float DEGREES_TO_RADIANS = 0.017453292519943295f;

static const icm42688_config_t g_icm_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void pg_init_config(playground_task_t task);
static void pg_decide_50hz(void);
static void pg_state_machine(void);
static uint8_t pg_count_active_channels(uint16_t mask);
static float pg_clamp(float val, float min, float max);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

bool PlaygroundTrack_Init(playground_task_t task) {
    printf("[PlaygroundTrack] ========== Initialization ==========\n");
    printf("[PlaygroundTrack] Task: %s\n",
           task == PLAYGROUND_TASK_LAP ? "Task 2 (Full Lap)" : "Task 4 (A→B Straight)");

    /* Clear all state */
    memset(&g_mc, 0, sizeof(g_mc));
    memset(&g_perc, 0, sizeof(g_perc));
    memset(&g_sf, 0, sizeof(g_sf));
    memset(&g_res, 0, sizeof(g_res));
    memset(&g_cfg, 0, sizeof(g_cfg));

    g_task = task;
    g_state = PT_IDLE;
    g_dist_m = 0.0f;
    g_v_cmd = 0.0f;
    g_cyc = 0;
    g_line_valid_count = 0;
    g_line_lost_count = 0;

    /* Initialize configuration parameters */
    pg_init_config(task);

    /* Step 1: Motor initialization and safe stop */
    printf("[PlaygroundTrack] Step 1: Motor init...\n");
    Motor_Init();
    Motor_Stop();

    /* Step 2: Encoder initialization */
    printf("[PlaygroundTrack] Step 2: Encoder init...\n");
    Encoder_Init();

    /* Step 3: IR sensor initialization */
    printf("[PlaygroundTrack] Step 3: IR sensor init (~2s warm-up)...\n");
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();

    /* Step 4: Config defaults and time */
    printf("[PlaygroundTrack] Step 4: Config and time init...\n");
    sd_config_reset_defaults();
    PlatformTime_Init();

    /* Step 5: IMU initialization (graceful skip on failure) */
    printf("[PlaygroundTrack] Step 5: ICM42688 init...\n");
    icm42688_stm32_bind(&g_icm_config);
    if (icm42688_init() != ICM42688_STATUS_OK) {
        printf("[WARNING] ICM42688 init failed (skipping)\n");
        goto skip_imu;
    }

    printf("[PlaygroundTrack] Step 6: ICM42688 gyro calibration (~1s)...\n");
    if (icm42688_calibrate_gyro(100U, 10U) != ICM42688_STATUS_OK) {
        printf("[WARNING] ICM42688 calibration failed\n");
        goto skip_imu;
    }

    /* Sync IMU metadata to Sens-Decision config */
    float accel_g_per_lsb, gyro_dps_per_lsb;
    icm42688_vector3f_t gyro_bias_dps;

    if (icm42688_get_scale_factors(&accel_g_per_lsb, &gyro_dps_per_lsb) == ICM42688_STATUS_OK &&
        icm42688_get_gyro_bias(&gyro_bias_dps) == ICM42688_STATUS_OK) {

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
    }

skip_imu:
    /* Step 7: Sensor HAL configuration */
    printf("[PlaygroundTrack] Step 7: Sensor HAL configure...\n");
    const sensor_hal_t *hal = SensorAdapter_GetInterface();
    if (sensors_configure_hal(hal) != SD_OK) {
        printf("[FATAL] sensors_configure_hal failed\n");
        Motor_Stop();
        return false;
    }

    if (sensors_init_all() != SD_OK) {
        printf("[FATAL] sensors_init_all failed\n");
        printf("[FATAL] Check:\n");
        printf("  1. Encoder connections (TIM3=Left, TIM4=Right)\n");
        printf("  2. IR sensor UART (USART2, 115200)\n");
        printf("  3. IMU SPI (SPI2, ICM42688)\n");
        Motor_Stop();
        return false;
    }

    /* Step 8: Perception module initialization */
    printf("[PlaygroundTrack] Step 8: Perception init...\n");
    perception_init(&g_perc);

    /* Step 9: Motion control initialization */
    printf("[PlaygroundTrack] Step 9: MotionControl init...\n");
    if (!MotionControl_Init(&g_mc, EncoderAdapter_GetInterface(),
                           MotorAdapter_GetInterface())) {
        printf("[ERROR] MotionControl_Init failed\n");
        Motor_Stop();
        return false;
    }

    MotionControl_Start(&g_mc);

    printf("[PlaygroundTrack] ========== Init Complete! ==========\n");
    printf("[PlaygroundTrack] Track geometry: 2×1.5m straights + 2×πR curves (R=0.5m)\n");
    printf("[PlaygroundTrack] Total perimeter: ~6.14 m\n");
    printf("[PlaygroundTrack] Ready to start control loop...\n");

    return true;
}

void PlaygroundTrack_RunFastCycle(void) {
    /* Layer 1: 500Hz - Encoder sampling */
    Encoder_Poll();

    /* Layer 2: 100Hz - PID control (every 5 cycles) */
    if ((g_cyc % 5) == 0) {
        MotionControl_Update(&g_mc);
    }

    /* Layer 3: 50Hz - Perception and decision (every 10 cycles) */
    if ((g_cyc % 10) == 0) {
        pg_decide_50hz();
    }

    /* Increment and wrap cycle counter */
    if (++g_cyc >= 10) {
        g_cyc = 0;
    }
}

bool PlaygroundTrack_IsComplete(void) {
    return (g_state == PT_STOPPED);
}

float PlaygroundTrack_GetDistance(void) {
    return g_dist_m;
}

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Initialize configuration parameters based on task
 */
static void pg_init_config(playground_task_t task) {
    /* Task 2 segment speeds (HALF SPEED FOR TESTING) */
    g_cfg.v_straight = 0.50f;  /* 1.00 → 0.50 m/s */
    g_cfg.v_curve = 0.30f;     /* 0.60 → 0.30 m/s */
    g_cfg.v_approach = 0.125f; /* 0.25 → 0.125 m/s */

    /* Task 2 PD gains - straights (REDUCED FOR TESTING - prevent oscillation) */
    g_cfg.kp_straight = 0.5f;   /* 1.5 → 0.5 (reduced to 1/3) */
    g_cfg.kd_straight = 0.3f;   /* 1.0 → 0.3 (reduced to 1/3) */
    g_cfg.omega_max_straight = 2.0f;  /* 3.0 → 2.0 */

    /* Task 2 PD gains - curves (REDUCED FOR TESTING) */
    g_cfg.kp_curve = 1.0f;      /* 2.5 → 1.0 (reduced to ~40%) */
    g_cfg.kd_curve = 0.6f;      /* 1.5 → 0.6 (reduced to 40%) */
    g_cfg.omega_max_curve = 2.0f;  /* 3.0 → 2.0 */

    /* Task 2 PD gains - approach (REDUCED FOR TESTING) */
    g_cfg.kp_approach = 0.8f;   /* 2.0 → 0.8 (reduced to 40%) */
    g_cfg.kd_approach = 0.5f;   /* 1.2 → 0.5 (reduced to ~40%) */
    g_cfg.omega_max_approach = 1.5f;  /* 2.0 → 1.5 */

    /* Task 4 parameters (HALF SPEED FOR TESTING) */
    g_cfg.v_task4_max = 0.25f;  /* 0.50 → 0.25 m/s */
    g_cfg.a_task4 = 0.15f;      /* 0.30 → 0.15 m/s² */
    g_cfg.kp_task4 = 0.3f;      /* 0.8 → 0.3 (reduced to ~40%) */
    g_cfg.kd_task4 = 0.2f;      /* 0.5 → 0.2 (reduced to 40%) */
    g_cfg.omega_max_task4 = 0.8f;  /* 1.0 → 0.8 */

    /* Segment boundaries (meters) */
    g_cfg.dist_ab_end = 1.500f;
    g_cfg.dist_bc_end = 3.071f;
    g_cfg.dist_cd_end = 4.571f;
    g_cfg.dist_da_early = 5.000f;
    g_cfg.approach_start_dist = 5.000f;

    /* A-line detection */
    g_cfg.transverse_min_ch = 6;
    g_cfg.a_detect_min_dist = 5.5f;

    /* Task 4 deceleration start */
    float d_ramp = (g_cfg.v_task4_max * g_cfg.v_task4_max) / (2.0f * g_cfg.a_task4);
    g_cfg.d_decel_start = g_cfg.dist_ab_end - d_ramp;

    /* Fault detection thresholds */
    g_cfg.line_lost_fault_lap = 10;  /* 200ms at 50Hz */
    g_cfg.line_lost_fault_ab = 5;    /* 100ms at 50Hz */

    printf("[PlaygroundTrack] Config loaded:\n");
    if (task == PLAYGROUND_TASK_LAP) {
        printf("  v_straight=%.2f, v_curve=%.2f, v_approach=%.2f\n",
               g_cfg.v_straight, g_cfg.v_curve, g_cfg.v_approach);
        printf("  kp_straight=%.2f, kp_curve=%.2f\n",
               g_cfg.kp_straight, g_cfg.kp_curve);
    } else {
        printf("  v_max=%.2f, a=%.2f, d_decel_start=%.2f\n",
               g_cfg.v_task4_max, g_cfg.a_task4, g_cfg.d_decel_start);
    }
}

/**
 * @brief 50Hz decision layer
 */
static void pg_decide_50hz(void) {
    const float dt = 0.020f;  /* 50 Hz = 20ms */

    /* Preprocess sensor data */
    sd_status_t status = preprocess_update(PlatformTime_GetUs64(), &g_sf);

    /* Update perception (lateral_error, heading_error, active_mask) */
    bool line_valid = false;
    if (status == SD_OK) {
        status = perception_update(&g_perc, &g_sf.ir, g_sf.timestamp_us, &g_res);
        line_valid = (status == SD_OK);
    }

    /* Track line validity for state transitions */
    if (line_valid) {
        g_line_valid_count++;
        g_line_lost_count = 0;
    } else {
        g_line_valid_count = 0;
        g_line_lost_count++;
    }

    /* Integrate distance (only when moving) */
    if (g_state != PT_IDLE && g_state != PT_STOPPED && g_state != PT_FAULT) {
        float vl, vr;
        MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
        g_dist_m += (vl + vr) * 0.5f * dt;
    }

    /* State machine */
    pg_state_machine();
}

/**
 * @brief Main state machine
 */
static void pg_state_machine(void) {
    const float dt = 0.020f;
    float omega = 0.0f;
    float kp, kd, omega_max;

    switch (g_state) {
        case PT_IDLE:
            /* Wait for stable line detection */
            if (g_line_valid_count >= 3) {
                if (g_task == PLAYGROUND_TASK_LAP) {
                    printf("[PlaygroundTrack] Line detected, starting Task 2 lap...\n");
                    g_state = PT_TASK2_RUN;
                    g_dist_m = 0.0f;
                } else {
                    printf("[PlaygroundTrack] Line detected, starting Task 4 accel...\n");
                    g_state = PT_TASK4_ACCEL;
                    g_dist_m = 0.0f;
                    g_v_cmd = 0.0f;
                }
            }
            MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);
            break;

        case PT_TASK2_RUN:
            /* Check for line loss fault */
            if (g_line_lost_count > g_cfg.line_lost_fault_lap) {
                printf("[ERROR] Line lost for >200ms, entering fault state\n");
                g_state = PT_FAULT;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
                break;
            }

            /* Select speed and gains based on distance (segment-aware) */
            if (g_dist_m < g_cfg.dist_ab_end) {
                /* Straight A→B */
                g_v_cmd = g_cfg.v_straight;
                kp = g_cfg.kp_straight;
                kd = g_cfg.kd_straight;
                omega_max = g_cfg.omega_max_straight;
            } else if (g_dist_m < g_cfg.dist_bc_end) {
                /* Curve B→C */
                g_v_cmd = g_cfg.v_curve;
                kp = g_cfg.kp_curve;
                kd = g_cfg.kd_curve;
                omega_max = g_cfg.omega_max_curve;
            } else if (g_dist_m < g_cfg.dist_cd_end) {
                /* Straight C→D */
                g_v_cmd = g_cfg.v_straight;
                kp = g_cfg.kp_straight;
                kd = g_cfg.kd_straight;
                omega_max = g_cfg.omega_max_straight;
            } else if (g_dist_m < g_cfg.approach_start_dist) {
                /* Curve D→A (early) */
                g_v_cmd = g_cfg.v_curve;
                kp = g_cfg.kp_curve;
                kd = g_cfg.kd_curve;
                omega_max = g_cfg.omega_max_curve;
            } else {
                /* Curve D→A (approach) */
                g_v_cmd = g_cfg.v_approach;
                kp = g_cfg.kp_approach;
                kd = g_cfg.kd_approach;
                omega_max = g_cfg.omega_max_approach;
            }

            /* Check for A-line detection (transverse line) */
            if (g_dist_m > g_cfg.a_detect_min_dist) {
                uint8_t active_count = pg_count_active_channels(g_res.active_mask);
                if (active_count >= g_cfg.transverse_min_ch) {
                    printf("[PlaygroundTrack] A-line detected! dist=%.3fm, switching to approach...\n",
                           g_dist_m);
                    g_state = PT_TASK2_APPROACH_A;
                    break;
                }
            }

            /* Calculate omega using PD control */
            omega = -(kp * g_res.lateral_error + kd * g_res.heading_error);
            omega = pg_clamp(omega, -omega_max, omega_max);

            MotionControl_SetVelocityCommand(&g_mc, g_v_cmd, omega);
            break;

        case PT_TASK2_APPROACH_A:
            /* Command zero velocity, MotionControl handles deceleration */
            MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);

            /* Check if stopped */
            float vl, vr;
            MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
            float v_actual = (vl + vr) * 0.5f;

            if (fabsf(v_actual) < 0.05f) {
                printf("[PlaygroundTrack] *** Task 2 Complete! Final dist=%.3fm ***\n",
                       g_dist_m);
                g_state = PT_STOPPED;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
            }
            break;

        case PT_TASK4_ACCEL:
            /* Check for line loss fault */
            if (g_line_lost_count > g_cfg.line_lost_fault_ab) {
                printf("[ERROR] Line lost during Task 4 accel\n");
                g_state = PT_FAULT;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
                break;
            }

            /* Ramp up speed */
            g_v_cmd += g_cfg.a_task4 * dt;
            if (g_v_cmd >= g_cfg.v_task4_max) {
                g_v_cmd = g_cfg.v_task4_max;
                printf("[PlaygroundTrack] Task 4: Reached cruise speed\n");
                g_state = PT_TASK4_CRUISE;
            }

            /* Apply gentle lateral correction */
            omega = -(g_cfg.kp_task4 * g_res.lateral_error +
                     g_cfg.kd_task4 * g_res.heading_error);
            omega = pg_clamp(omega, -g_cfg.omega_max_task4, g_cfg.omega_max_task4);

            MotionControl_SetVelocityCommand(&g_mc, g_v_cmd, omega);
            break;

        case PT_TASK4_CRUISE:
            /* Check for line loss fault */
            if (g_line_lost_count > g_cfg.line_lost_fault_ab) {
                printf("[ERROR] Line lost during Task 4 cruise\n");
                g_state = PT_FAULT;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
                break;
            }

            /* Check if deceleration point reached */
            if (g_dist_m >= g_cfg.d_decel_start) {
                printf("[PlaygroundTrack] Task 4: Starting deceleration at %.3fm\n",
                       g_dist_m);
                g_state = PT_TASK4_DECEL;
            }

            /* Maintain constant speed with lateral correction */
            omega = -(g_cfg.kp_task4 * g_res.lateral_error +
                     g_cfg.kd_task4 * g_res.heading_error);
            omega = pg_clamp(omega, -g_cfg.omega_max_task4, g_cfg.omega_max_task4);

            MotionControl_SetVelocityCommand(&g_mc, g_v_cmd, omega);
            break;

        case PT_TASK4_DECEL:
            /* Ramp down speed */
            g_v_cmd -= g_cfg.a_task4 * dt;
            if (g_v_cmd <= 0.0f) {
                g_v_cmd = 0.0f;
                printf("[PlaygroundTrack] *** Task 4 Complete! Final dist=%.3fm ***\n",
                       g_dist_m);
                g_state = PT_STOPPED;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
                break;
            }

            /* Continue lateral correction during decel */
            omega = -(g_cfg.kp_task4 * g_res.lateral_error +
                     g_cfg.kd_task4 * g_res.heading_error);
            omega = pg_clamp(omega, -g_cfg.omega_max_task4, g_cfg.omega_max_task4);

            MotionControl_SetVelocityCommand(&g_mc, g_v_cmd, omega);
            break;

        case PT_STOPPED:
        case PT_FAULT:
            /* Do nothing, motors already stopped */
            break;
    }
}

/**
 * @brief Count number of active channels in bitmask
 */
static uint8_t pg_count_active_channels(uint16_t mask) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 8; i++) {
        count += (uint8_t)((mask >> i) & 1U);
    }
    return count;
}

/**
 * @brief Clamp value to range [min, max]
 */
static float pg_clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

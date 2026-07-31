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
#include "spi_diagnostic.h"
#include "motion_control.h"
#include "motion_config.h"
#include "config.h"
#include "interface.h"
#include "preprocess.h"
#include "perception.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef GITHUB_MODE_SWITCH_PROFILE
#define GITHUB_MODE_SWITCH_PROFILE 0
#endif

#ifndef FAST_STEERING_TEST_PROFILE
#define FAST_STEERING_TEST_PROFILE 0
#endif

#ifndef FIFTEEN_SECOND_LAP_PROFILE
#define FIFTEEN_SECOND_LAP_PROFILE 0
#endif

#ifndef HIGH_SPEED_YAW_INNER_TEST_PROFILE
#define HIGH_SPEED_YAW_INNER_TEST_PROFILE 0
#endif

#ifndef INDEPENDENT_WHEEL_PI_TEST_PROFILE
#define INDEPENDENT_WHEEL_PI_TEST_PROFILE 0
#endif

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
    float v_precision;         /**< Speed in final precision-stop zone (m/s) */

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
    float precision_start_dist;/**< Distance to switch to precision speed (m) */

    /* A-line detection */
    uint8_t transverse_min_ch; /**< Min channels to detect A-line */
    uint8_t transverse_confirm_frames; /**< Consecutive frames required */
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
static uint8_t g_transverse_count;
static uint16_t g_decision_count;
static bool g_imu_available;
static float g_last_omega_cmd;
static float g_filtered_omega_cmd;
static float g_last_v_output;
static float g_last_valid_line_error;
static float g_last_gyro_z_radps;
static bool g_initialized;
#if GITHUB_MODE_SWITCH_PROFILE
static float g_last_line_omega_target;
static bool g_imu_steering_active;
static uint8_t g_imu_control_mode;
#if FAST_STEERING_TEST_PROFILE || HIGH_SPEED_YAW_INNER_TEST_PROFILE
static bool g_gyro_filter_initialized;
#endif
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
static bool g_yaw_inner_sample_valid;
#endif
#endif

static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float DEGREES_TO_RADIANS = 0.017453292519943295f;
static const float TRACK_CURVE_RADIUS_M = 0.50f;
static const float IR_ARRAY_LOOKAHEAD_M = 0.183f;
static const float TRACK_PERIMETER_M = 6.14159265f;

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
static float pg_steering_correction(float kp, float kd, float omega_max,
                                    float omega_feedforward);
static float pg_follow_speed(float segment_speed);
static bool pg_probe_imu_data(void);
static float pg_slew_omega(float target, float dt);
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
static void pg_update_yaw_inner_100hz(void);
#endif

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

bool PlaygroundTrack_Init(playground_task_t task) {
    printf("[PlaygroundTrack] ========== Initialization ==========\n");
    printf("[PlaygroundTrack] Boot calibration profile: Task 2 proven controller\n");

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
    g_transverse_count = 0;
    g_decision_count = 0;
    g_imu_available = false;
    g_last_omega_cmd = 0.0f;
    g_filtered_omega_cmd = 0.0f;
    g_last_v_output = 0.0f;
    g_last_valid_line_error = 0.0f;
    g_last_gyro_z_radps = 0.0f;
    g_initialized = false;
#if GITHUB_MODE_SWITCH_PROFILE
    g_last_line_omega_target = 0.0f;
    g_imu_steering_active = false;
    g_imu_control_mode = 0U;
#if FAST_STEERING_TEST_PROFILE || HIGH_SPEED_YAW_INNER_TEST_PROFILE
    g_gyro_filter_initialized = false;
#endif
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
    g_yaw_inner_sample_valid = false;
#endif
#endif

    /* Initialize configuration parameters */
    pg_init_config(task);

    /* Step 1: Motor initialization and safe stop */
    printf("[PlaygroundTrack] Step 1: Motor init...\n");
    Motor_Init();
    Motor_Stop();
    printf("[Hardware] Motor map: B=LEFT (positive command, no extra inversion), "
           "C=RIGHT (positive command)\n");

    /* Step 2: Encoder initialization */
    printf("[PlaygroundTrack] Step 2: Encoder init...\n");
    Encoder_Init();

    /* Step 3: IR sensor initialization */
    printf("[PlaygroundTrack] Step 3: IR sensor init (warms during IMU calibration)...\n");
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();

    /* Step 4: Config defaults and time */
    printf("[PlaygroundTrack] Step 4: Config and time init...\n");
    sd_config_reset_defaults();
    PlatformTime_Init();

    /* Step 5: IMU initialization (graceful skip on failure) */
    printf("[PlaygroundTrack] Step 5: ICM42688 init...\n");
    icm42688_stm32_bind(&g_icm_config);
    /*
     * Preserve the sequence proven by the original standalone v1.0 IMU test:
     * its first diagnostic read could return 0x00, while the immediately
     * following formal initialization consistently read WHO_AM_I=0x47 and
     * produced valid accel/gyro samples.  The diagnostic transactions act as
     * a bus/sensor warm-up on this module.
     */
    SpiDiag_TestICM42688();
    icm42688_status_t imu_status = icm42688_init();
    if (imu_status != ICM42688_STATUS_OK) {
        printf("[WARNING] ICM42688 init failed, status=%d "
               "(2=WHO_AM_I mismatch; expected 0x47)\n",
               (int)imu_status);
        if (imu_status != ICM42688_STATUS_BAD_ID || !pg_probe_imu_data()) {
            printf("[WARNING] No plausible live IMU samples; "
                   "Task 2 will use steering feed-forward fallback\n");
            goto skip_imu;
        }
        printf("[WARNING] WHO_AM_I unavailable, but live acceleration is "
               "plausible; enabling v1.0-compatible IMU data path\n");
    }

    printf("[PlaygroundTrack] Step 6: ICM42688 gyro calibration (~1s)...\n");
    imu_status = icm42688_calibrate_gyro(100U, 10U);
    if (imu_status != ICM42688_STATUS_OK) {
        printf("[WARNING] ICM42688 calibration failed, status=%d; "
               "Task 2 will use steering feed-forward fallback\n",
               (int)imu_status);
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
        g_imu_available = true;
    }

skip_imu:
    printf("[PlaygroundTrack] IMU: %s "
           "(gyro yaw-rate feedback when available; safe fallback otherwise)\n",
           g_imu_available ? "available" : "unavailable");

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

    /*
     * Hardware and sensor calibration are complete, but a competition task
     * must not move before its key is pressed. StartTask() resets and starts
     * the controller immediately when the debounced key event arrives.
     */
    MotionControl_Stop(&g_mc);
    Motor_Stop();
    g_initialized = true;

    printf("[PlaygroundTrack] ========== Init Complete! ==========\n");
    printf("[PlaygroundTrack] Track geometry: 2×1.5m straights + 2×πR curves (R=0.5m)\n");
    printf("[PlaygroundTrack] Total perimeter: ~6.14 m\n");
    printf("[PlaygroundTrack] Calibration ready; waiting for KEY1..KEY5\n");

    return true;
}

bool PlaygroundTrack_StartTask(playground_task_t task) {
    if (!g_initialized || PlaygroundTrack_IsRunning()) {
        return false;
    }

    Motor_Stop();
    MotionControl_Stop(&g_mc);
    Encoder_ResetCount(0U);
    Encoder_ResetCount(1U);
    perception_init(&g_perc);
    memset(&g_sf, 0, sizeof(g_sf));
    memset(&g_res, 0, sizeof(g_res));

    g_task = task;
    pg_init_config(task);
    g_dist_m = 0.0f;
    g_v_cmd = 0.0f;
    g_cyc = 0U;
    g_line_valid_count = 0U;
    g_line_lost_count = 0U;
    g_transverse_count = 0U;
    g_decision_count = 0U;
    g_last_omega_cmd = 0.0f;
    g_filtered_omega_cmd = 0.0f;
    g_last_v_output = 0.0f;
    g_last_valid_line_error = 0.0f;
#if GITHUB_MODE_SWITCH_PROFILE
    g_last_line_omega_target = 0.0f;
    g_imu_steering_active = false;
    g_imu_control_mode = 0U;
#if FAST_STEERING_TEST_PROFILE || HIGH_SPEED_YAW_INNER_TEST_PROFILE
    g_gyro_filter_initialized = false;
#endif
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
    g_yaw_inner_sample_valid = false;
#endif
#endif

    MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);
    MotionControl_Start(&g_mc);
    g_state = (task == PLAYGROUND_TASK_4_AB_6S)
                  ? PT_TASK4_ACCEL
                  : PT_TASK2_RUN;

    printf("[PlaygroundTrack] KEY start: Task %u\n",
           task == PLAYGROUND_TASK_2_LAP_FAST ? 2U :
           task == PLAYGROUND_TASK_4_AB_6S ? 4U : 5U);
    return true;
}

void PlaygroundTrack_Abort(void) {
    MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);
    MotionControl_Stop(&g_mc);
    Motor_Stop();
    g_state = PT_IDLE;
    g_v_cmd = 0.0f;
}

void PlaygroundTrack_RunFastCycle(void) {
    /* Layer 1: 500Hz - Encoder sampling */
    Encoder_Poll();

    /* Layer 2: 100Hz - PID control (every 5 cycles) */
    if ((g_cyc % 5) == 0) {
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
        /*
         * Refresh gyro feedback immediately before the 100 Hz motion loop.
         * The 50 Hz IR layer only updates r_ref; this loop closes yaw rate.
         */
        pg_update_yaw_inner_100hz();
#endif
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

bool PlaygroundTrack_IsFault(void) {
    return (g_state == PT_FAULT);
}

bool PlaygroundTrack_IsRunning(void) {
    return (g_state == PT_TASK2_RUN ||
            g_state == PT_TASK2_APPROACH_A ||
            g_state == PT_TASK4_ACCEL ||
            g_state == PT_TASK4_CRUISE ||
            g_state == PT_TASK4_DECEL);
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
#if FIFTEEN_SECOND_LAP_PROFILE
    /*
     * APPROXIMATELY 15-SECOND LAP PROFILE
     *
     * The measured lap is about 6.14 m, so the required average speed is
     * roughly 0.41 m/s.  These segment targets have a 14.1 s ideal traversal
     * time; startup acceleration, line-error speed scheduling and final
     * approach supply the remaining margin toward 15 s.
     *
     * Keep the proven low-speed IR/gyro gains.  Only speed, available yaw-rate
     * authority and the non-low-speed motor limits are changed.
     */
    g_cfg.v_straight = 0.52f;
    g_cfg.v_curve = 0.40f;
    g_cfg.v_approach = 0.34f;
    g_cfg.v_precision = 0.12f;

    g_cfg.kp_straight = 0.35f;
    g_cfg.kd_straight = 0.08f;
    g_cfg.omega_max_straight = 0.70f;

    g_cfg.kp_curve = 0.45f;
    g_cfg.kd_curve = 0.10f;
    g_cfg.omega_max_curve = 1.10f;

    g_cfg.kp_approach = 0.40f;
    g_cfg.kd_approach = 0.08f;
    g_cfg.omega_max_approach = 0.95f;

    g_cfg.v_task4_max = 0.45f;
    g_cfg.a_task4 = 0.40f;
#elif LOW_SPEED_TEST_PROFILE
    /*
     * LOW-SPEED BENCH PROFILE (2026-07-31)
     * Used for checking wheel direction, encoders and line polarity safely.
     */
    g_cfg.v_straight = 0.15f;
    g_cfg.v_curve = 0.10f;
    g_cfg.v_approach = 0.08f;
    g_cfg.v_precision = 0.06f;

    /* Task 2 PD gains - straights (REDUCED FOR TESTING - prevent oscillation) */
    g_cfg.kp_straight = 0.35f;
    g_cfg.kd_straight = 0.08f;
    g_cfg.omega_max_straight = 0.35f;

    /* Curve feed-forward supplies the nominal turn; IR only trims it. */
    g_cfg.kp_curve = 0.45f;
    g_cfg.kd_curve = 0.10f;
    g_cfg.omega_max_curve = 0.45f;

    g_cfg.kp_approach = 0.40f;
    g_cfg.kd_approach = 0.08f;
    g_cfg.omega_max_approach = 0.35f;

    /* Task 4 low-speed bench profile */
    g_cfg.v_task4_max = 0.12f;
    g_cfg.a_task4 = 0.08f;
#else
    /*
     * CONSERVATIVE COMPETITION PROFILE
     * Approx. 17-18 s nominal lap on the measured track, leaving margin for
     * the 20 s requirement without using excessive PWM.
     */
    g_cfg.v_straight = 0.45f;
    g_cfg.v_curve = 0.34f;
    g_cfg.v_approach = 0.25f;
    g_cfg.v_precision = 0.12f;

    g_cfg.kp_straight = 0.5f;
    g_cfg.kd_straight = 0.3f;
    g_cfg.omega_max_straight = 1.0f;

    g_cfg.kp_curve = 1.0f;
    g_cfg.kd_curve = 0.6f;
    g_cfg.omega_max_curve = 1.4f;

    g_cfg.kp_approach = 0.8f;
    g_cfg.kd_approach = 0.5f;
    g_cfg.omega_max_approach = 0.8f;

    g_cfg.v_task4_max = 0.35f;
    g_cfg.a_task4 = 0.20f;
#endif

    /*
     * Per-task speed selection.
     * Task 2 intentionally keeps the exact proven 15-second profile above.
     * Task 4 uses a symmetric 0.30 m/s trapezoid:
     *   1 s accel + 4 s cruise + 1 s decel over 1.5 m ~= 6 s.
     * Task 5 keeps the same stable steering/IMU/wheel-PI controller but slows
     * the segment speeds for an estimated 24-26 second lap.
     */
    if (task == PLAYGROUND_TASK_4_AB_6S) {
        g_cfg.v_task4_max = 0.30f;
        g_cfg.a_task4 = 0.30f;
    } else if (task == PLAYGROUND_TASK_5_LAP_25S) {
        g_cfg.v_straight = 0.28f;
        g_cfg.v_curve = 0.235f;
        g_cfg.v_approach = 0.18f;
        g_cfg.v_precision = 0.10f;
    }
    g_cfg.kp_task4 = 0.3f;      /* 0.8 → 0.3 (reduced to ~40%) */
    g_cfg.kd_task4 = 0.2f;      /* 0.5 → 0.2 (reduced to 40%) */
    g_cfg.omega_max_task4 = 0.8f;  /* 1.0 → 0.8 */

    /* Segment boundaries (meters) */
    g_cfg.dist_ab_end = 1.500f;
    g_cfg.dist_bc_end = 3.071f;
    g_cfg.dist_cd_end = 4.571f;
    g_cfg.dist_da_early = 5.000f;
    g_cfg.approach_start_dist = 5.000f;
    /*
     * The front IR array reaches the A transverse line when the axle distance
     * is approximately TRACK_PERIMETER_M - IR_ARRAY_LOOKAHEAD_M = 5.959 m.
     * Enter a slow precision zone about 0.21 m before that point.
     */
    g_cfg.precision_start_dist = 5.750f;

    /* A-line detection */
    g_cfg.transverse_min_ch = 6;
    g_cfg.transverse_confirm_frames = 3;
    g_cfg.a_detect_min_dist = 5.5f;

    /* Task 4 deceleration start */
    float d_ramp = (g_cfg.v_task4_max * g_cfg.v_task4_max) / (2.0f * g_cfg.a_task4);
    g_cfg.d_decel_start = g_cfg.dist_ab_end - d_ramp;

    /* Fault detection thresholds */
    /*
     * Pololu-style recovery keeps the last known line side briefly instead of
     * immediately commanding zero correction.  20 frames gives 400 ms to
     * reacquire the line at reduced speed before the safety stop.
     */
    g_cfg.line_lost_fault_lap = 20;  /* 400ms at 50Hz */
    g_cfg.line_lost_fault_ab = 5;    /* 100ms at 50Hz */

    printf("[PlaygroundTrack] Config loaded:\n");
#if INDEPENDENT_WHEEL_PI_TEST_PROFILE
    printf("  profile=15_SECOND_YAW_INNER_WHEEL_PI_TEST, PWM=55%%, "
           "yaw_inner=100Hz, wheelPI_alpha=0.25, wheelPI_limit=12\n");
    printf("  encoder_map=LEFT:B/TIM4(raw1), RIGHT:C/TIM3(raw0) [SWAP FIX]\n");
    printf("  precision_stop=0.12m/s from 5.75m, A-line confirmed stop\n");
#elif HIGH_SPEED_YAW_INNER_TEST_PROFILE
    printf("  profile=15_SECOND_YAW_INNER_TEST, PWM=55%%, yaw_inner=100Hz, Kr=8.0\n");
#elif FIFTEEN_SECOND_LAP_PROFILE
    printf("  profile=15_SECOND_LAP_TEST, PWM=55%%, omega_slew=10, gyro_alpha=0.60\n");
#elif FAST_STEERING_TEST_PROFILE
    printf("  profile=FAST_STEERING_TEST, PWM=25%%, omega_slew=10, gyro_alpha=0.60\n");
#elif LOW_SPEED_TEST_PROFILE
    printf("  profile=LOW_SPEED_TEST, hard PWM limit=25%%\n");
#else
    printf("  profile=CONSERVATIVE_COMPETITION, hard PWM limit=55%%\n");
#endif
    if (task != PLAYGROUND_TASK_4_AB_6S) {
        printf("  v_straight=%.2f, v_curve=%.2f, v_approach=%.2f, "
               "v_precision=%.2f@%.2fm\n",
               g_cfg.v_straight, g_cfg.v_curve, g_cfg.v_approach,
               g_cfg.v_precision, g_cfg.precision_start_dist);
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

    /*
     * Read every sensor, but gate Task 2 only on the fresh IR frame.  IMU is
     * optional for this line-following controller and must not masquerade as
     * a line-loss fault if SPI is unavailable.
     */
    sd_status_t preprocess_status =
        preprocess_update(PlatformTime_GetUs64(), &g_sf);
    (void)preprocess_status;

    /* Update perception (lateral_error, heading_error, active_mask) */
    bool line_valid = false;
    if (g_sf.ir_valid) {
        sd_status_t perception_status =
            perception_update(&g_perc, &g_sf.ir, g_sf.timestamp_us, &g_res);
        line_valid = (perception_status == SD_OK) && g_res.line_valid;
    }

#if GITHUB_MODE_SWITCH_PROFILE
#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
    /*
     * The 100 Hz fast-cycle reader owns gyro sampling and feedback validity.
     * IR remains at 50 Hz and only selects/updates the outer r_ref.
     */
    bool imu_frame_valid = g_imu_available && g_yaw_inner_sample_valid;
    g_imu_steering_active =
        imu_frame_valid &&
        (g_state == PT_TASK2_RUN ||
         g_state == PT_TASK4_ACCEL ||
         g_state == PT_TASK4_CRUISE ||
         g_state == PT_TASK4_DECEL);
    g_imu_control_mode =
        !g_imu_steering_active ? 0U : (line_valid ? 3U : 2U);
#else
    bool imu_frame_valid = g_imu_available && g_sf.imu_valid &&
                           isfinite(g_sf.imu.gyro_radps[2]);
    if (imu_frame_valid) {
        float gyro_z_sample =
            IMU_GYRO_Z_DIRECTION * g_sf.imu.gyro_radps[2];
#if FAST_STEERING_TEST_PROFILE
        /*
         * Light sensor-side filtering (new sample weight 0.60 at 50 Hz).
         * This suppresses single-frame vibration without adding the old
         * command-side 50 ms lag.
         */
        if (!g_gyro_filter_initialized) {
            g_last_gyro_z_radps = gyro_z_sample;
            g_gyro_filter_initialized = true;
        } else {
            g_last_gyro_z_radps +=
                0.60f * (gyro_z_sample - g_last_gyro_z_radps);
        }
#else
        g_last_gyro_z_radps = gyro_z_sample;
#endif
    } else {
        g_last_gyro_z_radps = 0.0f;
#if FAST_STEERING_TEST_PROFILE
        g_gyro_filter_initialized = false;
#endif
    }

    /*
     * Do not run two independent steering loops at once.  With a visible line,
     * pg_steering_correction() uses line position as P and gyro yaw rate as the
     * D/damping measurement in one controller.  During a brief line loss the
     * regular yaw-rate loop instead holds the last valid turn command.
     */
    g_imu_steering_active =
        imu_frame_valid && !line_valid &&
        (g_state == PT_TASK2_RUN ||
         g_state == PT_TASK4_ACCEL ||
         g_state == PT_TASK4_CRUISE ||
         g_state == PT_TASK4_DECEL);
    g_imu_control_mode =
        !imu_frame_valid ? 0U : (line_valid ? 1U :
        (g_imu_steering_active ? 2U : 0U));
    MotionControl_SetYawRateFeedback(&g_mc, g_last_gyro_z_radps,
                                     g_imu_steering_active);
#endif
#else
    /*
     * Feed the latest yaw rate to the 100 Hz steering inner loop.  The sample
     * is held for two motion-control ticks; if the current frame is invalid,
     * MotionControl immediately falls back to yaw feed-forward.
     */
    if (g_imu_available && g_sf.imu_valid &&
        isfinite(g_sf.imu.gyro_radps[2])) {
        g_last_gyro_z_radps =
            IMU_GYRO_Z_DIRECTION * g_sf.imu.gyro_radps[2];
        MotionControl_SetYawRateFeedback(&g_mc, g_last_gyro_z_radps, true);
    } else {
        g_last_gyro_z_radps = 0.0f;
        MotionControl_SetYawRateFeedback(&g_mc, 0.0f, false);
    }
#endif

    /* Track line validity for state transitions */
    if (line_valid) {
        g_line_valid_count++;
        g_line_lost_count = 0;
        g_last_valid_line_error = g_res.lateral_error;
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

    /* Low-rate runtime trace for field diagnosis without disturbing control. */
    if (++g_decision_count >= 50U) {
        float vl, vr;
        MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
        g_decision_count = 0U;
#if GITHUB_MODE_SWITCH_PROFILE
        printf("[Task2] state=%u dist=%.3f line=%u mask=0x%02X act=%u "
               "err=%+.3f hdg=%+.3f cmdV=%.3f omega=%+.3f lost=%u "
               "vL=%.3f vR=%.3f tL=%.3f tR=%.3f pwmL=%d pwmR=%d "
               "imuInit=%u imuFrame=%u imuMode=%u gyroZ=%+.3f "
               "yawErr=%+.3f turnPWM=%+.1f wheelPi=%+.1f\n",
               (unsigned)g_state, g_dist_m, line_valid ? 1U : 0U,
               (unsigned)(g_res.active_mask & 0xFFU),
               (unsigned)pg_count_active_channels(g_res.active_mask),
               g_res.lateral_error, g_res.heading_error,
               g_last_v_output, g_last_omega_cmd,
               (unsigned)g_line_lost_count, vl, vr,
               g_mc.wheel_left.target_velocity,
               g_mc.wheel_right.target_velocity,
               (int)g_mc.wheel_left.pwm_output,
               (int)g_mc.wheel_right.pwm_output,
               g_imu_available ? 1U : 0U,
               (g_imu_available && g_sf.imu_valid) ? 1U : 0U,
               (unsigned)g_imu_control_mode,
               g_last_gyro_z_radps,
               g_mc.yaw_rate_error_radps,
               g_mc.steering_pwm,
               g_mc.wheel_pi_steering_pwm);
#else
        printf("[Task2] state=%u dist=%.3f line=%u mask=0x%02X act=%u "
               "err=%+.3f hdg=%+.3f cmdV=%.3f omega=%+.3f lost=%u "
               "vL=%.3f vR=%.3f pwmL=%d pwmR=%d "
               "imuInit=%u imuFrame=%u gyroZ=%+.3f yawErr=%+.3f turnPWM=%+.1f\n",
               (unsigned)g_state, g_dist_m, line_valid ? 1U : 0U,
               (unsigned)(g_res.active_mask & 0xFFU),
               (unsigned)pg_count_active_channels(g_res.active_mask),
               g_res.lateral_error, g_res.heading_error,
               g_last_v_output, g_last_omega_cmd,
               (unsigned)g_line_lost_count, vl, vr,
               (int)g_mc.wheel_left.pwm_output,
               (int)g_mc.wheel_right.pwm_output,
               g_imu_available ? 1U : 0U,
               (g_imu_available && g_sf.imu_valid) ? 1U : 0U,
               g_last_gyro_z_radps,
               g_mc.yaw_rate_error_radps,
               g_mc.steering_pwm);
#endif
    }
}

/**
 * @brief Main state machine
 */
static void pg_state_machine(void) {
    const float dt = 0.020f;
    float omega = 0.0f;
    float omega_feedforward = 0.0f;
    float kp, kd, omega_max;
    g_last_omega_cmd = 0.0f;
    g_last_v_output = 0.0f;

    switch (g_state) {
        case PT_IDLE:
            /* Boot-ready state: sensors refresh, but only a key may start. */
            MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);
            break;

        case PT_TASK2_RUN:
            /* Check for line loss fault */
            if (g_line_lost_count >= g_cfg.line_lost_fault_lap) {
                printf("[ERROR] No fresh valid line for %ums "
                       "(ir_valid=%u, line_valid=%u), entering fault state\n",
                       (unsigned)(g_cfg.line_lost_fault_lap * 20U),
                       g_sf.ir_valid ? 1U : 0U, g_res.line_valid ? 1U : 0U);
                g_state = PT_FAULT;
                MotionControl_Stop(&g_mc);
                Motor_Stop();
                break;
            }

            /*
             * Select the segment at the front IR array, not at the wheel axle.
             * The array is 183 mm ahead, so waiting until axle distance=1.5 m
             * starts the B->C turn much too late.
             */
            float preview_dist = g_dist_m + IR_ARRAY_LOOKAHEAD_M;
            if (preview_dist >= TRACK_PERIMETER_M) {
                preview_dist -= TRACK_PERIMETER_M;
            }
            bool curve_segment = false;

            /*
             * Once the axle enters the final precision zone, stay in approach
             * mode until the A transverse line is confirmed. Do not allow the
             * front-array preview to wrap to zero and briefly select the next
             * lap's 0.52 m/s A->B straight profile.
             */
            if (g_dist_m >= g_cfg.precision_start_dist) {
                g_v_cmd = g_cfg.v_precision;
                kp = g_cfg.kp_approach;
                kd = g_cfg.kd_approach;
                omega_max = g_cfg.omega_max_approach;
                curve_segment = true;
            } else if (preview_dist < g_cfg.dist_ab_end) {
                /* Straight A→B */
                g_v_cmd = g_cfg.v_straight;
                kp = g_cfg.kp_straight;
                kd = g_cfg.kd_straight;
                omega_max = g_cfg.omega_max_straight;
            } else if (preview_dist < g_cfg.dist_bc_end) {
                /* Curve B→C */
                g_v_cmd = g_cfg.v_curve;
                kp = g_cfg.kp_curve;
                kd = g_cfg.kd_curve;
                omega_max = g_cfg.omega_max_curve;
                curve_segment = true;
            } else if (preview_dist < g_cfg.dist_cd_end) {
                /* Straight C→D */
                g_v_cmd = g_cfg.v_straight;
                kp = g_cfg.kp_straight;
                kd = g_cfg.kd_straight;
                omega_max = g_cfg.omega_max_straight;
            } else if (preview_dist < g_cfg.approach_start_dist) {
                /* Curve D→A (early) */
                g_v_cmd = g_cfg.v_curve;
                kp = g_cfg.kp_curve;
                kd = g_cfg.kd_curve;
                omega_max = g_cfg.omega_max_curve;
                curve_segment = true;
            } else {
                /* Curve D→A (approach) */
                g_v_cmd = g_cfg.v_approach;
                kp = g_cfg.kp_approach;
                kd = g_cfg.kd_approach;
                omega_max = g_cfg.omega_max_approach;
                curve_segment = true;
            }

            /* Check for A-line detection (transverse line) */
            if (g_dist_m > g_cfg.a_detect_min_dist) {
                uint8_t active_count = pg_count_active_channels(g_res.active_mask);
                if (active_count >= g_cfg.transverse_min_ch) {
                    if (g_transverse_count < UINT8_MAX) {
                        g_transverse_count++;
                    }
                } else {
                    g_transverse_count = 0U;
                }

                if (g_transverse_count >= g_cfg.transverse_confirm_frames) {
                    printf("[PlaygroundTrack] A-line detected! dist=%.3fm, switching to approach...\n",
                           g_dist_m);
                    /*
                     * Publish zero speed in this decision frame. Otherwise the
                     * old precision-speed command survives for one additional
                     * 20 ms frame before PT_TASK2_APPROACH_A executes.
                     */
                    g_last_v_output = 0.0f;
                    g_last_omega_cmd = 0.0f;
                    MotionControl_SetVelocityCommand(&g_mc, 0.0f, 0.0f);
                    g_state = PT_TASK2_APPROACH_A;
                    break;
                }
            } else {
                g_transverse_count = 0U;
            }

            /*
             * Channel 0 is physical left and has positive weight.
             * Positive lateral_error therefore means the line is on the left.
             * Positive omega makes the right wheel faster and turns left, so
             * the correction sign must be positive (old leading '-' caused
             * positive feedback and drove the car farther off the line).
             */
            g_last_v_output = pg_follow_speed(g_v_cmd);
            if (curve_segment) {
                /*
                 * Both BC and DA are clockwise half-circles, hence negative
                 * yaw.  IR feedback only corrects deviations around this
                 * known nominal curvature.
                 */
                omega_feedforward =
                    -g_last_v_output / TRACK_CURVE_RADIUS_M;
            }
            omega = pg_steering_correction(kp, kd, omega_max,
                                           omega_feedforward);
            omega = pg_slew_omega(omega, dt);
            g_last_omega_cmd = omega;

            MotionControl_SetVelocityCommand(&g_mc, g_last_v_output, omega);
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
            if (g_line_lost_count >= g_cfg.line_lost_fault_ab) {
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
            omega = pg_steering_correction(g_cfg.kp_task4, g_cfg.kd_task4,
                                           g_cfg.omega_max_task4, 0.0f);
            g_last_omega_cmd = omega;
            g_last_v_output = pg_follow_speed(g_v_cmd);

            MotionControl_SetVelocityCommand(&g_mc, g_last_v_output, omega);
            break;

        case PT_TASK4_CRUISE:
            /* Check for line loss fault */
            if (g_line_lost_count >= g_cfg.line_lost_fault_ab) {
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
            omega = pg_steering_correction(g_cfg.kp_task4, g_cfg.kd_task4,
                                           g_cfg.omega_max_task4, 0.0f);
            g_last_omega_cmd = omega;
            g_last_v_output = pg_follow_speed(g_v_cmd);

            MotionControl_SetVelocityCommand(&g_mc, g_last_v_output, omega);
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
            omega = pg_steering_correction(g_cfg.kp_task4, g_cfg.kd_task4,
                                           g_cfg.omega_max_task4, 0.0f);
            g_last_omega_cmd = omega;
            g_last_v_output = pg_follow_speed(g_v_cmd);

            MotionControl_SetVelocityCommand(&g_mc, g_last_v_output, omega);
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

/**
 * @brief Convert line position error to differential-drive yaw command.
 *
 * Positive error = line on physical left = command positive omega (left turn).
 * Negative error = line on physical right = command negative omega (right turn).
 */
static float pg_steering_correction(float kp, float kd, float omega_max,
                                    float omega_feedforward) {
    /*
     * If the line disappears briefly, continue turning toward its last known
     * side.  This mirrors the official Pololu QTR readLine behavior and avoids
     * changing to "drive straight" exactly when the outer sensor loses the
     * line.  The state machine still performs a hard stop after the configured
     * recovery window.
     */
    if (g_line_lost_count > 0U) {
#if GITHUB_MODE_SWITCH_PROFILE
        /*
         * The referenced arc controller holds its previous steering through a
         * short sensor dropout.  Do the same instead of jumping immediately
         * to +/-omega_max, which creates the observed snap and tail swing.
         */
        return pg_clamp(g_last_line_omega_target, -omega_max, omega_max);
#else
        if (fabsf(g_last_valid_line_error) < 0.10f) {
            return 0.0f;
        }
        return g_last_valid_line_error > 0.0f ? omega_max : -omega_max;
#endif
    }

#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
    /*
     * True cascade for this test:
     *   50 Hz IR outer loop -> yaw-rate reference
     *   100 Hz gyro inner loop -> differential PWM
     *
     * Do not also subtract gyro in the outer loop; that would count the same
     * feedback twice and make attribution of the real-car test impossible.
     */
    float omega = omega_feedforward +
                  kp * g_res.lateral_error;
#elif GITHUB_MODE_SWITCH_PROFILE
    /*
     * One fused steering law:
     *   IR centroid -> lateral P term
     *   ICM42688 yaw rate -> physical D damping
     *
     * The old numerical derivative of the quantized/wide-line IR centroid and
     * the second yaw-rate loop are deliberately not added here.  On a curve,
     * damp only the deviation from nominal feed-forward yaw rate, so damping
     * cannot cancel the intended turn.
     */
    float gyro_damping = 0.0f;
    if (g_imu_control_mode == 1U) {
        gyro_damping = kd * (g_last_gyro_z_radps - omega_feedforward);
    }
    float omega = omega_feedforward +
                  kp * g_res.lateral_error -
                  gyro_damping;
#else
    float omega = omega_feedforward +
                  kp * g_res.lateral_error +
                  kd * g_res.heading_error;
#endif
    omega = pg_clamp(omega, -omega_max, omega_max);
#if GITHUB_MODE_SWITCH_PROFILE
    g_last_line_omega_target = omega;
#endif
    return omega;
}

/**
 * @brief Reduce forward speed when the line is far from center or moving fast.
 *
 * The scale remains 1.0 for a centered broad line and bottoms at 0.45 for
 * severe deviation, giving the steering loop time to recover before line loss.
 */
static float pg_follow_speed(float segment_speed) {
    if (g_line_lost_count > 0U) {
        return segment_speed * 0.55f;
    }

    float scale = 1.0f -
                  0.15f * fabsf(g_res.lateral_error) -
                  0.08f * fabsf(g_res.heading_error);
    scale = pg_clamp(scale, 0.45f, 1.0f);
    return segment_speed * scale;
}

/**
 * @brief Validate the v1.0-compatible IMU path when WHO_AM_I is unreadable.
 *
 * A stationary ±8 g accelerometer should still contain a sizeable gravity
 * vector.  All-zero, all-ones and saturated SPI frames are rejected.
 */
static bool pg_probe_imu_data(void) {
    uint8_t plausible_count = 0U;

    for (uint8_t sample = 0U; sample < 6U; ++sample) {
        icm42688_data_t data;
        if (icm42688_read(&data) != ICM42688_STATUS_OK) {
            continue;
        }

        float accel_l1 = fabsf((float)data.acc_raw.x) +
                         fabsf((float)data.acc_raw.y) +
                         fabsf((float)data.acc_raw.z);
        bool saturated =
            (data.acc_raw.x == INT16_MIN || data.acc_raw.x == INT16_MAX) &&
            (data.acc_raw.y == INT16_MIN || data.acc_raw.y == INT16_MAX) &&
            (data.acc_raw.z == INT16_MIN || data.acc_raw.z == INT16_MAX);

        if (sample == 0U) {
            printf("[IMU probe] accelRaw=(%d,%d,%d) gyroRaw=(%d,%d,%d)\n",
                   (int)data.acc_raw.x, (int)data.acc_raw.y,
                   (int)data.acc_raw.z, (int)data.gyro_raw.x,
                   (int)data.gyro_raw.y, (int)data.gyro_raw.z);
        }

        if (!saturated && accel_l1 > 512.0f && accel_l1 < 90000.0f) {
            plausible_count++;
        }
    }

    printf("[IMU probe] plausible frames: %u/6\n",
           (unsigned)plausible_count);
    return plausible_count >= 4U;
}

/**
 * @brief Limit yaw-command changes so a front-mounted line array cannot make
 *        the rear of the vehicle alternate violently from side to side.
 */
static float pg_slew_omega(float target, float dt) {
#if FAST_STEERING_TEST_PROFILE
    /*
     * A +/-0.35 rad/s reversal now takes about 70 ms instead of 350 ms.
     * This retains a mechanical shock guard without dominating loop phase.
     */
    const float rate_limit_radps2 = 10.0f;
#else
    const float rate_limit_radps2 = g_imu_available ? 2.0f : 1.0f;
#endif
    float max_step = rate_limit_radps2 * dt;
    float step = pg_clamp(target - g_filtered_omega_cmd,
                          -max_step, max_step);
    g_filtered_omega_cmd += step;
    return g_filtered_omega_cmd;
}

#if HIGH_SPEED_YAW_INNER_TEST_PROFILE
/**
 * @brief Read and lightly filter gyro Z for the 100 Hz yaw-rate inner loop.
 *
 * icm42688_read() already subtracts the stationary calibration bias and
 * exposes physical gyro_dps.  Keep the proven new-sample weight of 0.60.
 */
static void pg_update_yaw_inner_100hz(void) {
    bool running =
        (g_state == PT_TASK2_RUN ||
         g_state == PT_TASK4_ACCEL ||
         g_state == PT_TASK4_CRUISE ||
         g_state == PT_TASK4_DECEL);

    if (!g_imu_available) {
        g_yaw_inner_sample_valid = false;
        g_imu_steering_active = false;
        MotionControl_SetYawRateFeedback(&g_mc, 0.0f, false);
        return;
    }

    icm42688_data_t data;
    if (icm42688_read(&data) != ICM42688_STATUS_OK ||
        !isfinite(data.gyro_dps.z)) {
        g_yaw_inner_sample_valid = false;
        g_imu_steering_active = false;
        MotionControl_SetYawRateFeedback(&g_mc, 0.0f, false);
        return;
    }

    float sample_radps =
        IMU_GYRO_Z_DIRECTION * data.gyro_dps.z * DEGREES_TO_RADIANS;
    if (!g_gyro_filter_initialized) {
        g_last_gyro_z_radps = sample_radps;
        g_gyro_filter_initialized = true;
    } else {
        g_last_gyro_z_radps +=
            0.60f * (sample_radps - g_last_gyro_z_radps);
    }

    g_yaw_inner_sample_valid = true;
    g_imu_steering_active = running;
    MotionControl_SetYawRateFeedback(&g_mc, g_last_gyro_z_radps, running);
}
#endif

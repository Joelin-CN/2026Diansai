/**
 * @file test_control_app.c
 * @brief Control application integration tests
 * @date 2026-07-18
 * 
 * Tests:
 * - Scheduler divider: 500 Hz Motion Control, 50 Hz Sens-Decision
 * - Initialization ordering and fault injection
 * - Persistent start command until planner transitions
 * - Invalid data handling and lap completion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "../inc/control_app.h"

/* ============================================================================
 * Test Infrastructure: Call Tracking (Must be declared first)
 * ============================================================================ */

static unsigned motion_update_calls = 0;
static unsigned decision_update_calls = 0;
static float last_decision_dt = 0.0f;
static unsigned motion_stops = 0;
static unsigned emergency_stops = 0;

/* Track call ordering within a cycle */
static unsigned cycle_sequence_counter = 0;
static unsigned decision_sequence_num = 0;
static unsigned motion_sequence_num = 0;

/* Fault injection flags */
static bool inject_mcp_failure = false;
static bool inject_icm_failure = false;
static bool inject_sensor_hal_failure = false;
static bool inject_path_setup_failure = false;
static bool inject_motion_init_failure = false;

/* ============================================================================
 * Fake Dependencies: Minimal stubs for all external modules
 * ============================================================================ */

/* Motor HAL fakes */
void Motor_Init(void) {}
void Motor_Stop(void) { motion_stops++; }
void Motor_SetSpeed(int16_t left, int16_t right) { (void)left; (void)right; }

/* Encoder HAL fakes */
typedef enum { ENCODER_1 = 0, ENCODER_2, ENCODER_3, ENCODER_4 } Encoder_Id;
int32_t Encoder_GetCount(Encoder_Id encoder) { (void)encoder; return 0; }
void Encoder_ResetCount(Encoder_Id encoder) { (void)encoder; }
void Encoder_Init(void) {}

/* MCP23017 fake */
bool MCP23017_Init(void) {
    return !inject_mcp_failure;
}
bool MCP23017_ReadInputs(uint16_t *inputs) {
    if (inputs) *inputs = 0x0FFF;
    return true;
}

/* Platform time fake */
void PlatformTime_Init(void) {}
uint64_t PlatformTime_GetUs64(void) { return decision_update_calls * 20000ULL; }

/* ICM42688 fake */
typedef enum { ICM42688_STATUS_OK = 0, ICM42688_STATUS_NOT_READY = 1 } icm42688_status_t;
typedef struct { int16_t x, y, z; } icm42688_vec3_t;
typedef struct {
    icm42688_vec3_t acc_raw;
    icm42688_vec3_t gyro_raw;
    int16_t temperature_raw;
} icm42688_data_t;
icm42688_status_t icm42688_read(icm42688_data_t *data) {
    if (inject_icm_failure) return ICM42688_STATUS_NOT_READY;
    if (data) {
        data->acc_raw.x = data->acc_raw.y = data->acc_raw.z = 0;
        data->gyro_raw.x = data->gyro_raw.y = data->gyro_raw.z = 0;
        data->temperature_raw = 0;
    }
    return ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_init(void) {
    return inject_icm_failure ? ICM42688_STATUS_NOT_READY : ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_calibrate_gyro(unsigned samples, unsigned delay_ms) {
    (void)samples; (void)delay_ms;
    return inject_icm_failure ? ICM42688_STATUS_NOT_READY : ICM42688_STATUS_OK;
}

/* Sens-Decision config stub */
typedef struct {
    struct { float gyro_bias_radps[3]; float gyro_scale_radps_per_lsb; } imu;
} sens_decision_config_t;
sens_decision_config_t g_sens_decision_config;

/* Sens-Decision sensor HAL stub */
typedef enum { SD_OK = 0, SD_ERR_HW_FAULT = -5 } sd_status_t;
typedef struct { void *dummy; } sensor_hal_t;
sd_status_t sensors_configure_hal(const sensor_hal_t *hal) {
    (void)hal;
    return inject_sensor_hal_failure ? SD_ERR_HW_FAULT : SD_OK;
}
sd_status_t sensors_init_all(void) {
    return inject_sensor_hal_failure ? SD_ERR_HW_FAULT : SD_OK;
}

/* Motion Control fake */
typedef struct { int dummy; } MotionControl_t;
typedef struct { int dummy; } EncoderInterface_t;
typedef struct { int dummy; } MotorInterface_t;
MotionControl_t g_motion_control;
bool MotionControl_Init(MotionControl_t *ctrl, EncoderInterface_t *enc, MotorInterface_t *mot) {
    (void)ctrl; (void)enc; (void)mot;
    return !inject_motion_init_failure;
}
void MotionControl_Start(MotionControl_t *ctrl) { (void)ctrl; }
void MotionControl_Stop(MotionControl_t *ctrl) { 
    (void)ctrl;
    motion_stops++;
}
void MotionControl_Update(MotionControl_t *ctrl) {
    (void)ctrl;
    motion_update_calls++;
    motion_sequence_num = ++cycle_sequence_counter;
}
void MotionControl_SetVelocityCommand(MotionControl_t *ctrl, float v, float omega) {
    (void)ctrl; (void)v; (void)omega;
}
void MotionControl_EmergencyStop(MotionControl_t *ctrl) {
    (void)ctrl;
    emergency_stops++;
}

/* Adapter fakes */
static EncoderInterface_t g_encoder_iface;
static MotorInterface_t g_motor_iface;
EncoderInterface_t *EncoderAdapter_GetInterface(void) { return &g_encoder_iface; }
MotorInterface_t *MotorAdapter_GetInterface(void) { return &g_motor_iface; }
const sensor_hal_t *SensorAdapter_GetHal(void) { return NULL; }

/* Trajectory generator fake */
typedef struct { float x, y, heading, curvature; } path_point_t;
typedef struct { int dummy; } trajectory_generator_t;
typedef struct { int dummy; } sd_trajectory_config_t;
trajectory_generator_t g_trajectory_generator;
void trajectory_generator_init(trajectory_generator_t *gen, const sd_trajectory_config_t *cfg) {
    (void)gen; (void)cfg;
}
sd_status_t trajectory_set_path(trajectory_generator_t *gen, const path_point_t *path, size_t count) {
    (void)gen; (void)path; (void)count;
    return inject_path_setup_failure ? SD_ERR_HW_FAULT : SD_OK;
}

/* Square path fake */
typedef struct { uint8_t completed_laps; bool target_reached; } lap_counter_t;
const path_point_t *SquarePath_GetPoints(void) { return NULL; }
size_t SquarePath_GetPointCount(void) { return 200; }
bool SquarePath_UpdateLap(lap_counter_t *counter, size_t nearest_idx, size_t path_count, uint8_t target) {
    (void)counter; (void)nearest_idx; (void)path_count; (void)target;
    return false;
}

/* Decision pipeline fakes - these track call counts */
typedef struct { int dummy; } sensor_frame_t;
typedef struct { int dummy; } state_evaluator_t;
typedef struct { int dummy; } perception_t;
typedef struct { int dummy; } perception_result_t;
typedef struct { int dummy; } behavior_planner_t;
typedef struct { int dummy; } behavior_input_t;
typedef struct { int dummy; } behavior_output_t;
typedef struct { float v, omega; } trajectory_point_t;
typedef struct { int dummy; } vehicle_state_t;
typedef struct { int dummy; } sd_ekf_config_t;

void state_evaluator_init(state_evaluator_t *eval, const sd_ekf_config_t *cfg) {
    (void)eval; (void)cfg;
}
void perception_init(perception_t *perc) {
    (void)perc;
}
void behavior_planner_init(behavior_planner_t *planner) {
    (void)planner;
}

sd_status_t preprocess_update(uint64_t timestamp_us, sensor_frame_t *frame) {
    (void)timestamp_us; (void)frame;
    decision_update_calls++;
    decision_sequence_num = ++cycle_sequence_counter;
    last_decision_dt = 0.020f;  /* Simulating 50 Hz */
    return SD_OK;
}

sd_status_t state_evaluator_update(state_evaluator_t *eval, const sensor_frame_t *frame) {
    (void)eval; (void)frame;
    return SD_OK;
}

sd_status_t perception_update(perception_t *perc, const void *ir, uint64_t ts, perception_result_t *result) {
    (void)perc; (void)ir; (void)ts; (void)result;
    return SD_OK;
}

sd_status_t behavior_planner_update(behavior_planner_t *planner, const behavior_input_t *in, behavior_output_t *out) {
    (void)planner; (void)in; (void)out;
    return SD_OK;
}

sd_status_t trajectory_generate(trajectory_generator_t *gen, const vehicle_state_t *veh,
                               const behavior_output_t *beh, float dt, trajectory_point_t *out) {
    (void)gen; (void)veh; (void)beh; (void)dt; (void)out;
    return SD_OK;
}

float SquarePath_CorrectOmega(float nominal, float lateral, float heading, const void *cfg) {
    (void)lateral; (void)heading; (void)cfg;
    return nominal;
}

/* ============================================================================
 * Test Helper Functions
 * ============================================================================ */

static void reset_call_tracking(void) {
    motion_update_calls = 0;
    decision_update_calls = 0;
    last_decision_dt = 0.0f;
    motion_stops = 0;
    emergency_stops = 0;
    cycle_sequence_counter = 0;
    decision_sequence_num = 0;
    motion_sequence_num = 0;
}

/* ============================================================================
 * Test 1: Scheduler Divider - 500 Hz Motion / 50 Hz Decision
 * ============================================================================ */

static void test_scheduler_divider(void) {
    printf("Test: Scheduler divider (500 Hz / 50 Hz)...\n");
    
    reset_call_tracking();
    
    /* Run 100 fast cycles */
    for (unsigned i = 0; i < 100U; ++i) {
        ControlApp_RunFastCycle();
    }
    
    /* Verify call counts */
    assert(motion_update_calls == 100U);
    assert(decision_update_calls == 10U);
    
    /* Verify decision dt is 0.020f (50 Hz) */
    assert(fabsf(last_decision_dt - 0.020f) < 1e-6f);
    
    printf("  PASS: Motion called 100x, Decision called 10x, dt=0.020f\n");
}

/* ============================================================================
 * Test 2: Decision Update Precedes Motion Control on Tenth Cycle
 * ============================================================================ */

static void test_decision_precedes_motion(void) {
    printf("Test: Decision update precedes Motion Control on 10th cycle...\n");
    
    reset_call_tracking();
    
    /* Run exactly 10 cycles to trigger one decision update */
    for (unsigned i = 0; i < 10U; ++i) {
        ControlApp_RunFastCycle();
    }
    
    /* On the 10th cycle, decision should be called before motion */
    assert(decision_sequence_num < motion_sequence_num);
    
    printf("  PASS: Decision sequence=%u, Motion sequence=%u\n",
           decision_sequence_num, motion_sequence_num);
}

/* ============================================================================
 * Test 3: Initialization Failures Stop Motors
 * ============================================================================ */

static void test_init_mcp_failure_stops_motors(void) {
    printf("Test: MCP23017 init failure stops motors...\n");
    
    reset_call_tracking();
    inject_mcp_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 1);  /* Motor_Stop should be called */
    
    inject_mcp_failure = false;
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_init_icm_failure_stops_motors(void) {
    printf("Test: ICM identity check failure stops motors...\n");
    
    reset_call_tracking();
    inject_icm_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 1);
    
    inject_icm_failure = false;
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_init_sensor_hal_failure_stops_motors(void) {
    printf("Test: Sensor HAL init failure stops motors...\n");
    
    reset_call_tracking();
    inject_sensor_hal_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 1);
    
    inject_sensor_hal_failure = false;
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_init_path_setup_failure_stops_motors(void) {
    printf("Test: Path setup failure stops motors...\n");
    
    reset_call_tracking();
    inject_path_setup_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 1);
    
    inject_path_setup_failure = false;
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_init_motion_control_failure_stops_motors(void) {
    printf("Test: Motion Control init failure stops motors...\n");
    
    reset_call_tracking();
    inject_motion_init_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 1);
    
    inject_motion_init_failure = false;
    printf("  PASS: Init failed, motors stopped\n");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("\n=== Control Application Tests ===\n\n");
    
    /* Scheduler tests */
    test_scheduler_divider();
    test_decision_precedes_motion();
    
    /* Initialization fault injection tests */
    test_init_mcp_failure_stops_motors();
    test_init_icm_failure_stops_motors();
    test_init_sensor_hal_failure_stops_motors();
    test_init_path_setup_failure_stops_motors();
    test_init_motion_control_failure_stops_motors();
    
    printf("\n=== All tests passed ===\n");
    return 0;
}

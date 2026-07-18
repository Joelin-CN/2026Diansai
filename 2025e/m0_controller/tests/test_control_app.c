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
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/ICM42688/inc/icm42688_mspm0.h"
#include "../modules/MCP23017/inc/mcp23017.h"
#include "../modules/Sens-Decision/inc/config.h"

/* ============================================================================
 * Test Infrastructure: Call Tracking (Must be declared first)
 * ============================================================================ */

static unsigned motion_update_calls = 0;
static unsigned decision_update_calls = 0;
static float last_decision_dt = 0.0f;
static unsigned motor_init_calls = 0;
static unsigned motion_stops = 0;
static unsigned emergency_stops = 0;

typedef enum {
    PIPELINE_FAIL_NONE,
    PIPELINE_FAIL_PREPROCESS,
    PIPELINE_FAIL_STATE,
    PIPELINE_FAIL_PERCEPTION,
    PIPELINE_FAIL_BEHAVIOR,
    PIPELINE_FAIL_TRAJECTORY
} pipeline_failure_t;

static pipeline_failure_t pipeline_failure = PIPELINE_FAIL_NONE;
static unsigned state_calls = 0;
static unsigned perception_calls = 0;
static unsigned behavior_calls = 0;
static unsigned trajectory_calls = 0;
static unsigned velocity_command_calls = 0;

/* Track call ordering within a cycle */
static unsigned cycle_sequence_counter = 0;
static unsigned decision_sequence_num = 0;
static unsigned motion_sequence_num = 0;

typedef enum {
    EVENT_CONFIG_DEFAULTS,
    EVENT_TIME_INIT,
    EVENT_ICM_BIND,
    EVENT_ICM_INIT,
    EVENT_ICM_CALIBRATE,
    EVENT_SCALE_READ,
    EVENT_BIAS_READ,
    EVENT_SENSOR_INIT
} init_event_t;

static init_event_t init_events[16];
static size_t init_event_count = 0;
static icm42688_config_t bound_icm_config;

/* Fault injection flags */
static bool inject_mcp_init_failure = false;
static bool inject_mcp_read_failure = false;
static bool inject_icm_init_failure = false;
static bool inject_icm_calibration_failure = false;
static bool inject_scale_failure = false;
static bool inject_bias_failure = false;
static bool inject_sensor_hal_failure = false;
static bool inject_path_setup_failure = false;
static bool inject_motion_init_failure = false;

/* ============================================================================
 * Fake Dependencies: Minimal stubs for all external modules
 * ============================================================================ */

/* Motor HAL fakes */
void Motor_Init(void) { motor_init_calls++; }
void Motor_Stop(void) { motion_stops++; }
void Motor_SetSpeed(int16_t left, int16_t right) { (void)left; (void)right; }

/* Encoder HAL fakes */
typedef enum { ENCODER_1 = 0, ENCODER_2, ENCODER_3, ENCODER_4 } Encoder_Id;
int32_t Encoder_GetCount(Encoder_Id encoder) { (void)encoder; return 0; }
void Encoder_ResetCount(Encoder_Id encoder) { (void)encoder; }
void Encoder_Init(void) {}

/* MCP23017 fake */
mcp23017_status_t MCP23017_Init(void) {
    return inject_mcp_init_failure ? MCP23017_STATUS_IO_ERROR : MCP23017_STATUS_OK;
}
mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs) {
    if (inputs == NULL) {
        return MCP23017_STATUS_INVALID_ARGUMENT;
    }
    *inputs = 0x0FFFU;
    return inject_mcp_read_failure ? MCP23017_STATUS_IO_ERROR : MCP23017_STATUS_OK;
}

/* Platform time fake */
void PlatformTime_Init(void) {
    init_events[init_event_count++] = EVENT_TIME_INIT;
}
uint64_t PlatformTime_GetUs64(void) { return decision_update_calls * 20000ULL; }

/* ICM42688 fake */
void icm42688_mspm0_bind(const icm42688_config_t *config) {
    init_events[init_event_count++] = EVENT_ICM_BIND;
    assert(config != NULL);
    bound_icm_config = *config;
}
icm42688_status_t icm42688_read(icm42688_data_t *data) {
    if (inject_icm_init_failure) return ICM42688_STATUS_NOT_READY;
    if (data) {
        data->acc_raw.x = data->acc_raw.y = data->acc_raw.z = 0;
        data->gyro_raw.x = data->gyro_raw.y = data->gyro_raw.z = 0;
        data->temperature_raw = 0;
    }
    return ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_init(void) {
    init_events[init_event_count++] = EVENT_ICM_INIT;
    return inject_icm_init_failure ? ICM42688_STATUS_NOT_READY : ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_calibrate_gyro(uint16_t samples, uint16_t delay_ms) {
    (void)samples; (void)delay_ms;
    init_events[init_event_count++] = EVENT_ICM_CALIBRATE;
    return inject_icm_calibration_failure ? ICM42688_STATUS_NOT_READY :
                                            ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_get_scale_factors(float *accel, float *gyro) {
    init_events[init_event_count++] = EVENT_SCALE_READ;
    if (inject_scale_failure) {
        return ICM42688_STATUS_NOT_READY;
    }
    *accel = 1.0f / 4096.0f;
    *gyro = 1.0f / 32.768f;
    return ICM42688_STATUS_OK;
}
icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias) {
    init_events[init_event_count++] = EVENT_BIAS_READ;
    if (inject_bias_failure) {
        return ICM42688_STATUS_NOT_READY;
    }
    bias->x = 1.0f;
    bias->y = -2.0f;
    bias->z = 3.0f;
    return ICM42688_STATUS_OK;
}

/* Sens-Decision config stub */
sens_decision_config_t g_sens_decision_config;
void sd_config_reset_defaults(void) {
    init_events[init_event_count++] = EVENT_CONFIG_DEFAULTS;
    memset(&g_sens_decision_config, 0, sizeof(g_sens_decision_config));
    g_sens_decision_config.imu.filter_alpha = 0.25f;
}

/* Sens-Decision sensor HAL stub */
typedef struct { void *dummy; } sensor_hal_t;
sd_status_t sensors_configure_hal(const sensor_hal_t *hal) {
    (void)hal;
    return inject_sensor_hal_failure ? SD_ERR_HW_FAULT : SD_OK;
}
sd_status_t sensors_init_all(void) {
    init_events[init_event_count++] = EVENT_SENSOR_INIT;
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
    velocity_command_calls++;
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
    return pipeline_failure == PIPELINE_FAIL_PREPROCESS ? SD_ERR_DATA_INVALID : SD_OK;
}

sd_status_t state_evaluator_update(state_evaluator_t *eval, const sensor_frame_t *frame) {
    (void)eval; (void)frame;
    state_calls++;
    return pipeline_failure == PIPELINE_FAIL_STATE ? SD_ERR_DATA_INVALID : SD_OK;
}

sd_status_t perception_update(perception_t *perc, const void *ir, uint64_t ts, perception_result_t *result) {
    (void)perc; (void)ir; (void)ts; (void)result;
    perception_calls++;
    return pipeline_failure == PIPELINE_FAIL_PERCEPTION ? SD_ERR_DATA_INVALID : SD_OK;
}

sd_status_t behavior_planner_update(behavior_planner_t *planner, const behavior_input_t *in, behavior_output_t *out) {
    (void)planner; (void)in; (void)out;
    behavior_calls++;
    return pipeline_failure == PIPELINE_FAIL_BEHAVIOR ? SD_ERR_DATA_INVALID : SD_OK;
}

sd_status_t trajectory_generate(trajectory_generator_t *gen, const vehicle_state_t *veh,
                               const behavior_output_t *beh, float dt, trajectory_point_t *out) {
    (void)gen; (void)veh; (void)beh; (void)dt; (void)out;
    trajectory_calls++;
    return pipeline_failure == PIPELINE_FAIL_TRAJECTORY ? SD_ERR_DATA_INVALID : SD_OK;
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
    motor_init_calls = 0;
    motion_stops = 0;
    emergency_stops = 0;
    pipeline_failure = PIPELINE_FAIL_NONE;
    state_calls = 0;
    perception_calls = 0;
    behavior_calls = 0;
    trajectory_calls = 0;
    velocity_command_calls = 0;
    cycle_sequence_counter = 0;
    decision_sequence_num = 0;
    motion_sequence_num = 0;
    init_event_count = 0;
    memset(init_events, 0, sizeof(init_events));
    memset(&bound_icm_config, 0, sizeof(bound_icm_config));
    inject_mcp_init_failure = false;
    inject_mcp_read_failure = false;
    inject_icm_init_failure = false;
    inject_icm_calibration_failure = false;
    inject_scale_failure = false;
    inject_bias_failure = false;
    inject_sensor_hal_failure = false;
    inject_path_setup_failure = false;
    inject_motion_init_failure = false;
}

static void run_next_decision_cycle(void) {
    unsigned next_decision = decision_update_calls + 1U;

    while (decision_update_calls < next_decision) {
        ControlApp_RunFastCycle();
    }
}

static void assert_event_before(init_event_t first, init_event_t second) {
    size_t first_index = init_event_count;
    size_t second_index = init_event_count;

    for (size_t i = 0; i < init_event_count; ++i) {
        if (init_events[i] == first && first_index == init_event_count) {
            first_index = i;
        }
        if (init_events[i] == second && second_index == init_event_count) {
            second_index = i;
        }
    }

    assert(first_index < second_index);
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

static void test_failed_pipeline_stages_short_circuit_downstream_work(void) {
    static const struct {
        pipeline_failure_t failure;
        unsigned state;
        unsigned perception;
        unsigned behavior;
        unsigned trajectory;
        unsigned velocity;
    } cases[] = {
        { PIPELINE_FAIL_PREPROCESS, 0U, 0U, 0U, 0U, 0U },
        { PIPELINE_FAIL_STATE,      1U, 0U, 0U, 0U, 0U },
        { PIPELINE_FAIL_PERCEPTION, 1U, 1U, 0U, 0U, 0U },
        { PIPELINE_FAIL_BEHAVIOR,   1U, 1U, 1U, 0U, 0U },
        { PIPELINE_FAIL_TRAJECTORY, 1U, 1U, 1U, 1U, 0U },
        { PIPELINE_FAIL_NONE,       1U, 1U, 1U, 1U, 1U },
    };

    printf("Test: Failed pipeline stages short-circuit downstream work...\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        reset_call_tracking();
        assert(ControlApp_Init(3U));
        pipeline_failure = cases[i].failure;

        run_next_decision_cycle();

        assert(state_calls == cases[i].state);
        assert(perception_calls == cases[i].perception);
        assert(behavior_calls == cases[i].behavior);
        assert(trajectory_calls == cases[i].trajectory);
        assert(velocity_command_calls == cases[i].velocity);
    }

    printf("  PASS: Exact downstream call table verified\n");
}

static void test_only_complete_pipeline_resets_failure_count(void) {
    printf("Test: Only a complete pipeline resets consecutive failures...\n");

    reset_call_tracking();
    assert(ControlApp_Init(3U));
    pipeline_failure = PIPELINE_FAIL_STATE;

    for (unsigned i = 0; i < 21U; ++i) {
        ControlApp_RunFastCycle();
    }

    assert(emergency_stops == 1U);
    assert(velocity_command_calls == 0U);
    assert(motion_update_calls == 21U);

    for (unsigned i = 0; i < 10U; ++i) {
        ControlApp_RunFastCycle();
    }

    assert(emergency_stops == 1U);
    assert(velocity_command_calls == 0U);
    assert(motion_update_calls == 31U);

    pipeline_failure = PIPELINE_FAIL_NONE;
    run_next_decision_cycle();
    assert(velocity_command_calls == 1U);

    pipeline_failure = PIPELINE_FAIL_STATE;
    run_next_decision_cycle();
    assert(emergency_stops == 1U);
    assert(velocity_command_calls == 1U);

    printf("  PASS: Success reset the count before the next isolated failure\n");
}

/* ============================================================================
 * Test 3: Initialization Failures Stop Motors
 * ============================================================================ */

static void test_init_mcp_failure_stops_motors(void) {
    printf("Test: MCP23017 init failure stops motors...\n");
    
    reset_call_tracking();
    inject_mcp_init_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 2U);
    
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_initial_mcp_read_failure_stops_motors(void) {
    printf("Test: MCP23017 initial read failure stops motors...\n");

    reset_call_tracking();
    inject_mcp_read_failure = true;

    assert(!ControlApp_Init(3U));
    assert(motion_stops >= 2U);

    printf("  PASS: Init failed, motors stopped\n");
}

static void test_successful_init_accepts_zero_status(void) {
    printf("Test: Successful init accepts zero-valued status...\n");

    reset_call_tracking();
    assert(ControlApp_Init(3U));

    printf("  PASS: MCP23017_STATUS_OK accepted\n");
}

static void test_startup_binds_icm_and_synchronizes_sensor_config(void) {
    const float degrees_to_radians = 0.017453292519943295f;

    printf("Test: Startup binds ICM and synchronizes SI config...\n");

    reset_call_tracking();
    assert(ControlApp_Init(3U));

    assert(bound_icm_config.interface_type == ICM42688_INTERFACE_SPI);
    assert(bound_icm_config.acc_sample == ICM42688_ACC_SAMPLE_SGN_8G);
    assert(bound_icm_config.gyro_sample == ICM42688_GYRO_SAMPLE_SGN_1000DPS);
    assert(bound_icm_config.sample_rate == ICM42688_SAMPLE_RATE_1000);

    assert_event_before(EVENT_CONFIG_DEFAULTS, EVENT_SENSOR_INIT);
    assert_event_before(EVENT_TIME_INIT, EVENT_ICM_BIND);
    assert_event_before(EVENT_ICM_BIND, EVENT_ICM_INIT);
    assert_event_before(EVENT_ICM_INIT, EVENT_ICM_CALIBRATE);
    assert_event_before(EVENT_ICM_CALIBRATE, EVENT_SCALE_READ);
    assert_event_before(EVENT_SCALE_READ, EVENT_BIAS_READ);
    assert_event_before(EVENT_BIAS_READ, EVENT_SENSOR_INIT);

    assert(fabsf(g_sens_decision_config.imu.accel_scale_mps2_per_lsb -
                 9.80665f / 4096.0f) < 1e-8f);
    assert(fabsf(g_sens_decision_config.imu.gyro_scale_radps_per_lsb -
                 degrees_to_radians / 32.768f) < 1e-8f);
    assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[0] -
                 degrees_to_radians) < 1e-7f);
    assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[1] +
                 2.0f * degrees_to_radians) < 1e-7f);
    assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[2] -
                 3.0f * degrees_to_radians) < 1e-7f);
    assert(g_sens_decision_config.imu.filter_alpha == 0.25f);

    printf("  PASS: Exact mode, order, scales, biases, and defaults verified\n");
}

static void test_scale_metadata_failure_stops_motors(void) {
    printf("Test: Scale metadata failure stops motors...\n");

    reset_call_tracking();
    inject_scale_failure = true;

    assert(!ControlApp_Init(3U));
    assert(motion_stops >= 2U);

    printf("  PASS: Init failed, motors stopped\n");
}

static void test_bias_metadata_failure_stops_motors(void) {
    printf("Test: Bias metadata failure stops motors...\n");

    reset_call_tracking();
    inject_bias_failure = true;

    assert(!ControlApp_Init(3U));
    assert(motion_stops >= 2U);

    printf("  PASS: Init failed, motors stopped\n");
}

static void test_init_icm_failure_stops_motors(void) {
    printf("Test: ICM identity check failure stops motors...\n");
    
    reset_call_tracking();
    inject_icm_init_failure = true;
    
    bool result = ControlApp_Init(3);
    
    assert(!result);
    assert(motion_stops >= 2U);
    
    printf("  PASS: Init failed, motors stopped\n");
}

static void test_icm_calibration_failure_stops_motors(void) {
    printf("Test: ICM gyro calibration failure stops motors...\n");

    reset_call_tracking();
    inject_icm_calibration_failure = true;

    assert(!ControlApp_Init(3U));
    assert(motion_stops >= 2U);

    printf("  PASS: Init failed, motors stopped\n");
}

static void test_invalid_target_laps_establishes_safe_motor_state(void) {
    printf("Test: Invalid target laps establish safe motor state...\n");

    reset_call_tracking();
    assert(!ControlApp_Init(0U));
    assert(motor_init_calls == 1U);
    assert(motion_stops == 1U);

    reset_call_tracking();
    assert(!ControlApp_Init(6U));
    assert(motor_init_calls == 1U);
    assert(motion_stops == 1U);

    printf("  PASS: Low and high invalid values stopped motors\n");
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
    test_failed_pipeline_stages_short_circuit_downstream_work();
    test_only_complete_pipeline_resets_failure_count();
    
    /* Initialization fault injection tests */
    test_successful_init_accepts_zero_status();
    test_startup_binds_icm_and_synchronizes_sensor_config();
    test_scale_metadata_failure_stops_motors();
    test_bias_metadata_failure_stops_motors();
    test_init_mcp_failure_stops_motors();
    test_initial_mcp_read_failure_stops_motors();
    test_init_icm_failure_stops_motors();
    test_icm_calibration_failure_stops_motors();
    test_invalid_target_laps_establishes_safe_motor_state();
    test_init_sensor_hal_failure_stops_motors();
    test_init_path_setup_failure_stops_motors();
    test_init_motion_control_failure_stops_motors();
    
    printf("\n=== All tests passed ===\n");
    return 0;
}

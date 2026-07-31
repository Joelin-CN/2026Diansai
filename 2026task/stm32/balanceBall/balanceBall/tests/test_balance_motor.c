#include <math.h>
#include <stdio.h>
#include <string.h>

#include "balance_motor.h"

static int failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

typedef struct {
    BalanceMotorTxResult result;
    uint8_t frame[EMM_V5_MAX_FRAME_SIZE];
    size_t length;
    uint8_t expected_function;
    size_t expected_length;
    unsigned attempt_count;
    unsigned send_count;
} FakeTransport;

static const BalanceMotorConfig config = {
    .address = 0x01U,
    .pulses_per_position_unit = 100.0f,
    .max_consecutive_failures = 3U,
};

static BalanceMotorTxResult fake_send(void *context, const uint8_t *frame,
                                      size_t length, uint8_t expected_function,
                                      size_t expected_length)
{
    FakeTransport *fake = context;
    fake->attempt_count++;
    if (fake->result == BALANCE_MOTOR_TX_ACCEPTED) {
        memcpy(fake->frame, frame, length);
        fake->length = length;
        fake->expected_function = expected_function;
        fake->expected_length = expected_length;
        fake->send_count++;
    }
    return fake->result;
}

static void complete_ack(BalanceMotor *motor, uint8_t function)
{
    const uint8_t response[] = {0x01U, function, EMM_V5_ACK_COMPLETE, 0x6BU};
    balance_motor_on_response(motor, function, response, sizeof(response));
}

static void setup(BalanceMotor *motor, FakeTransport *fake)
{
    memset(fake, 0, sizeof(*fake));
    fake->result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_init(motor, &config,
                       (BalanceMotorTransport){.send = fake_send, .context = fake});
}

static void check_frame(const FakeTransport *fake, const uint8_t *expected,
                        size_t expected_length)
{
    CHECK_TRUE(fake->length == expected_length);
    CHECK_TRUE(memcmp(fake->frame, expected, expected_length) == 0);
}

static void establish_zero(BalanceMotor *motor, int32_t position)
{
    const uint32_t magnitude = position < 0 ? (uint32_t)(-(int64_t)position)
                                            : (uint32_t)position;
    const uint8_t response[] = {
        0x01U, 0x36U, position < 0 ? 1U : 0U,
        (uint8_t)(magnitude >> 24U), (uint8_t)(magnitude >> 16U),
        (uint8_t)(magnitude >> 8U), (uint8_t)magnitude, 0x6BU,
    };
    CHECK_TRUE(balance_motor_request_zero(motor) == BALANCE_MOTOR_OK);
    balance_motor_on_response(motor, 0x36U, response, sizeof(response));
    CHECK_TRUE(balance_motor_has_zero(motor));
}

static void test_init_and_zero_query(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const uint8_t zero_response[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 3U, 0xE8U, 0x6BU};
    const uint8_t bad_response[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 3U, 0xE8U, 0x00U};
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};

    setup(&motor, &fake);
    CHECK_TRUE(fake.send_count == 0U);
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_NOT_ZEROED);
    CHECK_TRUE(balance_motor_request_zero(&motor) == BALANCE_MOTOR_OK);
    check_frame(&fake, (const uint8_t[]){0x01U, 0x36U, 0x6BU}, 3U);
    CHECK_TRUE(fake.expected_function == 0x36U && fake.expected_length == 8U);
    balance_motor_on_response(&motor, 0x36U, bad_response, sizeof(bad_response));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    balance_motor_on_response(&motor, 0x36U, zero_response, sizeof(zero_response));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_request_zero(&motor) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0x36U, zero_response, sizeof(zero_response));
    CHECK_TRUE(balance_motor_has_zero(&motor));
}

static void test_unsolicited_and_mismatched_responses_are_ignored(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const uint8_t zero_response[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 3U, 0xE8U, 0x6BU};
    const uint8_t ack_response[] = {0x01U, 0xFDU, EMM_V5_ACK_COMPLETE, 0x6BU};

    setup(&motor, &fake);
    balance_motor_on_response(&motor, 0x36U, zero_response, sizeof(zero_response));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_request_zero(&motor) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0xFDU, ack_response, sizeof(ack_response));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    balance_motor_on_response(&motor, 0x36U, zero_response, sizeof(zero_response));
    CHECK_TRUE(balance_motor_has_zero(&motor));
}

static void test_position_offsets_emit_complete_absolute_frames(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    BalanceActuatorCommand command = {.position = 2.5f, .speed = 20.0f,
                                      .acceleration = 50.0f};

    setup(&motor, &fake);
    establish_zero(&motor, 1000);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFDU, 0x00U, 0x00U, 0x14U,
                                        0x32U, 0x00U, 0x00U, 0x04U, 0xE2U,
                                        0x01U, 0x00U, 0x6BU}, 13U);
    complete_ack(&motor, 0xFDU);
    command.position = -2.5f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFDU, 0x00U, 0x00U, 0x14U,
                                        0x32U, 0x00U, 0x00U, 0x02U, 0xEEU,
                                        0x01U, 0x00U, 0x6BU}, 13U);
}

static void test_invalid_and_overflow_commands_are_rejected(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    BalanceActuatorCommand command = {.position = 0.0f, .speed = 1.0f,
                                      .acceleration = 1.0f};

    setup(&motor, &fake);
    establish_zero(&motor, INT32_MAX);
    command.position = NAN;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_INVALID_ARGUMENT);
    command.position = INFINITY;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_INVALID_ARGUMENT);
    command.position = 0.0f;
    command.speed = -1.0f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_INVALID_ARGUMENT);
    command.speed = 1.0f;
    command.acceleration = -1.0f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_INVALID_ARGUMENT);
    command.acceleration = 1.0f;
    command.position = (float)INT32_MAX;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW);
    command.position = 0.01f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW);
    command.position = 0.0f;
    command.speed = 65535.5f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW);
    command.speed = 1.0f;
    command.acceleration = 255.5f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW);
}

static void test_float_value_above_int32_max_is_rejected_before_cast(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    BalanceMotorConfig unit_config = config;
    const BalanceActuatorCommand command = {
        .position = 2147483648.0f,
        .speed = 1.0f,
        .acceleration = 1.0f,
    };

    unit_config.pulses_per_position_unit = 1.0f;
    memset(&fake, 0, sizeof(fake));
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_init(&motor, &unit_config,
                       (BalanceMotorTransport){.send = fake_send, .context = &fake});
    establish_zero(&motor, 0);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW);
}

static void test_busy_transport_keeps_only_latest_target(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                      .acceleration = 50.0f};
    setup(&motor, &fake);
    establish_zero(&motor, 1000);
    fake.result = BALANCE_MOTOR_TX_BUSY;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    command.position = 2.0f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    command.position = 3.0f;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 2U);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFDU, 0x00U, 0x00U, 0x14U,
                                        0x32U, 0x00U, 0x00U, 0x05U, 0x14U,
                                        0x01U, 0x00U, 0x6BU}, 13U);
}

static void test_priority_commands_clear_motion_and_run_first(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};
    setup(&motor, &fake);
    establish_zero(&motor, 1000);
    fake.result = BALANCE_MOTOR_TX_BUSY;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    CHECK_TRUE(balance_motor_stop(&motor) == BALANCE_MOTOR_QUEUED);
    CHECK_TRUE(balance_motor_disable(&motor) == BALANCE_MOTOR_QUEUED);
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_process(&motor);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xF3U, 0xABU, 0x00U, 0x00U, 0x6BU}, 6U);
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 2U);
}

static void test_pending_stop_precedes_later_motion(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};

    setup(&motor, &fake);
    establish_zero(&motor, 1000);
    fake.result = BALANCE_MOTOR_TX_BUSY;
    CHECK_TRUE(balance_motor_stop(&motor) == BALANCE_MOTOR_QUEUED);
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    CHECK_TRUE(fake.send_count == 1U);
    balance_motor_process(&motor);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFEU, 0x98U, 0x00U, 0x6BU}, 5U);
    complete_ack(&motor, 0xFEU);
    balance_motor_process(&motor);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFDU, 0x00U, 0x00U, 0x14U,
                                        0x32U, 0x00U, 0x00U, 0x04U, 0x4CU,
                                        0x01U, 0x00U, 0x6BU}, 13U);
}

static void test_failures_lock_and_rezero_is_required(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};
    const uint8_t bad_ack[] = {0x01U, 0xFDU, EMM_V5_ACK_BAD_COMMAND, 0x6BU};
    setup(&motor, &fake);
    establish_zero(&motor, 1000);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0xFDU, bad_ack, sizeof(bad_ack));
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0xFDU, bad_ack, sizeof(bad_ack));
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0xFDU, bad_ack, sizeof(bad_ack));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_LOCKED);
    CHECK_TRUE(balance_motor_stop(&motor) == BALANCE_MOTOR_LOCKED);
    balance_motor_clear_fault(&motor);
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_NOT_ZEROED);
    establish_zero(&motor, 1000);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OK);
}

static void test_failed_priority_retries_before_queued_target(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};
    setup(&motor, &fake);
    establish_zero(&motor, 0);
    fake.result = BALANCE_MOTOR_TX_FAILED;
    CHECK_TRUE(balance_motor_stop(&motor) == BALANCE_MOTOR_TRANSPORT_ERROR);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 2U);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFEU, 0x98U, 0x00U, 0x6BU}, 5U);
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 2U);
    complete_ack(&motor, 0xFEU);
    balance_motor_process(&motor);
    CHECK_TRUE(fake.send_count == 3U);
    check_frame(&fake, (const uint8_t[]){0x01U, 0xFDU, 0x00U, 0x00U, 0x14U,
                                        0x32U, 0x00U, 0x00U, 0x00U, 0x64U,
                                        0x01U, 0x00U, 0x6BU}, 13U);
}

static void test_lockout_and_clear_discard_all_pending_work(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const BalanceActuatorCommand command = {.position = 1.0f, .speed = 20.0f,
                                            .acceleration = 50.0f};
    unsigned attempts_before_process;

    setup(&motor, &fake);
    establish_zero(&motor, 0);
    fake.result = BALANCE_MOTOR_TX_BUSY;
    CHECK_TRUE(balance_motor_stop(&motor) == BALANCE_MOTOR_QUEUED);
    CHECK_TRUE(balance_motor_submit(&motor, &command) == BALANCE_MOTOR_QUEUED);
    balance_motor_on_transport_error(&motor);
    balance_motor_on_transport_error(&motor);
    balance_motor_on_transport_error(&motor);
    attempts_before_process = fake.attempt_count;
    fake.result = BALANCE_MOTOR_TX_ACCEPTED;
    balance_motor_process(&motor);
    CHECK_TRUE(fake.attempt_count == attempts_before_process);
    balance_motor_clear_fault(&motor);
    balance_motor_process(&motor);
    CHECK_TRUE(fake.attempt_count == attempts_before_process);
}

static void test_stale_zero_response_after_fault_clear_is_ignored(void)
{
    BalanceMotor motor;
    FakeTransport fake;
    const uint8_t stale_zero[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 3U, 0xE8U, 0x6BU};

    setup(&motor, &fake);
    CHECK_TRUE(balance_motor_request_zero(&motor) == BALANCE_MOTOR_OK);
    balance_motor_on_transport_error(&motor);
    balance_motor_on_transport_error(&motor);
    balance_motor_on_transport_error(&motor);
    balance_motor_clear_fault(&motor);
    balance_motor_on_response(&motor, 0x36U, stale_zero, sizeof(stale_zero));
    CHECK_TRUE(!balance_motor_has_zero(&motor));
    CHECK_TRUE(balance_motor_request_zero(&motor) == BALANCE_MOTOR_OK);
    balance_motor_on_response(&motor, 0x36U, stale_zero, sizeof(stale_zero));
    CHECK_TRUE(balance_motor_has_zero(&motor));
}

int main(void)
{
    test_init_and_zero_query();
    test_unsolicited_and_mismatched_responses_are_ignored();
    test_position_offsets_emit_complete_absolute_frames();
    test_invalid_and_overflow_commands_are_rejected();
    test_float_value_above_int32_max_is_rejected_before_cast();
    test_busy_transport_keeps_only_latest_target();
    test_priority_commands_clear_motion_and_run_first();
    test_pending_stop_precedes_later_motion();
    test_failures_lock_and_rezero_is_required();
    test_failed_priority_retries_before_queued_target();
    test_lockout_and_clear_discard_all_pending_work();
    test_stale_zero_response_after_fault_clear_is_ignored();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

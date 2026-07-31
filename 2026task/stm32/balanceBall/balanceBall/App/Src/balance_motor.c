#include "balance_motor.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define EMM_V5_POSITION_FUNCTION 0xFDU
#define EMM_V5_POSITION_RESPONSE_LENGTH 8U
#define EMM_V5_ACK_RESPONSE_LENGTH 4U

static void lock_if_needed(BalanceMotor *motor)
{
    if (motor->config.max_consecutive_failures != 0U
        && motor->consecutive_failures >= motor->config.max_consecutive_failures) {
        motor->locked = true;
        motor->zero_valid = false;
        motor->priority.pending = false;
        motor->target.pending = false;
    }
}

static void record_failure(BalanceMotor *motor)
{
    if (motor == NULL || motor->locked) {
        return;
    }
    if (motor->consecutive_failures < UINT8_MAX) {
        motor->consecutive_failures++;
    }
    lock_if_needed(motor);
}

static BalanceMotorResult send_frame(BalanceMotor *motor,
                                     BalanceMotorPendingFrame *pending)
{
    BalanceMotorTxResult result;

    if (motor->transport.send == NULL) {
        pending->pending = false;
        record_failure(motor);
        return BALANCE_MOTOR_TRANSPORT_ERROR;
    }
    result = motor->transport.send(motor->transport.context, pending->bytes,
                                   pending->length, pending->expected_function,
                                   pending->expected_length);
    if (result == BALANCE_MOTOR_TX_BUSY) {
        pending->pending = true;
        return BALANCE_MOTOR_QUEUED;
    }

    pending->pending = false;
    if (result == BALANCE_MOTOR_TX_FAILED) {
        record_failure(motor);
        return BALANCE_MOTOR_TRANSPORT_ERROR;
    }
    return BALANCE_MOTOR_OK;
}

static BalanceMotorResult encode_priority(BalanceMotor *motor, bool disable)
{
    EmmV5Frame frame;
    EmmV5Result encode_result;

    if (motor == NULL) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    if (motor->locked) {
        return BALANCE_MOTOR_LOCKED;
    }
    motor->target.pending = false;
    frame = (EmmV5Frame){.data = motor->priority.bytes,
                         .capacity = sizeof(motor->priority.bytes)};
    if (disable) {
        encode_result = emm_v5_encode_enable(motor->config.address, false, false,
                                             &frame);
        motor->priority.expected_function = 0xF3U;
    } else {
        encode_result = emm_v5_encode_stop(motor->config.address, false, &frame);
        motor->priority.expected_function = 0xFEU;
    }
    if (encode_result != EMM_V5_OK) {
        motor->priority.pending = false;
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    motor->priority.length = frame.length;
    motor->priority.expected_length = EMM_V5_ACK_RESPONSE_LENGTH;
    motor->priority.pending = true;
    return send_frame(motor, &motor->priority);
}

void balance_motor_init(BalanceMotor *motor,
                        const BalanceMotorConfig *config,
                        BalanceMotorTransport transport)
{
    if (motor == NULL) {
        return;
    }
    memset(motor, 0, sizeof(*motor));
    motor->transport = transport;
    if (config == NULL) {
        motor->locked = true;
        return;
    }
    motor->config = *config;
    if (config->address == 0U || !isfinite(config->pulses_per_position_unit)
        || config->pulses_per_position_unit <= 0.0f
        || config->max_consecutive_failures == 0U) {
        motor->locked = true;
    }
}

BalanceMotorResult balance_motor_request_zero(BalanceMotor *motor)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE];
    EmmV5Frame frame = {.data = bytes, .capacity = sizeof(bytes)};
    BalanceMotorPendingFrame query = {0};
    BalanceMotorResult result;

    if (motor == NULL) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    if (motor->locked) {
        return BALANCE_MOTOR_LOCKED;
    }
    if (emm_v5_encode_position_query(motor->config.address, &frame) != EMM_V5_OK) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    memcpy(query.bytes, bytes, frame.length);
    query.length = frame.length;
    query.expected_function = 0x36U;
    query.expected_length = EMM_V5_POSITION_RESPONSE_LENGTH;
    result = send_frame(motor, &query);
    return result == BALANCE_MOTOR_QUEUED ? BALANCE_MOTOR_BUSY : result;
}

BalanceMotorResult balance_motor_submit(BalanceMotor *motor,
                                        const BalanceActuatorCommand *command)
{
    float scaled;
    float rounded_speed;
    float rounded_acceleration;
    int32_t offset;
    int64_t absolute_target;
    uint64_t magnitude;
    EmmV5PositionCommand position;
    EmmV5Frame frame;

    if (motor == NULL || command == NULL) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    if (motor->locked) {
        return BALANCE_MOTOR_LOCKED;
    }
    if (!motor->zero_valid) {
        return BALANCE_MOTOR_NOT_ZEROED;
    }
    if (!isfinite(command->position) || !isfinite(command->speed)
        || !isfinite(command->acceleration) || command->speed < 0.0f
        || command->acceleration < 0.0f) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    if (command->speed > (float)UINT16_MAX
        || command->acceleration > (float)UINT8_MAX) {
        return BALANCE_MOTOR_OVERFLOW;
    }
    rounded_speed = roundf(command->speed);
    rounded_acceleration = roundf(command->acceleration);
    if (rounded_speed > (float)UINT16_MAX
        || rounded_acceleration > (float)UINT8_MAX) {
        return BALANCE_MOTOR_OVERFLOW;
    }
    scaled = command->position * motor->config.pulses_per_position_unit;
    if (!isfinite(scaled) || (double)scaled < (double)INT32_MIN
        || (double)scaled > (double)INT32_MAX) {
        return BALANCE_MOTOR_OVERFLOW;
    }
    scaled = roundf(scaled);
    if ((double)scaled < (double)INT32_MIN || (double)scaled > (double)INT32_MAX) {
        return BALANCE_MOTOR_OVERFLOW;
    }
    offset = (int32_t)scaled;
    absolute_target = (int64_t)motor->zero_position + (int64_t)offset;
    if (absolute_target < INT32_MIN || absolute_target > INT32_MAX) {
        return BALANCE_MOTOR_OVERFLOW;
    }
    magnitude = absolute_target < 0 ? (uint64_t)(-absolute_target)
                                    : (uint64_t)absolute_target;
    position = (EmmV5PositionCommand){
        .direction = absolute_target < 0 ? EMM_V5_DIRECTION_CCW
                                        : EMM_V5_DIRECTION_CW,
        .speed_rpm = (uint16_t)rounded_speed,
        .acceleration = (uint8_t)rounded_acceleration,
        .pulse_count = (uint32_t)magnitude,
        .absolute = true,
        .synchronized = false,
    };
    frame = (EmmV5Frame){.data = motor->target.bytes,
                         .capacity = sizeof(motor->target.bytes)};
    if (emm_v5_encode_position(motor->config.address, &position, &frame) != EMM_V5_OK) {
        return BALANCE_MOTOR_INVALID_ARGUMENT;
    }
    motor->target.length = frame.length;
    motor->target.expected_function = EMM_V5_POSITION_FUNCTION;
    motor->target.expected_length = EMM_V5_ACK_RESPONSE_LENGTH;
    motor->target.pending = true;
    if (motor->priority.pending) {
        return BALANCE_MOTOR_QUEUED;
    }
    return send_frame(motor, &motor->target);
}

BalanceMotorResult balance_motor_stop(BalanceMotor *motor)
{
    return encode_priority(motor, false);
}

BalanceMotorResult balance_motor_disable(BalanceMotor *motor)
{
    return encode_priority(motor, true);
}

void balance_motor_on_response(BalanceMotor *motor,
                               uint8_t expected_function,
                               const uint8_t *response,
                               size_t response_length)
{
    EmmV5Result result;

    if (motor == NULL || motor->locked) {
        return;
    }
    if (expected_function == 0x36U) {
        int32_t position;
        result = emm_v5_parse_position(motor->config.address, response,
                                       response_length, &position);
        if (result == EMM_V5_OK) {
            motor->zero_position = position;
            motor->zero_valid = true;
        }
    } else {
        EmmV5Ack ack;
        result = emm_v5_parse_ack(motor->config.address, expected_function,
                                  response, response_length, &ack);
    }
    if (result == EMM_V5_OK) {
        motor->consecutive_failures = 0U;
    } else {
        record_failure(motor);
    }
}

void balance_motor_on_transport_error(BalanceMotor *motor)
{
    record_failure(motor);
}

void balance_motor_process(BalanceMotor *motor)
{
    if (motor == NULL || motor->locked) {
        return;
    }
    if (motor->priority.pending) {
        (void)send_frame(motor, &motor->priority);
    } else if (motor->target.pending) {
        (void)send_frame(motor, &motor->target);
    }
}

void balance_motor_clear_fault(BalanceMotor *motor)
{
    if (motor == NULL) {
        return;
    }
    motor->locked = false;
    motor->consecutive_failures = 0U;
    motor->zero_valid = false;
    motor->priority.pending = false;
    motor->target.pending = false;
}

bool balance_motor_has_zero(const BalanceMotor *motor)
{
    return motor != NULL && motor->zero_valid && !motor->locked;
}

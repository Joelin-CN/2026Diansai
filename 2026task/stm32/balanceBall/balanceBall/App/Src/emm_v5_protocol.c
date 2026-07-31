#include "emm_v5_protocol.h"

#include <string.h>

typedef struct {
    EmmV5Frame *frame;
    size_t next;
} EmmV5Builder;

static EmmV5Result builder_begin(EmmV5Builder *builder, EmmV5Frame *frame,
                                 size_t required)
{
    if (frame == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    frame->length = 0U;
    if (frame->data == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    if (frame->capacity < required) {
        return EMM_V5_BUFFER_TOO_SMALL;
    }
    builder->frame = frame;
    builder->next = 0U;
    return EMM_V5_OK;
}

static void builder_u8(EmmV5Builder *builder, uint8_t value)
{
    builder->frame->data[builder->next++] = value;
}

static void builder_u16_be(EmmV5Builder *builder, uint16_t value)
{
    builder_u8(builder, (uint8_t)(value >> 8U));
    builder_u8(builder, (uint8_t)value);
}

static void builder_u32_be(EmmV5Builder *builder, uint32_t value)
{
    builder_u8(builder, (uint8_t)(value >> 24U));
    builder_u8(builder, (uint8_t)(value >> 16U));
    builder_u8(builder, (uint8_t)(value >> 8U));
    builder_u8(builder, (uint8_t)value);
}

static EmmV5Result builder_finish(EmmV5Builder *builder)
{
    builder_u8(builder, EMM_V5_FRAME_END);
    builder->frame->length = builder->next;
    return EMM_V5_OK;
}

static EmmV5Result begin_addressed(EmmV5Builder *builder, uint8_t address,
                                   EmmV5Frame *frame, size_t required)
{
    EmmV5Result result = builder_begin(builder, frame, required);
    if (result != EMM_V5_OK) {
        return result;
    }
    if (address == 0U) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    builder_u8(builder, address);
    return EMM_V5_OK;
}

static EmmV5Result invalid_argument(EmmV5Frame *frame)
{
    if (frame != NULL) {
        frame->length = 0U;
    }
    return EMM_V5_INVALID_ARGUMENT;
}

static bool direction_is_valid(EmmV5Direction direction)
{
    return direction == EMM_V5_DIRECTION_CW || direction == EMM_V5_DIRECTION_CCW;
}

EmmV5Result emm_v5_encode_enable(uint8_t address, bool enable,
                                  bool synchronized, EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 6U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xF3U);
    builder_u8(&builder, 0xABU);
    builder_u8(&builder, enable ? 1U : 0U);
    builder_u8(&builder, synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_velocity(uint8_t address, EmmV5Direction direction,
                                    uint16_t speed_rpm, uint8_t acceleration,
                                    bool synchronized, EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result;
    if (!direction_is_valid(direction)) {
        return invalid_argument(frame);
    }
    result = begin_addressed(&builder, address, frame, 8U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xF6U);
    builder_u8(&builder, (uint8_t)direction);
    builder_u16_be(&builder, speed_rpm);
    builder_u8(&builder, acceleration);
    builder_u8(&builder, synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_position(uint8_t address,
                                    const EmmV5PositionCommand *command,
                                    EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result;
    if (command == NULL || !direction_is_valid(command->direction)) {
        return invalid_argument(frame);
    }
    result = begin_addressed(&builder, address, frame, 13U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xFDU);
    builder_u8(&builder, (uint8_t)command->direction);
    builder_u16_be(&builder, command->speed_rpm);
    builder_u8(&builder, command->acceleration);
    builder_u32_be(&builder, command->pulse_count);
    builder_u8(&builder, command->absolute ? 1U : 0U);
    builder_u8(&builder, command->synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_fast_position_setup(uint8_t address,
                                               uint16_t speed_rpm,
                                               uint8_t acceleration,
                                               bool absolute,
                                               bool synchronized,
                                               EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 8U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xF1U);
    builder_u16_be(&builder, speed_rpm);
    builder_u8(&builder, acceleration);
    builder_u8(&builder, absolute ? 1U : 0U);
    builder_u8(&builder, synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_fast_move(uint8_t address, uint32_t pulse_count,
                                    EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 7U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xFCU);
    builder_u32_be(&builder, pulse_count);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_sync_trigger(EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = builder_begin(&builder, frame, 4U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0x00U);
    builder_u8(&builder, 0xFFU);
    builder_u8(&builder, 0x66U);
    return builder_finish(&builder);
}

static EmmV5Result encode_query(uint8_t address, uint8_t opcode,
                                EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 3U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, opcode);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_position_query(uint8_t address, EmmV5Frame *frame)
{
    return encode_query(address, 0x36U, frame);
}

EmmV5Result emm_v5_encode_status_query(uint8_t address, EmmV5Frame *frame)
{
    return encode_query(address, 0x3AU, frame);
}

EmmV5Result emm_v5_encode_microstep(uint8_t address, bool save,
                                    uint8_t microstep, EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 6U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0x84U);
    builder_u8(&builder, 0x8AU);
    builder_u8(&builder, save ? 1U : 0U);
    builder_u8(&builder, microstep);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_stop(uint8_t address, bool synchronized,
                               EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 5U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0xFEU);
    builder_u8(&builder, 0x98U);
    builder_u8(&builder, synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_set_zero(uint8_t address, bool save,
                                   EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 5U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0x93U);
    builder_u8(&builder, 0x88U);
    builder_u8(&builder, save ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_home(uint8_t address, EmmV5HomeMode mode,
                               bool synchronized, EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result;
    if (mode < EMM_V5_HOME_SINGLE_TURN || mode > EMM_V5_HOME_LIMIT_SWITCH) {
        return invalid_argument(frame);
    }
    result = begin_addressed(&builder, address, frame, 5U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0x9AU);
    builder_u8(&builder, (uint8_t)mode);
    builder_u8(&builder, synchronized ? 1U : 0U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_abort_home(uint8_t address, EmmV5Frame *frame)
{
    EmmV5Builder builder;
    EmmV5Result result = begin_addressed(&builder, address, frame, 4U);
    if (result != EMM_V5_OK) {
        return result;
    }
    builder_u8(&builder, 0x9CU);
    builder_u8(&builder, 0x48U);
    return builder_finish(&builder);
}

EmmV5Result emm_v5_encode_pid_query(uint8_t address, EmmV5Frame *frame)
{
    return encode_query(address, 0x21U, frame);
}

static EmmV5Result validate_response(uint8_t address, uint8_t function,
                                     const uint8_t *response, size_t length,
                                     size_t expected_length)
{
    if (response == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    if (length != expected_length || response[length - 1U] != EMM_V5_FRAME_END) {
        return EMM_V5_INVALID_FRAME;
    }
    if (response[0] != address || response[1] != function) {
        return EMM_V5_UNEXPECTED_RESPONSE;
    }
    return EMM_V5_OK;
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U)
           | ((uint32_t)bytes[1] << 16U)
           | ((uint32_t)bytes[2] << 8U)
           | (uint32_t)bytes[3];
}

EmmV5Result emm_v5_parse_ack(uint8_t address, uint8_t function,
                             const uint8_t *response, size_t length,
                             EmmV5Ack *ack)
{
    EmmV5Result result;
    EmmV5Ack decoded;

    if (ack == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    result = validate_response(address, function, response, length, 4U);
    if (result != EMM_V5_OK) {
        return result;
    }

    switch (response[2]) {
    case EMM_V5_ACK_COMPLETE:
    case EMM_V5_ACK_START:
    case EMM_V5_ACK_END:
    case EMM_V5_ACK_HOME_FAILED:
    case EMM_V5_ACK_CONFLICT:
    case EMM_V5_ACK_BAD_COMMAND:
        decoded = (EmmV5Ack)response[2];
        break;
    default:
        return EMM_V5_INVALID_FRAME;
    }

    *ack = decoded;
    if (decoded == EMM_V5_ACK_HOME_FAILED || decoded == EMM_V5_ACK_CONFLICT
        || decoded == EMM_V5_ACK_BAD_COMMAND) {
        return EMM_V5_DRIVER_ERROR;
    }
    return EMM_V5_OK;
}

EmmV5Result emm_v5_parse_position(uint8_t address, const uint8_t *response,
                                  size_t length, int32_t *position)
{
    EmmV5Result result;
    uint32_t magnitude;

    if (position == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    result = validate_response(address, 0x36U, response, length, 8U);
    if (result != EMM_V5_OK) {
        return result;
    }
    if (response[2] > 1U) {
        return EMM_V5_INVALID_FRAME;
    }
    magnitude = read_u32_be(&response[3]);
    if (magnitude > (uint32_t)INT32_MAX) {
        return EMM_V5_INVALID_FRAME;
    }

    *position = response[2] == 0U ? (int32_t)magnitude : -(int32_t)magnitude;
    return EMM_V5_OK;
}

EmmV5Result emm_v5_parse_status(uint8_t address, const uint8_t *response,
                                size_t length, uint8_t *status)
{
    EmmV5Result result;

    if (status == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    result = validate_response(address, 0x3AU, response, length, 4U);
    if (result != EMM_V5_OK) {
        return result;
    }
    *status = response[2];
    return EMM_V5_OK;
}

EmmV5Result emm_v5_parse_pid(uint8_t address, const uint8_t *response,
                             size_t length, EmmV5Pid *pid)
{
    EmmV5Result result;
    EmmV5Pid decoded;
    uint32_t value;

    if (pid == NULL) {
        return EMM_V5_INVALID_ARGUMENT;
    }
    result = validate_response(address, 0x21U, response, length, 15U);
    if (result != EMM_V5_OK) {
        return result;
    }

    value = read_u32_be(&response[2]);
    memcpy(&decoded.kp, &value, sizeof(decoded.kp));
    value = read_u32_be(&response[6]);
    memcpy(&decoded.ki, &value, sizeof(decoded.ki));
    value = read_u32_be(&response[10]);
    memcpy(&decoded.kd, &value, sizeof(decoded.kd));
    *pid = decoded;
    return EMM_V5_OK;
}

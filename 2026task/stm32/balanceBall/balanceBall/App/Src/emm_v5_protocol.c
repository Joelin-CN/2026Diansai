#include "emm_v5_protocol.h"

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

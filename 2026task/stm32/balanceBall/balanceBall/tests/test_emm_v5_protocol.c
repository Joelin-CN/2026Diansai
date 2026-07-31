#include <stdio.h>
#include <string.h>

#include "emm_v5_protocol.h"

static int failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

#define CHECK_BYTES(actual, expected, count) do { \
    const uint8_t *actual_bytes = (actual); \
    const uint8_t *expected_bytes = (expected); \
    const size_t byte_count = (count); \
    CHECK_TRUE(memcmp(actual_bytes, expected_bytes, byte_count) == 0); \
} while (0)

static EmmV5Frame make_frame(uint8_t *bytes)
{
    const EmmV5Frame frame = {
        .data = bytes,
        .capacity = EMM_V5_MAX_FRAME_SIZE,
        .length = 0U,
    };
    return frame;
}

static void check_frame(const EmmV5Frame *frame,
                        const uint8_t *expected,
                        size_t expected_length)
{
    CHECK_TRUE(frame->length == expected_length);
    CHECK_BYTES(frame->data, expected, expected_length);
}

static void test_enable_command(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_enable(0x01U, true, true, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x01, 0xF3, 0xAB, 0x01, 0x01, 0x6B}, 6U);
}

static void test_velocity_command_is_big_endian(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_velocity(0x02U, EMM_V5_DIRECTION_CCW,
                                      0x1234U, 0x56U, false, &frame) == EMM_V5_OK);
    check_frame(&frame,
                (const uint8_t[]){0x02, 0xF6, 0x01, 0x12, 0x34, 0x56, 0x00, 0x6B},
                8U);
}

static void test_position_command_is_big_endian(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    const EmmV5PositionCommand command = {
        .direction = EMM_V5_DIRECTION_CCW,
        .speed_rpm = 0x1234U,
        .acceleration = 0x56U,
        .pulse_count = 0x789ABCDEUL,
        .absolute = true,
        .synchronized = false,
    };

    CHECK_TRUE(emm_v5_encode_position(0x01U, &command, &frame) == EMM_V5_OK);
    check_frame(&frame,
                (const uint8_t[]){0x01, 0xFD, 0x01, 0x12, 0x34, 0x56,
                                  0x78, 0x9A, 0xBC, 0xDE, 0x01, 0x00, 0x6B},
                13U);
}

static void test_fast_position_commands(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_fast_position_setup(0x03U, 0x1234U, 0x56U,
                                                  true, true, &frame) == EMM_V5_OK);
    check_frame(&frame,
                (const uint8_t[]){0x03, 0xF1, 0x12, 0x34, 0x56, 0x01, 0x01, 0x6B},
                8U);
    CHECK_TRUE(emm_v5_encode_fast_move(0x03U, 0x789ABCDEUL, &frame) == EMM_V5_OK);
    check_frame(&frame,
                (const uint8_t[]){0x03, 0xFC, 0x78, 0x9A, 0xBC, 0xDE, 0x6B},
                7U);
}

static void test_sync_and_query_commands(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_sync_trigger(&frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x00, 0xFF, 0x66, 0x6B}, 4U);
    CHECK_TRUE(emm_v5_encode_position_query(0x04U, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x04, 0x36, 0x6B}, 3U);
    CHECK_TRUE(emm_v5_encode_status_query(0x04U, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x04, 0x3A, 0x6B}, 3U);
    CHECK_TRUE(emm_v5_encode_pid_query(0x04U, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x04, 0x21, 0x6B}, 3U);
}

static void test_configuration_and_stop_commands(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_microstep(0x05U, true, 0x80U, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x05, 0x84, 0x8A, 0x01, 0x80, 0x6B}, 6U);
    CHECK_TRUE(emm_v5_encode_stop(0x05U, true, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x05, 0xFE, 0x98, 0x01, 0x6B}, 5U);
    CHECK_TRUE(emm_v5_encode_set_zero(0x05U, false, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x05, 0x93, 0x88, 0x00, 0x6B}, 5U);
}

static void test_home_commands(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_home(0x06U, EMM_V5_HOME_LIMIT_SWITCH,
                                  false, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x06, 0x9A, 0x02, 0x00, 0x6B}, 5U);
    CHECK_TRUE(emm_v5_encode_abort_home(0x06U, &frame) == EMM_V5_OK);
    check_frame(&frame, (const uint8_t[]){0x06, 0x9C, 0x48, 0x6B}, 4U);
}

static void test_small_output_buffer_is_rejected(void)
{
    uint8_t bytes[4] = {0};
    EmmV5Frame frame = { .data = bytes, .capacity = sizeof(bytes) };
    CHECK_TRUE(emm_v5_encode_stop(0x01U, false, &frame) == EMM_V5_BUFFER_TOO_SMALL);
    CHECK_TRUE(frame.length == 0U);
}

static void test_null_pointers_are_rejected(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    const EmmV5PositionCommand command = {0};
    CHECK_TRUE(emm_v5_encode_enable(0x01U, true, false, NULL) == EMM_V5_INVALID_ARGUMENT);
    frame.data = NULL;
    frame.length = 9U;
    CHECK_TRUE(emm_v5_encode_status_query(0x01U, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
    frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_position(0x01U, NULL, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
    CHECK_TRUE(emm_v5_encode_position(0x01U, &command, NULL) == EMM_V5_INVALID_ARGUMENT);
}

static void test_non_broadcast_commands_reject_zero_address(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    CHECK_TRUE(emm_v5_encode_position_query(0x00U, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
    CHECK_TRUE(emm_v5_encode_enable(0x00U, true, false, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
}

static void test_invalid_direction_and_mode_are_rejected(void)
{
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE] = {0};
    EmmV5Frame frame = make_frame(bytes);
    EmmV5PositionCommand command = { .direction = (EmmV5Direction)2 };
    CHECK_TRUE(emm_v5_encode_velocity(0x01U, (EmmV5Direction)2,
                                      0U, 0U, false, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
    CHECK_TRUE(emm_v5_encode_position(0x01U, &command, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
    CHECK_TRUE(emm_v5_encode_home(0x01U, (EmmV5HomeMode)3,
                                  false, &frame) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(frame.length == 0U);
}

int main(void)
{
    test_enable_command();
    test_velocity_command_is_big_endian();
    test_position_command_is_big_endian();
    test_fast_position_commands();
    test_sync_and_query_commands();
    test_configuration_and_stop_commands();
    test_home_commands();
    test_small_output_buffer_is_rejected();
    test_null_pointers_are_rejected();
    test_non_broadcast_commands_reject_zero_address();
    test_invalid_direction_and_mode_are_rejected();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

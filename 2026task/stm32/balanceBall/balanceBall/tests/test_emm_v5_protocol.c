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

static void test_ack_response_decodes_progress_and_errors(void)
{
    static const struct {
        uint8_t status;
        EmmV5Ack ack;
        EmmV5Result result;
    } cases[] = {
        {0x02U, EMM_V5_ACK_COMPLETE, EMM_V5_OK},
        {0x12U, EMM_V5_ACK_START, EMM_V5_OK},
        {0x22U, EMM_V5_ACK_END, EMM_V5_OK},
        {0x9FU, EMM_V5_ACK_HOME_FAILED, EMM_V5_DRIVER_ERROR},
        {0xE2U, EMM_V5_ACK_CONFLICT, EMM_V5_DRIVER_ERROR},
        {0xEEU, EMM_V5_ACK_BAD_COMMAND, EMM_V5_DRIVER_ERROR},
    };
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const uint8_t response[] = {0x01U, 0xFDU, cases[index].status, 0x6BU};
        EmmV5Ack ack = EMM_V5_ACK_COMPLETE;
        CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU, response,
                                    sizeof(response), &ack) == cases[index].result);
        CHECK_TRUE(ack == cases[index].ack);
    }
}

static void test_position_response_preserves_sign(void)
{
    const uint8_t negative[] = {0x01U, 0x36U, 0x01U, 0x00U,
                                0x00U, 0x12U, 0x34U, 0x6BU};
    const uint8_t positive[] = {0x01U, 0x36U, 0x00U, 0x00U,
                                0x00U, 0x12U, 0x34U, 0x6BU};
    int32_t position = 0;

    CHECK_TRUE(emm_v5_parse_position(0x01U, negative, sizeof(negative), &position)
               == EMM_V5_OK);
    CHECK_TRUE(position == -0x1234);
    CHECK_TRUE(emm_v5_parse_position(0x01U, positive, sizeof(positive), &position)
               == EMM_V5_OK);
    CHECK_TRUE(position == 0x1234);
}

static void test_position_response_rejects_unrepresentable_negative_magnitude(void)
{
    const uint8_t response[] = {0x01U, 0x36U, 0x01U, 0x80U,
                                0x00U, 0x00U, 0x00U, 0x6BU};
    int32_t position = 123;

    CHECK_TRUE(emm_v5_parse_position(0x01U, response, sizeof(response), &position)
               == EMM_V5_INVALID_FRAME);
}

static void test_status_response_decodes_status_byte(void)
{
    const uint8_t response[] = {0x02U, 0x3AU, 0xA5U, 0x6BU};
    uint8_t status = 0U;

    CHECK_TRUE(emm_v5_parse_status(0x02U, response, sizeof(response), &status)
               == EMM_V5_OK);
    CHECK_TRUE(status == 0xA5U);
}

static void test_pid_response_decodes_signed_big_endian_values(void)
{
    const uint8_t response[] = {
        0x03U, 0x21U,
        0x00U, 0x00U, 0x12U, 0x34U,
        0xFFU, 0xFFU, 0xFFU, 0xFEU,
        0x7FU, 0xFFU, 0xFFU, 0xFFU,
        0x6BU,
    };
    EmmV5Pid pid = {0};

    CHECK_TRUE(emm_v5_parse_pid(0x03U, response, sizeof(response), &pid)
               == EMM_V5_OK);
    CHECK_TRUE(pid.kp == 0x1234);
    CHECK_TRUE(pid.ki == -2);
    CHECK_TRUE(pid.kd == INT32_MAX);
}

static void test_pid_response_rejects_x_series_length(void)
{
    uint8_t response[19] = {0x01U, 0x21U};
    EmmV5Pid pid = {0};
    response[18] = 0x6BU;
    CHECK_TRUE(emm_v5_parse_pid(0x01U, response, sizeof(response), &pid)
               == EMM_V5_INVALID_FRAME);
}

static void test_response_parsers_reject_null_pointers(void)
{
    const uint8_t ack_response[] = {0x01U, 0xFDU, 0x02U, 0x6BU};
    const uint8_t position_response[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 0U, 1U, 0x6BU};
    const uint8_t status_response[] = {0x01U, 0x3AU, 0x01U, 0x6BU};
    const uint8_t pid_response[15] = {0x01U, 0x21U, [14] = 0x6BU};
    EmmV5Ack ack;
    int32_t position;
    uint8_t status;
    EmmV5Pid pid;

    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU, NULL, 4U, &ack)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU, ack_response,
                                sizeof(ack_response), NULL) == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_position(0x01U, NULL, 8U, &position)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_position(0x01U, position_response,
                                     sizeof(position_response), NULL)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_status(0x01U, NULL, 4U, &status)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_status(0x01U, status_response,
                                   sizeof(status_response), NULL)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_pid(0x01U, NULL, 15U, &pid)
               == EMM_V5_INVALID_ARGUMENT);
    CHECK_TRUE(emm_v5_parse_pid(0x01U, pid_response,
                                sizeof(pid_response), NULL)
               == EMM_V5_INVALID_ARGUMENT);
}

static void test_response_parsers_reject_malformed_frames(void)
{
    const uint8_t ack_response[] = {0x01U, 0xFDU, 0x02U, 0x6BU};
    const uint8_t position_response[] = {0x01U, 0x36U, 0x00U, 0U, 0U, 0U, 1U, 0x6BU};
    const uint8_t status_response[] = {0x01U, 0x3AU, 0x01U, 0x6BU};
    const uint8_t pid_response[15] = {0x01U, 0x21U, [14] = 0x6BU};
    uint8_t bad_trailer[15];
    EmmV5Ack ack;
    int32_t position;
    uint8_t status;
    EmmV5Pid pid;

    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU, ack_response, 3U, &ack)
               == EMM_V5_INVALID_FRAME);
    CHECK_TRUE(emm_v5_parse_position(0x01U, position_response, 7U, &position)
               == EMM_V5_INVALID_FRAME);
    CHECK_TRUE(emm_v5_parse_status(0x01U, status_response, 3U, &status)
               == EMM_V5_INVALID_FRAME);
    CHECK_TRUE(emm_v5_parse_pid(0x01U, pid_response, 14U, &pid)
               == EMM_V5_INVALID_FRAME);

    memcpy(bad_trailer, pid_response, sizeof(bad_trailer));
    bad_trailer[14] = 0U;
    CHECK_TRUE(emm_v5_parse_pid(0x01U, bad_trailer, sizeof(bad_trailer), &pid)
               == EMM_V5_INVALID_FRAME);
}

static void test_cross_parser_rejects_position_response_in_ack_parser(void)
{
    /* Feed an 8-byte position response to the 4-byte ACK parser.
     * Length mismatch must be caught before any content check. */
    const uint8_t position_response_bytes[] = {0x01U, 0x36U, 0x00U,
                                                0x00U, 0x00U, 0x00U,
                                                0x01U, 0x6BU};
    EmmV5Ack ack = EMM_V5_ACK_COMPLETE;
    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU,
                                position_response_bytes,
                                sizeof(position_response_bytes),
                                &ack) == EMM_V5_INVALID_FRAME);
}

static void test_ack_parser_rejects_unknown_status(void)
{
    /* Status 0x03 is not a recognised ACK code; parser must reject it. */
    const uint8_t response[] = {0x01U, 0xFDU, 0x03U, 0x6BU};
    EmmV5Ack ack = EMM_V5_ACK_COMPLETE;
    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU,
                                response, sizeof(response),
                                &ack) == EMM_V5_INVALID_FRAME);
}

static void test_ack_parser_rejects_bad_trailer(void)
{
    /* Replace the trailing 0x6B with 0x00 – frame is corrupt. */
    const uint8_t response[] = {0x01U, 0xFDU, 0x02U, 0x00U};
    EmmV5Ack ack = EMM_V5_ACK_COMPLETE;
    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU,
                                response, sizeof(response),
                                &ack) == EMM_V5_INVALID_FRAME);
}

static void test_response_parsers_reject_unexpected_address_and_function(void)
{
    const uint8_t wrong_ack_address[] = {0x02U, 0xFDU, 0x02U, 0x6BU};
    const uint8_t wrong_position_function[] = {0x01U, 0x3AU, 0x00U, 0U, 0U, 0U, 1U, 0x6BU};
    const uint8_t wrong_status_address[] = {0x02U, 0x3AU, 0x01U, 0x6BU};
    const uint8_t wrong_pid_function[15] = {0x01U, 0x22U, [14] = 0x6BU};
    EmmV5Ack ack;
    int32_t position;
    uint8_t status;
    EmmV5Pid pid;

    CHECK_TRUE(emm_v5_parse_ack(0x01U, 0xFDU, wrong_ack_address,
                                sizeof(wrong_ack_address), &ack)
               == EMM_V5_UNEXPECTED_RESPONSE);
    CHECK_TRUE(emm_v5_parse_position(0x01U, wrong_position_function,
                                     sizeof(wrong_position_function), &position)
               == EMM_V5_UNEXPECTED_RESPONSE);
    CHECK_TRUE(emm_v5_parse_status(0x01U, wrong_status_address,
                                   sizeof(wrong_status_address), &status)
               == EMM_V5_UNEXPECTED_RESPONSE);
    CHECK_TRUE(emm_v5_parse_pid(0x01U, wrong_pid_function,
                                sizeof(wrong_pid_function), &pid)
               == EMM_V5_UNEXPECTED_RESPONSE);
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
    test_ack_response_decodes_progress_and_errors();
    test_position_response_preserves_sign();
    test_position_response_rejects_unrepresentable_negative_magnitude();
    test_status_response_decodes_status_byte();
    test_pid_response_decodes_signed_big_endian_values();
    test_pid_response_rejects_x_series_length();
    test_response_parsers_reject_null_pointers();
    test_response_parsers_reject_malformed_frames();
    test_response_parsers_reject_unexpected_address_and_function();
    test_cross_parser_rejects_position_response_in_ack_parser();
    test_ack_parser_rejects_unknown_status();
    test_ack_parser_rejects_bad_trailer();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

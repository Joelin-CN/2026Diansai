#ifndef EMM_V5_PROTOCOL_H
#define EMM_V5_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EMM_V5_FRAME_END 0x6BU
#define EMM_V5_MAX_FRAME_SIZE 19U

typedef enum {
    EMM_V5_OK = 0,
    EMM_V5_INVALID_ARGUMENT,
    EMM_V5_BUFFER_TOO_SMALL,
    EMM_V5_INVALID_FRAME,
    EMM_V5_UNEXPECTED_RESPONSE,
    EMM_V5_DRIVER_ERROR
} EmmV5Result;

typedef enum {
    EMM_V5_DIRECTION_CW = 0,
    EMM_V5_DIRECTION_CCW = 1
} EmmV5Direction;

typedef enum {
    EMM_V5_HOME_SINGLE_TURN = 0,
    EMM_V5_HOME_STALL = 1,
    EMM_V5_HOME_LIMIT_SWITCH = 2
} EmmV5HomeMode;

typedef enum {
    EMM_V5_ACK_COMPLETE = 0x02,
    EMM_V5_ACK_START = 0x12,
    EMM_V5_ACK_END = 0x22,
    EMM_V5_ACK_HOME_FAILED = 0x9F,
    EMM_V5_ACK_CONFLICT = 0xE2,
    EMM_V5_ACK_BAD_COMMAND = 0xEE
} EmmV5Ack;

typedef struct {
    int32_t kp;
    int32_t ki;
    int32_t kd;
} EmmV5Pid;

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
} EmmV5Frame;

typedef struct {
    EmmV5Direction direction;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t pulse_count;
    bool absolute;
    bool synchronized;
} EmmV5PositionCommand;

EmmV5Result emm_v5_encode_enable(uint8_t address, bool enable,
                                  bool synchronized, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_velocity(uint8_t address, EmmV5Direction direction,
                                    uint16_t speed_rpm, uint8_t acceleration,
                                    bool synchronized, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_position(uint8_t address,
                                    const EmmV5PositionCommand *command,
                                    EmmV5Frame *frame);
EmmV5Result emm_v5_encode_fast_position_setup(uint8_t address,
                                               uint16_t speed_rpm,
                                               uint8_t acceleration,
                                               bool absolute,
                                               bool synchronized,
                                               EmmV5Frame *frame);
EmmV5Result emm_v5_encode_fast_move(uint8_t address, uint32_t pulse_count,
                                    EmmV5Frame *frame);
EmmV5Result emm_v5_encode_sync_trigger(EmmV5Frame *frame);
EmmV5Result emm_v5_encode_position_query(uint8_t address, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_status_query(uint8_t address, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_microstep(uint8_t address, bool save,
                                    uint8_t microstep, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_stop(uint8_t address, bool synchronized,
                               EmmV5Frame *frame);
EmmV5Result emm_v5_encode_set_zero(uint8_t address, bool save,
                                   EmmV5Frame *frame);
EmmV5Result emm_v5_encode_home(uint8_t address, EmmV5HomeMode mode,
                               bool synchronized, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_abort_home(uint8_t address, EmmV5Frame *frame);
EmmV5Result emm_v5_encode_pid_query(uint8_t address, EmmV5Frame *frame);
EmmV5Result emm_v5_parse_ack(uint8_t address, uint8_t function,
                             const uint8_t *response, size_t length,
                             EmmV5Ack *ack);
EmmV5Result emm_v5_parse_position(uint8_t address, const uint8_t *response,
                                  size_t length, int32_t *position);
EmmV5Result emm_v5_parse_status(uint8_t address, const uint8_t *response,
                                size_t length, uint8_t *status);
EmmV5Result emm_v5_parse_pid(uint8_t address, const uint8_t *response,
                             size_t length, EmmV5Pid *pid);

#endif

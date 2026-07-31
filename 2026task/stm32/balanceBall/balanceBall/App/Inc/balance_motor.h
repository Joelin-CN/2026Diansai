#ifndef BALANCE_MOTOR_H
#define BALANCE_MOTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "balance_actuator.h"
#include "emm_v5_protocol.h"

typedef enum {
    BALANCE_MOTOR_TX_ACCEPTED,
    BALANCE_MOTOR_TX_BUSY,
    BALANCE_MOTOR_TX_FAILED
} BalanceMotorTxResult;

typedef BalanceMotorTxResult (*BalanceMotorSendFn)(void *context,
                                                    const uint8_t *frame,
                                                    size_t length,
                                                    uint8_t expected_function,
                                                    size_t expected_length);

typedef struct {
    BalanceMotorSendFn send;
    void *context;
} BalanceMotorTransport;

typedef struct {
    uint8_t address;
    float pulses_per_position_unit;
    uint8_t max_consecutive_failures;
} BalanceMotorConfig;

typedef enum {
    BALANCE_MOTOR_OK,
    BALANCE_MOTOR_QUEUED,
    BALANCE_MOTOR_NOT_ZEROED,
    BALANCE_MOTOR_BUSY,
    BALANCE_MOTOR_INVALID_ARGUMENT,
    BALANCE_MOTOR_OVERFLOW,
    BALANCE_MOTOR_LOCKED,
    BALANCE_MOTOR_TRANSPORT_ERROR,
    BALANCE_MOTOR_PROTOCOL_ERROR
} BalanceMotorResult;

typedef struct {
    uint8_t bytes[EMM_V5_MAX_FRAME_SIZE];
    size_t length;
    uint8_t expected_function;
    size_t expected_length;
    bool pending;
} BalanceMotorPendingFrame;

typedef struct {
    BalanceMotorConfig config;
    BalanceMotorTransport transport;
    int32_t zero_position;
    uint8_t consecutive_failures;
    uint8_t outstanding_function;
    bool zero_valid;
    bool locked;
    bool outstanding_valid;
    bool outstanding_priority;
    BalanceMotorPendingFrame priority;
    BalanceMotorPendingFrame target;
} BalanceMotor;

void balance_motor_init(BalanceMotor *motor,
                        const BalanceMotorConfig *config,
                        BalanceMotorTransport transport);
BalanceMotorResult balance_motor_request_zero(BalanceMotor *motor);
BalanceMotorResult balance_motor_submit(BalanceMotor *motor,
                                        const BalanceActuatorCommand *command);
BalanceMotorResult balance_motor_stop(BalanceMotor *motor);
BalanceMotorResult balance_motor_disable(BalanceMotor *motor);
void balance_motor_on_response(BalanceMotor *motor,
                               uint8_t expected_function,
                               const uint8_t *response,
                               size_t response_length);
void balance_motor_on_transport_error(BalanceMotor *motor);
void balance_motor_process(BalanceMotor *motor);
void balance_motor_clear_fault(BalanceMotor *motor);
bool balance_motor_has_zero(const BalanceMotor *motor);

#endif

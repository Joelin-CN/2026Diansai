#include "balance_controller.h"

static float clampf(float value, float limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

static float absf(float value) { return value < 0.0f ? -value : value; }

void balance_controller_init(BalanceController *controller,
                             const BalanceControllerConfig *config)
{
    controller->config = *config;
    balance_controller_reset(controller);
}

void balance_controller_reset(BalanceController *controller)
{
    controller->integral = 0.0f;
}

BalanceControlOutput balance_controller_step(BalanceController *controller,
                                             float target_cm,
                                             const BalanceEstimate *estimate,
                                             float dt_s,
                                             bool allow_integral)
{
    BalanceControlOutput output;
    output.error_cm = target_cm - estimate->position_cm;
    const float base = controller->config.kp * output.error_cm
                     - controller->config.kv * estimate->velocity_cm_s;
    float candidate_integral = controller->integral;
    if (allow_integral && absf(output.error_cm) <= controller->config.integral_zone_cm) {
        candidate_integral = clampf(controller->integral + output.error_cm * dt_s,
                                    controller->config.integral_limit);
    }

    const float candidate_raw = base + controller->config.ki * candidate_integral;
    const bool pushes_positive_limit = candidate_raw > controller->config.output_limit
                                    && output.error_cm > 0.0f;
    const bool pushes_negative_limit = candidate_raw < -controller->config.output_limit
                                    && output.error_cm < 0.0f;
    if (!pushes_positive_limit && !pushes_negative_limit) {
        controller->integral = candidate_integral;
    }

    output.raw = base + controller->config.ki * controller->integral;
    output.limited = clampf(output.raw, controller->config.output_limit);
    output.saturated = output.raw != output.limited;
    return output;
}

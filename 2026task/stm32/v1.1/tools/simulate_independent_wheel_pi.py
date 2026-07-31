#!/usr/bin/env python3
"""
Virtual half-circle test for the Task-2 wheel/yaw control chain.

The model compares the current mixer (independent wheel PI results averaged,
therefore discarding their differential correction) with a revised mixer:

    turn_pwm = calibrated_yaw_feedforward
             + yaw_rate_feedback
             + limited(left/right PI differential feedback)

The plant deliberately includes motor-side asymmetry, encoder noise and PWM
quantisation so the test exercises the failure mode observed on the real curve.
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path


DT = 0.001
CONTROL_DT = 0.010
OUTER_DT = 0.020
TOTAL_TIME = 4.0
WHEEL_BASE_M = 0.214
CURVE_RADIUS_M = 0.50
LOOKAHEAD_M = 0.183
TARGET_SPEED_MPS = 0.40
SPEED_KP = 80.0
SPEED_KI = 25.0
SPEED_FB_LIMIT = 35.0
FF_K_FRICTION = 40.0
FF_K_STATIC = 20.0
PWM_LIMIT = 55.0
TURN_LIMIT = 44.0
STEERING_FF = 20.0
YAW_KP = 8.0
WHEEL_PI_TURN_LIMIT = 12.0
GYRO_ALPHA = 0.60
OMEGA_SLEW = 10.0
OMEGA_LIMIT_CURVE = 1.10
OUTER_KP_CURVE = 0.45
IR_WEIGHTS = [3.9861, 2.8472, 1.7083, 0.5694,
              -0.5694, -1.7083, -2.8472, -3.9861]
IR_POSITIONS_M = [weight * 0.010 for weight in IR_WEIGHTS]


@dataclass(frozen=True)
class Scenario:
    name: str
    retain_wheel_pi_differential: bool
    wheel_pi_filter_alpha: float = 1.0


SCENARIOS = [
    Scenario("current_average_mixer", False),
    Scenario("raw_independent_wheel_pi_mixer", True),
    Scenario("filtered_independent_wheel_pi_mixer", True, 0.25),
]


def clamp(value: float, low: float, high: float) -> float:
    return min(max(value, low), high)


def ir_centroid(line_relative_m: float) -> float:
    strengths = []
    active = []
    for sensor_position in IR_POSITIONS_M:
        distance = abs(sensor_position - line_relative_m)
        strength = 0.5 * (1.0 + math.tanh((0.020 - distance) / 0.006))
        strengths.append(strength)
        active.append(strength > 0.50)
    indices = [index for index, detected in enumerate(active) if detected]
    if not indices:
        return IR_WEIGHTS[0] if line_relative_m > 0.0 else IR_WEIGHTS[-1]
    binary = sum(IR_WEIGHTS[index] for index in indices) / len(indices)
    energy = sum(strengths[index] for index in indices)
    analog = sum(
        IR_WEIGHTS[index] * strengths[index] for index in indices
    ) / energy
    return 0.70 * binary + 0.30 * analog


class PI:
    def __init__(self) -> None:
        self.integral = 0.0

    def update(self, target: float, measured: float) -> float:
        error = target - measured
        self.integral += error * CONTROL_DT
        self.integral = clamp(
            self.integral,
            -SPEED_FB_LIMIT / SPEED_KI,
            SPEED_FB_LIMIT / SPEED_KI,
        )
        return clamp(
            SPEED_KP * error + SPEED_KI * self.integral,
            -SPEED_FB_LIMIT,
            SPEED_FB_LIMIT,
        )


def deterministic_encoder_noise(step: int, side: int) -> float:
    # Repeatable ±8 mm/s mixture: quantisation, tooth eccentricity and wiring noise.
    phase = 0.73 if side == 0 else 1.91
    return (
        0.0050 * math.sin(0.071 * step + phase)
        + 0.0025 * math.sin(0.193 * step + 2.0 * phase)
    )


def simulate(scenario: Scenario) -> tuple[list[dict[str, float]], dict[str, float]]:
    left_pi = PI()
    right_pi = PI()
    v_actual = TARGET_SPEED_MPS
    yaw_rate = 0.0
    gyro_filtered = 0.0
    omega_cmd = 0.0
    common_pwm = FF_K_STATIC + FF_K_FRICTION * TARGET_SPEED_MPS
    turn_pwm = 0.0
    filtered_wheel_pi_turn = 0.0
    left_pwm = common_pwm
    right_pwm = common_pwm
    path_error_m = 0.0
    heading_error_rad = 0.0
    world_heading_rad = 0.0
    x_m = 0.0
    y_m = 0.0
    measured_left = TARGET_SPEED_MPS
    measured_right = TARGET_SPEED_MPS
    rows: list[dict[str, float]] = []

    control_steps = round(CONTROL_DT / DT)
    outer_steps = round(OUTER_DT / DT)
    total_steps = round(TOTAL_TIME / DT)

    for step in range(total_steps + 1):
        time_s = step * DT

        if step % outer_steps == 0:
            sensor_error_m = -(
                path_error_m + LOOKAHEAD_M * math.sin(heading_error_rad)
            )
            ir_error = ir_centroid(sensor_error_m)
            omega_feedforward = -TARGET_SPEED_MPS / CURVE_RADIUS_M
            omega_target = clamp(
                omega_feedforward + OUTER_KP_CURVE * ir_error,
                -OMEGA_LIMIT_CURVE,
                OMEGA_LIMIT_CURVE,
            )
            omega_cmd += clamp(
                omega_target - omega_cmd,
                -OMEGA_SLEW * OUTER_DT,
                OMEGA_SLEW * OUTER_DT,
            )

        if step % control_steps == 0:
            gyro_filtered += GYRO_ALPHA * (yaw_rate - gyro_filtered)
            left_target = (
                TARGET_SPEED_MPS - 0.5 * WHEEL_BASE_M * omega_cmd
            )
            right_target = (
                TARGET_SPEED_MPS + 0.5 * WHEEL_BASE_M * omega_cmd
            )

            left_feedback = left_pi.update(left_target, measured_left)
            right_feedback = right_pi.update(right_target, measured_right)
            left_total = clamp(
                FF_K_STATIC + FF_K_FRICTION * left_target + left_feedback,
                0.0,
                PWM_LIMIT,
            )
            right_total = clamp(
                FF_K_STATIC + FF_K_FRICTION * right_target + right_feedback,
                0.0,
                PWM_LIMIT,
            )
            common_pwm = 0.5 * (left_total + right_total)
            yaw_error = omega_cmd - gyro_filtered
            turn_pwm = STEERING_FF * omega_cmd + YAW_KP * yaw_error
            wheel_pi_turn = 0.5 * (right_feedback - left_feedback)
            if scenario.retain_wheel_pi_differential:
                filtered_wheel_pi_turn += scenario.wheel_pi_filter_alpha * (
                    wheel_pi_turn - filtered_wheel_pi_turn
                )
                turn_pwm += clamp(
                    filtered_wheel_pi_turn,
                    -WHEEL_PI_TURN_LIMIT,
                    WHEEL_PI_TURN_LIMIT,
                )
            turn_pwm = clamp(turn_pwm, -TURN_LIMIT, TURN_LIMIT)

            left_pwm = round(clamp(common_pwm - turn_pwm, 0.0, PWM_LIMIT))
            right_pwm = round(clamp(common_pwm + turn_pwm, 0.0, PWM_LIMIT))

        # Coupled forward/yaw plant.  A fixed +0.16 rad/s yaw bias represents
        # unequal B/C motor gain and curve load before wheel-speed correction.
        v_equilibrium = max(0.0, (common_pwm - FF_K_STATIC) / FF_K_FRICTION)
        yaw_equilibrium = turn_pwm / STEERING_FF + 0.16
        v_actual += (v_equilibrium - v_actual) / 0.120 * DT
        yaw_rate += (yaw_equilibrium - yaw_rate) / 0.095 * DT

        left_actual = v_actual - 0.5 * WHEEL_BASE_M * yaw_rate
        right_actual = v_actual + 0.5 * WHEEL_BASE_M * yaw_rate
        if step % control_steps == 0:
            measured_left = (
                left_actual + deterministic_encoder_noise(step // control_steps, 0)
            )
            measured_right = (
                right_actual + deterministic_encoder_noise(step // control_steps, 1)
            )

        ideal_yaw_rate = -v_actual / CURVE_RADIUS_M
        heading_error_rad += (yaw_rate - ideal_yaw_rate) * DT
        path_error_m += v_actual * math.sin(heading_error_rad) * DT
        world_heading_rad += yaw_rate * DT
        x_m += v_actual * math.cos(world_heading_rad) * DT
        y_m += v_actual * math.sin(world_heading_rad) * DT

        if step % control_steps == 0:
            rows.append(
                {
                    "time_s": round(time_s, 3),
                    "path_error_mm": path_error_m * 1000.0,
                    "heading_error_deg": math.degrees(heading_error_rad),
                    "v_left_target": left_target,
                    "v_right_target": right_target,
                    "v_left_actual": left_actual,
                    "v_right_actual": right_actual,
                    "omega_cmd": omega_cmd,
                    "gyro_z": yaw_rate,
                    "yaw_error": omega_cmd - yaw_rate,
                    "wheel_pi_turn": wheel_pi_turn,
                    "turn_pwm": turn_pwm,
                    "left_pwm": left_pwm,
                    "right_pwm": right_pwm,
                    "x_m": x_m,
                    "y_m": y_m,
                }
            )

    settled = [row for row in rows if row["time_s"] >= 0.50]
    left_errors = [
        row["v_left_target"] - row["v_left_actual"] for row in settled
    ]
    right_errors = [
        row["v_right_target"] - row["v_right_actual"] for row in settled
    ]
    yaw_errors = [row["omega_cmd"] - row["gyro_z"] for row in settled]
    path_errors = [row["path_error_mm"] for row in settled]
    turn_deltas = [
        settled[index]["turn_pwm"] - settled[index - 1]["turn_pwm"]
        for index in range(1, len(settled))
    ]
    metrics = {
        "left_speed_error_rms_mps": math.sqrt(
            sum(value * value for value in left_errors) / len(left_errors)
        ),
        "right_speed_error_rms_mps": math.sqrt(
            sum(value * value for value in right_errors) / len(right_errors)
        ),
        "yaw_error_rms_radps": math.sqrt(
            sum(value * value for value in yaw_errors) / len(yaw_errors)
        ),
        "path_error_rms_mm": math.sqrt(
            sum(value * value for value in path_errors) / len(path_errors)
        ),
        "path_error_peak_to_peak_mm": max(path_errors) - min(path_errors),
        "turn_pwm_step_rms": math.sqrt(
            sum(value * value for value in turn_deltas) / len(turn_deltas)
        ),
        "final_path_error_mm": path_errors[-1],
    }
    return rows, metrics


def main() -> None:
    output_dir = Path(__file__).resolve().parent / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    all_rows = {}
    all_metrics = {}
    for scenario in SCENARIOS:
        rows, metrics = simulate(scenario)
        all_rows[scenario.name] = rows
        all_metrics[scenario.name] = metrics
        with (output_dir / f"{scenario.name}.csv").open(
            "w", newline="", encoding="utf-8"
        ) as output:
            writer = csv.DictWriter(output, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)
    with (output_dir / "independent_wheel_pi_results.json").open(
        "w", encoding="utf-8"
    ) as output:
        json.dump(
            {"metrics": all_metrics, "series": all_rows},
            output,
            indent=2,
            ensure_ascii=False,
        )
    print(json.dumps(all_metrics, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()

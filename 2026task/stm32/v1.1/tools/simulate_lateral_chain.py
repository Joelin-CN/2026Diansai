#!/usr/bin/env python3
"""
Offline virtual-data test for the Task-2 straight-line control chain.

The timing and controller constants mirror the firmware test profile:
  IR outer loop          50 Hz
  yaw-rate inner loop   100 Hz
  forward speed          0.52 m/s
  IR look-ahead          0.183 m
  omega slew             10 rad/s^2
  steering FF            20 PWM/(rad/s)
  yaw-rate Kp             8 PWM/(rad/s)

This is intentionally a lateral/yaw model, not a wheel/motor identification
model.  It answers whether closing yaw rate alone can remove cross-track
velocity, and compares that with explicit cross-track-velocity damping.
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass
from pathlib import Path


DT = 0.001
OUTER_DT = 0.020
INNER_DT = 0.010
TOTAL_TIME = 4.0
V_FORWARD = 0.52
LOOKAHEAD_M = 0.183
KP_OUTER = 0.35
OMEGA_MAX = 0.70
OMEGA_SLEW = 10.0
STEERING_FF = 20.0
TURN_PWM_LIMIT = 44.0
YAW_PLANT_PWM_PER_RADPS = 20.0
YAW_PLANT_TAU_S = 0.090
GYRO_ALPHA = 0.60
IR_WEIGHTS = [3.9861, 2.8472, 1.7083, 0.5694,
              -0.5694, -1.7083, -2.8472, -3.9861]
IR_POSITIONS_M = [weight * 0.010 for weight in IR_WEIGHTS]
BLACK_LINE_HALF_WIDTH_M = 0.020
IR_EDGE_BLUR_M = 0.006


@dataclass(frozen=True)
class Scenario:
    name: str
    yaw_kp: float
    lateral_velocity_k: float
    continuous_ir: bool = False
    lookahead_m: float = LOOKAHEAD_M
    actuator_delay_s: float = 0.0
    yaw_plant_pwm_per_radps: float = YAW_PLANT_PWM_PER_RADPS
    use_true_lateral_velocity: bool = False


SCENARIOS = [
    Scenario("current_yaw_inner", yaw_kp=8.0, lateral_velocity_k=0.0),
    Scenario("delayed_current_100ms", yaw_kp=8.0,
             lateral_velocity_k=0.0, actuator_delay_s=0.100),
    Scenario("delayed_strong_yaw_inner_100ms", yaw_kp=24.0,
             lateral_velocity_k=0.0, actuator_delay_s=0.100),
    Scenario("ideal_lateral_velocity_damping", yaw_kp=8.0,
             lateral_velocity_k=1.0, use_true_lateral_velocity=True),
]


def clamp(value: float, low: float, high: float) -> float:
    return min(max(value, low), high)


def ir_centroid(line_relative_to_array_m: float, continuous: bool) -> float:
    """Return the same normalized sign convention used by the firmware."""
    if continuous:
        return clamp(line_relative_to_array_m / 0.010,
                     IR_WEIGHTS[-1], IR_WEIGHTS[0])

    strengths: list[float] = []
    active: list[bool] = []
    for sensor_position in IR_POSITIONS_M:
        distance = abs(sensor_position - line_relative_to_array_m)
        strength = 0.5 * (
            1.0
            + math.tanh(
                (BLACK_LINE_HALF_WIDTH_M - distance) / IR_EDGE_BLUR_M
            )
        )
        strengths.append(strength)
        active.append(strength > 0.50)

    active_indices = [index for index, detected in enumerate(active) if detected]
    if not active_indices:
        # Match the short-dropout hold behavior by saturating to the last side.
        return IR_WEIGHTS[0] if line_relative_to_array_m > 0.0 else IR_WEIGHTS[-1]

    binary_centroid = (
        sum(IR_WEIGHTS[index] for index in active_indices)
        / len(active_indices)
    )
    strength_sum = sum(strengths[index] for index in active_indices)
    analog_centroid = (
        sum(
            IR_WEIGHTS[index] * strengths[index]
            for index in active_indices
        )
        / strength_sum
    )
    return 0.70 * binary_centroid + 0.30 * analog_centroid


def simulate(scenario: Scenario) -> tuple[list[dict[str, float]], dict[str, float]]:
    # Positive y/psi/velocity is physical left.  Start 40 mm to the right.
    y_m = -0.040
    psi_rad = 0.0
    yaw_rate = 0.0
    gyro_filtered = 0.0
    omega_cmd = 0.0
    turn_pwm = 0.0
    applied_turn_pwm = 0.0
    measured_error_m = 0.0
    previous_error_m = 0.0
    filtered_error_rate = 0.0
    lateral_velocity_est = 0.0
    rows: list[dict[str, float]] = []

    outer_steps = round(OUTER_DT / DT)
    inner_steps = round(INNER_DT / DT)
    sample_steps = round(0.010 / DT)
    total_steps = round(TOTAL_TIME / DT)
    delay_steps = max(0, round(scenario.actuator_delay_s / DT))
    turn_delay = [0.0] * (delay_steps + 1)

    for step in range(total_steps + 1):
        time_s = step * DT

        if step % inner_steps == 0:
            gyro_filtered += GYRO_ALPHA * (yaw_rate - gyro_filtered)
            yaw_error = omega_cmd - gyro_filtered
            turn_pwm = clamp(
                STEERING_FF * omega_cmd + scenario.yaw_kp * yaw_error,
                -TURN_PWM_LIMIT,
                TURN_PWM_LIMIT,
            )

        if step % outer_steps == 0:
            sensor_y_m = y_m + scenario.lookahead_m * math.sin(psi_rad)
            line_relative_m = -sensor_y_m
            error_norm = ir_centroid(
                line_relative_m, scenario.continuous_ir
            )
            measured_error_m = error_norm * 0.010
            raw_error_rate = (
                measured_error_m - previous_error_m
            ) / OUTER_DT
            # Strong filtering is essential because the binary-dominant
            # centroid changes in sensor-pitch-sized steps.
            filtered_error_rate += 0.18 * (
                raw_error_rate - filtered_error_rate
            )
            previous_error_m = measured_error_m

            # e = -(y + L*psi), so vy = -e_dot - L*yaw_rate.
            lateral_velocity_est = (
                -filtered_error_rate - scenario.lookahead_m * gyro_filtered
            )
            damping_velocity = (
                V_FORWARD * math.sin(psi_rad)
                if scenario.use_true_lateral_velocity
                else lateral_velocity_est
            )
            omega_target = (
                KP_OUTER * error_norm
                - scenario.lateral_velocity_k * damping_velocity
            )
            omega_target = clamp(omega_target, -OMEGA_MAX, OMEGA_MAX)
            max_omega_step = OMEGA_SLEW * OUTER_DT
            omega_cmd += clamp(
                omega_target - omega_cmd,
                -max_omega_step,
                max_omega_step,
            )

        lateral_velocity = V_FORWARD * math.sin(psi_rad)
        turn_delay.append(turn_pwm)
        applied_turn_pwm = turn_delay.pop(0)
        yaw_rate_dot = (
            applied_turn_pwm / scenario.yaw_plant_pwm_per_radps - yaw_rate
        ) / YAW_PLANT_TAU_S
        yaw_rate += yaw_rate_dot * DT
        psi_rad += yaw_rate * DT
        y_m += lateral_velocity * DT

        if step % sample_steps == 0:
            rows.append(
                {
                    "time_s": round(time_s, 3),
                    "y_mm": y_m * 1000.0,
                    "vy_mps": lateral_velocity,
                    "psi_deg": math.degrees(psi_rad),
                    "ir_error": measured_error_m / 0.010,
                    "omega_cmd": omega_cmd,
                    "gyro_z": yaw_rate,
                    "vy_est_mps": lateral_velocity_est,
                    "turn_pwm": turn_pwm,
                }
            )

    y_values = [row["y_mm"] for row in rows]
    vy_values = [row["vy_mps"] for row in rows]
    crossings: list[dict[str, float]] = []
    for index in range(1, len(rows)):
        y_prev = rows[index - 1]["y_mm"]
        y_now = rows[index]["y_mm"]
        if y_prev == 0.0 or y_prev * y_now < 0.0:
            crossings.append(
                {
                    "time_s": rows[index]["time_s"],
                    "vy_mps": rows[index]["vy_mps"],
                }
            )

    settle_time = math.nan
    window_samples = round(0.5 / 0.010)
    for index in range(len(rows) - window_samples):
        window = rows[index:]
        if all(
            abs(row["y_mm"]) <= 5.0 and abs(row["vy_mps"]) <= 0.010
            for row in window
        ):
            settle_time = rows[index]["time_s"]
            break

    after_one_second = [
        row for row in rows if row["time_s"] >= 1.0
    ]
    metrics = {
        "first_crossing_time_s": (
            crossings[0]["time_s"] if crossings else math.nan
        ),
        "first_crossing_vy_mps": (
            crossings[0]["vy_mps"] if crossings else math.nan
        ),
        "zero_crossings": float(len(crossings)),
        "peak_abs_y_after_1s_mm": max(
            abs(row["y_mm"]) for row in after_one_second
        ),
        "rms_y_after_1s_mm": math.sqrt(
            sum(row["y_mm"] ** 2 for row in after_one_second)
            / len(after_one_second)
        ),
        "max_abs_vy_mps": max(abs(value) for value in vy_values),
        "settle_time_s": settle_time,
        "final_y_mm": y_values[-1],
        "final_vy_mps": vy_values[-1],
    }
    return rows, metrics


def main() -> None:
    output_dir = Path(__file__).resolve().parent / "results"
    output_dir.mkdir(parents=True, exist_ok=True)
    all_rows: dict[str, list[dict[str, float]]] = {}
    all_metrics: dict[str, dict[str, float]] = {}

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

    with (output_dir / "lateral_chain_results.json").open(
        "w", encoding="utf-8"
    ) as output:
        json.dump(
            {"metrics": all_metrics, "series": all_rows},
            output,
            indent=2,
            ensure_ascii=False,
            allow_nan=True,
        )

    print(json.dumps(all_metrics, indent=2, allow_nan=True))


if __name__ == "__main__":
    main()

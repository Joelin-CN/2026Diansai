"""Detailed simulation test with debug output."""

import numpy as np
import mujoco
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from src.encoder_sim import EncoderSim
from src.imu_sim import ImuSim
from src.ir_sensor import IrSensorSim
from firmware_bridge.control_bridge import ControlBridge

# Load model
print("Loading model...")
model = mujoco.MjModel.from_xml_path("models/scene.xml")
data = mujoco.MjData(model)

DT_SIM = 0.002
model.opt.timestep = DT_SIM

# Initialize
encoder_sim = EncoderSim(model)
imu_sim = ImuSim(model)
ir_sim = IrSensorSim(model)
ctrl_bridge = ControlBridge()

# Set initial position
chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
qpos_adr = model.body_jntadr[chassis_id]
data.qpos[qpos_adr:qpos_adr+3] = [0.6505, -0.105, 0.033]
data.qpos[qpos_adr+3:qpos_adr+7] = [0.7071, 0, 0, 0.7071]  # 90° Z rotation
mujoco.mj_forward(model, data)

# Get actuator IDs
act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

print("\nRunning detailed test...")
timestamp_us = 0

for step in range(200):
    # Read sensors
    ir_raw = ir_sim.read(model, data)
    timestamp_us += int(DT_SIM * 1e6)

    # Decision every 10 steps
    if step % 10 == 0:
        v_cmd, omega_cmd = ctrl_bridge.sens_decision_update(
            ir_raw, timestamp_us, DT_SIM * 10
        )

        perception = ctrl_bridge.get_perception_result()
        behavior = ctrl_bridge.get_behavior_state()

        # Detailed logging
        if step % 20 == 0:
            pos = data.qpos[qpos_adr:qpos_adr+3]
            print(f"\nStep {step:3d} | t={step*DT_SIM:.3f}s")
            print(f"  Position: X={pos[0]:.4f}, Y={pos[1]:.4f}, Z={pos[2]:.4f}")
            print(f"  IR raw: {ir_raw}")
            print(f"  Perception: lat_err={perception['lateral_error']:.4f}, "
                  f"valid={perception['line_valid']}")
            print(f"  Behavior: {behavior}")
            print(f"  Commands: v={v_cmd:.4f} m/s, ω={omega_cmd:.4f} rad/s")

    # Motion control
    v_left_actual, v_right_actual = encoder_sim.get_wheel_speeds(data, DT_SIM)
    pwm_l, pwm_r, omega_l, omega_r = ctrl_bridge.motion_control_update(
        v_cmd, omega_cmd, v_left_actual, v_right_actual, DT_SIM
    )

    if step % 20 == 0:
        print(f"  Motion: V_L={v_left_actual:.4f}, V_R={v_right_actual:.4f}")
        print(f"  Actuators: PWM=({pwm_l:4d},{pwm_r:4d}), ω=({omega_l:6.2f},{omega_r:6.2f})")

    # Apply actuators
    data.ctrl[act_lf] = omega_l
    data.ctrl[act_lr] = omega_l
    data.ctrl[act_rf] = omega_r
    data.ctrl[act_rr] = omega_r

    # Step simulation
    mujoco.mj_step(model, data)

print("\nTest complete!")
print(f"Final position: {data.qpos[qpos_adr:qpos_adr+3]}")

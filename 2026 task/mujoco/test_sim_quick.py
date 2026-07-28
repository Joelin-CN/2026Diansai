"""Quick simulation test to diagnose issues."""

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

# Set timestep
DT_SIM = 0.002
model.opt.timestep = DT_SIM
print(f"Timestep: {DT_SIM*1000:.1f} ms")

# Initialize sensors
print("Initializing sensors...")
encoder_sim = EncoderSim(model)
imu_sim = ImuSim(model)
ir_sim = IrSensorSim(model)

# Initialize control bridge
print("Initializing control...")
ctrl_bridge = ControlBridge()

# Set initial position
chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
qpos_adr = model.body_jntadr[chassis_id]
data.qpos[qpos_adr:qpos_adr+3] = [0.5455, 0.0, 0.033]
data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]
mujoco.mj_forward(model, data)
print(f"Initial position: {data.qpos[qpos_adr:qpos_adr+3]}")

# Get actuator IDs
act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

print("\nRunning 100 steps...")
timestamp_us = 0
v_cmd = 0.0
omega_cmd = 0.0

for step in range(100):
    # Read sensors
    enc_counts = encoder_sim.update(data)
    ir_raw = ir_sim.read(model, data)
    acc_raw, gyro_raw = imu_sim.read(data, DT_SIM)

    timestamp_us += int(DT_SIM * 1e6)

    # Decision every 10 steps (50Hz)
    if step % 10 == 0:
        v_cmd, omega_cmd = ctrl_bridge.sens_decision_update(
            ir_raw, timestamp_us, DT_SIM * 10
        )

    # Motion control
    v_left_actual, v_right_actual = encoder_sim.get_wheel_speeds(data, DT_SIM)
    pwm_l, pwm_r, omega_l, omega_r = ctrl_bridge.motion_control_update(
        v_cmd, omega_cmd, v_left_actual, v_right_actual, DT_SIM
    )

    # Apply actuators
    data.ctrl[act_lf] = omega_l
    data.ctrl[act_lr] = omega_l
    data.ctrl[act_rf] = omega_r
    data.ctrl[act_rr] = omega_r

    # Step simulation
    mujoco.mj_step(model, data)

    # Log every 20 steps
    if step % 20 == 0:
        perception = ctrl_bridge.get_perception_result()
        print(f"Step {step:3d}: IR={ir_raw[3]:4d} {ir_raw[4]:4d}, "
              f"Lat={perception['lateral_error']:7.4f}, "
              f"V_cmd={v_cmd:6.3f}, ω_cmd={omega_cmd:7.3f}, "
              f"PWM=({pwm_l:4d},{pwm_r:4d}), ω=({omega_l:6.2f},{omega_r:6.2f})")

print(f"\nFinal position: {data.qpos[qpos_adr:qpos_adr+3]}")
print("Test complete!")

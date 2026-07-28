"""
Main simulation entry point for MuJoCo line-following robot.
Runs complete control loop: sensors → decision → motion control → actuators.
"""

import numpy as np
import mujoco
import time
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from src.encoder_sim import EncoderSim
from src.imu_sim import ImuSim
from src.ir_sensor import IrSensorSim
from firmware_bridge.control_bridge import ControlBridge

# Simulation parameters
DT_SIM = 0.002              # 2 ms, 500 Hz control rate
STEPS_TOTAL = 30000         # 60 seconds simulation
SENS_PERIOD = 10            # Every 10 steps = 50 Hz sens-decision rate
LOG_PERIOD = 500            # Every 500 steps = 1 second logging

# Initial robot pose (start on track - scaled 5x, on top edge)
# Track top edge: X=0.6505, Y from -0.2775 to +0.2775
# Robot heading: +Y direction (along track)
# IR sensors at robot frame (X=+0.105, Y varying)
# After 90° Z rotation: robot +X becomes world +Y
# So sensors are at world (chassis_y + 0.105, chassis_x - sensor_x_offset)
# To place sensors at track centerline (X=0.6505, Y=0):
#   chassis_y + 0.105 = 0  =>  chassis_y = -0.105
#   chassis_x - sensor_x_offset = 0.6505  =>  chassis_x = 0.6505 (for center sensors)
INITIAL_POS = [0.6505, -0.105, 0.033]    # Position so sensors are on track centerline
INITIAL_QUAT = [0.7071, 0, 0, 0.7071]    # 90° rotation around Z axis (X->Y alignment)


def main():
    """Run MuJoCo simulation with full control pipeline."""
    print("=" * 70)
    print("MuJoCo Line-Following Robot Simulation")
    print("=" * 70)

    # Load model - use relative path to avoid Windows encoding issues
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    model_path = os.path.join(project_dir, "models", "scene.xml")

    if not os.path.exists(model_path):
        print(f"ERROR: Model file not found: {model_path}")
        return 1

    print(f"Loading model: {model_path}")

    # Change to project directory for relative includes in XML
    orig_cwd = os.getcwd()
    os.chdir(project_dir)

    try:
        model = mujoco.MjModel.from_xml_path("models/scene.xml")
    finally:
        os.chdir(orig_cwd)
    data = mujoco.MjData(model)

    # Set timestep
    model.opt.timestep = DT_SIM
    print(f"Timestep: {DT_SIM*1000:.1f} ms ({1/DT_SIM:.0f} Hz)")

    # Initialize sensors
    print("Initializing sensors...")
    encoder_sim = EncoderSim(model)
    imu_sim = ImuSim(model)
    ir_sim = IrSensorSim(model)

    # Initialize control bridge
    print("Initializing control bridge...")
    ctrl_bridge = ControlBridge()

    # Set initial robot position
    chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
    qpos_adr = model.body_jntadr[chassis_id]
    data.qpos[qpos_adr:qpos_adr+3] = INITIAL_POS
    data.qpos[qpos_adr+3:qpos_adr+7] = INITIAL_QUAT

    # Forward kinematics
    mujoco.mj_forward(model, data)
    print(f"Initial position: {INITIAL_POS}")

    # Get actuator IDs
    act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
    act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
    act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
    act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

    # Control variables
    v_cmd = 0.0
    omega_cmd = 0.0
    timestamp_us = 0

    print("\n" + "=" * 70)
    print("Starting simulation...")
    print("=" * 70)
    print(f"{'Step':<8} {'Time(s)':<8} {'Lat.Err':<10} {'V_cmd':<10} {'Omega':<10} {'V_L':<10} {'V_R':<10}")
    print("-" * 70)

    start_time = time.time()

    # Main simulation loop
    for step in range(STEPS_TOTAL):
        # ── Read sensors ──
        enc_counts = encoder_sim.update(data)
        ir_raw = ir_sim.read(model, data)
        acc_raw, gyro_raw = imu_sim.read(data, DT_SIM)

        timestamp_us += int(DT_SIM * 1e6)

        # ── 50 Hz Sens-Decision ──
        if step % SENS_PERIOD == 0:
            v_cmd, omega_cmd = ctrl_bridge.sens_decision_update(
                ir_raw,
                timestamp_us,
                DT_SIM * SENS_PERIOD
            )

        # ── 500 Hz Motion Control ──
        v_left_actual, v_right_actual = encoder_sim.get_wheel_speeds(data, DT_SIM)
        pwm_l, pwm_r, omega_l, omega_r = ctrl_bridge.motion_control_update(
            v_cmd,
            omega_cmd,
            v_left_actual,
            v_right_actual,
            DT_SIM
        )

        # ── Write actuators (velocity control, rad/s) ──
        data.ctrl[act_lf] = omega_l
        data.ctrl[act_lr] = omega_l
        data.ctrl[act_rf] = omega_r
        data.ctrl[act_rr] = omega_r

        # ── Step simulation ──
        mujoco.mj_step(model, data)

        # ── Logging (every 1 second) ──
        if step % LOG_PERIOD == 0:
            t = step * DT_SIM
            perception = ctrl_bridge.get_perception_result()
            lateral_err = perception['lateral_error']

            print(f"{step:<8} {t:<8.2f} {lateral_err:<10.4f} {v_cmd:<10.4f} "
                  f"{omega_cmd:<10.4f} {v_left_actual:<10.4f} {v_right_actual:<10.4f}")

    # Simulation complete
    elapsed = time.time() - start_time
    print("-" * 70)
    print(f"Simulation complete: {STEPS_TOTAL} steps in {elapsed:.2f}s")
    print(f"Real-time factor: {(STEPS_TOTAL * DT_SIM) / elapsed:.2f}x")
    print("=" * 70)

    return 0


if __name__ == '__main__':
    sys.exit(main())

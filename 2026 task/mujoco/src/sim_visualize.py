"""
MuJoCo simulation with real-time visualization.
Press ESC to exit, SPACE to pause/unpause.
"""

import numpy as np
import mujoco
import mujoco.viewer
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
SENS_PERIOD = 10            # Every 10 steps = 50 Hz sens-decision rate

# Initial robot pose (scaled track)
INITIAL_POS = [0.6505, -0.105, 0.033]    # Position so sensors are on track centerline
INITIAL_QUAT = [0.7071, 0, 0, 0.7071]    # 90° rotation around Z axis (X->Y alignment)
INITIAL_QUAT = [1, 0, 0, 0]


def main():
    """Run MuJoCo simulation with visualization."""
    print("=" * 70)
    print("MuJoCo Line-Following Robot Simulation (Visualized)")
    print("=" * 70)
    print("Controls:")
    print("  ESC - Exit simulation")
    print("  SPACE - Pause/Unpause")
    print("  Mouse - Rotate camera")
    print("=" * 70)

    # Load model
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)

    orig_cwd = os.getcwd()
    os.chdir(project_dir)

    try:
        model = mujoco.MjModel.from_xml_path("models/scene.xml")
    finally:
        os.chdir(orig_cwd)

    data = mujoco.MjData(model)
    model.opt.timestep = DT_SIM

    print(f"Model loaded successfully")
    print(f"Timestep: {DT_SIM*1000:.1f} ms ({1/DT_SIM:.0f} Hz)")

    # Initialize sensors
    encoder_sim = EncoderSim(model)
    imu_sim = ImuSim(model)
    ir_sim = IrSensorSim(model)

    # Initialize control bridge
    ctrl_bridge = ControlBridge()

    # Set initial robot position
    chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
    qpos_adr = model.body_jntadr[chassis_id]
    data.qpos[qpos_adr:qpos_adr+3] = INITIAL_POS
    data.qpos[qpos_adr+3:qpos_adr+7] = INITIAL_QUAT

    # Forward kinematics
    mujoco.mj_forward(model, data)

    # Get actuator IDs
    act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
    act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
    act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
    act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

    # Control variables
    v_cmd = 0.0
    omega_cmd = 0.0
    timestamp_us = 0
    step = 0

    print("\nStarting visualization...")
    print("(Close the viewer window to exit)\n")

    # Launch passive viewer
    with mujoco.viewer.launch_passive(model, data) as viewer:
        # Set camera to follow robot
        viewer.cam.lookat[:] = [0.044, 0.0, 0.0]  # Track center
        viewer.cam.distance = 0.5
        viewer.cam.elevation = -30
        viewer.cam.azimuth = 90

        start_time = time.time()
        last_print_time = start_time

        while viewer.is_running():
            step_start = time.time()

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

            # ── Write actuators ──
            data.ctrl[act_lf] = omega_l
            data.ctrl[act_lr] = omega_l
            data.ctrl[act_rf] = omega_r
            data.ctrl[act_rr] = omega_r

            # ── Step simulation ──
            mujoco.mj_step(model, data)

            # Update viewer
            viewer.sync()

            # ── Console logging (every 1 second) ──
            current_time = time.time()
            if current_time - last_print_time >= 1.0:
                t = step * DT_SIM
                perception = ctrl_bridge.get_perception_result()
                lateral_err = perception['lateral_error']
                pos = data.qpos[qpos_adr:qpos_adr+3]

                print(f"t={t:5.1f}s | Pos=[{pos[0]:6.3f}, {pos[1]:6.3f}] | "
                      f"Lat={lateral_err:6.3f} | V={v_cmd:5.2f} | "
                      f"ω={omega_cmd:6.3f} | PWM=[{pwm_l:4d},{pwm_r:4d}]")

                last_print_time = current_time

            step += 1

            # Real-time pacing (optional - remove for max speed)
            elapsed = time.time() - step_start
            if elapsed < DT_SIM:
                time.sleep(DT_SIM - elapsed)

    print("\nSimulation ended")
    print(f"Total time: {time.time() - start_time:.2f}s")
    print(f"Total steps: {step}")

    return 0


if __name__ == '__main__':
    sys.exit(main())

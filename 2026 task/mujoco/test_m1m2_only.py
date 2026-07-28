"""Test M1/M2 (left wheels) rotation only."""

import numpy as np
import mujoco
import mujoco.viewer
import time

# Load model
model = mujoco.MjModel.from_xml_path("models/scene.xml")
data = mujoco.MjData(model)

# Set initial position
chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
qpos_adr = model.body_jntadr[chassis_id]
data.qpos[qpos_adr:qpos_adr+3] = [0.5, 0.0, 0.0325]
data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]

# Get actuator IDs
act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")  # M1
act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")  # M2
act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")  # M3
act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")  # M4

print("=" * 60)
print("M1/M2 (Left Wheels) Rotation Test")
print("=" * 60)
print("M1 = wheel_lf (left front)")
print("M2 = wheel_lr (left rear)")
print()
print("Only M1 and M2 will rotate at +10 rad/s")
print("Watch the yellow markers!")
print()
print("Expected: Car should move forward AND turn right")
print("          (because only left side is powered)")
print("=" * 60)

# Launch viewer
with mujoco.viewer.launch_passive(model, data) as viewer:
    # Only M1/M2 forward
    print("\nM1/M2 rotating at +10 rad/s...")
    print("Press ESC to exit")

    data.ctrl[act_lf] = 10.0  # M1
    data.ctrl[act_lr] = 10.0  # M2
    data.ctrl[act_rf] = 0.0   # M3 off
    data.ctrl[act_rr] = 0.0   # M4 off

    # Keep running
    while viewer.is_running():
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

"""Test M3/M4 (right wheels) rotation only."""

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
print("M3/M4 (Right Wheels) Rotation Test")
print("=" * 60)
print("M3 = wheel_rf (right front)")
print("M4 = wheel_rr (right rear)")
print()
print("Only M3 and M4 will rotate at +10 rad/s")
print("Watch the yellow markers!")
print()
print("Expected: Car should move forward AND turn LEFT")
print("          (because only right side is powered)")
print("=" * 60)

# Launch viewer
with mujoco.viewer.launch_passive(model, data) as viewer:
    print("\nM3/M4 rotating at +10 rad/s...")
    print("Press ESC to exit")

    data.ctrl[act_lf] = 0.0    # M1 off
    data.ctrl[act_lr] = 0.0    # M2 off
    data.ctrl[act_rf] = 10.0   # M3
    data.ctrl[act_rr] = 10.0   # M4

    # Keep running
    while viewer.is_running():
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

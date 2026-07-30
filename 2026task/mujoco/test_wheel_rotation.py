"""Test wheel rotation directions with visualization."""

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
act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

print("Wheel Rotation Test")
print("=" * 60)
print("Robot heading: +Y direction (green axis)")
print("Wheel rotation axis: X axis (red axis)")
print()
print("Test sequence:")
print("1. All wheels forward (+10 rad/s) - should drive forward (+Y)")
print("2. Left wheels only - should turn right")
print("3. Right wheels only - should turn left")
print("4. Opposite wheels - should spin in place")
print()
print("Watch the yellow markers on each wheel!")
print("=" * 60)

# Launch viewer
with mujoco.viewer.launch_passive(model, data) as viewer:
    # Test 1: All wheels forward
    print("\n[Test 1] All wheels FORWARD (+10 rad/s)")
    print("Expected: Car moves forward in +Y direction (green axis)")
    data.ctrl[act_lf] = 10.0
    data.ctrl[act_lr] = 10.0
    data.ctrl[act_rf] = 10.0
    data.ctrl[act_rr] = 10.0

    start_time = time.time()
    while time.time() - start_time < 3.0:
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

    # Reset position
    data.qpos[qpos_adr:qpos_adr+3] = [0.5, 0.0, 0.0325]
    data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]
    data.qvel[:] = 0
    mujoco.mj_forward(model, data)
    time.sleep(1.0)

    # Test 2: Left wheels only
    print("\n[Test 2] LEFT wheels only (+10 rad/s)")
    print("Expected: Car turns RIGHT")
    data.ctrl[act_lf] = 10.0
    data.ctrl[act_lr] = 10.0
    data.ctrl[act_rf] = 0.0
    data.ctrl[act_rr] = 0.0

    start_time = time.time()
    while time.time() - start_time < 3.0:
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

    # Reset
    data.qpos[qpos_adr:qpos_adr+3] = [0.5, 0.0, 0.0325]
    data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]
    data.qvel[:] = 0
    mujoco.mj_forward(model, data)
    time.sleep(1.0)

    # Test 3: Right wheels only
    print("\n[Test 3] RIGHT wheels only (+10 rad/s)")
    print("Expected: Car turns LEFT")
    data.ctrl[act_lf] = 0.0
    data.ctrl[act_lr] = 0.0
    data.ctrl[act_rf] = 10.0
    data.ctrl[act_rr] = 10.0

    start_time = time.time()
    while time.time() - start_time < 3.0:
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

    print("\n[Test Complete] Press ESC to exit")

    # Keep window open
    while viewer.is_running():
        mujoco.mj_step(model, data)
        viewer.sync()
        time.sleep(0.002)

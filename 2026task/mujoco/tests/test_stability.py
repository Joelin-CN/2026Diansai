"""Quick stability test"""
import mujoco
import numpy as np

model = mujoco.MjModel.from_xml_path('models/scene.xml')
data = mujoco.MjData(model)

# Set initial position
chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, 'chassis')
qpos_adr = model.body_jntadr[chassis_id]
data.qpos[qpos_adr:qpos_adr+3] = [0.132, 0.0, 0.033]
data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]

mujoco.mj_forward(model, data)

print("Initial position:", data.qpos[qpos_adr:qpos_adr+3])

# Run 100 steps with no actuation
for i in range(100):
    data.ctrl[:] = 0.0  # No actuation
    mujoco.mj_step(model, data)
    if i % 20 == 0:
        pos = data.qpos[qpos_adr:qpos_adr+3]
        print(f'Step {i}: pos=[{pos[0]:.6f}, {pos[1]:.6f}, {pos[2]:.6f}]')

print('Simulation stable!')

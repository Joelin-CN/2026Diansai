"""
Simple test: Apply constant wheel velocities and record trajectory.
Generates a plot showing the robot's path.
"""

import numpy as np
import matplotlib.pyplot as plt
import mujoco
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Load model - change to project directory first
project_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(project_dir)
model = mujoco.MjModel.from_xml_path("models/scene.xml")
data = mujoco.MjData(model)
model.opt.timestep = 0.002

# Set initial position
chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
qpos_adr = model.body_jntadr[chassis_id]
data.qpos[qpos_adr:qpos_adr+3] = [0.132, 0.0, 0.033]
data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]

mujoco.mj_forward(model, data)

# Get actuator IDs
act_lf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lf")
act_lr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_lr")
act_rf = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rf")
act_rr = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, "act_rr")

# Record trajectory
positions = []
times = []

# Test 1: Forward motion
print("Test 1: Forward motion (5 rad/s on all wheels)")
for step in range(2500):  # 5 seconds
    # Apply constant forward velocity to all wheels
    data.ctrl[act_lf] = 5.0
    data.ctrl[act_lr] = 5.0
    data.ctrl[act_rf] = 5.0
    data.ctrl[act_rr] = 5.0

    mujoco.mj_step(model, data)

    if step % 50 == 0:  # Record every 0.1s
        pos = data.qpos[qpos_adr:qpos_adr+3].copy()
        positions.append(pos[:2])  # X, Y only
        times.append(step * 0.002)

positions = np.array(positions)

# Generate track outline for reference
from src.track_generator import TRACK_SEGMENTS

# Plot results
plt.figure(figsize=(10, 8))

# Plot track
track_plotted = False
for seg in TRACK_SEGMENTS:
    if seg['type'] == 'line':
        p1, p2 = seg['p1'], seg['p2']
        label = 'Track' if not track_plotted else ''
        plt.plot([p1[0], p2[0]], [p1[1], p2[1]], 'k-', linewidth=3, label=label)
        track_plotted = True

# Plot robot trajectory
plt.plot(positions[:, 0], positions[:, 1], 'r-', linewidth=2, label='Robot path')
plt.plot(positions[0, 0], positions[0, 1], 'go', markersize=10, label='Start')
plt.plot(positions[-1, 0], positions[-1, 1], 'ro', markersize=10, label='End')

plt.xlabel('X (m)')
plt.ylabel('Y (m)')
plt.title('Robot Trajectory Test - Forward Motion')
plt.legend()
plt.grid(True)
plt.axis('equal')
plt.tight_layout()

# Save figure
output_file = 'logs/trajectory_test.png'
os.makedirs('logs', exist_ok=True)
plt.savefig(output_file, dpi=150)
print(f"\nTrajectory plot saved to: {output_file}")

# Print statistics
displacement = np.linalg.norm(positions[-1] - positions[0])
print(f"\nResults:")
print(f"  Initial position: [{positions[0, 0]:.4f}, {positions[0, 1]:.4f}]")
print(f"  Final position:   [{positions[-1, 0]:.4f}, {positions[-1, 1]:.4f}]")
print(f"  Total displacement: {displacement:.4f} m")
print(f"  Simulation time: {times[-1]:.2f} s")

if displacement > 0.01:
    print("\n✓ Robot is moving!")
else:
    print("\n✗ Robot not moving (displacement < 1cm)")

plt.show()

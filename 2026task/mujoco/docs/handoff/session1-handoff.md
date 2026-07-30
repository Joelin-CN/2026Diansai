# MuJoCo Simulation Project Handoff - Session 1

**Date:** 2026-07-29
**Project:** E:\B306\2026\电赛\2026 task\mujoco
**Status:** Core implementation complete, debugging in progress

## Completed Components

### 1. Track Generator (`src/track_generator.py`)
- ✅ Converts user waypoints (mm, Y-forward) to MuJoCo coordinates (m, Z-up)
- ✅ Generates rectangular track segments from 12 waypoints
- ✅ Provides `min_dist_to_track()` for IR sensor simulation
- ✅ Outputs track XML geometry

### 2. MuJoCo Models
- ✅ `models/scene.xml` - Main simulation scene
- ✅ `models/robot.xml` - Differential drive robot with 4 wheels
  - Chassis at z=0.033m (wheel radius height)
  - Freejoint for motion
  - 8 IR sensor sites
  - IMU site at chassis center
  - 4 wheel bodies with hinge joints (axis=0 1 0, Y-axis rotation)
  - Velocity actuators (kv=50)
- ✅ `models/track.xml` - Black line track from generated segments

### 3. Sensor Simulations
- ✅ `src/encoder_sim.py` - Converts joint angles to encoder counts (PPR=334)
- ✅ `src/imu_sim.py` - Simulates ICM42688P with world→body frame transform
- ✅ `src/ir_sensor.py` - Sigmoid distance model for reflectance (0-4095)

### 4. Firmware Bridge (Python ports)
- ✅ `firmware_bridge/perception.py` - Weighted centroid, heading filter (α=0.3)
- ✅ `firmware_bridge/behavior_planner.py` - FSM with 7 states
- ✅ `firmware_bridge/trajectory_generator.py` - Pure Pursuit (L=0.25m)
- ✅ `firmware_bridge/motion_control.py` - FF+PI wheel speed control
  - KP=200, KI=50, output limit=±500
  - FF: k_accel=50, k_friction=300, k_static=80
- ✅ `firmware_bridge/control_bridge.py` - Unified interface

### 5. Main Simulation
- ✅ `src/sim_main.py` - 500Hz control loop, 50Hz decision
- ✅ Unit tests for perception and motion control (all passing)

## Current Issue

**Problem:** Robot not moving - wheel velocities remain near zero

**Observed:**
- Perception detects line (lateral errors ~0.01)
- Trajectory generator outputs v_cmd, omega_cmd
- Motion control computes PWM and omega targets
- BUT: Measured wheel speeds V_L, V_R ≈ 0 (occasional huge spikes)

**Suspected Root Cause:**
1. Actuator commands may not be applied correctly
2. PWM→omega conversion scaling issue
3. MIN_SPEED deadzone (0.10 m/s) might be blocking low commands
4. Wheel-ground contact might have issues

## Next Steps

1. **Debug actuator application:**
   - Verify data.ctrl[] indexing
   - Check actuator gear ratios
   - Test with constant omega command

2. **Fix wheel speed measurement:**
   - Current implementation reads qvel directly
   - May need to use joint velocity sensors instead

3. **Tune control parameters:**
   - Lower MIN_SPEED threshold if needed
   - Increase initial speed commands
   - Check if FF+PI outputs reasonable PWM values

4. **Verify robot-ground contact:**
   - Ensure wheels have proper friction
   - Check for penetration or floating

## File Structure
```
mujoco/
├── models/
│   ├── scene.xml
│   ├── robot.xml
│   └── track.xml
├── src/
│   ├── sim_main.py
│   ├── track_generator.py
│   ├── encoder_sim.py
│   ├── imu_sim.py
│   └── ir_sensor.py
├── firmware_bridge/
│   ├── perception.py
│   ├── behavior_planner.py
│   ├── trajectory_generator.py
│   ├── motion_control.py
│   └── control_bridge.py
├── tests/
│   ├── test_perception.py (6/6 passing)
│   ├── test_motion_control.py (6/6 passing)
│   └── test_sim_smoke.py
└── docs/
    └── simulation-design.md
```

## Test Results
- Perception unit tests: ✅ 6/6 passed
- Motion control unit tests: ✅ 6/6 passed
- Model loads: ✅ No XML errors
- Simulation runs: ✅ 60s without crash (with stability warning at t=0.028s, then stable)

## Commands to Continue
```bash
cd "E:\B306\2026\电赛\2026 task\mujoco"
conda activate RL

# Run simulation
python src/sim_main.py

# Run tests
python tests/test_perception.py
python tests/test_motion_control.py
```

## Key Parameters (from firmware)
- WHEEL_BASE = 0.150 m
- WHEEL_RADIUS = 0.033 m
- ENCODER_PPR = 334
- SPEED_KP = 200, SPEED_KI = 50
- FF_K_ACCEL = 50, FF_K_FRICTION = 300, FF_K_STATIC = 80
- PWM_MAX = 1000, SPEED_OUTPUT_MAX = 500
- MIN_SPEED = 0.10 m/s (deadzone)
- IR_WEIGHTS = [-7,-5,-3,-1,1,3,5,7]
- LOOKAHEAD = 0.25 m

**Session ended due to debugging in progress. Resume with actuator diagnostics.**

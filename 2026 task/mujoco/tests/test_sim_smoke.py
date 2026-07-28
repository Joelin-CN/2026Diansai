"""
Smoke test - verify simulation can start and run basic scenario.
"""

import pytest
import numpy as np
import mujoco
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def test_model_loads():
    """Test that MuJoCo model loads without errors."""
    model_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "scene.xml"
    )
    model_path = os.path.abspath(model_path)

    assert os.path.exists(model_path), f"Model not found: {model_path}"

    # Load model
    model = mujoco.MjModel.from_xml_path(model_path)
    data = mujoco.MjData(model)

    assert model is not None
    assert data is not None


def test_sensors_exist():
    """Test that all required sensors are defined in model."""
    model_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "scene.xml"
    )
    model = mujoco.MjModel.from_xml_path(os.path.abspath(model_path))

    # Check encoder sensors
    for name in ["enc_lf", "enc_lr", "enc_rf", "enc_rr"]:
        sensor_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SENSOR, name)
        assert sensor_id >= 0, f"Sensor {name} not found"

    # Check IR sites
    for i in range(8):
        site_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SITE, f"ir{i}")
        assert site_id >= 0, f"IR site ir{i} not found"

    # Check IMU sensors
    for name in ["imu_linvel", "imu_angvel"]:
        sensor_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_SENSOR, name)
        assert sensor_id >= 0, f"Sensor {name} not found"


def test_actuators_exist():
    """Test that all wheel actuators are defined."""
    model_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "scene.xml"
    )
    model = mujoco.MjModel.from_xml_path(os.path.abspath(model_path))

    for name in ["act_lf", "act_lr", "act_rf", "act_rr"]:
        act_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, name)
        assert act_id >= 0, f"Actuator {name} not found"


def test_simulation_runs():
    """Test that simulation runs for 100 steps without crashing."""
    from src.encoder_sim import EncoderSim
    from src.imu_sim import ImuSim
    from src.ir_sensor import IrSensorSim

    model_path = os.path.join(
        os.path.dirname(__file__), "..", "models", "scene.xml"
    )
    model = mujoco.MjModel.from_xml_path(os.path.abspath(model_path))
    data = mujoco.MjData(model)

    model.opt.timestep = 0.002

    # Initialize sensors
    encoder_sim = EncoderSim(model)
    imu_sim = ImuSim(model)
    ir_sim = IrSensorSim(model)

    # Set initial position
    chassis_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "chassis")
    qpos_adr = model.body_jntadr[chassis_id]
    data.qpos[qpos_adr:qpos_adr+3] = [0.132, 0.0, 0.053]
    data.qpos[qpos_adr+3:qpos_adr+7] = [1, 0, 0, 0]

    mujoco.mj_forward(model, data)

    # Run 100 steps
    for step in range(100):
        enc_counts = encoder_sim.update(data)
        ir_raw = ir_sim.read(model, data)
        acc_raw, gyro_raw = imu_sim.read(data, 0.002)

        # Simple forward motion
        for act_name in ["act_lf", "act_lr", "act_rf", "act_rr"]:
            act_id = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_ACTUATOR, act_name)
            data.ctrl[act_id] = 5.0  # Small forward velocity

        mujoco.mj_step(model, data)

    # Check robot moved
    final_pos = data.qpos[qpos_adr:qpos_adr+3]
    initial_pos = np.array([0.132, 0.0, 0.053])
    displacement = np.linalg.norm(final_pos - initial_pos)

    assert displacement > 0.01, "Robot should have moved"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])

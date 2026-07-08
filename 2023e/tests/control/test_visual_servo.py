from control.types import MotorCommand
from control.visual_servo import ServoConfig, VisualServoController
from coordinate.types import ScreenPoint


def test_visual_servo_converts_screen_error_to_motor_steps():
    controller = VisualServoController(
        ServoConfig(
            yaw_kp_steps_per_mm=2.0,
            pitch_kp_steps_per_mm=-3.0,
            deadband_mm=1.0,
            max_yaw_delta_steps=100,
            max_pitch_delta_steps=100,
        )
    )

    command = controller.update(
        target=ScreenPoint(60, 30),
        current=ScreenPoint(50, 40),
    )

    assert command == MotorCommand(yaw_delta_steps=20, pitch_delta_steps=30)


def test_visual_servo_applies_deadband():
    controller = VisualServoController(
        ServoConfig(
            yaw_kp_steps_per_mm=10.0,
            pitch_kp_steps_per_mm=10.0,
            deadband_mm=3.0,
            max_yaw_delta_steps=100,
            max_pitch_delta_steps=100,
        )
    )

    command = controller.update(
        target=ScreenPoint(52, 101),
        current=ScreenPoint(50, 100),
    )

    assert command == MotorCommand(yaw_delta_steps=0, pitch_delta_steps=0)


def test_visual_servo_clamps_large_commands():
    controller = VisualServoController(
        ServoConfig(
            yaw_kp_steps_per_mm=10.0,
            pitch_kp_steps_per_mm=10.0,
            deadband_mm=0.0,
            max_yaw_delta_steps=80,
            max_pitch_delta_steps=50,
        )
    )

    command = controller.update(
        target=ScreenPoint(100, -100),
        current=ScreenPoint(0, 0),
    )

    assert command == MotorCommand(yaw_delta_steps=80, pitch_delta_steps=-50)

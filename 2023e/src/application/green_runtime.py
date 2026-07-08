from vision.types import VisionMode
from task.green_tasks import GreenTrackRedTask


green_task = GreenTrackRedTask()

while True:
    frame = camera.read()

    result = vision_pipeline.process(
        frame,
        mode=VisionMode.RUNTIME_TRACK_RED_GREEN
    )

    if result.red_laser is None or not result.red_laser.found:
        continue

    if result.green_laser is None or not result.green_laser.found:
        continue

    red_point = transformer.image_point_to_screen(
        result.red_laser.image_center
    )

    green_point = transformer.image_point_to_screen(
        result.green_laser.image_center
    )

    target = green_task.next_target(red_point)

    command = controller.update(
        target=target.point,
        current=green_point
    )

    motor_port.send(command)
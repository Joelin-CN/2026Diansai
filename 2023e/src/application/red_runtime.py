from vision.types import VisionMode
from coordinate.quad import tape_centerline_quad
from task.path import interpolate_quad_path
from task.red_tasks import PathFollowerTask


# 初始化阶段
red_task = None

while True:
    frame = camera.read()

    result = vision_pipeline.process(
        frame,
        mode=VisionMode.RUNTIME_A4_RED
    )

    if result.red_laser is None or not result.red_laser.found:
        continue

    red_point = transformer.image_point_to_screen(
        result.red_laser.image_center
    )

    if red_task is None:
        if result.tape_quad is None or not result.tape_quad.found:
            continue

        outer = transformer.image_quad_to_screen(result.tape_quad.outer_quad)
        inner = transformer.image_quad_to_screen(result.tape_quad.inner_quad)

        centerline = tape_centerline_quad(outer, inner)
        path = interpolate_quad_path(centerline, points_per_edge=40)

        red_task = PathFollowerTask(path)

    target = red_task.next_target(red_point)

    command = controller.update(
        target=target.point,
        current=red_point
    )

    motor_port.send(command)
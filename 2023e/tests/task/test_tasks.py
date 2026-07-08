from coordinate.types import ScreenPoint, ScreenQuad
from task.green_tasks import GreenTrackRedTask
from task.path import interpolate_quad_path
from task.red_tasks import PathFollowerTask


def test_interpolate_quad_path_generates_expected_fake_square_points():
    quad = ScreenQuad(
        points=(
            ScreenPoint(0, 0),
            ScreenPoint(100, 0),
            ScreenPoint(100, 100),
            ScreenPoint(0, 100),
        )
    )

    path = interpolate_quad_path(quad, points_per_edge=2)

    assert len(path) == 8
    assert path == [
        ScreenPoint(0.0, 0.0),
        ScreenPoint(50.0, 0.0),
        ScreenPoint(100.0, 0.0),
        ScreenPoint(100.0, 50.0),
        ScreenPoint(100.0, 100.0),
        ScreenPoint(50.0, 100.0),
        ScreenPoint(0.0, 100.0),
        ScreenPoint(0.0, 50.0),
    ]


def test_path_follower_advances_when_current_point_reaches_target():
    path = [
        ScreenPoint(0, 0),
        ScreenPoint(100, 0),
        ScreenPoint(100, 100),
    ]
    task = PathFollowerTask(path, arrive_dist_mm=5)

    first = task.next_target(ScreenPoint(50, 50))
    second = task.next_target(ScreenPoint(0, 0))

    assert first.point == ScreenPoint(0, 0)
    assert second.point == ScreenPoint(100, 0)
    assert task.index == 1


def test_path_follower_wraps_to_first_point_after_last_target():
    task = PathFollowerTask([ScreenPoint(0, 0), ScreenPoint(10, 0)], arrive_dist_mm=1)

    task.next_target(ScreenPoint(0, 0))
    wrapped = task.next_target(ScreenPoint(10, 0))

    assert wrapped.point == ScreenPoint(0, 0)
    assert task.index == 0


def test_green_track_red_task_returns_detected_red_point_as_target():
    red_point = ScreenPoint(230, 140)
    task = GreenTrackRedTask()

    target = task.next_target(red_point)

    assert target.point == red_point
    assert target.done is False

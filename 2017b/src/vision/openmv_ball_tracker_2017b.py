# 2017B ball plate vision tracker for OpenMV.
#
# Function:
#   1. Capture an RGB565 image from OpenMV.
#   2. Track the bright ball by LAB color threshold.
#   3. Pick the largest valid blob as the ball.
#   4. Smooth the center point and send it to the controller by UART.
#
# UART packet:
#   [x=064 y=072 ok=1 *]  ball found
#   [x=-01 y=-01 ok=0 *]  ball lost
#
# If the old controller only parses x and y, it can still read x/y first.

import sensor
import image
import time
from pyb import UART


# --------------------------- User parameters ---------------------------

UART_ID = 3
UART_BAUDRATE = 115200

# LAB threshold copied from the original program. Tune it in OpenMV IDE if the
# ball or plate color changes. Format: (L min, L max, A min, A max, B min, B max)
BALL_THRESHOLDS = [(90, 100, -6, 2, -21, 6)]

# Camera setup.
FRAME_SIZE = sensor.QQVGA
WINDOWING = (10, 0, 124, 140)
LENS_CORR_STRENGTH = 1.8
EXPOSURE_US = 170000

# Blob filter. Increase these values to reject noise; decrease them if the ball
# becomes small in the image.
PIXELS_THRESHOLD = 20
AREA_THRESHOLD = 20
MERGE_MARGIN = 5

# Tracking behavior.
SMOOTH_ALPHA = 0.45       # 0.0 = very slow, 1.0 = no smoothing
LOST_REPORT_PERIOD = 5    # send lost packet every N lost frames
DEBUG_DRAW = True
DEBUG_PRINT = True


# --------------------------- Helper functions ---------------------------

def clamp_int(value, low, high):
    if value < low:
        return low
    if value > high:
        return high
    return int(value)


def select_ball_blob(blobs):
    """Return the most likely ball blob from a list of blobs."""
    best_blob = None
    best_score = -1

    for blob in blobs:
        # pixels() is more stable than area() when there are small holes or
        # shadows in the thresholded target.
        score = blob.pixels()
        if score > best_score:
            best_score = score
            best_blob = blob

    return best_blob


def send_packet(uart, x, y, ok):
    if ok:
        uart.write("[x=%03d y=%03d ok=1 *]" % (x, y))
    else:
        uart.write("[x=-01 y=-01 ok=0 *]")


# --------------------------- Initialization ---------------------------

uart = UART(UART_ID, UART_BAUDRATE, timeout_char=3000)

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(FRAME_SIZE)
sensor.set_windowing(WINDOWING)
sensor.skip_frames(time=2000)

# Fixed camera parameters make color threshold tracking much more stable.
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.set_auto_exposure(False, EXPOSURE_US)

clock = time.clock()
smooth_x = -1
smooth_y = -1
lost_count = 0


# --------------------------- Main loop ---------------------------

while True:
    clock.tick()

    img = sensor.snapshot()
    if LENS_CORR_STRENGTH:
        img = img.lens_corr(LENS_CORR_STRENGTH)

    blobs = img.find_blobs(
        BALL_THRESHOLDS,
        pixels_threshold=PIXELS_THRESHOLD,
        area_threshold=AREA_THRESHOLD,
        merge=True,
        margin=MERGE_MARGIN
    )

    ball = select_ball_blob(blobs)

    if ball:
        raw_x = ball.cx()
        raw_y = ball.cy()

        if smooth_x < 0 or smooth_y < 0:
            smooth_x = raw_x
            smooth_y = raw_y
        else:
            smooth_x = int((1.0 - SMOOTH_ALPHA) * smooth_x + SMOOTH_ALPHA * raw_x)
            smooth_y = int((1.0 - SMOOTH_ALPHA) * smooth_y + SMOOTH_ALPHA * raw_y)

        x = clamp_int(smooth_x, 0, img.width() - 1)
        y = clamp_int(smooth_y, 0, img.height() - 1)
        lost_count = 0

        if DEBUG_DRAW:
            img.draw_rectangle(ball.rect(), color=(255, 0, 0))
            img.draw_cross(x, y, color=(0, 255, 0))

        send_packet(uart, x, y, 1)

        if DEBUG_PRINT:
            print("ball: raw=(%d,%d), smooth=(%d,%d), pixels=%d, fps=%.1f" %
                  (raw_x, raw_y, x, y, ball.pixels(), clock.fps()))
    else:
        smooth_x = -1
        smooth_y = -1
        lost_count += 1

        # Avoid flooding the UART when the ball is outside the view.
        if lost_count == 1 or (lost_count % LOST_REPORT_PERIOD) == 0:
            send_packet(uart, -1, -1, 0)

        if DEBUG_PRINT:
            print("ball lost, fps=%.1f" % clock.fps())

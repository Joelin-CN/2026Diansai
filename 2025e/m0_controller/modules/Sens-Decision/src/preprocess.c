#include "preprocess.h"

#include <stddef.h>
#include <string.h>

sd_status_t preprocess_update(uint64_t timestamp_us, sensor_frame_t *frame) {
    sd_status_t first_status = SD_OK;
    sd_status_t status;
    size_t index;

    if (frame == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    memset(frame, 0, sizeof(*frame));
    frame->timestamp_us = timestamp_us;

    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        status = sensor_read(sensor_get((sensor_id_t)index),
                             &frame->encoders[index], timestamp_us);
        if (status == SD_OK) {
            frame->encoder_valid[index] = true;
        } else if (first_status == SD_OK) {
            first_status = status;
        }
    }
    status = sensor_read(sensor_get(SENSOR_ID_IMU), &frame->imu, timestamp_us);
    if (status == SD_OK) {
        frame->imu_valid = true;
    } else if (first_status == SD_OK) {
        first_status = status;
    }
    status = sensor_read(sensor_get(SENSOR_ID_IR_ARRAY), &frame->ir,
                         timestamp_us);
    if (status == SD_OK) {
        frame->ir_valid = true;
    } else if (first_status == SD_OK) {
        first_status = status;
    }
    return first_status;
}

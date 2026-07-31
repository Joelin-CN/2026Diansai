#include "preprocess.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief IMU坐标系适配函数：将物理传感器坐标系转换为代码算法坐标系
 *
 * @param imu [in,out] IMU数据结构指针，将就地修改其加速度和角速度数据
 *
 * @details 坐标系定义:
 *          - 物理IMU安装方向: X=右侧, Y=前方, Z=上方
 *          - 代码算法约定:     X=前方, Y=左侧, Z=上方
 *
 * @note 坐标变换公式（加速度和角速度均适用）:
 *       - code_X =  phys_Y   (前方 = 物理Y)
 *       - code_Y = -phys_X   (左侧 = -(物理X/右侧))
 *       - code_Z =  phys_Z   (上方，不变)
 *
 * @note gyro_radps[2] (偏航角速度) 不需要变号，因为两个坐标系共享
 *       相同的Z轴方向和正逆时针旋转约定（逆时针为正）。
 *
 * @warning 本函数必须在preprocess_update()读取IMU数据后立即调用，
 *          在将数据传递给EKF和感知层之前完成转换。
 *
 * @see logs/2026-07-30_coord_system_complete_fix.md - 坐标系修复完整记录
 * @see logs/2026-07-30_imu_coord_adapter_fix.md - 坐标变换推导过程
 * @date 2026-07-30
 */
static void imu_adapt_to_code_frame(imu_data_t *imu) {
    float tmp[3];

    // 变换加速度向量
    tmp[0] =  imu->accel_mps2[1];  // 前方 = 物理Y
    tmp[1] = -imu->accel_mps2[0];  // 左侧 = -物理X
    tmp[2] =  imu->accel_mps2[2];  // 上方（不变）
    memcpy(imu->accel_mps2, tmp, sizeof(tmp));

    // 变换角速度向量
    tmp[0] =  imu->gyro_radps[1];  // 前方 = 物理Y
    tmp[1] = -imu->gyro_radps[0];  // 左侧 = -物理X
    tmp[2] =  imu->gyro_radps[2];  // 上方（不变）
    memcpy(imu->gyro_radps, tmp, sizeof(tmp));
}

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
        imu_adapt_to_code_frame(&frame->imu);
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

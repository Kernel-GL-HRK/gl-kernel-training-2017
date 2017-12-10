#pragma once

#include <linux/types.h>

#define MPU6050_DEVICE_MAJOR                               42
#define MPU6050_DEVICE_MINOR                               0
#define MPU6050_DEVICE_COUNT                               2
#define MPU6050_DEVICE_NAME                                "mpu6050"

#define MPU6050_DEVICE_MINOR_STREAM                        0
#define MPU6050_DEVICE_MINOR_BUFFERED                      1

enum mpu6050_data_pos {
	MPU6050_POS_X = 0,
	MPU6050_POS_Y,
	MPU6050_POS_Z,
};

struct mpu6050_data {
	__s16 gyro[3];
	__s16 accel[3];
	__s32 temp;
};

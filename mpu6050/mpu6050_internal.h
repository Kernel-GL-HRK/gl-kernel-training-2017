#pragma once

/* sysfs interface API */

struct i2c_device_id;
struct i2c_client;

int mpu6050_sysfs_init(void);
void mpu6050_sysfs_exit(void);

int mpu6050_sysfs_probe(struct i2c_client *drv_client, const struct i2c_device_id *id);
int mpu6050_sysfs_remove(struct i2c_client *drv_client);

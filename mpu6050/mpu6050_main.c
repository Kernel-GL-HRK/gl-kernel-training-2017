#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "mpu6050_internal.h"
#include "mpu6050_regs.h"

static int mpu6050_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
	int ret;

	dev_info(&drv_client->dev, "i2c client address is [%#08X]\n", drv_client->addr);

	/* Read who_am_i register */
	ret = i2c_smbus_read_byte_data(drv_client, REG_WHO_AM_I);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&drv_client->dev, "i2c_smbus_read_byte_data() failed with error: %d\n", ret);
		return ret;
	}

	if (ret != MPU6050_WHO_AM_I) {
		dev_err(&drv_client->dev, "wrong i2c device found: expected [%#X], found [%#X]\n", MPU6050_WHO_AM_I, ret);
		return -1;
	}

	dev_info(&drv_client->dev, "found, WHO_AM_I register value = [%#X]\n", ret);

	/* Setup the device */
	/* No error handling here! */
	i2c_smbus_write_byte_data(drv_client, REG_CONFIG, 0);
	i2c_smbus_write_byte_data(drv_client, REG_GYRO_CONFIG, 0);
	i2c_smbus_write_byte_data(drv_client, REG_ACCEL_CONFIG, 0);
	i2c_smbus_write_byte_data(drv_client, REG_FIFO_EN, 0);
	i2c_smbus_write_byte_data(drv_client, REG_INT_PIN_CFG, 0);
	i2c_smbus_write_byte_data(drv_client, REG_INT_ENABLE, 0);
	i2c_smbus_write_byte_data(drv_client, REG_USER_CTRL, 0);
	i2c_smbus_write_byte_data(drv_client, REG_PWR_MGMT_1, 0);
	i2c_smbus_write_byte_data(drv_client, REG_PWR_MGMT_2, 0);

	dev_info(&drv_client->dev, "probed\n");
	mpu6050_devfs_probe(drv_client, id);
	return mpu6050_sysfs_probe(drv_client, id);
}

static int mpu6050_remove(struct i2c_client *drv_client)
{
	mpu6050_sysfs_remove(drv_client);
	mpu6050_devfs_remove(drv_client);
	dev_info(&drv_client->dev, "driver removed\n");
	return 0;
}

static const struct i2c_device_id mpu6050_idtable[] = {
	{ "gl_mpu6050", 0 },
	{ }
};

static struct i2c_driver mpu6050_i2c_driver = {
	.driver = {
		.name = "gl_mpu6050",
	},

	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = mpu6050_idtable,
};

static int mpu6050_init(void)
{
	int ret;

	mpu6050_sysfs_init();
	mpu6050_devfs_init();

	/* Create i2c driver */
	ret = i2c_add_driver(&mpu6050_i2c_driver);
	if (ret) {
		pr_err("mpu6050: failed to add new i2c driver: %d\n", ret);
		return ret;
	}

	return 0;
}

static void mpu6050_exit(void)
{
	i2c_del_driver(&mpu6050_i2c_driver);
	mpu6050_sysfs_exit();
	mpu6050_devfs_exit();
	pr_info("mpu6050: module exited\n");
}

module_init(mpu6050_init);
module_exit(mpu6050_exit);

MODULE_AUTHOR("Yaroslav Syrytsia <me@ys.lc>");
MODULE_DESCRIPTION("mpu6050 I2C acc&gyro");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

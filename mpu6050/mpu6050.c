#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/sysfs.h>

#include "mpu6050-regs.h"

#define MPU6050_ATTR(_name, _func, _var)                               \
	struct dev_ext_attribute dev_attr_##_name =                        \
		{ __ATTR(_name, 0444, _func, NULL), (void *)(_var) }

#define MPU6050_ATTR_POS(_name, _reg)                                  \
	MPU6050_ATTR(_name, mpu6050_attr_show_pos,  _reg)

#define MPU6050_ATTR_TEMP(_name, _reg)                                 \
	MPU6050_ATTR(_name, mpu6050_attr_show_temp, _reg)

static struct class_compat *mpu6050_class;

static ssize_t mpu6050_attr_show_temp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dev_ext_attribute *ea = container_of(attr, struct dev_ext_attribute, attr);
	struct i2c_client *drv_client = to_i2c_client(dev);
	int val;

	val = (s16)((u16)i2c_smbus_read_word_swapped(drv_client, (u32)ea->var));
	val = (val + 12420 + 170) / 340;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t mpu6050_attr_show_pos(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dev_ext_attribute *ea = container_of(attr, struct dev_ext_attribute, attr);
	struct i2c_client *drv_client = to_i2c_client(dev);
	int val;

	val = (s16)((u16)i2c_smbus_read_word_swapped(drv_client, (u32)ea->var));

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

/* Gyroscope */
static MPU6050_ATTR_POS(gyro_x, REG_GYRO_XOUT_H);
static MPU6050_ATTR_POS(gyro_y, REG_GYRO_YOUT_H);
static MPU6050_ATTR_POS(gyro_z, REG_GYRO_ZOUT_H);

/* Accelerometer */
static MPU6050_ATTR_POS(accel_x, REG_ACCEL_XOUT_H);
static MPU6050_ATTR_POS(accel_y, REG_ACCEL_YOUT_H);
static MPU6050_ATTR_POS(accel_z, REG_ACCEL_ZOUT_H);

/* Temperature */
static MPU6050_ATTR_TEMP(temperature, REG_TEMP_OUT_H);

static struct attribute *mpu6050_sysfs_attrs[] = {
	&dev_attr_gyro_x.attr.attr,
	&dev_attr_gyro_y.attr.attr,
	&dev_attr_gyro_z.attr.attr,
	&dev_attr_accel_x.attr.attr,
	&dev_attr_accel_y.attr.attr,
	&dev_attr_accel_z.attr.attr,
	&dev_attr_temperature.attr.attr,
	NULL
};

static struct attribute_group mpu6050_sysfs_attr_group = {
	.attrs = mpu6050_sysfs_attrs,
};

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

	ret = sysfs_create_group(&drv_client->dev.kobj, &mpu6050_sysfs_attr_group);
	if (IS_ERR_VALUE(ret)) {
		dev_info(&drv_client->dev, "cannot add sysfs group: error %d\n", ret);
		return ret;
	}

	/* Add compatibility link */
	ret = class_compat_create_link(mpu6050_class, &drv_client->dev, NULL);
	if (IS_ERR_VALUE(ret)) {
		dev_info(&drv_client->dev, "cannot add sysfs compatibility link: error %d\n", ret);
		return ret;
	}

	dev_info(&drv_client->dev, "probed\n");
	return 0;
}

static int mpu6050_remove(struct i2c_client *drv_client)
{
	dev_info(&drv_client->dev, "driver removed\n");
	class_compat_remove_link(mpu6050_class, &drv_client->dev, NULL);
	sysfs_remove_group(&drv_client->dev.kobj, &mpu6050_sysfs_attr_group);
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

	/* Create /sys/class/mpu6050 */
	mpu6050_class = class_compat_register("mpu6050");
	if (!mpu6050_class) {
		pr_err("mpu6050: failed to add sysfs class");
		return -ENOMEM;
	}

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
	if (mpu6050_class) {
		class_compat_unregister(mpu6050_class);
	}
	i2c_del_driver(&mpu6050_i2c_driver);
	pr_info("mpu6050: module exited\n");
}

module_init(mpu6050_init);
module_exit(mpu6050_exit);

MODULE_AUTHOR("Yaroslav Syrytsia <me@ys.lc>");
MODULE_DESCRIPTION("mpu6050 I2C acc&gyro");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

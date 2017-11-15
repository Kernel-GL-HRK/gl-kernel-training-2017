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

int mpu6050_sysfs_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
	int ret;

	if (!drv_client) {
		pr_err("mpu6050: sysfs: device pointer is invalid\n");
		return -1;
	}

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

	dev_info(&drv_client->dev, "sysfs: probed\n");
	return 0;
}

int mpu6050_sysfs_remove(struct i2c_client *drv_client)
{
	class_compat_remove_link(mpu6050_class, &drv_client->dev, NULL);
	sysfs_remove_group(&drv_client->dev.kobj, &mpu6050_sysfs_attr_group);
	pr_info("mpu6050: sysfs: removed\n");
	return 0;
}

int mpu6050_sysfs_init(void)
{
	/* Create /sys/class/mpu6050 */
	mpu6050_class = class_compat_register("mpu6050");
	if (!mpu6050_class) {
		pr_err("mpu6050: sysfs: failed to add class\n");
		return -ENOMEM;
	}
	pr_info("mpu6050: sysfs: init done\n");
	return 0;
}

void mpu6050_sysfs_exit(void)
{
	if (mpu6050_class)
		class_compat_unregister(mpu6050_class);
	pr_info("mpu6050: sysfs: exited\n");
}

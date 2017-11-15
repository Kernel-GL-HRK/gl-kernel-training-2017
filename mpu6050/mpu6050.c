#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "mpu6050-regs.h"

enum AXES {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	AXES_NUM
};

enum VALUE_TYPE {
	VALUE_TYPE_ACCEL,
	VALUE_TYPE_TMPR,
	VALUE_TYPE_GYRO
};

#define READ_REG_NUM (AXES_NUM * 2 + 1)

struct mpu6050_data {
	struct i2c_client *drv_client;
	union {
		struct {
			int accel[AXES_NUM];
			int temperature;
			int gyro [AXES_NUM];
		} values;
		int raw[READ_REG_NUM];
	} data;
};

static struct mpu6050_data g_mpu6050_data;

static ssize_t show_item(struct class *class, struct class_attribute *attr, char *buf);

static int mpu6050_read_data(void)
{
	u8 values[READ_REG_NUM * sizeof(u16)];
	int temp, result, i, *p;
	struct i2c_client *drv_client = g_mpu6050_data.drv_client;

	if (drv_client == 0)
		return -ENODEV;

	result = i2c_smbus_read_i2c_block_data(drv_client, REG_ACCEL_XOUT_H,
			READ_REG_NUM * sizeof(u16), (char*) values);

	if(result != READ_REG_NUM * sizeof(u16)) {
		dev_err(&drv_client->dev, "i2c_smbus_read_i2c_block_data wrong %d\n", result);
		return -EINVAL;
	}

	for (i = 0, p = g_mpu6050_data.data.raw; i < READ_REG_NUM * sizeof(u16); i += 2, ++p)
		*p = (s16) ((u16) values[i] << 8 | (u16)values[i + 1]);

	/* Temperature in degrees C =
	 * (TEMP_OUT Register Value  as a signed quantity)/340 + 36.53
	 */
	temp = g_mpu6050_data.data.values.temperature;
	g_mpu6050_data.data.values.temperature = (temp + 12420 + 170) / 340;

	dev_info(&drv_client->dev, "sensor data read:\n");

	dev_info(&drv_client->dev, "ACCEL[X,Y,Z] = [%d, %d, %d]\n",
		g_mpu6050_data.data.values.accel[AXIS_X],
		g_mpu6050_data.data.values.accel[AXIS_Y],
		g_mpu6050_data.data.values.accel[AXIS_Z]);

	dev_info(&drv_client->dev, "GYRO[X,Y,Z] = [%d, %d, %d]\n",
		g_mpu6050_data.data.values.gyro[AXIS_X],
		g_mpu6050_data.data.values.gyro[AXIS_Y],
		g_mpu6050_data.data.values.gyro[AXIS_Z]);

	dev_info(&drv_client->dev, "TEMP = %d\n",
			g_mpu6050_data.data.values.temperature);

	return 0;
}

static int mpu6050_probe(struct i2c_client *drv_client,
			 const struct i2c_device_id *id)
{
	int ret;

	dev_info(&drv_client->dev,
		"i2c client address is 0x%X\n", drv_client->addr);

	/* Read who_am_i register */
	ret = i2c_smbus_read_byte_data(drv_client, REG_WHO_AM_I);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&drv_client->dev,
			"i2c_smbus_read_byte_data() failed with error: %d\n",
			ret);
		return ret;
	}
	if (ret != MPU6050_WHO_AM_I) {
		dev_err(&drv_client->dev,
			"wrong i2c device found: expected 0x%X, found 0x%X\n",
			MPU6050_WHO_AM_I, ret);
		return -1;
	}
	dev_info(&drv_client->dev,
		"i2c mpu6050 device found, WHO_AM_I register value = 0x%X\n",
		ret);

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

	g_mpu6050_data.drv_client = drv_client;

	dev_info(&drv_client->dev, "i2c driver probed\n");
	return 0;
}

static int mpu6050_remove(struct i2c_client *drv_client)
{
	g_mpu6050_data.drv_client = 0;

	dev_info(&drv_client->dev, "i2c driver removed\n");
	return 0;
}

static const struct i2c_device_id mpu6050_idtable[] = {
	{ "mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_idtable);

static struct i2c_driver mpu6050_i2c_driver = {
	.driver = {
		.name = "gl_mpu6050",
	},

	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = mpu6050_idtable,
};

struct class_attribute class_attr_array[READ_REG_NUM] = {
	{ .attr = { .name = "accel_x", .mode = S_IRUGO }, .show = &show_item, },
	{ .attr = { .name = "accel_y", .mode = S_IRUGO }, .show = &show_item, },
	{ .attr = { .name = "accel_z", .mode = S_IRUGO }, .show = &show_item, },

	{ .attr = { .name = "temperature", .mode = S_IRUGO }, .show = &show_item, },

	{ .attr = { .name = "gyro_x", .mode = S_IRUGO }, .show = &show_item, },
	{ .attr = { .name = "gyro_y", .mode = S_IRUGO }, .show = &show_item, },
	{ .attr = { .name = "gyro_z", .mode = S_IRUGO }, .show = &show_item, }
};

static ssize_t show_item(struct class *class, struct class_attribute *attr, char *buf)
{
	int index = attr - class_attr_array;
	int value = 0;

	mpu6050_read_data();

	if (index >=0 && index < READ_REG_NUM) {
		value = g_mpu6050_data.data.raw[index];
	}

	sprintf(buf, "%d\n", value);
	return strlen(buf);
}

static struct class *attr_class;

static int mpu6050_init(void)
{
	int ret, i;

	/* Create i2c driver */
	ret = i2c_add_driver(&mpu6050_i2c_driver);
	if (ret) {
		pr_err("mpu6050: failed to add new i2c driver: %d\n", ret);
		return ret;
	}
	pr_info("mpu6050: i2c driver created\n");

	/* Create class */
	attr_class = class_create(THIS_MODULE, "mpu6050");
	if (IS_ERR(attr_class)) {
		ret = PTR_ERR(attr_class);
		pr_err("mpu6050: failed to create sysfs class: %d\n", ret);
		return ret;
	}
	pr_info("mpu6050: sysfs class created\n");

	for (i = 0; i < READ_REG_NUM; ++i) {
		ret = class_create_file(attr_class, &class_attr_array[i]);
		if (ret) {
			pr_err("mpu6050: failed to create sysfs class attribute accel_x: %d\n", ret);
			return ret;
		}
	}

	pr_info("mpu6050: sysfs class attributes created\n");

	pr_info("mpu6050: module loaded\n");
	return 0;
}

static void mpu6050_exit(void)
{
	if (attr_class) {
		int i;

		for (i = 0; i < READ_REG_NUM; ++i) {
			class_remove_file(attr_class, &class_attr_array[i]);
		}
		pr_info("mpu6050: sysfs class attributes removed\n");

		class_destroy(attr_class);
		pr_info("mpu6050: sysfs class destroyed\n");
	}

	i2c_del_driver(&mpu6050_i2c_driver);
	pr_info("mpu6050: i2c driver deleted\n");

	pr_info("mpu6050: module exited\n");
}

module_init(mpu6050_init);
module_exit(mpu6050_exit);

MODULE_AUTHOR("Dmytro Kirtoka <dimk334@gmail.com>");
MODULE_DESCRIPTION("mpu6050 I2C acc&gyro");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

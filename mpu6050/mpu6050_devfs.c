#define pr_fmt(fmt) KBUILD_MODNAME ": devfs: " fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/timekeeping.h>

#include "mpu6050_internal.h"
#include "mpu6050_regs.h"
#include "mpu6050.h"

/* KFIFO size must be a power of 2 */
#define MPU6050_FIFO_SIZE                                  16

static struct _mpu6050_devfs_context {
	struct cdev cdev;
	rwlock_t fifo_lock;
	DECLARE_KFIFO(fifo, struct mpu6050_data, MPU6050_FIFO_SIZE);
	ktime_t last_access_time;
	struct semaphore sem_stream;
	struct i2c_client *drv_client;
} *mpu6050_devfs_context;

static inline s16 mpu6050_devfs_read_reg(struct i2c_client *drv, u32 reg)
{
	return (s16)((u16)i2c_smbus_read_word_swapped(drv, reg));
}

static void mpu6050_devfs_read_data(struct i2c_client *drv, struct mpu6050_data *buf)
{
	buf->gyro[MPU6050_POS_X] = mpu6050_devfs_read_reg(drv, REG_GYRO_XOUT_H);
	buf->gyro[MPU6050_POS_Y] = mpu6050_devfs_read_reg(drv, REG_GYRO_YOUT_H);
	buf->gyro[MPU6050_POS_Z] = mpu6050_devfs_read_reg(drv, REG_GYRO_ZOUT_H);

	buf->accel[MPU6050_POS_X] = mpu6050_devfs_read_reg(drv, REG_ACCEL_XOUT_H);
	buf->accel[MPU6050_POS_Y] = mpu6050_devfs_read_reg(drv, REG_ACCEL_YOUT_H);
	buf->accel[MPU6050_POS_Z] = mpu6050_devfs_read_reg(drv, REG_ACCEL_ZOUT_H);

	buf->temp = mpu6050_devfs_read_reg(drv, REG_TEMP_OUT_H);
	buf->temp = (buf->temp + 12420 + 170) / 340;
}

static int mpu6050_devfs_open(struct inode *inode, struct file *file)
{
	struct _mpu6050_devfs_context *ctx = container_of(inode->i_cdev, struct _mpu6050_devfs_context, cdev);

	if (MINOR(inode->i_rdev) == MPU6050_DEVICE_MINOR_STREAM) {
		if (down_trylock(&ctx->sem_stream) != 0)
			return -EBUSY;
	}

	file->private_data = ctx;
	return 0;
}

static int mpu6050_devfs_release(struct inode *inode, struct file *file)
{
	struct _mpu6050_devfs_context *ctx = file->private_data;

	if (MINOR(inode->i_rdev) == MPU6050_DEVICE_MINOR_STREAM)
		up(&ctx->sem_stream);

	return 0;
}

static inline bool mpu6050_devfs_data_can_save(struct _mpu6050_devfs_context *ctx)
{
	ktime_t time_now = ktime_get();
	ktime_t time_diff;
	const ktime_t time_zero = ktime_set(0, 0); /* Call ktime_set() for the portability reason (4.9 -> 4.13) */

	if (ktime_after(ctx->last_access_time, time_zero)) {
		time_diff = ktime_sub(time_now, ctx->last_access_time);
		if (ktime_to_ms(time_diff) < 1000L)
			return false;
	}

	ctx->last_access_time = time_now;
	return true;
}

static ssize_t mpu6050_devfs_read_current(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	struct _mpu6050_devfs_context *ctx = file->private_data;
	struct mpu6050_data data = { };

	if (size < sizeof(data))
		return -EINVAL;

	/* Read the data ... */
	mpu6050_devfs_read_data(ctx->drv_client, &data);

	/* Copy to user */
	if (copy_to_user(buf, &data, sizeof(data)))
		return -EINVAL;

	if (mpu6050_devfs_data_can_save(ctx)) {
		write_lock(&ctx->fifo_lock);

		/* Check for free space and drop the first value */
		if (kfifo_is_full(&ctx->fifo))
			kfifo_skip(&ctx->fifo);

		/* Add intem */
		kfifo_put(&ctx->fifo, data);

		write_unlock(&ctx->fifo_lock);
	}
	return sizeof(data);
}

static ssize_t mpu6050_devfs_read_buffered(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	struct _mpu6050_devfs_context *ctx = file->private_data;
	struct mpu6050_data data = { };
	int ret = -1;

	if (size < sizeof(struct mpu6050_data)) {
		ret = -EINVAL;
		goto exit;
	}

	read_lock(&ctx->fifo_lock);
	ret = kfifo_get(&ctx->fifo, &data);
	if (ret == 0)
		goto out; /* FIFO is empty */

	ret = copy_to_user(buf, &data, sizeof(data));
	if (ret == 0)
		ret = sizeof(data);
out:
	read_unlock(&ctx->fifo_lock);
exit:
	return ret;
}

static ssize_t mpu6050_devfs_read(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	struct inode *inode = file_inode(file);
	int minor = MINOR(inode->i_rdev);
	struct _mpu6050_devfs_context *ctx = file->private_data;

	if (!ctx->drv_client) {
		pr_debug("driver is not probed\n");
		return -EIO;
	}

	switch (minor) {

	case MPU6050_DEVICE_MINOR_STREAM:
		return mpu6050_devfs_read_current(file, buf, size, pos);

	case MPU6050_DEVICE_MINOR_BUFFERED:
		return mpu6050_devfs_read_buffered(file, buf, size, pos);

	default:
		pr_err("unknown minor number: %d\n", minor);
		return -EFAULT;
	}
}

static const struct file_operations mpu6050_devfs_fops = {
	.owner   = THIS_MODULE,
	.open    = mpu6050_devfs_open,
	.release = mpu6050_devfs_release,
	.read    = mpu6050_devfs_read,
};

int mpu6050_devfs_probe(struct i2c_client *drv_client, const struct i2c_device_id *id)
{
	dev_info(&drv_client->dev, "devfs: probed\n");
	mpu6050_devfs_context->drv_client = drv_client;
	return 0;
}

int mpu6050_devfs_remove(struct i2c_client *drv_client)
{
	dev_info(&drv_client->dev, "devfs: removed\n");
	mpu6050_devfs_context->drv_client = NULL;
	return 0;
}

int mpu6050_devfs_init(void)
{
	dev_t devno;
	int ret;
	struct _mpu6050_devfs_context *ctx;

	devno = MKDEV(MPU6050_DEVICE_MAJOR, MPU6050_DEVICE_MINOR);
	ret = register_chrdev_region(devno, MPU6050_DEVICE_COUNT, MPU6050_DEVICE_NAME);
	if (ret) {
		pr_err("failed to register region: %d\n", ret);
		return ret;
	}

	ctx = kzalloc(sizeof(struct _mpu6050_devfs_context), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err;
	}

	cdev_init(&ctx->cdev, &mpu6050_devfs_fops);
	ctx->cdev.owner = THIS_MODULE;
	ret = cdev_add(&ctx->cdev, devno, MPU6050_DEVICE_COUNT);
	if (ret) {
		pr_err("failed to add devices: %d\n", ret);
		goto err_add;
	}

	/* Semaphore for 'stream' device */
	sema_init(&ctx->sem_stream, 1);

	/* FIFO init */
	rwlock_init(&ctx->fifo_lock);
	INIT_KFIFO(ctx->fifo);

	mpu6050_devfs_context = ctx;
	pr_info("init done\n");
	return 0;

err_add:
	kfree(ctx);
err:
	unregister_chrdev_region(devno, MPU6050_DEVICE_COUNT);
	return ret;
}

void mpu6050_devfs_exit(void)
{
	dev_t devno;

	devno = MKDEV(MPU6050_DEVICE_MAJOR, MPU6050_DEVICE_MINOR);
	unregister_chrdev_region(devno, MPU6050_DEVICE_COUNT);
	if (mpu6050_devfs_context) {
		cdev_del(&mpu6050_devfs_context->cdev);
		kfree(mpu6050_devfs_context);
	}
	pr_info("exited\n");
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#include "mpu6050.h"

#define DEVICE_STREAM            "/dev/" MPU6050_DEVICE_NAME "_stream"
#define DEVICE_BUFFERED          "/dev/" MPU6050_DEVICE_NAME "_buffered"

static long long
get_current_time()
{
	struct timespec t = {};

	clock_gettime(CLOCK_MONOTONIC_COARSE, &t);
	return t.tv_sec * 1000ull + t.tv_nsec / 1000000ull;
}


static int
xmknod(const char *pathname, mode_t mode, dev_t dev)
{
	int r;

	r = mknod(pathname, mode, dev);
	if (r)
		err(1, "cannot create [%s] with mode [%#x]", pathname, mode);

	return r;
}

static void
dev_create()
{
	if (access(DEVICE_STREAM, F_OK))
		xmknod(DEVICE_STREAM, S_IFCHR | 0666, makedev(MPU6050_DEVICE_MAJOR, MPU6050_DEVICE_MINOR_STREAM));

	if (access(DEVICE_BUFFERED, F_OK))
		xmknod(DEVICE_BUFFERED, S_IFCHR | 0666, makedev(MPU6050_DEVICE_MAJOR, MPU6050_DEVICE_MINOR_BUFFERED));
}

static void
dev_read(const char *pathname, const char *title, useconds_t delay)
{
	int fd, r;
	struct mpu6050_data data = {};

	fd = open(pathname, O_RDONLY);
	if (fd < 0)
		err(1, "cannot open [%s] in read-only mode", pathname);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	for (;;) {
		memset(&data, 0, sizeof(data));
		r = read(fd, &data, sizeof(data));
		if (r && r != sizeof(data)) {
			warnx("cannot read [%zu] bytes from [%s], got only [%d]", (size_t)sizeof(data), pathname, r);
			sleep(1);
			continue;
		}
		else if (r == 0) {
			goto sleep;
		}
		printf("[%06lld][%-10s]: GYRO: X:%d, Y:%d, Z:%d; ACCEL: X:%d, Y:%d, Z:%d; TEMP: %d\n",
			get_current_time(),
			title,
			data.gyro[MPU6050_POS_X],  data.gyro[MPU6050_POS_Y],  data.gyro[MPU6050_POS_Z],
			data.accel[MPU6050_POS_X], data.accel[MPU6050_POS_Y], data.accel[MPU6050_POS_Z],
			data.temp
		);
sleep:
		usleep(delay);
	}
}

static void*
dev_stream_read(void *arg)
{
	dev_read(DEVICE_STREAM, "STREAM", 750000); /* 0.75 sec */
	return NULL;
}

static void*
dev_buffered_read(void *arg)
{
	dev_read(DEVICE_BUFFERED, "BUFFERED", 250000); /* 0.25 sec */
	return NULL;
}

int
main(void)
{
	pthread_t stream, buffered;
	sigset_t set;
	int sig;

	/* Create char devices */
	dev_create();

	/* Spawn threads */
	pthread_create(&stream,   NULL, dev_stream_read,   NULL);
	pthread_create(&buffered, NULL, dev_buffered_read, NULL);

	/* Wait for a term signal */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigwait(&set, &sig);

	/* Stop threads */
	pthread_cancel(stream);
	pthread_cancel(buffered);

	return 0;
}

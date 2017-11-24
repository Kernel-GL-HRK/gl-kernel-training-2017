#
# mpu6050 accelerometer & gyroscope
#
ifneq ($(KERNELRELEASE),)

obj-m += mpu6050.o
mpu6050-objs := mpu6050_main.o
mpu6050-objs += mpu6050_sysfs.o
mpu6050-objs += mpu6050_devfs.o

else


ifeq ($(KERNELDIR),)
ifeq ($(BBB_KERNEL),)
	$(error Path to kernel tree - KERNELDIR or BBB_KERNEL variable is not defined!)
endif
endif

KERNELDIR ?= $(BBB_KERNEL)

export ARCH = arm
export CROSS_COMPILE ?= arm-linux-gnueabihf-


.PHONY: all clean dtb

all: reader
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules

clean:
	$(RM) reader
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) clean

dtb:
	cp am335x-bone-common.dtsi $(KERNELDIR)/arch/arm/boot/dts/
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) dtbs

reader: mpu6050_reader.c
	$(CROSS_COMPILE)gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE=1 $^ -o $@ -lpthread

endif

CC = gcc
CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)

PROGLIST = ioctl-diag fchange
TARGET = dubfl
obj-m  := $(TARGET).o

all: default $(PROGLIST) clean

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

ioctl-diag: ioctl.h ioctl-diag.c
	$(CC) ioctl-diag.c -o ioctl-diag

fchange: ioctl.h fchange.c
	$(CC) fchange.c -o fchange

clean:
	@rm -f *.o .*.cmd .*.flags *.mod.c *.order
	@rm -f .*.*.cmd *.symvers *~ *.*~ TODO.*
	@rm -fR .tmp*
	@rm -rf .tmp_versions

disclean: clean
	@rm -f *.ko $(PROGLIST)


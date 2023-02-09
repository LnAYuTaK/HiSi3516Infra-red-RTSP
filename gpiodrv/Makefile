obj-m := gpiodrv.o
export ARCH=arm
export CROSS_COMPILE=arm-himix100-linux-
KERDIR := /root/Workspace/SDK/package/osdrv/osdrv/opensource/kernel/linux-4.9.y
CURDIR := $(shell pwd)
all:
	make -C $(KERDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-himix100-linux-
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers

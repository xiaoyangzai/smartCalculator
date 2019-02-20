obj-m := mini210_hx711.o
KDIR=/home/wy/arm_project/tiny210/linux-3.0.8

CROSS_COMPILE=arm-linux-
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules


clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order

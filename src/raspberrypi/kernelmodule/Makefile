KERNEL_HEADERS=/lib/modules/$(shell uname -r)/build
obj-m := rascsidrv.o
ccflags-y := -O3

all:
	$(MAKE) -C $(KERNEL_HEADERS) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_HEADERS) M=$(PWD) clean

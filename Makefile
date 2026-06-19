obj-m += fortune_device.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean load unload reload logs test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: all
	sudo insmod fortune_device.ko

unload:
	sudo rmmod fortune_device

reload: unload load

logs:
	sudo dmesg | tail -n 30

test:
	@echo "Reading default fortune:"
	@sudo cat /dev/fortune_device
	@echo "Writing new fortune:"
	@echo "The kernel keeps its own address space." | sudo tee /dev/fortune_device >/dev/null
	@echo "Reading updated fortune:"
	@sudo cat /dev/fortune_device
	@echo "Recent kernel log:"
	@sudo dmesg | tail -n 30

obj-m += fortune_device.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
USER_TEST := fortune_user_test
USER_CC ?= gcc
USER_CFLAGS ?= -Wall -Wextra -O2
DEVICE ?= /dev/fortune_device

.PHONY: all clean load unload reload logs test user-test run-user-test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm -f $(USER_TEST)
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: all
	sudo insmod fortune_device.ko

unload:
	sudo rmmod fortune_device

reload: unload load

logs:
	sudo dmesg | tail -n 30

user-test: $(USER_TEST)

$(USER_TEST): fortune_user_test.c
	$(USER_CC) $(USER_CFLAGS) -o $@ $<

run-user-test: $(USER_TEST)
	sudo ./$(USER_TEST) $(DEVICE)

test:
	@echo "Reading default fortune:"
	@sudo cat /dev/fortune_device
	@echo "Writing new fortune:"
	@echo "The kernel keeps its own address space." | sudo tee /dev/fortune_device >/dev/null
	@echo "Reading updated fortune:"
	@sudo cat /dev/fortune_device
	@echo "Recent kernel log:"
	@sudo dmesg | tail -n 30

# Short instruction for building kernel module

ifneq ($(KERNELRELEASE),)
# call from kernel build system

# Add your debugging flag (or not) to CFLAGS
# debug by default, comment next line if required
DEBUG = y
ifeq ($(DEBUG),y)
  ccflags-y += -DSHOFER_DEBUG
endif

obj-m	:= shofer.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

ifneq (${KERNELRELEASE},)

# KERNELRELEASE defined: we are being compiled as part of the Kernel
        obj-m := vga_ball.o

else

# We are being compiled as a module: use the Kernel build system

	KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
        PWD := $(shell pwd)



# ...
else

KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
PWD           := $(shell pwd)

# user‐space sources and flags
US_SRCS       := hello.c controller.c
US_OBJS       := $(US_SRCS:.c=.o)
CFLAGS       += -I.           # so #include "controller.h" works
LDFLAGS      += -lusb-1.0 -lpthread

.PHONY: all
all: module hello

# 1) kernel module build
module:
    $(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules

# 2) user‐space program
hello: $(US_SRCS)
    $(CC) $(CFLAGS) -o $@ $(US_SRCS) $(LDFLAGS)

clean:
    $(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
    $(RM) hello *.o

# bundle everything
TARFILES  := Makefile README vga_ball.h vga_ball.c hello.c controller.c controller.h
TARFILE   := lab3-sw.tar.gz
.PHONY: tar
tar: $(TARFILE)

$(TARFILE): $(TARFILES)
    tar zcfC $(TARFILE) .. $(TARFILES:%=lab3-sw/%)

endif

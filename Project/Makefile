# Makefile for the VGA Space Shooter demo

# For FPGA hardware
HARDWARE_FLAGS = -Wall -O3 -g
HARDWARE_FILES = vga_spaceship.sv

# For Linux kernel module
KERNEL_FLAGS = -Wall -Werror
KERNEL_MOD = vga_spaceship
obj-m = $(KERNEL_MOD).o

# For user space program
USER_FLAGS = -Wall -O3 -g
USER_FILES = space_shooter.c

# Default targets
all: hardware module program

# Hardware build (SystemVerilog)
hardware: $(HARDWARE_FILES)
	@echo "Building hardware..."
	# Add your quartus or other FPGA synthesis commands here
	@echo "Hardware build completed."

# Linux kernel module
module:
	@echo "Building kernel module..."
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# User program
program: $(USER_FILES)
	@echo "Building user program..."
	gcc $(USER_FLAGS) -o space_shooter $(USER_FILES)

# Clean everything
clean:
	@echo "Cleaning up..."
	rm -f space_shooter
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# Add commands to clean hardware build files if needed

.PHONY: all hardware module program clean
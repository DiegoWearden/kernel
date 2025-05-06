# Cross compiler and tools
CURRENT_OS = $(strip $(shell uname -s))
ifeq ($(findstring Darwin,$(CURRENT_OS)),Darwin)
    CROSS_COMPILE = aarch64-elf-
else
    CROSS_COMPILE = aarch64-linux-gnu-
endif

AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Directories
BUILD_DIR = build

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_IMG = $(BUILD_DIR)/kernel8.img

# Linker flags
LDFLAGS = -T linker.ld

# QEMU run options
QEMU_ARGS = -M raspi3b -kernel $(KERNEL_IMG) -smp 4 -serial stdio -usb -device usb-net,netdev=net0 -netdev user,id=net0 -device usb-mouse -device usb-kbd -drive file=sdcard.dd,if=sd,format=raw

# Build all
all: $(KERNEL_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Dummy kernel elf rule – add object files as needed
$(KERNEL_ELF): | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF)

# Convert ELF to raw kernel image
$(KERNEL_IMG): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_IMG)

clean:
	rm -rf $(BUILD_DIR)

run:
	qemu-system-aarch64 $(QEMU_ARGS)

debug:
	qemu-system-aarch64 $(QEMU_ARGS) -S -gdb tcp::1234

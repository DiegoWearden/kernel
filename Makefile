# Cross compiler and tools
CROSS_COMPILE = aarch64-none-elf-
AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Directories
BUILD_DIR = build
SRC_DIR = src
INCLUDE_DIR = include

# Automatically find all source files
ASM_SRC = $(wildcard $(SRC_DIR)/*.S)
C_SRC = $(wildcard $(SRC_DIR)/*.c)
CPP_SRC = $(wildcard $(SRC_DIR)/*.cpp)

# Change file extensions to suffix source type
ASM_OBJ = $(patsubst $(SRC_DIR)/%.S, $(BUILD_DIR)/%_S.o, $(ASM_SRC))
C_OBJ   = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%_c.o, $(C_SRC))
CPP_OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%_cpp.o, $(CPP_SRC))

# Output files
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_IMG = $(BUILD_DIR)/kernel8.img
MAP_FILE   = $(BUILD_DIR)/kernel.map

# Compiler and linker flags
CFLAGS = -Wall -Wextra -nostdlib -ffreestanding -I$(INCLUDE_DIR) -g -mcpu=cortex-a53 -march=armv8-a+crc -latomic -mstrict-align -mno-outline-atomics
LDFLAGS = -T linker.ld

# Default target
all: $(KERNEL_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile ASM files
$(BUILD_DIR)/%_S.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	$(CC) -E -x assembler-with-cpp -I$(INCLUDE_DIR) $< -o $(BUILD_DIR)/$(basename $(@F)).i
	$(AS) $(BUILD_DIR)/$(basename $(@F)).i -o $@
	rm $(BUILD_DIR)/$(basename $(@F)).i

# Compile C files
$(BUILD_DIR)/%_c.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(BUILD_DIR)/%_cpp.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

# Link all object files into ELF and generate map
$(KERNEL_ELF): $(ASM_OBJ) $(C_OBJ) $(CPP_OBJ) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -Map $(MAP_FILE) -o $@ $^
	@echo "Generated map: $(MAP_FILE)"

# Convert ELF to binary image
$(KERNEL_IMG): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)

run:
	qemu-system-aarch64 -M raspi3b -kernel $(KERNEL_IMG) -smp 4 -serial stdio

debug:
	qemu-system-aarch64 -M raspi3b -kernel $(KERNEL_IMG) -smp 4 -serial stdio -S -gdb tcp::1234

.PHONY: all clean run debug

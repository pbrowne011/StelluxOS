# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

# Architecture detection
ifeq ($(ARCH),arm64)
    TARGET_ARCH := ARM64
	PREPROCESSOR_DEFINES := -DARCH_$(TARGET_ARCH)
    # ARM64 toolchain
    AS := $(CROSS_COMPILE)gcc
    CC := $(CROSS_COMPILE)g++
    LD := $(CROSS_COMPILE)ld
    
    # ARM64 flags
    ASFLAGS := -g -ffreestanding -nostdlib -I$(INC_DIR) -I$(INC_DIR)/core $(PREPROCESSOR_DEFINES)
    CXXFLAGS := -g -O0 -I$(INC_DIR) -I$(INC_DIR)/core $(PREPROCESSOR_DEFINES) \
                -ffreestanding -Wall -Werror -Wextra -nostdlib -fno-exceptions -fno-rtti -std=c++17 \
                -fno-allow-store-data-races -fno-reorder-blocks -fno-ipa-cp-clone -fno-partial-inlining \
                -Wimplicit-fallthrough=5 -Wno-unused-but-set-variable -Wno-unused-const-variable \
                -fno-omit-frame-pointer -fno-optimize-sibling-calls
    LINKER_SCRIPT := $(SRC_DIR)/arch/arm/stellux-arm64.ld
	LDFLAGS := -T $(LINKER_SCRIPT) -g
else
    TARGET_ARCH := X86_64
	PREPROCESSOR_DEFINES := -DARCH_$(TARGET_ARCH)
    # x86_64 toolchain
    AS := gcc
    CC := g++
    LD := ld
    
    # x86_64 flags
    ASFLAGS := -m64 -g -ffreestanding -nostdlib -I$(INC_DIR) -I$(INC_DIR)/core $(PREPROCESSOR_DEFINES)
    CXXFLAGS := -m64 -g -O0 -I$(INC_DIR) -I$(INC_DIR)/core $(PREPROCESSOR_DEFINES) \
                -ffreestanding -Wall -Werror -Wextra -nostdlib -fno-exceptions -fno-rtti -std=c++17 -mno-red-zone \
                -fno-allow-store-data-races -fno-reorder-blocks -fno-ipa-cp-clone -fno-partial-inlining -Wframe-larger-than=2048 \
                -Wimplicit-fallthrough=5 -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls \
                -fno-stack-clash-protection -Wno-stringop-truncation -Wno-zero-length-bounds -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict \
                -Wno-maybe-uninitialized -fno-strict-overflow -fno-stack-check -fconserve-stack -fno-stack-protector -mno-sse -mno-mmx
    LINKER_SCRIPT := $(SRC_DIR)/arch/x86/stellux.ld
	LDFLAGS := -m elf_x86_64 -T $(LINKER_SCRIPT) -g
endif

# Output files
KERNEL := $(BUILD_DIR)/stellux

# Function to recursively find files in subdirectories
rwildcard = $(foreach d,$(wildcard $(1)/*), \
             $(call rwildcard,$d,$2)) \
             $(filter $(subst *,%,$2),$d)

# Source and header files
ifeq ($(ARCH),arm64)
    # ARM64-specific sources
    #ASM_SRCS := $(call rwildcard, $(SRC_DIR)/arch/arm, *.S)
	ASM_SRCS := $(SRC_DIR)/arch/arm/boot/boot.S
    #CPP_SRCS := $(call rwildcard, $(SRC_DIR), *.cpp)
	CPP_SRCS :=
    #CPP_SRCS := $(filter-out $(SRC_DIR)/arch/x86/%.cpp,$(CPP_SRCS))
else
    # x86 sources
    ASM_SRCS := $(call rwildcard, $(SRC_DIR)/arch/x86, *.S)
    CPP_SRCS := $(call rwildcard, $(SRC_DIR), *.cpp)
    CPP_SRCS := $(filter-out $(SRC_DIR)/arch/arm/%.cpp,$(CPP_SRCS))
endif
HEADERS  := $(call rwildcard, $(SRC_DIR), *.h)

# Object files
ASM_OBJS := $(ASM_SRCS:$(SRC_DIR)/%.S=$(OBJ_DIR)/%.o)
CPP_OBJS := $(CPP_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
OBJS     := $(ASM_OBJS) $(CPP_OBJS)

# Default target
all: $(KERNEL)

# Create build directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile assembly files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S $(HEADERS)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# Compile C++ files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) -c $< -o $@

# Link the kernel
$(KERNEL): $(OBJS) $(LINKER_SCRIPT) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

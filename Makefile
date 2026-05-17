# Khazar OS Makefile
# WSL2/Linux ortamında çalışır

# Toolchain (32-bit)
AS = nasm
CC ?= i686-linux-gnu-gcc
LD ?= i686-linux-gnu-ld

# Flags (32-bit)
ASFLAGS = -f elf32
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib -Ikernel/include -fno-pie -m32
LDFLAGS = -T linker.ld -nostdlib -no-pie -m elf_i386

# Dizinler
KERNEL_DIR = kernel
BUILD_DIR = build

# Kaynak dosyaları (Tüm alt dizinleri tara, 64-bit dosyaları hariç tut)
SRC_C = $(shell find $(KERNEL_DIR) -name "*.c" ! -name "*64.c")
SRC_ASM = $(shell find $(KERNEL_DIR) -name "*.asm" ! -name "*64.asm")
SRC_S = $(shell find $(KERNEL_DIR) -name "*.s" ! -name "*64.s")

# Object dosyaları (build dizini altında aynı hiyerarşi)
OBJ_C = $(SRC_C:$(KERNEL_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJ_ASM = $(SRC_ASM:$(KERNEL_DIR)/%.asm=$(BUILD_DIR)/%.o)
OBJ_S = $(SRC_S:$(KERNEL_DIR)/%.s=$(BUILD_DIR)/%.o)

OBJ = $(OBJ_C) $(OBJ_ASM) $(OBJ_S)

# Çıktı dosyaları
KERNEL_BIN = khazar_kernel.bin
ISO_DIR = iso
ISO_FILE = khazar_os.iso

# Userland Toolchain
USER_CC ?= i686-w64-mingw32-gcc
USER_AS ?= nasm

USER_CFLAGS = -Iuserland/include -ffreestanding -nostdlib -O2 -Wall -Wextra -m32 -mno-stack-arg-probe
USER_LDFLAGS = -nostdlib -m32 -Wl,--entry=_start -Wl,--subsystem,console

USER_LIBC_SRC_C = $(wildcard userland/libc/*.c)
USER_LIBC_SRC_S = $(wildcard userland/libc/*.s)
USER_LIBC_OBJ = $(USER_LIBC_SRC_C:userland/libc/%.c=$(BUILD_DIR)/userland/libc/%.o) \
                $(USER_LIBC_SRC_S:userland/libc/%.s=$(BUILD_DIR)/userland/libc/%.o)

USER_APPS_SRC = $(wildcard userland/bin/*.c)
USER_APPS_BIN = $(USER_APPS_SRC:userland/bin/%.c=$(ISO_DIR)/bin/%.exe)

# Default target
all: $(ISO_FILE)

# C dosyalarını derle
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# ASM dosyalarını (NASM) derle
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) $< -o $@

# S dosyalarını (Giriş: .s, Çıkış: .o) NASM ile derle (Khazar OS pratiği)
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.s
	@mkdir -p $(dir $@)
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) $< -o $@

# Userland Libc C
$(BUILD_DIR)/userland/libc/%.o: userland/libc/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling Userland C $<..."
	$(USER_CC) $(USER_CFLAGS) -c $< -o $@

# Userland Libc NASM
$(BUILD_DIR)/userland/libc/%.o: userland/libc/%.s
	@mkdir -p $(dir $@)
	@echo "Assembling Userland S $<..."
	$(USER_AS) -f win32 $< -o $@

# Userland Apps
$(ISO_DIR)/bin/%.exe: userland/bin/%.c $(USER_LIBC_OBJ)
	@mkdir -p $(dir $@)
	@echo "Building Userland App $@..."
	$(USER_CC) $(USER_CFLAGS) $< $(USER_LIBC_OBJ) -o $@ $(USER_LDFLAGS)

# Kernel binary oluştur
$(KERNEL_BIN): $(OBJ)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJ)

# ISO image oluştur
$(ISO_FILE): $(KERNEL_BIN) $(USER_APPS_BIN)
	@echo "Creating ISO image..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/$(KERNEL_BIN)
	@cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
	@echo "Build complete! ISO: $(ISO_FILE)"

# Temizlik
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(KERNEL_BIN) $(ISO_FILE) $(ISO_DIR)
	@echo "Clean complete!"

# Bilgi
info:
	@echo "=== Khazar OS Build Info ==="
	@echo "Sources: $(words $(SRC_C)) C, $(words $(SRC_ASM)) ASM, $(words $(SRC_S)) S"
	@echo "Objects: $(words $(OBJ))"
	@echo "Include path: -Ikernel/include"
	@echo "Compiler: $(CC)"

.PHONY: all clean info

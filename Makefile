# MiniOS bootstrap Makefile (Phase 0-2)

PROJECT := mvos
BUILD_DIR := build
OUTPUT_DIR := $(BUILD_DIR)
ISO_DIR := boot/iso_root
LIMINE_DIR := boot/limine
ISO_NAME := $(PROJECT).iso
KERNEL_ELF := $(OUTPUT_DIR)/$(PROJECT).elf
KERNEL_BIN := $(OUTPUT_DIR)/$(PROJECT).bin

CC := $(shell if command -v x86_64-none-elf-gcc >/dev/null 2>&1; then echo x86_64-none-elf-gcc; elif command -v x86_64-elf-gcc >/dev/null 2>&1; then echo x86_64-elf-gcc; else echo gcc; fi)
LD := $(shell if command -v x86_64-none-elf-ld >/dev/null 2>&1; then echo x86_64-none-elf-ld; elif command -v x86_64-elf-ld >/dev/null 2>&1; then echo x86_64-elf-ld; else echo ld; fi)
AS := $(shell if command -v nasm >/dev/null 2>&1; then echo nasm; else echo nasm; fi)
OBJCOPY := $(shell if command -v x86_64-none-elf-objcopy >/dev/null 2>&1; then echo x86_64-none-elf-objcopy; elif command -v x86_64-elf-objcopy >/dev/null 2>&1; then echo x86_64-elf-objcopy; else echo objcopy; fi)

QEMU := qemu-system-x86_64

INCLUDES := -I$(CURDIR)/kernel/include/mvos -I$(CURDIR)/kernel/include -I$(CURDIR)/libc

CFLAGS := -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -m64 -mno-red-zone -mcmodel=large -O2 -g \
          -Wall -Wextra -Wno-unused-parameter -Werror=implicit-function-declaration $(INCLUDES)
LDFLAGS := -T linker/x86_64.ld -z max-page-size=0x1000 -nostdlib

ifeq ($(PANIC_TEST),1)
  CFLAGS += -DMINIOS_PANIC_TEST=1
endif

ifeq ($(FAULT_TEST),div0)
  CFLAGS += -DMINIOS_FAULT_TEST_DIVIDE_BY_ZERO=1
endif
ifeq ($(FAULT_TEST),opcode)
  CFLAGS += -DMINIOS_FAULT_TEST_INVALID_OPCODE=1
endif
ifeq ($(FAULT_TEST),gpf)
  CFLAGS += -DMINIOS_FAULT_TEST_GP_FAULT=1
endif
ifeq ($(FAULT_TEST),pf)
  CFLAGS += -DMINIOS_FAULT_TEST_PAGE_FAULT=1
endif

C_SRCS := $(wildcard kernel/core/*.c kernel/dev/*.c libc/*.c)
ARCH_SRCS := \
	$(wildcard kernel/arch/x86_64/gdt/*.c) \
	$(wildcard kernel/arch/x86_64/idt/*.c) \
	$(wildcard kernel/arch/x86_64/interrupt/*.c)
ifneq ($(strip $(ARCH_SRCS)),)
C_SRCS += $(ARCH_SRCS)
endif
ASM_SRCS := kernel/arch/x86_64/boot/entry.asm

C_OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,$(OUTPUT_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)

FLAGS_MARK := $(OUTPUT_DIR)/.build-flags

.PHONY: all run debug iso clean test-smoke

all: $(KERNEL_ELF)

$(FLAGS_MARK):
	@mkdir -p $(OUTPUT_DIR)
	@printf '%s\n' "$(CFLAGS)" > $@

$(KERNEL_ELF): $(FLAGS_MARK) $(OBJS)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $(ASM_OBJS) $(C_OBJS)
	@$(OBJCOPY) -O binary $@ $(KERNEL_BIN)

$(OUTPUT_DIR)/%.o: %.c $(FLAGS_MARK)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/kernel/arch/x86_64/boot/entry.o: kernel/arch/x86_64/boot/entry.asm $(FLAGS_MARK)
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	@$(AS) -f elf64 $< -o $@

run: $(KERNEL_ELF)
	bash scripts/run_qemu.sh

debug: $(KERNEL_ELF)
	bash scripts/debug_qemu.sh

iso: $(KERNEL_ELF)
	bash scripts/make_iso.sh

test-smoke: $(KERNEL_ELF)
	bash scripts/test_smoke.sh

clean:
	rm -rf $(OUTPUT_DIR)


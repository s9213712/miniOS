# MiniOS bootstrap Makefile (Phase 3)

PROJECT := mvos
BUILD_DIR := build
OUTPUT_DIR := $(BUILD_DIR)
ISO_DIR := build/iso_root
LIMINE_DIR := boot/limine
ISO_NAME := $(PROJECT).iso
KERNEL_ELF := $(OUTPUT_DIR)/$(PROJECT).elf
KERNEL_BIN := $(OUTPUT_DIR)/$(PROJECT).bin

CC := $(shell if command -v x86_64-none-elf-gcc >/dev/null 2>&1; then echo x86_64-none-elf-gcc; elif command -v x86_64-elf-gcc >/dev/null 2>&1; then echo x86_64-elf-gcc; else echo gcc; fi)
LD := $(shell if command -v x86_64-none-elf-ld >/dev/null 2>&1; then echo x86_64-none-elf-ld; elif command -v x86_64-elf-ld >/dev/null 2>&1; then echo x86_64-elf-ld; else echo ld; fi)
AS := $(shell if command -v nasm >/dev/null 2>&1; then echo nasm; else echo nasm; fi)
CXX := $(shell if command -v x86_64-none-elf-g++ >/dev/null 2>&1; then echo x86_64-none-elf-g++; elif command -v x86_64-elf-g++ >/dev/null 2>&1; then echo x86_64-elf-g++; else echo g++; fi)
OBJCOPY := $(shell if command -v x86_64-none-elf-objcopy >/dev/null 2>&1; then echo x86_64-none-elf-objcopy; elif command -v x86_64-elf-objcopy >/dev/null 2>&1; then echo x86_64-elf-objcopy; else echo objcopy; fi)

QEMU := qemu-system-x86_64

INCLUDES := -I$(CURDIR)/kernel/include/mvos -I$(CURDIR)/kernel/include -I$(CURDIR)/libc

CFLAGS := -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -m64 -mno-red-zone -mcmodel=large -O2 -g -mgeneral-regs-only \
          -Wall -Wextra -Wno-unused-parameter -Werror=implicit-function-declaration $(INCLUDES)
CXXFLAGS := -std=c++17 -ffreestanding -fno-pic -fno-pie -fno-exceptions -fno-rtti -fno-stack-protector \
            -fno-builtin -fno-asynchronous-unwind-tables -m64 -mno-red-zone -mcmodel=large -O2 -g \
            -mgeneral-regs-only -Wall -Wextra -Wno-unused-parameter $(INCLUDES)
LDFLAGS := -T linker/x86_64.ld -z max-page-size=0x1000 -nostdlib
PYTHON := $(shell command -v python3 2>/dev/null || echo python3)

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
ifeq ($(ENABLE_SHELL),1)
  CFLAGS += -DMINIOS_ENABLE_SHELL=1
endif
ifeq ($(PHASE20_DEMO),1)
  CFLAGS += -DMINIOS_PHASE20_DEMO=1
endif

C_SRCS := $(wildcard kernel/core/*.c kernel/dev/*.c kernel/mm/*.c libc/*.c)
ARCH_SRCS := \
	$(wildcard kernel/arch/x86_64/gdt/*.c) \
	$(wildcard kernel/arch/x86_64/idt/*.c) \
	$(wildcard kernel/arch/x86_64/interrupt/*.c)
CXX_SRCS := $(wildcard kernel/core/*.cpp)
ifneq ($(strip $(ARCH_SRCS)),)
C_SRCS += $(ARCH_SRCS)
endif
ASM_SRCS := kernel/arch/x86_64/boot/entry.asm
ASM_SRCS += kernel/arch/x86_64/userproc.asm

C_OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(C_SRCS))
CXX_OBJS := $(patsubst %.cpp,$(OUTPUT_DIR)/%.o,$(CXX_SRCS))
ASM_OBJS := $(patsubst %.asm,$(OUTPUT_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS)

FLAGS_MARK := $(OUTPUT_DIR)/.build-flags

.PHONY: all run debug iso clean test-smoke smoke smoke-full smoke-build smoke-offline prefetch-limine host-programs build-host-programs clean-host-programs test-host-programs refresh-elf-sample

HOST_PROGRAMS_SRC_DIR := samples/user-programs
HOST_PROGRAMS_OUT_DIR := $(OUTPUT_DIR)/host-programs
HOST_PROGRAMS_MANIFEST := $(HOST_PROGRAMS_OUT_DIR)/manifest.json

all: $(KERNEL_ELF)

$(FLAGS_MARK):
	@mkdir -p $(OUTPUT_DIR)
	@printf '%s\n' "$(CFLAGS)" > $@

$(KERNEL_ELF): $(FLAGS_MARK) $(OBJS)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $(ASM_OBJS) $(C_OBJS) $(CXX_OBJS)
	@cp $@ $(KERNEL_BIN)

$(OUTPUT_DIR)/%.o: %.c $(FLAGS_MARK)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/%.o: %.asm $(FLAGS_MARK)
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	@$(AS) -f elf64 $< -o $@

$(OUTPUT_DIR)/%.o: %.cpp $(FLAGS_MARK)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(KERNEL_ELF)
	bash scripts/run_qemu.sh

debug: $(KERNEL_ELF)
	bash scripts/debug_qemu.sh

iso: $(KERNEL_ELF)
	bash scripts/make_iso.sh

test-smoke: $(KERNEL_ELF)
	bash scripts/test_smoke.sh

smoke: $(KERNEL_ELF)
	@echo "[make] running full smoke test"
	@bash scripts/test_smoke.sh

smoke-full: test-smoke

smoke-build: $(KERNEL_ELF)
	SKIP_SMOKE_RUN=1 bash scripts/test_smoke.sh

smoke-offline: $(KERNEL_ELF)
	@if [ -z "$(strip $(LIMINE_LOCAL_DIR)$(LIMINE_CACHE_DIR))" ]; then \
	  echo "[make] smoke-offline requires LIMINE_LOCAL_DIR or LIMINE_CACHE_DIR"; \
	  echo "Example: LIMINE_LOCAL_DIR=/path/to/Limine make smoke-offline"; \
	  exit 1; \
	fi
	@SMOKE_OFFLINE=1 bash scripts/test_smoke.sh

host-programs:
	@if [ ! -d "$(HOST_PROGRAMS_SRC_DIR)" ]; then \
	  echo "[build-host-programs] source directory missing: $(HOST_PROGRAMS_SRC_DIR)"; \
	  exit 1; \
	fi
	@$(PYTHON) scripts/build_user_programs.py \
	  --source-dir "$(HOST_PROGRAMS_SRC_DIR)" \
	  --out-dir "$(HOST_PROGRAMS_OUT_DIR)" \
	  --manifest "$(HOST_PROGRAMS_MANIFEST)" \
	  $(if $(MHOST_VERBOSE),--verbose,)

build-host-programs: host-programs

test-host-programs:
	@bash scripts/test_host_programs.sh

refresh-elf-sample:
	@bash scripts/update_elf_sample_blob.sh

clean-host-programs:
	rm -rf "$(HOST_PROGRAMS_OUT_DIR)"

prefetch-limine: $(KERNEL_ELF)
	@echo "[make] prefetching Limine artifacts into cache"
	@LIMINE_CACHE_DIR="${LIMINE_CACHE_DIR:-$(CURDIR)/.cache/miniOS-limine}" \
	SMOKE_KEEP_LOGS=1 \
	SMOKE_OFFLINE=0 \
	TEST_SMOKE_LOG_DIR="${CURDIR}/.cache/miniOS-smoke-logs" \
	TEST_SMOKE_BASENAME="prefetch" \
	bash scripts/make_iso.sh >/dev/null

clean:
	rm -rf $(OUTPUT_DIR)

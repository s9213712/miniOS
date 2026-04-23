# MinimalOS v1 (Phase 4)

A small educational x86_64 OS prototype that boots with Limine and prints early boot output
via serial.

## Prerequisites

Install:
- `gcc` (or `x86_64-none-elf-gcc`/`x86_64-elf-gcc`)
- `nasm`
- `ld`/`objcopy`
- `make`
- `qemu-system-x86_64`
- `xorriso`
- `git`
- Optionally: `wget`/`curl`

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils gcc make nasm qemu-system-x86 qemu-utils xorriso git curl
```

## Build targets

- `make` builds `build/mvos.elf`
- `make run` boots ISO via QEMU (builds ISO if missing)
- `make debug` starts QEMU in GDB wait mode
- `make iso` creates `build/mvos.iso`
- `make clean` removes `build/`
- `make test-smoke` builds and runs a basic smoke test
- `PANIC_TEST=1 make run` exercises panic path
- `FAULT_TEST=div0|opcode|gpf|pf make run` exercises fault path

### Limine bootstrap

Phase 1 expects Limine assets in `boot/limine/`:
- `limine-bios.sys`
- `limine-bios-cd.bin`
- `limine-uefi-cd.bin`
- `BOOTX64.EFI`
- `limine.conf` (already present as project default)

If missing, `make iso` attempts to clone `v11.x-binary` and copy these assets automatically.

## Expected serial output

On successful boot you should see:
- `[serial] ready`
- `MiniOS Phase 4 bootstrap`
- `boot banner: kernel entering C`
- `hello from kernel`
- `gdt/idt initialized`
- `MiniOS shell (phase 4)`
- `Available commands:`

Shell example:
- `mvos>` prompt
- `help`
- `hello from shell`

Optional panic path:
- `PANIC_TEST=1 make run` prints:
  - `[panic] panic test path enabled`
  - `System halted.`

Optional fault path:
- `FAULT_TEST=div0 make run` prints:
  - `[fault]`
  - `Divide Error`
- `FAULT_TEST=opcode make run` prints:
  - `Invalid Opcode`
- `FAULT_TEST=gpf make run` prints:
  - `General Protection Fault`
- `FAULT_TEST=pf make run` prints:
  - `Page Fault`

## Smoke test

`make test-smoke` (or `./scripts/test_smoke.sh`) verifies the banner and optionally panic/fault marker.

### Shell smoke check

- `make run`
- wait for `mvos>` prompt
- type `help` + Enter
- type `hello` + Enter

Expected output is command feedback in the same serial session.

## Layout

- `boot/limine.conf` and `boot/limine/`
- `kernel/arch/x86_64/boot/entry.asm`
- `kernel/arch/x86_64/gdt/`, `kernel/arch/x86_64/idt/`, `kernel/arch/x86_64/interrupt/`
- `kernel/core/main.c`, `kernel/core/panic.c`, `kernel/core/log.c`, `kernel/core/assert.c`, `kernel/core/shell.c`
- `kernel/dev/serial.c`, `kernel/dev/console.c`, `kernel/dev/keyboard.c`
- `kernel/include/mvos/` headers
- `libc/*.c`
- `linker/x86_64.ld`
- `scripts/{make_iso.sh,run_qemu.sh,debug_qemu.sh,test_smoke.sh}`
- `docs/{architecture.md,roadmap.md,debugging.md}`

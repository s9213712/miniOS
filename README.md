# MinimalOS v1 (Phase 3)

A small educational x86_64 OS prototype that boots with Limine and prints early boot output
via serial.

## Prerequisites

Install:
- `git`
- `make`
- `nasm`
- `binutils` (`ld`/`objcopy`)
- `gcc` (or `x86_64-none-elf-gcc`/`x86_64-elf-gcc`)
- `xorriso`
- `qemu-system-x86`
- `qemu-system-gui` or `qemu-utils` (if your distro splits runtime packages)
- `curl` or `wget` (for fetching Limine when needed)

WSL/Ubuntu example:
```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils gcc make nasm qemu-system-x86 qemu-utils xorriso git curl
```
Debian/Ubuntu package note:
- `qemu-system-x86_64` is the legacy name used in some scripts and docs; if unavailable, use `qemu-system-x86`.
- If `gcc` cross packages are unavailable, the current toolchain falls back to native `gcc` via the Makefile probes.

## Bootstrap/Runtime Requirements

For an offline environment (no GitHub access), add:
- `LIMINE_LOCAL_DIR=/path/to/Limine-bootloader/Limine` when invoking `make test-smoke`.
- The optional `make` step in a local Limine source tree is only needed if you want to build the native `limine` BIOS installer.

## Full test status

Current status:
- Build verification: pass (`SKIP_SMOKE_RUN=1 make test-smoke`)
- Full boot test: environment-dependent; if `make test-smoke` fails to start boot output, check Limine bootstrap state and QEMU compatibility.

```bash
# Online flow (downloads Limine automatically if missing)
make test-smoke

# Offline flow (no network, Limine already downloaded)
LIMINE_LOCAL_DIR=/path/to/limine-bin make test-smoke
```

Required Limine assets (either in `boot/limine/` or `LIMINE_LOCAL_DIR`):
- `limine-bios.sys`
- `limine-bios-cd.bin`
- `limine-uefi-cd.bin`
- `BOOTX64.EFI`

Optional:
- Build native `limine` utility in local clone so BIOS patching can run on Linux:
```bash
cd /tmp/limine-bin
make
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
If network access is blocked, set `LIMINE_LOCAL_DIR` to a local directory containing the files above.

## Expected serial output

On successful boot you should see:
- `[serial] ready`
- `MiniOS Phase 3 bootstrap`
- `boot banner: kernel entering C`
- `hello from kernel`
- `gdt/idt initialized`
- `[pmm] memory map`
- `- base=... len=... type=...` (per memory-map entry)
- `[pmm] selected region ...`
- `free pages: ...`
- `[phase3] memory allocator ready`
- `kmalloc(256) = ...`

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

## Layout

- `boot/limine.conf` and `boot/limine/`
- `kernel/arch/x86_64/boot/entry.asm`
- `kernel/arch/x86_64/gdt/`, `kernel/arch/x86_64/idt/`, `kernel/arch/x86_64/interrupt/`
- `kernel/core/main.c`, `kernel/core/panic.c`, `kernel/core/log.c`, `kernel/core/assert.c`
- `kernel/mm/pmm.c`, `kernel/mm/heap.c`
- `kernel/dev/serial.c`
- `kernel/include/mvos/` headers
- `kernel/include/mvos/pmm.h`, `kernel/include/mvos/heap.h`
- `libc/*.c`
- `linker/x86_64.ld`
- `scripts/{make_iso.sh,run_qemu.sh,debug_qemu.sh,test_smoke.sh}`
- `docs/{architecture.md,roadmap.md,debugging.md}`

# Architecture and phase notes

MiniOS is a small x86_64 educational OS with incremental phases and readable boundaries.

- Target: x86_64
- Bootloader: Limine
- Host dependencies: GNU toolchain + QEMU + xorriso + Git
- Kernel model: monolithic, C-first with tiny x86_64 assembly shim

## Stage status

- **Phase 0**: baseline hardening and smoke diagnosability.
- **Phase 1**: Limine handoff and serial-first startup visibility.
- **Phase 2**: HHDM + memmap parsing + PMM/heap bootstrap.
- **Phase 3**: interrupt/timer foundations.
- **Phase 5**: cooperative scheduler scaffold.
- **Phase 6**: read-only minimal VFS/initramfs-style diagnostics.

## Core layout

- `kernel/arch/x86_64/gdt/` stores `gdt_init()`.
- `kernel/arch/x86_64/idt/` stores `idt_init()` and interrupt descriptor storage.
- `kernel/arch/x86_64/interrupt/` stores timer and fault handlers.
- `kernel/core/main.c` is the first C entry point and coordinates phase-driven init.
- `kernel/mm/pmm.c` provides a simple bump-page allocator from the highest usable memory-map region.
- `kernel/mm/heap.c` exposes `kmalloc` as a first kernel-heap shim.
- `kernel/core/scheduler.c` hosts the cooperative round-robin teaching scheduler.
- `kernel/core/vfs.c` implements read-only initramfs-like file APIs (`open/read/list/close`).
- `kernel/core/{panic.c,assert.c,log.c}` provide serial-backed fail-fast behavior.

Build output layout:
- `build/mvos.elf` (linked kernel image)
- `build/mvos.bin` (flat binary)
- `build/mvos.iso` (Limine-bootable ISO)

Current architecture keeps framebuffer mostly out of core kernel boot validation and emphasizes serial logging.

## Host prerequisites

- `bash` and GNU build tools (`make`, `gcc`, `binutils`, `nasm`).
- `xorriso`.
- Emulator (`qemu-system-x86` or `qemu-system-x86_64`).
- `git`, and `curl`/`wget` for Limine bootstrap download.
- `./scripts/setup-dev.sh` to validate and optionally install requirements.
- Ubuntu/WSL may not expose `gcc-x86-64-elf` packages; native `gcc/ld/objcopy` fallback is supported if elf-prefixed cross tools are absent.
- Optional cache path: `LIMINE_CACHE_DIR` (default `.cache/miniOS-limine`).
- `make prefetch-limine` for explicit local cache warm-up.

Quick check:
```bash
./scripts/setup-dev.sh
```

Expected host command sequence:
```bash
make
make iso
make smoke-offline
make test-smoke
```

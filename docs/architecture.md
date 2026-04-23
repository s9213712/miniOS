# Architecture and phase notes

MiniOS starts with a minimal x86_64 boot-capable skeleton and keeps all boot plumbing intentionally small.

- Target: x86_64
- Bootloader: Limine
- Host dependencies: GNU toolchain + QEMU + xorriso + Git
- Kernel model: monolithic, C-first with tiny x86_64 assembly shim

## Stage status

- **Phase 0**: scaffolding, scripts, docs, and baseline build/run/debug pipeline.
- **Phase 1**: Limine boot handoff, minimal serial logging, panic halt path.
- **Phase 2**: GDT/IDT initialization and fault visibility.
- **Phase 3**: HHDM + simple physical memory map parsing + page allocator/bump heap.

## Core layout

- `kernel/arch/x86_64/gdt/` stores `gdt_init()`.
- `kernel/arch/x86_64/idt/` stores `idt_init()` and interrupt descriptor storage.
- `kernel/arch/x86_64/interrupt/` stores exception handlers.
- `kernel/core/main.c` is the first C entry point and the phase 2 fault test hooks.
- `kernel/mm/pmm.c` provides a simple bump-page allocator from the highest usable memory-map region.
- `kernel/mm/heap.c` exposes `kmalloc` as a first kernel-heap shim.
- `kernel/core/{panic.c,assert.c,log.c}` provide serial-backed fail-fast behavior.

Build output layout:
- `build/mvos.elf` (linked kernel image)
- `build/mvos.bin` (flat binary)
- `build/mvos.iso` (Limine-bootable ISO)

Current architecture intentionally excludes framebuffer, scheduler, userspace, network, SMP, and VFS.

## Host prerequisites

- `bash` and GNU build tools (`make`, `gcc`, `binutils`, `nasm`).
- `xorriso`.
- Emulator (`qemu-system-x86` or `qemu-system-x86_64`).
- `git`, and `curl`/`wget` for Limine bootstrap download.
- `./scripts/setup-dev.sh` to validate and optionally install requirements.

Quick check:
```bash
./scripts/setup-dev.sh
```

Expected host command sequence:
```bash
make
make iso
make test-smoke
```

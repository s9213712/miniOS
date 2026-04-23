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

## Core layout

- `kernel/arch/x86_64/gdt/` stores `gdt_init()`.
- `kernel/arch/x86_64/idt/` stores `idt_init()` and interrupt descriptor storage.
- `kernel/arch/x86_64/interrupt/` stores exception handlers.
- `kernel/core/main.c` is the first C entry point and the phase 2 fault test hooks.
- `kernel/core/{panic.c,assert.c,log.c}` provide serial-backed fail-fast behavior.

Build output layout:
- `build/mvos.elf` (linked kernel image)
- `build/mvos.bin` (flat binary)
- `build/mvos.iso` (Limine-bootable ISO)

Current architecture intentionally excludes framebuffer, scheduler, userspace, network, SMP, and VFS.

# Architecture and phase notes

MiniOS keeps a small monolithic x86_64 kernel for learning. The goal is to keep
the boot path fully deterministic and debug-friendly while adding one phase at a time.

- Target: x86_64
- Bootloader: Limine
- Host dependencies: GNU toolchain + QEMU + xorriso + Git
- Kernel model: monolithic kernel with C-first code and tiny x86_64 assembly entry

## Stage status

- **Phase 0**: repository scaffold, scripts, docs, and toolchain path.
- **Phase 1**: Limine boot handoff, serial banner and panic path.
- **Phase 2**: GDT/IDT initialization and CPU fault logging.
- **Phase 4**: serial-first console abstraction, keyboard input, and first shell
  command loop.

## Core layout

- `kernel/arch/x86_64/boot/entry.asm` sets a minimal kernel stack and jumps to `kmain`.
- `kernel/core/main.c` is the first C entry body and now moves into shell mode.
- `kernel/core/log.c` uses a console abstraction for output.
- `kernel/core/shell.c` provides a tiny interactive REPL (`mvos>`).
- `kernel/core/{panic.c,assert.c}` implement fail-fast halt behavior.
- `kernel/dev/serial.c` initializes UART and provides byte-level writes.
- `kernel/dev/console.c` centralises output and supports serial vs. framebuffer target selection.
- `kernel/dev/keyboard.c` provides polling PS/2 scancode-to-ASCII input.
- `kernel/arch/x86_64/{gdt,idt,interrupt}` hold bootstrap CPU setup and handlers.
- `kernel/include/mvos/{console.h,keyboard.h,shell.h}` provide public phase-4 interfaces.
- `libc/` keeps minimal C helper functions.

Build output:
- `build/mvos.elf` (linked kernel image)
- `build/mvos.bin` (raw binary)
- `build/mvos.iso` (Limine-bootable ISO)

Current architecture intentionally excludes framebuffer rendering, scheduler, SMP,
VFS, userspace, networking, and network stack in this phase.

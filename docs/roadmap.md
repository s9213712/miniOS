# Roadmap

## Current completion
- Phase 0: Repository scaffold, build scripts, docs, and conventions
- Phase 1: Limine boot handoff, linker layout, serial banner/hello, panic path
- Phase 2: GDT/IDT setup and first fault-path logging
- Phase 4: Console abstraction, keyboard input, first shell command loop

## Next phases

### Phase 3 (deferred)
- Memory map helpers and allocator foundation

### Phase 4
- Serial/Framebuffer console abstraction (serial live now, framebuffer wiring point set)
- Keyboard input, one-line input buffer, and backspace/enter handling
- First shell:
  - `help`
  - `hello`
  - `echo`
  - `version`
  - `panic`
  - `quit`

### Phase 5
- Timer source and tick counter
- Basic diagnostics command (for example `ticks`)

### Phase 6
- Simple memory allocator integration
- Heap/page-usage visibility


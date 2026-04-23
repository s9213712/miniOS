# Roadmap

## Current completion
- Phase 0: Repository scaffold, Makefile targets, scripts, docs and repository conventions
- Phase 1: Limine boot handoff, linker script, serial banner/hello, panic path
- Phase 2: GDT/IDT setup and first fault path logging
- Phase 3: HHDM + page allocator + basic bump heap

## Next phases

### Phase 2
- Interrupt/exception setup (GDT/IDT)
- Fault logging to serial
- `FAULT_TEST=div0|opcode|gpf|pf` smoke path

### Phase 3
- (completed) HHDM + basic frame allocator

### Phase 4
- Serial/Framebuffer console abstraction
- Keyboard input
- First shell commands

### Phase 5
- Timer source and tick counter
- `ticks` style diagnostics

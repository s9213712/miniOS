# Smoke tests (Phase 3)

The current smoke tests are script-driven and rely on serial output.

- `hello from kernel` should be present in a normal boot.
- `[pmm] memory map` and `[phase3] memory allocator ready` should be present in normal boot.
- With `PANIC_TEST=1`, kernel should hit panic path and emit `[panic]`.
- With `FAULT_TEST=div0|opcode|gpf|pf`, kernel should hit fault path and emit `[fault]`.

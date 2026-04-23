# Smoke tests (Phase 4)

The current smoke tests are script-driven and rely on serial output.

- `hello from kernel` should be present in a normal boot.
- With `PANIC_TEST=1`, kernel should hit panic path and emit `[panic]`.
- With `FAULT_TEST=div0|opcode|gpf|pf`, kernel should hit fault path and emit `[fault]`.

Phase 4 shell path (manual):
- boot to serial prompt `mvos>`
- send `help` and verify command list output

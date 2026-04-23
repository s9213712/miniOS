# Smoke Tests (Phase 22)

The current smoke tests are script-driven and rely on serial output.

- `hello from kernel` should be present in a normal boot.
- `[pmm] memory map` and `[phase3] memory allocator ready` should be present in normal boot.
- `[vfs] listing /boot/init/` and `[phase5] scheduler ready` should appear after phase-6/scheduler checks.
- With `PANIC_TEST=1`, kernel should hit panic path and emit `[panic]`.
- With `FAULT_TEST=div0|opcode|gpf|pf`, kernel should hit fault path and emit `[fault]`.
- `run cpp` is available in the shell as a userapp demo, and Phase 22 doc trail is updated in this docs pack.

Commands:
```bash
# dependency check
./scripts/setup-dev.sh

# build check only
SKIP_SMOKE_RUN=1 make smoke-build
SKIP_SMOKE_RUN=1 make test-smoke

# full smoke
make test-smoke
make smoke
make smoke-full

# offline smoke (local Limine)
LIMINE_LOCAL_DIR=/path/to/limine-bin make smoke-offline

# quick pass command
TEST_SMOKE_BASENAME=smoke-quick QEMU_TIMEOUT=8 LIMINE_LOCAL_DIR=/path/to/limine-bin make smoke-offline

# prefetch cache only
make prefetch-limine

# keep logs
TEST_SMOKE_LOG_DIR=./artifacts TEST_SMOKE_BASENAME=smoke-run SMOKE_KEEP_LOGS=1 make smoke
```

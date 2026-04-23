# Debugging guide (Phase 3)

## Serial output (default path)
- All kernel logs print on UART COM1.
- QEMU is launched with `-serial stdio -monitor none`, so logs appear directly in the terminal.

## Phase 3 memory checks
- `[pmm] memory map` marks the memory map dump loop.
- `[pmm] selected region` shows which usable block is used for allocator bootstrap.
- `free pages:` confirms allocator availability after PMM init.
- `pmm_allocate_pages(1)` and `kmalloc(256)` logs confirm allocator path.

## Panic path
- Call `panic("message")` to print an explicit marker and halt in an infinite `cli; hlt` loop.
- Assertions route to `kassert_fail(...)` and use the same halt path.

## Fault path
- Exception handlers for divide-by-zero, invalid opcode, general-protection, and page fault are installed by IDT setup in phase 2.
- When a fault occurs, the handler prints:
  - `[fault]`
  - fault name
  - vector number
  - error code (if present)
  - RIP
- The handler then calls `panic()` and halts.
- Test commands:
  - `FAULT_TEST=div0 make run`
  - `FAULT_TEST=opcode make run`
  - `FAULT_TEST=gpf make run`
  - `FAULT_TEST=pf make run`

## Debug mode
- `make debug` starts QEMU with:
  - `-S -s` (GDB waits on TCP 1234)
  - `-serial stdio -monitor none`

A typical workflow:
```bash
make debug
(gdb) target remote :1234
(gdb) break kmain
```

## Boot verification workflow

To verify full boot path:
```bash
./scripts/setup-dev.sh
make clean
SKIP_SMOKE_RUN=1 make test-smoke
LIMINE_LOCAL_DIR=/path/to/limine-bin make test-smoke
```

If boot output still does not appear, common causes are:
- Limine bootstrap files not copied into `boot/limine/` correctly
- BIOS/UEFI image mismatch on older QEMU builds
- `boot` entry in ISO not found due boot image layout changes

Check the last lines in `/tmp/make_iso.log` for Limine patch/install status:
```bash
make iso 2>&1 | tee /tmp/make_iso.log
tail -n 40 /tmp/make_iso.log
```

Optional environment flags:
- `LIMINE_LOCAL_DIR` points to a local Limine directory when network fetch is blocked.
- `QEMU_MACHINE`, `QEMU_CPU`, `QEMU_MEM` can be passed for emulator tuning.
- `QEMU_TIMEOUT` can be set in seconds for smoke tests.

Example:
```bash
QEMU_MACHINE=q35 QEMU_CPU=qemu64 QEMU_MEM=1G QEMU_TIMEOUT=10 make test-smoke
```

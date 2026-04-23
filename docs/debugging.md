# Debugging guide (Phase 2)

## Serial output (default path)
- All kernel logs print on UART COM1.
- QEMU is launched with `-serial stdio`, so logs appear directly in the terminal.

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

- 觸發測試建議：
  - `FAULT_TEST=div0 make run`
  - `FAULT_TEST=opcode make run`
  - `FAULT_TEST=gpf make run`
  - `FAULT_TEST=pf make run`

## Debug mode
- `make debug` starts QEMU with:
  - `-S -s` (GDB waits on TCP 1234)
  - `-serial stdio`

A typical workflow:
```bash
make debug
(gdb) target remote :1234
(gdb) break kmain
```

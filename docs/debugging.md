# Debugging guide (Phase 4)

## Serial output (default path)
- All kernel logs print on UART COM1.
- QEMU is launched with `-serial stdio`, so logs appear directly in the terminal.

## Panic path
- Call `panic("message")` to print a marker and halt in an infinite `cli; hlt` loop.
- Assertions route to `kassert_fail(...)` and use the same halt path.

## Fault path
- Exception handlers for divide-by-zero, invalid opcode, general-protection, and page fault are installed by IDT setup.
- On a fault, the handler logs:
  - `[fault]`
  - fault name
  - vector
  - optional error code / RIP
  - then calls `panic()` and halts.

## Interactive shell checks (Phase 4)
- After boot, you should see:
  - `MiniOS shell (phase 4)`
  - `mvos>` prompt
- Try commands:
  - `help`
  - `hello`
  - `echo hi`
  - `version`
- `panic` should route to the panic/halt path.

## Debug mode
- `make debug` starts QEMU with:
  - `-S -s` (GDB waits on TCP 1234)
  - `-serial stdio`

Typical workflow:
```bash
make debug
(gdb) target remote :1234
(gdb) break kmain
```

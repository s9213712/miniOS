# MinimalOS v1 (Phase 3)

A small educational x86_64 OS prototype that boots with Limine and prints early boot output
via serial.

## What works now

- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 會成功執行 smoke 檢查並輸出 `hello from kernel`。
- `make` 會輸出 ELF 格式的 `build/mvos.elf`，`build/mvos.bin` 保留為同一份 ELF 檔便於 limine 載入。
- `.requests` section 與 Limine 請求標記已放在 kernel 早期段落，entry 已做 serial 初始化與早期輸出。

## Prerequisites

Use the setup helper first:
```bash
./scripts/setup-dev.sh
```

If dependencies are missing, `scripts/setup-dev.sh --install` (Debian/Ubuntu only) will install them.

### Common install command (WSL/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils gcc make nasm qemu-system-x86 qemu-system-gui xorriso git curl wget
```

Some Ubuntu/WSL repositories do not provide `gcc-x86-64-elf` or `binutils-x86-64-elf` package names. This project falls back to native `gcc/ld/objcopy` automatically if x86_64 elf-prefixed cross tools are not installed:
```bash
./scripts/setup-dev.sh --install
```

Required host commands:
- `git`
- `bash` + `make`
- `nasm`
- `gcc`/`ld`/`objcopy` (or cross toolchain `x86_64-*-elf-*`)
- `xorriso`
- `qemu-system-x86`/`qemu-system-x86_64`
- `curl` or `wget`
- `python3` (for some smoke debug helpers)

If your package source has no dedicated ELF cross tools, native `gcc/ld/objcopy` are used automatically by Make.

Recommended first-run bootstrap command:
```bash
./scripts/setup-dev.sh --install
```

Minimal required check:
```bash
make
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

### Kernel binary format note

`build/mvos.bin` is intentionally built as ELF, not raw. Do not convert it with `objcopy -O binary` unless your bootloader is explicitly raw-compatible.

## Quick flow (3 steps)

```bash
make
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
LIMINE_LOCAL_DIR=/tmp/limine-bin make run
```

If `LIMINE_LOCAL_DIR` points to your local Limine binary tree, smoke and run use that path for both build and boot.

## Bootstrap/Runtime Requirements

For an offline environment (no GitHub access), add:
- `LIMINE_LOCAL_DIR=/path/to/Limine-bootloader/Limine` when invoking `make test-smoke`.
- The optional `make` step in a local Limine source tree is only needed if you want to build the native `limine` BIOS installer.

## Full test status

- Build verification: pass (`SKIP_SMOKE_RUN=1 make test-smoke`)
- Full boot test: run `make test-smoke` and check serial markers below; if this fails, inspect `make iso`/QEMU logs and `bootstrap` paths in `make_iso.sh`.

```bash
# Online flow (downloads Limine automatically if missing)
make test-smoke

# Offline flow (no network, Limine already downloaded)
LIMINE_LOCAL_DIR=/path/to/limine-bin make test-smoke
```

Required Limine assets (either in `boot/limine/` or `LIMINE_LOCAL_DIR`):
- `limine-bios.sys`
- `limine-bios-cd.bin`
- `limine-uefi-cd.bin`
- `BOOTX64.EFI`

Optional:
- Build native `limine` utility in local clone so BIOS patching can run on Linux:
```bash
cd /tmp/limine-bin
make
```

## Build targets

- `make` builds `build/mvos.elf`
- `make run` boots ISO via QEMU (builds ISO if missing)
- `make debug` starts QEMU in GDB wait mode
- `make iso` creates `build/mvos.iso`
- `make clean` removes `build/`
- `make test-smoke` builds and runs a basic smoke test
- `make smoke` / `make smoke-full` runs full boot smoke test (alias)
- `make smoke-build` builds artifacts and validates without QEMU run
- `make smoke-offline` runs smoke using `LIMINE_LOCAL_DIR`/`LIMINE_CACHE_DIR`
- `make prefetch-limine` warms local Limine cache (offline bootstrap helper)
- `PANIC_TEST=1 make run` exercises panic path
- `FAULT_TEST=div0|opcode|gpf|pf make run` exercises fault path

### Limine bootstrap

`make iso` checks assets in this order:
- `boot/limine/` if already populated
- `LIMINE_LOCAL_DIR` if provided
- `LIMINE_CACHE_DIR` if present
- online download of `v11.x-binary` if network is available

A typical offline setup uses:
- `boot/limine` for committed default files
- `boot/limine.conf` for config

Phase 1 expects Limine assets in `boot/limine/`:
- `limine-bios.sys`
- `limine-bios-cd.bin`
- `limine-uefi-cd.bin`
- `BOOTX64.EFI`
- `limine.conf` (already present as project default)

If missing, `make iso` attempts to clone `v11.x-binary` and copy these assets automatically.
If network access is blocked, set `LIMINE_LOCAL_DIR` or `LIMINE_CACHE_DIR` to a local directory containing the files above.
The `.github/workflows/ci.yml` job uses `LIMINE_CACHE_DIR` and retries `make test-smoke` once on failure.

The offline smoke path is:
```bash
LIMINE_LOCAL_DIR=/path/to/limine-bin make smoke-offline
```

## Expected serial output

On successful boot you should see:
- `[serial] ready`
- `MiniOS Phase 3 bootstrap`
- `boot banner: kernel entering C`
- `hello from kernel`
- `gdt/idt initialized`
- `[pmm] memory map`
- `- base=... len=... type=...` (per memory-map entry)
- `[pmm] selected region ...`
- `free pages: ...`
- `[phase3] memory allocator ready`
- `kmalloc(256) = ...`

Boot sequence to look for during smoke:
- `Booting from DVD/CD...`
- `hello from kernel`
- `[pmm] selected region ...`

Optional panic path:
- `PANIC_TEST=1 make run` prints `[panic] panic test path enabled` and `System halted.`

Optional fault path:
- `FAULT_TEST=div0 make run` prints `[fault]` and `Divide Error`.
- `FAULT_TEST=opcode make run` prints `Invalid Opcode`.
- `FAULT_TEST=gpf make run` prints `General Protection Fault`.
- `FAULT_TEST=pf make run` prints `Page Fault`.

## Smoke test

`make test-smoke` (or `./scripts/test_smoke.sh`) verifies boot output and optional fault/panic markers.

```bash
# build-only check (fast)
SKIP_SMOKE_RUN=1 make test-smoke
make smoke-build

# full boot smoke
make test-smoke
make smoke
make smoke-full
TEST_SMOKE_LOG_DIR=./artifacts TEST_SMOKE_BASENAME=smoke-run SMOKE_KEEP_LOGS=1 make smoke

# offline build + boot
LIMINE_LOCAL_DIR=/path/to/limine-bin make test-smoke
make smoke-offline
LIMINE_LOCAL_DIR=/path/to/limine-bin make smoke-offline
LIMINE_CACHE_DIR=./boot/limine make smoke-offline

# cache-only smoke bootstrap
make prefetch-limine
```

Smoke helper (recommended for diagnosis):
```bash
TEST_SMOKE_LOG_DIR=./artifacts TEST_SMOKE_BASENAME=smoke-run SMOKE_KEEP_LOGS=1 SMOKE_OFFLINE=1 LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

The test will verify:
- `hello from kernel`
- `[pmm] memory map`
- optional panic/fault markers when enabled
- explicit failure reasons instead of only timeout.

## Layout

- `boot/limine.conf` and `boot/limine/`
- `kernel/arch/x86_64/boot/entry.asm`
- `kernel/arch/x86_64/gdt/`, `kernel/arch/x86_64/idt/`, `kernel/arch/x86_64/interrupt/`
- `kernel/core/main.c`, `kernel/core/panic.c`, `kernel/core/log.c`, `kernel/core/assert.c`
- `kernel/mm/pmm.c`, `kernel/mm/heap.c`
- `kernel/dev/serial.c`
- `kernel/include/mvos/` headers
- `kernel/include/mvos/pmm.h`, `kernel/include/mvos/heap.h`
- `libc/*.c`
- `linker/x86_64.ld`
- `scripts/{make_iso.sh,run_qemu.sh,debug_qemu.sh,test_smoke.sh}`
- `docs/{architecture.md,roadmap.md,debugging.md}`

## Troubleshooting

Common causes:
- Limine config not using `KERNEL_PATH=boot:///boot/mvos.bin`.
- offline bootstrap missing Limine assets.
- stale boot artifacts in `build/` after config changes.

Useful checks:
- `readelf -l build/mvos.elf` should show `VirtAddr` in high-half for PHDR load.
- `file build/mvos.bin` should show `ELF 64-bit`.
- `grep -n "PROTOCOL\\|KERNEL_PATH\\|SERIAL" boot/limine.conf` should be valid in one of the accepted formats.

Reset and retest:
```bash
make clean
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

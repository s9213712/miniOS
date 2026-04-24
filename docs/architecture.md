# Architecture and phase notes

MiniOS is a small x86_64 educational OS with incremental phases and readable boundaries.

- Target: x86_64
- Bootloader: Limine
- Host dependencies: GNU toolchain + QEMU + xorriso + Git
- Kernel model: monolithic, C-first with tiny x86_64 assembly shim

## Stage status

- **Phase 0**: baseline hardening and smoke diagnosability.
- **Phase 1**: Limine handoff and serial-first startup visibility.
- **Phase 2**: HHDM + memmap parsing + PMM/heap bootstrap.
- **Phase 3**: interrupt/timer foundations.
- **Phase 5**: cooperative scheduler scaffold.
- **Phase 6**: read-only minimal VFS/initramfs-style diagnostics.
- **Phase 9**: framebuffer request diagnostics and VGA text mirror hookup.
- **Phase 10**: framebuffer boot-window demo rendering.
- **Phase 11**: optional interactive shell path (`ENABLE_SHELL=1`) without changing default boot path.
- **Phase 12**: shell-driven GUI redraw command (`gui`) for manual verification.
- **Phase 13**: first GUI app command (`app`) demonstrating an independent drawing path inside framebuffer.
- **Phase 14**: `app alt` variant for a second window demo to prepare extensible GUI app model.
- **Phase 15**: `app` control subcommands (`list`, `status`) for lightweight GUI app management metadata.
- **Phase 16**: explicit `app launch <name>` syntax to formalize app command routing.
- **Phase 17**: add `app info <name>` to expose app metadata and verify discoverability by command path.
- **Phase 18**: add `tasks` shell command to expose scheduler task names and run counters without changing core boot or smoke behavior.
- **Phase 21**: add shell command visibility for initfs content (`ls` + `cat`) to support user-level file-system introspection.
- **Phase 22**: add a C++-compiled userapp path demonstration (`run cpp`) as a minimum non-Python basic program sample. (Completed; executed as kernel-mode fallback in this stage.)
- **Phase 26**: add a host-side `make host-programs` build path for `.c/.cpp` samples so users can compile C/C++ program outputs from the repository immediately.
- **Phase 27**: add in-OS capability disclosure (`cap` / `capabilities`) to make runtime scope explicit.
- **Phase 30**: add Linux ABI preview syscall path (`write/getpid/exit`) for user-mode teaching experiments.
- **Phase 31**: expand Linux ABI preview syscall coverage (`writev/brk/uname/gettid/set_tid_address/arch_prctl/exit_group`) and add fallback probe output.
- **Phase 32**: add ELF64 metadata inspection path (`run elf-inspect`) plus reproducible sample refresh (`make refresh-elf-sample`).
- **Phase 33**: add ELF sample regression test path (`make test-elf-sample`) to keep refresh output verifiable.
- **Phase 34**: add writable `/tmp` VFS overlay plus shell file mutation commands (`write/append/touch/rm`).
- **Phase 35**: add scheduler task control APIs and shell control command (`task start/stop/reset/list`).
- **Phase 36**: add VMM scaffold (`map/unmap/regions`) and route Linux ABI `brk` through shared VMM state.
- **Phase 37**: add user image loader skeleton (`run elf-load`) to map embedded ELF `PT_LOAD` layout into VMM metadata.

## Core layout

- `kernel/arch/x86_64/gdt/` stores `gdt_init()`.
- `kernel/arch/x86_64/idt/` stores `idt_init()` and interrupt descriptor storage.
- `kernel/arch/x86_64/interrupt/` stores timer and fault handlers.
- `kernel/core/main.c` is the first C entry point and coordinates phase-driven init.
- `kernel/mm/pmm.c` provides a simple bump-page allocator from the highest usable memory-map region.
- `kernel/mm/heap.c` exposes `kmalloc` as a first kernel-heap shim.
- `kernel/core/scheduler.c` hosts the cooperative round-robin teaching scheduler.
- `kernel/core/vfs.c` implements read-only initramfs-like file APIs (`open/read/list/close`).
- `kernel/core/{panic.c,assert.c,log.c}` provide serial-backed fail-fast behavior.

Build output layout:
- `build/mvos.elf` (linked kernel image)
- `build/mvos.bin` (flat binary)
- `build/mvos.iso` (Limine-bootable ISO)

Current architecture keeps serial as primary boot diagnostics, with framebuffer request metadata logged in startup, optional VGA text mirror fallback, and optional `shell` entry behind `ENABLE_SHELL=1`.

## Host prerequisites

- `bash` and GNU build tools (`make`, `gcc`, `binutils`, `nasm`).
- `xorriso`.
- Emulator (`qemu-system-x86` or `qemu-system-x86_64`).
- `git`, and `curl`/`wget` for Limine bootstrap download.
- `./scripts/setup-dev.sh` to validate and optionally install requirements.
- Ubuntu/WSL may not expose `gcc-x86-64-elf` packages; native `gcc/ld/objcopy` fallback is supported if elf-prefixed cross tools are absent.
- Optional cache path: `LIMINE_CACHE_DIR` (default `.cache/miniOS-limine`).
- `make prefetch-limine` for explicit local cache warm-up.

Current user-visible runtime limit:
- `run cpp` demonstrates C++ build flow, while miniOS still does not include a Python interpreter for executing `.py` programs inside the VM.
- `make host-programs` compiles sample C/C++ sources (`samples/user-programs`) into `build/host-programs`; these are currently host-side artifacts and are not yet runnable inside miniOS.
- `run linux-abi` verifies a Linux syscall-number-compatible preview path (currently 1/12/20/39/60/63/158/186/218/231), not full POSIX process compatibility.
- `run elf-inspect` validates an embedded Linux user ELF sample and prints entry/program-header/load-range metadata.
- `run elf-load` builds a layout-only mapped image plan in VMM metadata for the embedded ELF sample and reports mapped entry/range.
- `make refresh-elf-sample` regenerates `kernel/core/elf_sample_blob.c` from `samples/linux-user/hello_linux_tiny.S`.
- `make test-elf-sample` re-runs sample generation and checks blob contract (ELF magic, symbols, minimum byte count).
- `make test-vfs-rw` compiles and runs a host regression for writable VFS behavior (`/tmp` write/append/remove/list).
- `make test-scheduler-ctl` compiles and runs a host regression for scheduler task state control and run-counter reset behavior.
- `make test-vmm-basic` compiles and runs a host regression for VMM map/unmap validation and user-brk bounds.
- `make test-userimg-loader` compiles and runs a host regression for loader layout mapping idempotence and VMM tag coverage.
- `cap` / `capabilities` shell command reports:
  - user app execution mode coverage (kernel vs user placeholder),
  - host-program build workflow status,
  - and unsupported Linux-native workloads (e.g. transmission/htop/nano) for roadmap clarity.

Quick check:
```bash
./scripts/setup-dev.sh
```

Expected host command sequence:
```bash
make
make iso
make smoke-offline
make test-smoke
```

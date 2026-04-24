# Changelog

## Phase 44 – Native Toolchain Filesystem Syscalls

### Added
- Added a first userspace file-descriptor bridge from Linux x86-64 syscalls to the miniOS VFS.
- Added syscall coverage for `read`, `close`, `fstat`, `openat`, and `newfstatat`.
- Added a VFS-backed ELF load path via `userimg_prepare_image(image, len, ...)`.
- Exposed the tiny Linux ELF as `/boot/init/hello_linux_tiny` in initfs.
- Added minimal Linux `mmap`/`munmap` support for anonymous private user mappings.
- Added Linux `lseek` support for VFS-backed user file descriptors.
- Added VMM-backed user range validation and routed syscall pointer reads/writes through checked copy helpers in strict userspace mode.
- Added minimal runtime syscalls: `getcwd`, `access`, `faccessat`, `clock_gettime`, and `getrandom`.
- Added minimal `mprotect` support for exact VMM ranges and synchronized runtime PTE write permissions.
- Added readonly/private VFS file-backed `mmap` support by copying fd contents into backed user pages.
- Added VFS-backed `pread64` support for offset-based reads without mutating fd cursor state.
- Added minimal `fcntl` and `ioctl` syscall handling for common runtime fd capability probes.
- Extended the tiny Linux ELF demo to open and read `/boot/init/readme.txt` through real user-mode syscalls.
- Extended execve smoke checks to require the initfs read marker from userspace.
- Added host regression coverage for shell command parsing and CI coverage for all host regressions.

### Validation
- `make host-regressions`
- `make test-userimg-loader`
- `make test-elf-sample`
- `make smoke-build`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

### Fixed
- Fixed shell command dispatch for commands with arguments by sharing a tested parser between shell runtime and host tests.
- Fixed stale dynamic VFS handles after remove/reuse and initialized static file checksums during `vfs_list()`.
- Preserved callee-saved registers across the execve trampoline so user programs cannot corrupt the kernel C continuation.
- Restored interrupts before syscall exit continuations and cleared the long-mode L bit from GDT data descriptors.
- Honored `TEST_SMOKE_BASENAME` in smoke logs and kept ELF/Limine metadata checks active under `SKIP_SMOKE_RUN=1`.
- Hardened PMM allocation bounds and selected the largest usable memory-map region.
- Guarded VMM alignment overflow and made user image region rollback include the just-mapped region on backing failures.
- Prevented VMM from promoting existing supervisor page-table branches to user/writable mappings.
- Cleared user leaf PTEs during `vmm_unmap_range()` and backed runtime `brk` heap pages.
- Changed `execve()` to load ELF bytes from miniOS VFS instead of directly using the embedded sample global.

## Phase 43 – x86-64 `syscall` Userspace Entry

### Added
- Added long-mode `syscall` MSR setup (`EFER.SCE`, `STAR`, `LSTAR`, `SFMASK`) during kernel init.
- Extended syscall dispatch plumbing to carry the full Linux x86-64 argument set (`rdi/rsi/rdx/r10/r8/r9`).
- Added `syscall_entry` in `kernel/arch/x86_64/userproc.asm`:
  - switches from user RSP to the miniOS syscall kernel stack
  - builds an `iretq` return frame from `RCX/R11/RSP`
  - dispatches Linux x86-64 syscall numbers through `userproc_dispatch`
  - resumes the supervisor continuation on `exit`/`exit_group`
- Extended `test-elf-sample` to reject `int $0x80` in the tiny Linux sample.
- Extended execve smoke checks to require the Phase 43 uname release marker.

### Changed
- Updated built-in userapp stubs and `samples/linux-user/hello_linux_tiny.S` to use the x86-64 `syscall` instruction.
- Refreshed the embedded ELF sample blob.
- Updated Stage 3 docs and version strings to Phase 43.

### Validation
- `make test-elf-sample`
- `make test-userimg-loader`
- `make smoke-build`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 42 – Minimal Execve Userspace Run

### Added
- Added a real tiny static ELF `execve` execution path:
  - maps embedded ELF segments and user stack to backed user pages
  - copies load segments and prepared `argc/argv/envp/auxv` stack into user memory
  - enters ring3 with Linux-style initial registers
  - returns to kernel after `exit_group`
- Added an assembly execve trampoline that owns the kernel continuation return address.
- Added a dedicated execve syscall kernel stack by temporarily switching TSS `rsp0`, avoiding live C stack corruption on nested user syscalls.
- Added `EXECVE_DEMO=1` boot probe wiring for serial-smoke verification.

### Changed
- Updated the embedded Linux tiny sample to use the then-supported `int 0x80` teaching path.
- Updated Stage 3 docs and version strings to Phase 42.

### Validation
- `make test-userimg-loader`
- `make test-elf-sample`
- `make smoke-build`
- `make host-programs`
- `make test-host-programs`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `EXECVE_DEMO=1 LIMINE_LOCAL_DIR=/tmp/limine-bin make -B smoke-offline`

## Phase 41 – Execve Scaffold Syscall Path

### Added
- Added Linux syscall `execve` (`rax=59`) scaffold handling in `kernel/core/userproc.c`.
- Added minimal `execve` flow:
  - validate `path/argv/envp` pointers and bounded string copies
  - route known path (`hello_linux_tiny`) to embedded ELF sample load
  - run existing loader + handoff dry-run + exec stack preparation
  - on success, emit diagnostic and return user-exit signal (`1`) to syscall trampoline
- Extended Linux ABI probe to include an `execve` scaffold call.
- Extended host regression `test_userimg_loader` to validate:
  - successful `execve` scaffold dispatch path
  - failure paths (`ENOENT`, `EFAULT`) and running-state behavior
- Updated stage/version strings to Phase 41 (`shell version`, `uname.release`, `uname.version`).

### Validation
- `make test-userimg-loader`
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make test-elf-sample`
- `make test-host-programs`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 40 – Exec Stack Scaffold

### Added
- Added `userproc_prepare_exec_stack()` and `userproc_stack_result_name()` in `kernel/core/userproc.c`.
- Added initial userspace stack layout scaffold for `argc/argv/envp/auxv` (AT_NULL pair), writing into a stack buffer with user virtual addresses.
- Wired `run elf-load` to execute and print exec stack preparation results using loader-generated stack range.
- Extended `test_userimg_loader` to verify:
  - exec stack success path
  - argv/envp payload and null terminators
  - auxv terminator pair
  - expected failures (tiny stack overflow, null argv with argc>0)
- Added long-term parity target document: `docs/linux-parity-goals.md`.

### Validation
- `make test-userimg-loader`
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make test-elf-sample`
- `make test-host-programs`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 39 – Handoff Dry-run Wiring

### Added
- Added `userproc_handoff_dry_run(entry, stack_top)` and `userproc_handoff_result_name()` in `kernel/core/userproc.c`.
- Wired `run elf-load` to execute handoff dry-run using loader-generated `mapped_entry` and `stack_top`.
- Extended host regression `test_userimg_loader`:
  - links `kernel/core/userproc.c`
  - validates dry-run success path
  - validates dry-run rejection for unaligned stack and invalid entry
- Updated shell and Linux ABI version strings to Phase 39.

### Validation
- `make test-userimg-loader`
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make test-elf-sample`
- `make test-host-programs`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 38 – User Image Execution Context Scaffold

### Added
- Extended user image loader layout path in `kernel/core/userimg.c`:
  - adds mapped user stack planning (`stack_base/stack_top/stack_size`)
  - maps `userimg-stack` region in VMM metadata alongside `userimg-load`
- Extended `run elf-load` output to report stack mapping context.
- Extended host regression (`test_userimg_loader`) to validate stack tag, permissions, and report consistency.
- Updated shell and Linux ABI version strings to Phase 38.

### Validation
- `make test-userimg-loader`
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make test-elf-sample`
- `make test-host-programs`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 37 – User Image Loader Skeleton

### Added
- Added user image loader module:
  - `kernel/core/userimg.c`
  - `kernel/include/mvos/userimg.h`
- Added `run elf-load` user app path to prepare embedded ELF `PT_LOAD` layout into VMM metadata (layout-only path for teaching).
- Added host regression coverage:
  - `tests/host/test_userimg_loader.c`
  - `scripts/test_userimg_loader.sh`
  - `make test-userimg-loader`
- Updated shell capability text/build hints and phase strings to reflect Phase 37.

### Validation
- `make test-userimg-loader`
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make test-elf-sample`
- `make test-host-programs`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 36 – VMM Scaffold and `brk` State Wiring

### Added
- Added VMM module:
  - `kernel/mm/vmm.c`
  - `kernel/include/mvos/vmm.h`
- Added VMM APIs for teaching-path virtual memory metadata:
  - `vmm_map_range`, `vmm_unmap_range`
  - `vmm_region_count`, `vmm_region_at`
  - `vmm_user_heap_init`, `vmm_user_brk_get`, `vmm_user_brk_set`, `vmm_user_brk_limit`
- Wired Linux ABI `brk` in `kernel/core/userproc.c` to VMM user-heap state.
- Added shell `vmm` command to inspect current VMM regions and `user_brk/limit`.
- Added host regression coverage:
  - `tests/host/test_vmm_basic.c`
  - `scripts/test_vmm_basic.sh`
  - `make test-vmm-basic`

### Validation
- `make test-vmm-basic`
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-elf-sample`
- `make test-host-programs`

## Phase 35 – Scheduler Task Control

### Added
- Extended scheduler control APIs in `kernel/core/scheduler.c`:
  - `scheduler_task_active`
  - `scheduler_set_task_active`
  - `scheduler_find_task`
  - `scheduler_reset_task_runs`
  - `scheduler_reset_all_task_runs`
- Hardened scheduler dispatch path to recover when current task is inactive and to skip execution when no active tasks remain.
- Added shell task control command family:
  - `task list`
  - `task start <id|name>`
  - `task stop <id|name>`
  - `task reset <id|name|all>`
- Added host regression coverage:
  - `tests/host/test_scheduler_ctl.c`
  - `scripts/test_scheduler_ctl.sh`
  - `make test-scheduler-ctl`

### Validation
- `make test-scheduler-ctl`
- `make test-vfs-rw`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-elf-sample`
- `make test-host-programs`

## Phase 34 – Writable /tmp VFS Overlay

### Added
- Added writable overlay support in `kernel/core/vfs.c`:
  - new APIs: `vfs_write_file`, `vfs_remove_file`
  - writable path scope: `/tmp/*`
  - fixed-size in-memory file slots for deterministic behavior
- Added shell commands for file mutation:
  - `write <path> <text>`
  - `append <path> <text>`
  - `touch <path>`
  - `rm <path>`
- Added host regression coverage:
  - `tests/host/test_vfs_rw.c`
  - `scripts/test_vfs_rw.sh`
  - `make test-vfs-rw`

### Validation
- `make test-vfs-rw`
- `make test-elf-sample`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-host-programs`

## Phase 33 – ELF Sample Regression Coverage

### Added
- Added `scripts/test_elf_sample.sh` and `make test-elf-sample` target.
- `test-elf-sample` now validates:
  - `refresh-elf-sample` completes successfully
  - ELF magic bytes appear in `kernel/core/elf_sample_blob.c`
  - required blob symbols exist
  - blob contains at least a minimum byte count
- Updated shell capability/build hints to include the new regression command.

### Validation
- `make test-elf-sample`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-host-programs`

## Phase 32 – ELF Inspect Groundwork

### Added
- Added `kernel/core/elf.c` and `kernel/include/mvos/elf.h` for ELF64 metadata inspection (`entry`, program-header counts, load segment ranges, file offset ranges).
- Added `run elf-inspect` app path in `userapp` so the embedded Linux ELF sample can be inspected directly from miniOS shell.
- Added reproducible sample refresh pipeline:
  - `samples/linux-user/hello_linux_tiny.S`
  - `scripts/update_elf_sample_blob.sh`
  - `make refresh-elf-sample` target in `Makefile`
- Added embedded sample blob source `kernel/core/elf_sample_blob.c` generated from the tiny Linux ELF sample.

### Validation
- `make refresh-elf-sample`
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-host-programs`

## Phase 31 – Linux ABI Syscall Expansion

### Added
- Expanded Linux ABI preview syscall coverage in `userproc_dispatch`:
  - `write` (`rax=1`)
  - `brk` (`rax=12`)
  - `writev` (`rax=20`)
  - `getpid` (`rax=39`)
  - `uname` (`rax=63`)
  - `arch_prctl` (`rax=158`)
  - `gettid` (`rax=186`)
  - `set_tid_address` (`rax=218`)
  - `exit_group` (`rax=231`)
- Added fallback probe output path for `run linux-abi` so syscall behavior is observable even when user mode is disabled by default.
- Updated `minios_userapp_linux_abi` assembly demo path to exercise `writev/gettid/exit_group`.

### Validation
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Issue Follow-ups – 2026-04-24

### Fixed
- `scripts/dev_status.py --build-programs` help/runtime wording now consistently uses `make host-programs`.
- Added `build-host-programs` alias target in `Makefile` for backward compatibility.
- Moved ISO staging path from `boot/iso_root` to `build/iso_root` in build/smoke scripts.

### Added
- Added `scripts/test_host_programs.sh` and `make test-host-programs` for host-program manifest regression coverage.
- Regression checks now validate both dynamic and static modes, including:
  - top-level `default_link_mode`
  - per-program `link_mode`
  - `-static` flag behavior by mode

### Validation
- `make test-host-programs`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 30 – Linux ABI Preview Scaffold

### Added
- Added a Linux ABI preview syscall subset in `userproc_dispatch`:
  - `write` (`rax=1`, fd 1/2)
  - `getpid` (`rax=39`)
  - `exit` (`rax=60`)
- Added `run linux-abi` user app wiring for teaching-path verification.
- Updated syscall return path in `isr_user_syscall` so non-exit syscalls return values back to user mode in `rax`.
- Updated docs and shell version output to Phase 30.

### Validation
- `make -B`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 25 – Run Command Naming Consistency

### Added
- Added `run list` as an explicit alias for the default run listing behavior.
- Clarified shell help text to list all `run` subcommand entry points consistently (`help run`, `run help`, `run --help`, `run list`).

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 26 – Host Program Compile Path

### Added
- Added `samples/user-programs/` 示例原始碼（`hello.c`, `hello.cpp`）做為主機端程式編譯起點。
- 新增 `scripts/build_user_programs.py`，支援一次編譯所有 `.c`/`.cpp` 範例，並輸出 `build/host-programs/manifest.json`。
- 新增 Make 目標：
  - `make host-programs`：編譯主機端示例程式
  - `make clean-host-programs`：清除 `build/host-programs`
- 擴充 `python3 scripts/dev_status.py --build-programs` 檢查與執行上述 pipeline。

### Validation
- `python3 scripts/build_user_programs.py --source-dir samples/user-programs --out-dir build/host-programs`
- `python3 scripts/dev_status.py --build-programs`

## Phase 27 – Capability Visibility

### Added
- 新增 `cap` / `capabilities` shell 指令，提供內建程式能力、host 編譯能力、Linux 應用支援限制的即時矩陣輸出。
- 更新 `shell` 版本字串至 `Phase 27` 以對齊里程碑進度。
- 補齊 `README`、`docs/commands.md`、`docs/architecture.md`、`docs/roadmap.md` 的能力邊界與 phase 進度描述。

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 24 – Shell Help Consistency

### Added
- Added `help run` in interactive shell for targeted topic help.
- Added `run help` alias to existing run help flow.
- Enhanced `run --status` output with index and summary counters (`summary: user=... kernel=...`).

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 23 – Run Command Observability

### Added
- Added `run --help` and `run --status` in shell for explicit usage/status discovery.
- Added `run -h` alias to the same help output.
- Added user/kernel execution mode visibility for built-in user apps via `run --status`.
- Documented updated `run` behavior in README, roadmap, and command handbook.

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`

## Phase 18 – Scheduler Observability

### Added
- Added `tasks` shell command for scheduler visibility.
- Exposed scheduler query helpers (`scheduler_task_count`, `scheduler_task_name`, `scheduler_task_runs`) for non-invasive runtime inspection.
- Kept fault-injection helpers wrapped by build flags to remove unused-function noise during normal builds.

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-smoke`

## Phase 9 – Framebuffer Request Diagnostics

### Added
- 補齊 `kernel/include/mvos/limine.h` 的 framebuffer request/response 結構與常數，提供與 Limine framebuffer 協定對齊的啟動路徑資料。
- 在 `kernel/core/main.c` 增加 framebuffer 回應可用性與 metadata 記錄（count/size/bpp/addr/pitch）。
- 讓核心日誌走 `console_*` 抽象輸出，便於未來切換輸出後端。

### Fixed
- 統一序列輸出與 console 背景路徑，避免某些日誌僅綁死於 serial 寫法。

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-smoke`

## Phase 7 – Release Preparation

### Added
- 新增 `docs/phase7-merge-and-smoke-guide.md`：記錄所有 phase 合併流程、驗證命令與推送結果。
- 更新 `README.md` 與 `docs/roadmap.md`，同步 Phase 7 狀態（主線為主、分支已合併清理後以提交歷史追蹤）。

### Fixed
- 文件層級一致性修正：將分支管理與里程碑描述統一到實際 git 歷程（已移除「分支保留」表述，改為以 `main` 歷史回顧）。
- 為後續教學與維運保留清楚的「下一階段收斂條件」。

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-smoke`
- 兩者皆需通過且包含 `Boot text found`、`Phase 3 memory map verified`。

# Changelog

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

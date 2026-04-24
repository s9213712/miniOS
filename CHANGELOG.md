# Changelog

## Phase 25 – Run Command Naming Consistency

### Added
- Added `run list` as an explicit alias for the default run listing behavior.
- Clarified shell help text to list all `run` subcommand entry points consistently (`help run`, `run help`, `run --help`, `run list`).

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

# miniOS Roadmap（Stage 版）

最後更新：2026-04-24（目前 Stage 3）

這份文件改用 `Stage` 管理進度，避免長期只看 Phase 編號造成閱讀負擔。  
Phase 仍保留在 git history 與 `CHANGELOG.md`，但日常規劃以 Stage 為主。

## Stage 概覽

## Stage 1：Boot + Kernel Baseline

- 範圍：Phase 0-21
- 重點：
  - Limine handoff 與 serial-first 診斷
  - 基本記憶體與中斷初始化
  - scheduler/VFS/shell 基線可用
  - smoke 測試可重複
- 狀態：完成

## Stage 2：Tooling + Runtime Expansion

- 範圍：Phase 22-34
- 重點：
  - `run` 指令體驗補齊
  - host C/C++ 編譯流程與 manifest
  - writable `/tmp`、scheduler task control、VMM scaffold
  - capability matrix 與回歸腳本增強
- 狀態：完成

## Stage 3：Linux ABI + Minimal Exec Bring-up

- 範圍：Phase 35-44（持續中）
- 已完成：
  - Linux ABI 預覽 syscall 子集合
  - `run elf-inspect`
  - `run elf-load`（load+stack mapping、handoff dry-run、exec stack scaffold）
  - `execve` syscall scaffold（路徑驗證 + 內嵌 ELF 載入 + stack prep）
  - `execve("/bin/hello_linux_tiny", ...)` 最小實執行鏈路（ring3 entry、x86-64 `syscall`、tiny ELF output、`exit_group` return）
  - `make test-userimg-loader`
- 狀態：進行中（最小 `execve` 實執行已完成，下一步補齊一般化與隔離）

## Stage 4：System Completeness

- 目標：
  - 一般化 userspace runtime 與 `execve` 鏈路
  - persistent FS
  - network stack
  - 權限與安全模型
  - 更完整 Linux userspace 相容能力
- 狀態：待開始

## Stage 通用規範

- 每個 Stage 內的子步驟都要：
  - 至少新增或更新一個可重複驗證測試
  - 保持 `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通過
  - 同步更新 `README.md`、`docs/architecture.md`、`CHANGELOG.md`

## 參考文件

- `CHANGELOG.md`：完整 Phase 歷史
- `docs/linux-parity-goals.md`：對齊 Linux 的缺口、里程碑與驗收標準

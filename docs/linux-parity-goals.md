# miniOS Linux Parity Goals（Stage 版）

最後更新：2026-04-24（Stage 3 / Phase 40 baseline）

這份文件聚焦 miniOS 與一般 Linux 系統之間的差距，並用 Stage 管理收斂路徑。  
Phase 細節只作為歷史，不作為主要規劃單位。

## 目前定位

- 已有：boot 穩定、核心基礎模組、Linux ABI 子集合、ELF 載入 scaffold。
- 目前 Stage：Stage 3。
- 現況關鍵字：`layout + validation`，尚未到 `real userspace execution`。

## 與 Linux 的主要差距

- 真正 userspace 隔離（每行程 page table）。
- 完整 `execve` 鏈路（ELF 載入、初始 stack、entry、退出回收）。
- 更完整 syscall/POSIX 語意。
- 動態連結器與 libc 相容層。
- persistent 檔案系統與 block I/O。
- 網路堆疊與 socket 生態。
- 權限與安全模型（uid/gid/capability）。

## Stage 收斂路線

## Stage 3（現在）

- 目標：建立可靠 userspace 前置 scaffold。
- 目前已具備：
  - `run elf-load` mapping + handoff dry-run + exec stack scaffold
  - `make test-userimg-loader` 回歸
- 尚缺：
  - 真實 userspace entry/return
  - `execve` 可執行鏈路

## Stage 4（下一步）

- 目標：從最小可執行 userspace runtime 走向 Linux-like 基本可用系統。
- 工作：
  - ELF segment 真正 copy/zero
  - 初始 register/context 設定與進入 userspace
  - `exit_group` 回收路徑
  - syscall 子集合擴充（mmap/openat/read/fstat...）
  - FS、網路、安全模型
  - 非 toy userspace 程式兼容性提升
- 驗收：
  - sample ELF 可實際執行並退出
  - 出錯時有明確分類（載入、權限、對齊、syscall）
  - 至少一類實用 CLI 程式可穩定執行
  - 系統流程可在 VM 重複驗證

## 每次迭代共同規範

- 每次功能推進都要有對應 `make test-*`。
- `smoke-offline` 不可退化。
- `README.md`、`docs/roadmap.md`、`CHANGELOG.md` 同步更新。

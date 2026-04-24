# miniOS Linux Parity Goals

最後更新：2026-04-24（Phase 40）

這份文件是 miniOS 從「教學型核心骨架」往「可執行一般 Linux userspace 程式」的目標清單。
重點不是一次到位，而是每個 phase 都能有可驗證產出。

## 現況定位

- 已完成：可開機核心、基本記憶體管理、教學型 scheduler/VFS、Linux ABI 子集合、ELF 載入骨架。
- 目前定位：`run elf-load` 可完成映射規劃 + handoff dry-run + exec stack scaffold，但尚未真正 `execve`。

## 主要缺口（對照 Linux）

- 行程/執行模型
- 真正 userspace 隔離（每行程 page table）
- 完整 `execve` 鏈路（argv/envp/auxv + ELF 載入 + 程式進入）
- 完整 syscall/POSIX 子集合
- 動態連結與 libc 相容層
- 儲存與檔案系統（block device + persistent FS）
- 網路堆疊（socket/TCP/IP）
- 權限與安全模型（uid/gid/permission/capability）

## 里程碑與驗收

## Milestone A：User Execution Foundation

- 目標：從 dry-run 走到可控的 user entry/return。
- 工作：
  - user image mapping 與 stack mapping 穩定化
  - handoff 前置檢查（entry/stack/flags）擴充
  - exec stack 佈局（argc/argv/envp/auxv）建構
- 驗收：
  - `make test-userimg-loader` 通過
  - `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通過

## Milestone B：Minimal execve Path

- 目標：最小 `execve` 路徑能啟動一個靜態 ELF userspace 程式。
- 工作：
  - ELF segment copy/zero（非僅 metadata）
  - 初始 userspace register/context 設定
  - `exit_group` 回收路徑
- 驗收：
  - 指定 sample ELF 可進入 userspace 並退出
  - 失敗時有清楚錯誤分類（載入、對齊、權限、syscall）

## Milestone C：Linux Userspace Compatibility (Basic)

- 目標：支援更多 CLI 類 Linux 程式（先 static）。
- 工作：
  - syscall 子集合擴充（mmap/openat/close/read/fstat 等）
  - `/proc` 與基礎 FS 介面補齊
  - 更完整訊號/錯誤碼語意
- 驗收：
  - 至少一個非 toy 的 static userspace 工具可啟動並執行核心功能

## Milestone D：Toward Daily-use Kernel

- 目標：朝一般 OS 能力靠攏。
- 工作：
  - persistent FS + block I/O
  - network stack
  - 權限與安全模型
  - 可維運診斷工具（trace/log/crash dump）
- 驗收：
  - 可在 VM 內重複完成檔案、程序、網路基本操作流程

## 每階段規範

- 每個 phase 都必須：
  - 有至少一個可重複測試入口（`make test-*`）
  - `smoke-offline` 不退化
  - 文件同步更新（README + roadmap + changelog）


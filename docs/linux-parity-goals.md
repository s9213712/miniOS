# miniOS Linux Parity Goals（Stage 版）

最後更新：2026-04-25（Stage 4）

這份文件聚焦 miniOS 與一般 Linux 系統之間的差距，並用 Stage 管理收斂路徑。  
Phase 細節只作為歷史，不作為主要規劃單位。

## 目前定位

- 已有：boot 穩定、核心基礎模組、x86-64 `syscall` 入口的 Linux ABI 子集合、ELF 載入與 tiny static ELF `execve` demo。
- 目前 Stage：Stage 4。
- 現況關鍵字：`minimal userspace execution`，並加強 `execve` 後的 process-state 替換隔離。

## 與 Linux 的主要差距

- 真正 userspace 隔離（每行程 page table）。
- 完整 Linux `execve` 語意（多行程 address space、權限、interp、資源回收）。
- 更完整 syscall/POSIX 語意。
- 動態連結器與 libc 相容層。
- persistent 檔案系統與 block I/O。
- 網路堆疊與 socket 生態。
- 權限與安全模型（uid/gid/capability）。

## Stage 收斂路線

## Stage 3（完成）

- 目標：建立可靠 userspace 前置 scaffold，並完成 tiny static ELF 的最小實執行鏈路。
- 目前已具備：
  - `run elf-load` mapping + handoff dry-run + exec stack scaffold
  - 內嵌 tiny static ELF segment copy/zero、user page backing、entry register setup
  - `execve("/bin/hello_linux_tiny", ...)` ring3 entry、x86-64 `syscall` dispatch 與 `exit_group` 返回
  - `make test-userimg-loader` 回歸
- 尚缺：
  - 每行程 address space
  - 動態連結、一般 ELF interp 與完整回收

## Stage 4（現在）

- 目標：從最小可執行 userspace runtime 走向 Linux-like 基本可用系統。
- 工作：
  - 每行程 page table 與 userspace 隔離
  - `mmap`/檔案-backed mapping 與更完整 `brk`
  - `execve` 資源替換與錯誤語意補齊（含 userspace mapping 重設）
  - syscall 子集合擴充（mmap/openat/read/fstat...）
  - FS、網路、安全模型
  - 非 toy userspace 程式兼容性提升
- 驗收：
  - toy sample 以外的 static ELF 可實際執行並退出
  - 出錯時有明確分類（載入、權限、對齊、syscall）
  - 至少一類實用 CLI 程式可穩定執行
  - 系統流程可在 VM 重複驗證

## 每次迭代共同規範

- 每次功能推進都要有對應 `make test-*`。
- `smoke-offline` 不可退化。
- `README.md`、`docs/roadmap.md`、`CHANGELOG.md` 同步更新。
- Stage 4 目前完成項目：
  - `execve` 成功時清除舊有 `mmap`/`userimg` 映射，避免後續換映像殘留。
  - host regression 驗證了換映像後舊 `mmap` 不能再被 `munmap` 成功清除。

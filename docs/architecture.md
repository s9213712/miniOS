# Architecture Notes（Stage 版）

miniOS 是 x86_64 教學型核心，採用「可開機 + 可回歸 + 漸進擴充」路線。

- Target: x86_64
- Bootloader: Limine
- Kernel model: monolithic（C 為主，少量 x86_64 ASM）
- 主要診斷通道：serial-first

## Stage 狀態

- Stage 1（完成）：Boot baseline 與 smoke 診斷。
- Stage 2（完成）：開發者工具鏈與核心擴充（host 驗證、writable `/tmp`、scheduler control、VMM scaffold）。
- Stage 3（完成）：Linux ABI + ELF loader + minimal `execve` bring-up。
- Stage 4（進行中）：FS/網路/安全與 Linux-like 能力擴展。

## 核心目錄

- `kernel/core/main.c`：C 入口與初始化流程協調。
- `kernel/arch/x86_64/gdt/*`、`idt/*`、`interrupt/*`：CPU 表與中斷路徑。
- `kernel/mm/pmm.c`、`heap.c`、`vmm.c`：實體/堆積/虛擬記憶體 scaffold。
- `kernel/core/scheduler.c`：cooperative scheduler。
- `kernel/core/vfs.c`：最小 VFS。
- `kernel/core/userproc.c`、`userimg.c`：Linux ABI preview、ELF 載入與最小 `execve` userspace 路徑。

### 檔案系統現況

- miniOS 目前使用的是「自建 minimal VFS」：啟動期注入唯讀 initfs 節點（`/boot/init/*`）與記憶體中的可寫 `/tmp/*` 覆寫層。
- 目前尚未接上持久化 block device/磁碟檔案系統，`/tmp/*` 的資料只會存在於記憶體，重啟後會遺失。

## Build 輸出

- `build/mvos.elf`
- `build/mvos.bin`
- `build/mvos.iso`

## Stage 3 目前能力邊界

- `run linux-abi`：透過 x86-64 `syscall` 入口執行 Linux syscall 編號相容的預覽子集合，不是完整 POSIX。
- `run elf-inspect`：檢查 embedded ELF metadata。
- `run elf-load`：執行
  - load+stack mapping 檢查
  - handoff dry-run
  - exec stack scaffold（`argc/argv/envp/auxv`）
- `make smoke-offline`：啟動時驗證 tiny static ELF 可進入 ring3、輸出並經 `exit_group` 返回。
- `make test-userimg-loader`：對上面流程做 host regression。

## Stage 4 進行中項目

- 每次 `execve` 成功時會清除前一次 userspace 狀態（含 `userimg`、`mmap`、`brk` 映射），為換映像後資源隔離建立最小保障。
- `make test-userimg-loader` 同步新增「舊 `mmap` 在新 `execve` 後應不可被 `munmap`」回歸。

## 仍缺能力（完成 Stage 4 前）

- 每行程 address space 與完整 userspace 隔離。
- 一般 ELF/動態連結器支援與完整 Linux `execve` 語意。

## 參考

- `README.md`：使用與當前 Stage 概覽
- `docs/roadmap.md`：Stage 路線圖
- `docs/linux-parity-goals.md`：與 Linux 差距與收斂目標
- `CHANGELOG.md`：完整 Phase 歷史

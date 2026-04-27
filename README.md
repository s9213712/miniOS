# MinimalOS v1 (Stage 4, 教學型專案)

miniOS 是 x86_64 教學型作業系統專案，主線目標是：
- 維持可開機、可驗證的核心基線
- 逐步往 Linux userspace 相容能力前進
- 每次改動都能用固定測試回歸

## Stage 路線圖（取代大量 Phase 直接閱讀）

| Stage | 主題 | 狀態 |
| --- | --- | --- |
| Stage 1 | Boot + Kernel 基線（Limine/serial/smoke + 核心基本子系統） | 完成 |
| Stage 2 | 開發者體驗 + 核心擴充（host tooling、回歸、VFS/tmp、scheduler control） | 完成 |
| Stage 3 | Linux ABI + ELF loader + Minimal `execve` bring-up | 完成 |
| Stage 4 | 系統完整化（FS/網路/安全 + Linux userspace 相容） | 進行中 |

目前所在：`Stage 4 (syscall fd 行為收斂)`
Phase 細節歷史保留在 `CHANGELOG.md`，Stage 視角主文件如下：
- `docs/roadmap.md`
- `docs/linux-parity-goals.md`
- `docs/python-c-practice.md`（miniOS 內 Python 測試腳本與主機端 C 編譯範例）

## 目前可做的事

- 可穩定開機並通過 smoke（serial 8 秒內可見 `hello from kernel`）。
- 有基本 kernel 子系統：PMM/heap、interrupt/timer、scheduler、VFS、shell。
- 有 Linux ABI 預覽子集合（`write/writev/brk/uname/getpid/gettid/...`），userspace 透過 x86-64 `syscall` 指令進 kernel。
- `run linux-abi` 保留為較完整的 kernel probe + `execve` 教學路徑；目前 ring-3 userspace 證據仍以 tiny ELF `execve` demo 為主。
- 有 ELF 載入與最小 `execve` demo：`run elf-inspect`、`run elf-load`、`EXECVE_DEMO=1` boot probe。
- `run elf-load` 現在可驗證：
  - load+stack 映射
  - handoff dry-run
  - `argc/argv/envp/auxv` 初始 stack scaffold
- `execve("/bin/hello_linux_tiny", ...)` 可載入內嵌 tiny static ELF、進入 ring3、印出訊息並經 `exit_group` 返回 kernel probe。
- 成功的 `execve` 會清理前次 userspace 狀態，避免新流程沿用舊 `mmap`/`userimg` mapping。

## 尚未做到（和 Linux 的差距）

- 每行程 page table 與完整 userspace 隔離。
- 完整 Linux `execve` 語意與更完整 syscall/POSIX。
- 動態連結/libc 相容層。
- persistent 檔案系統、網路堆疊、安全模型。

## 本地準備

```bash
./scripts/setup-dev.sh --install
```

Ubuntu/WSL 常用依賴：

```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils gcc make nasm qemu-system-x86 qemu-system-gui xorriso git curl wget
```

如果找不到 `gcc-x86-64-elf` 套件，Make 會自動退回本機 `gcc/ld/objcopy`。

## 建置與測試

```bash
make -B
make test-elf-sample
make test-vfs-rw
make test-scheduler-ctl
make test-vmm-basic
make test-userimg-loader
make test-host-programs
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
make pre-push
```

常用命令：
- `make host-programs`
- `make refresh-elf-sample`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline-basic`（boot-only 快速檢查）
- `ENABLE_SHELL=1 make run`
- `./minios.sh`（建議用一鍵腳本，預設 TTY + shell）
- `./minios.sh -g`（GUI 模式）
- `python3 scripts/dev_status.py --build`
- `make pre-push`（推前建議標準流程）

## 目前 Stage 4 的下一步

目標是把 Stage 4 的 userspace 隔離機制繼續深化，逐步進入更完整的 Linux-like 行為：
- 擴充 ELF/user memory 邊界檢查
- 增加更多 syscall 與錯誤分類
- 補齊每行程 address space 與資源回收

驗收標準會維持：
- `make test-*` 回歸全綠
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 持續通過

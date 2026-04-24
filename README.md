# MinimalOS v1 (Stage 3, 教學型專案)

miniOS 是 x86_64 教學型作業系統專案，主線目標是：
- 維持可開機、可驗證的核心基線
- 逐步往 Linux userspace 相容能力前進
- 每次改動都能用固定測試回歸

## Stage 路線圖（取代大量 Phase 直接閱讀）

| Stage | 主題 | 狀態 |
| --- | --- | --- |
| Stage 1 | Boot + Kernel 基線（Limine/serial/smoke + 核心基本子系統） | 完成 |
| Stage 2 | 開發者體驗 + 核心擴充（host tooling、回歸、VFS/tmp、scheduler control） | 完成 |
| Stage 3 | Linux ABI + ELF loader + Minimal `execve` bring-up（目前） | 進行中 |
| Stage 4 | 系統完整化（FS/網路/安全 + Linux userspace 相容） | 待開始 |

目前所在：`Stage 3 (Phase 41 baseline)`  
Phase 細節歷史保留在 `CHANGELOG.md`，Stage 視角主文件如下：
- `docs/roadmap.md`
- `docs/linux-parity-goals.md`

## 目前可做的事

- 可穩定開機並通過 smoke（serial 8 秒內可見 `hello from kernel`）。
- 有基本 kernel 子系統：PMM/heap、interrupt/timer、scheduler、VFS、shell。
- 有 Linux ABI 預覽子集合（`write/writev/brk/uname/getpid/gettid/...`）。
- 有 ELF 載入 scaffold：`run elf-inspect`、`run elf-load`。
- `run elf-load` 現在可驗證：
  - load+stack 映射
  - handoff dry-run
  - `argc/argv/envp/auxv` 初始 stack scaffold

## 尚未做到（和 Linux 的差距）

- 真正 user mode 執行生命週期（不是 scaffold/dry-run）。
- 完整 `execve` 鏈路與更完整 syscall/POSIX。
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
```

常用命令：
- `make host-programs`
- `make refresh-elf-sample`
- `ENABLE_SHELL=1 make run`
- `python3 scripts/dev_status.py --build`

## 目前 Stage 3 的下一步

目標是完成 Stage 3 的最小 `execve` 路徑，之後再進入 Stage 4：
- ELF segment 真正 copy/zero
- userspace context 實際進入與返回
- `exit_group` 回收路徑

驗收標準會維持：
- `make test-*` 回歸全綠
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 持續通過

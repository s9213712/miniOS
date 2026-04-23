# MinimalOS v1 (Phase 7, 教學型專案)

這個專案是逐階段開發的小型 x86_64 作業系統，目標是讓每個階段都能在 `make smoke-offline` 下驗證。  
每個階段都有明確目的、實作範圍與預期成效，並保留在 `main` 歷史中的歷程提交作為教學紀錄。

## 專案目的

- 以 `Limine` 為 bootloader 建立可開機核心骨架
- 用 serial 輸出做早期可觀測性，避免只靠 framebuffer
- 逐步加入 `PMM`、`heap`、中斷、scheduler、VFS 等核心概念
- 以可重複的 smoke 測試保證每個階段都能回歸

## 分支策略

- `main`：穩定可開機基線
- 已完成的 phase 提交已合併到 `main`，可用 commit 記錄回看各階段演進。

## Phase 7：正式發佈前整理（完成）

- 完成 Phase 7 文檔整理：里程碑、變更紀錄、回歸清單
- 固定本次測試流程為所有階段共通驗收入口

## 每階段目的與預期成效

- Phase 0：啟動基線穩定化
  - 目的：完成 `limine.conf`、`.requests`、smoke 錯誤訊息與離線流程標準化
  - 預期成效：`hello from kernel` 於 8 秒內在 serial 出現；失敗時有明確原因

- Phase 1：早期可觀測性
  - 目的：固定 boot 早期輸出與錯誤路徑可視化
  - 預期成效：`hello from kernel` 與 `gdt/idt initialized` 穩定出現在 serial

- Phase 2：記憶體管理
  - 目的：建立 HHDM + `memmap` 驗證、PMM/heap 初始化
  - 預期成效：`[phase3] memory allocator ready`、`free pages` 數值可觀察且可重複

- Phase 4：輸入與 shell
  - 目的：保留對輸入子系統的接線實作
  - 預期成效：可追查 shell/鍵盤相關程式，但不影響 boot smoke 基本路徑

- Phase 5：任務切換骨架
  - 目的：加入以 timer tick 駆動的最小 cooperative scheduler
  - 預期成效：`task-a` / `task-b` 有預期節拍輸出（`[sched]`）

- Phase 6：最小 VFS（initramfs-like）
  - 目的：加入 `open/read/list` 與缺檔錯誤路徑
  - 預期成效：能列出 `/boot/init/*`、讀取檔案、找不到檔案時回報 `missing=ok`

- Phase 7：發佈前整理
  - 目的：補齊變更摘要與回歸測試清單，形成可交付階段文件
  - 預期成效：`CHANGELOG.md` 已建立，`test-smoke` 可作為每次提交後回歸基準

## 目前可驗證狀態

- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通常能通過
- `build/mvos.elf` 為 ELF 格式，`build/mvos.bin` 指向同一 kernel 影像（與 Limine 相容）
- Limine request symbols (`request_start` / `request_end`) 可被測試腳本驗證
- kernel serial 流中會輸出：`MiniOS Phase 3 bootstrap`、`hello from kernel`

## 本地準備

```bash
./scripts/setup-dev.sh --install
```

常用指令（Ubuntu/WSL 範例）：

```bash
sudo apt-get update
sudo apt-get install -y build-essential binutils gcc make nasm qemu-system-x86 qemu-system-gui xorriso git curl wget
```

若套件源缺少 `gcc-x86-64-elf`，Make 會自動退回本機 `gcc/ld/objcopy`。

## 建置 / 測試流程

```bash
make
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
LIMINE_LOCAL_DIR=/tmp/limine-bin make run
```

### 重要測試命令

- `SKIP_SMOKE_RUN=1 make test-smoke`：只做建置階段驗證
- `make smoke` / `make smoke-full`：完整 smoke alias
- `make smoke-offline`：離線模式 smoke（建議搭配 `SMOKE_OFFLINE=1`）
- `PANIC_TEST=1 make run`：啟用 panic path
- `FAULT_TEST=div0|opcode|gpf|pf make run`：啟用 fault path

## 預期 serial 輸出

常態開機時建議至少看到：

- `MiniOS Phase 3 bootstrap`
- `boot banner: kernel entering C`
- `hello from kernel`
- `gdt/idt initialized`
- `[phase3] memory allocator ready`
- `[vfs] files=...`
- `[phase5] scheduler ready`
- `Booting from DVD/CD...`（QEMU 韌體路徑）

可選路徑：

- `PANIC_TEST=1 make run` 會看到 `[panic]`
- `FAULT_TEST=... make run` 會看到 `[fault]`

## 專案目錄

- `boot/limine.conf`、`boot/limine/`、`boot/iso_root/boot/limine.conf`
- `kernel/`：核心、mm、裝置、arch
- `docs/`：架構與 road map
- `scripts/`：smoke/build/qemu 工具
- `tests/smoke/`：smoke 使用說明

## 常見問題排查

- `readelf -S build/mvos.elf | grep ".requests"` 不存在：檢查 Limine request block
- `hello from kernel` 未出現：確認 `boot/limine.conf` 有 `KERNEL_PATH=boot:///boot/mvos.bin`
- 無法載入 Limine：使用 `LIMINE_LOCAL_DIR` 指向已準備好的 bootloader 檔案

重跑建議：

```bash
make clean
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

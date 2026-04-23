# MinimalOS v1 (Phase 15, 教學型專案)

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

- Phase 8：FrameBuffer GUI（完成）
  - 加入簡易 VGA 文字介面鏡射輸出（目前以 0xB8000 文字緩衝為主）
  - 保留 serial 作為主測試輸出（避免 smoke 失效）
  - 可用 `QEMU_GUI=1 make run` 開啟 QEMU 視窗
- Phase 9：Framebuffer Request（進行中）
  - 補齊 Limine framebuffer request 定義並輸出基本 framebuffer 資訊
  - 將 framebuffer 可用性納入啟動早期可觀測性，維持 serial 不退化

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
- Phase 8：FrameBuffer GUI
  - 目的：將關鍵 boot log 同步輸出到 VGA 文字控制台
  - 預期成效：serial 測試不變，視窗內可見 `hello from kernel`
- Phase 9：Framebuffer Request
  - 目的：在 boot 時確認 Limine framebuffer 回應
  - 預期成效：輸出 framebuf metadata（count/size/bpp）並在缺少請求時明確告警
- Phase 10：First GUI Window
  - 目的：使用 framebuffer 進行最小視窗渲染 demo
  - 實作重點：新增 framebuffer graphics backend、可選的視窗邊框/標題欄/關閉按鈕方塊；保留 serial 與 VGA 文字鏡射不變
  - 預期成效：`QEMU_GUI=1` 跑起時可看到視覺化 boot window（需 QEMU 有 GUI 顯示環境）
- Phase 11：互動式 Shell（可選）
  - 目的：補齊第一層使用者互動介面，提供簡易命令入口作為第二階段使用者體驗
  - 實作重點：`shell.c` 已存在，改以 `ENABLE_SHELL=1` 開關進入互動式 shell，預設關閉避免影響 smoke 時序
  - 預期成效：啟用後可在 serial 視窗輸入 `help`、`mem`、`ticks`、`hello`、`quit` 等命令
- Phase 12：GUI 互動指令（可選）
  - 目的：在 shell 中加入可重複驗證的簡單 GUI 動作
  - 實作重點：新增 `gui` 指令，可在 `ENABLE_SHELL=1` 環境下透過 serial shell 觸發視窗重繪（只在已啟用 graphics backend 才作用）
  - 預期成效：`gui` 指令後輸出 `GUI demo window drawn.`
- Phase 13：第一個視窗程式（可選）
  - 目的：新增可重複執行的簡易 GUI 應用示例，模擬「第一個視窗程式」概念
  - 實作重點：新增 `app` 指令，會在 framebuffer 端繪製第二個獨立視窗 demo
  - 預期成效：`app` 指令後輸出 `GUI app launched.`
- Phase 14：視窗程式樣式擴展（可選）
  - 目的：在同一 GUI app 管道下新增變體指令，作為「視窗程式」可擴充樣版
  - 實作重點：新增 `app alt`，會繪製第二個風格視窗 demo
  - 預期成效：`app` 後輸出 `GUI app launched.`, `app alt` 後輸出 `GUI alt app launched.`
- Phase 15：GUI app 管理子命令（可選）
  - 目的：增加 `app` 的維運能力，讓 shell 能列出與查詢視窗程式可用資訊
  - 實作重點：新增 `app list`、`app status`
  - 預期成效：`app list` 回報可用 app，`app status` 回報 framebuffer metadata

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
- `QEMU_GUI=1 make run`：開啟 QEMU VGA 視窗並同步顯示 framebuffer 文字輸出
- `QEMU_GUI=1 make run`：在有 GUI 環境的機器上也能看到 framebuffer 視窗 demo

啟用互動 shell（預設關閉）：

```bash
ENABLE_SHELL=1 make run
ENABLE_SHELL=1 LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

GUI 限制：
- 無法在沒有可用 display 的環境啟動 GUI，QEMU 會輸出 `gtk initialization failed`
- 這類環境可先用 `QEMU_GUI=0 make smoke-offline` 做 boot/smoke 驗證；GUI 視覺確認可改到有桌面/有 GUI 的節點執行
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

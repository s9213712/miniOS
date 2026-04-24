# MinimalOS v1 (Phase 35, 教學型專案)

這個專案是逐階段開發的小型 x86_64 作業系統，目標是讓每個階段都能在 `make smoke-offline` 下驗證。  
每個階段都有明確目的、實作範圍與預期成效，並保留在 `main` 歷史中的歷程提交作為教學紀錄。

## 專案目的

- 以 `Limine` 為 bootloader 建立可開機核心骨架
- 用 serial 輸出做早期可觀測性，避免只靠 framebuffer
- 逐步加入 `PMM`、`heap`、中斷、scheduler、VFS 等核心概念
- 以可重複的 smoke 測試保證每個階段都能回歸

## 最近工程進度

- 2026-04-24：`Phase 22` 完成 `run cpp`，並補上可直接呼叫的 C++ 示範入口。
- 2026-04-24：`Phase 23` 完善 shell `run` 指令體驗，加入 `run --help`、`run --status`，並可見 `user/kernel` 執行模式。
- `Phase 21` 完成 `ls`/`cat` initfs 檢視流程，確保唯讀測試資料可直接在 shell 驗證。
- `Phase 20` 的 `run hello` 先以 kernel fallback 啟動，保留後續啟用 ring3 的開關。
- `Phase 18` 的 `tasks`、`Phase 17` 的 `app` 查詢、`Phase 22` 的 C++ 路徑都已可作為 smoke 之外的進階教學示例。
- 2026-04-24：`Phase 26` 補齊主機端 C/C++ 編譯 pipeline（`make host-programs`）並完成 `manifest.json` 產出。
- 2026-04-24：`Phase 27` 上線 `cap` / `capabilities`，可在 shell 快速查閱能力邊界與 Linux 支援限制。
- 2026-04-24：`Phase 28` 強化 host-side 編譯可追蹤性，`make host-programs` 輸出新增 `status`、`flags`、`sha256` 與錯誤摘要。
- 2026-04-24：`Phase 29` 補上 host 編譯輸出連結模式控制（`MHOST_STATIC`），並將輸出模式納入 manifest 以供可重現打包。
- 2026-04-24：`Phase 30` 新增 Linux ABI 預覽路徑（`run linux-abi`），打通最小 `write/getpid/exit` syscall 骨架。
- 2026-04-24：`Phase 31` 擴充 Linux ABI 預覽 syscall 子集合（`writev/brk/uname/gettid/set_tid_address/arch_prctl/exit_group`），並讓 `run linux-abi` 在 fallback 也可做 probe。
- 2026-04-24：`Phase 32` 新增 `run elf-inspect` 與 ELF64 檢查器，並補齊 `make refresh-elf-sample` 讓 embedded Linux ELF 樣本可重生。
- 2026-04-24：`Phase 33` 新增 `make test-elf-sample`，將 ELF 樣本重生流程納入可回歸檢查。
- 2026-04-24：`Phase 34` 新增可寫入 `/tmp` overlay VFS，shell 支援 `write/append/touch/rm`，並補上 `make test-vfs-rw`。
- 2026-04-24：`Phase 35` 強化 scheduler 控制面，新增 `task start/stop/reset/list` 與 `make test-scheduler-ctl`。

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
- Phase 9：Framebuffer Request（完成）
  - 補齊 Limine framebuffer request 定義並輸出基本 framebuffer 資訊
  - 將 framebuffer 可用性納入啟動早期可觀測性，維持 serial 不退化
- Phase 18：可見性增強（完成）
  - 加入 scheduler 任務可觀測指令，提供 `tasks` 查詢任務清單與執行次數
- Phase 21：最小檔案可見性介面（完成）
  - shell 增加 `ls`/`cat`，可列舉並讀出 initfs 的虛擬檔內容（`/boot/init/...`）。
  - 保留讀寫分離（目前階段仍為唯讀，適合 smoke 路徑不破壞）。
  - 預期成效：在互動 shell 中執行 `ls`、`ls /boot/init`、`cat /boot/init/readme.txt` 均有穩定輸出。
- Phase 22：基礎 C++ 範例程式（完成）
  - 新增 `run cpp` 示範 app（C++ 函式已編入核心映像並可呼叫）。
  - `Python` 仍不支援直接於 miniOS 內部執行；先以 C/C++ kernel-mode fallback 為主。
- Phase 23：互動指令可觀測性（完成）
  - 改善 `run` 命令幫助與狀態檢視，支援 `run --help` 與 `run --status`。
  - `run --status` 會顯示每個 app 的執行模式（user/kernel），避免混淆未啟用 ring3 的情況。
  - `run -h` 提供與 `run --help` 相同的命令說明。
- Phase 24：shell help 細節可視化（完成）
  - 增加 `help run` 快速查詢 `run` 用法。
  - `run help` 視為幫助別名，與 `run --help` 行為一致。
  - `run --status` 補上編號與 user/kernel 摘要，方便逐項比對。
- Phase 25：run 快捷指令清晰化（完成）
  - `run list` 明確列為命令別名，和預設行為一致，降低使用學習成本。
- Phase 26：主機端程式編譯（完成）
  - 新增 `samples/user-programs` C/C++ 範例
  - 新增 `scripts/build_user_programs.py` 與 `make host-programs`
  - 建構 `build/host-programs`，輸出 `manifest.json` 供下一步 loader 導入做準備
- Phase 27：能力邊界可見化（完成）
  - 新增 `cap` / `capabilities`，輸出現在能力矩陣（可執行 app、host build 能力、尚未支援功能）
- Phase 28：主機編譯可診斷化（完成）
  - 增加 `make host-programs` 的可選 verbose，並加入 `MHOST_VERBOSE`、`MHOST_COMMON_FLAGS`、`MHOST_CFLAGS`、`MHOST_CXXFLAGS`。
  - `build/host-programs/manifest.json` 新增 `status`、`built_at`、`flags`、`sha256` 與編譯錯誤欄位，讓每次輸出可追溯。
- Phase 29：編譯輸出可重現性（完成）
  - 增加 `MHOST_STATIC=1`，可輸出靜態連結主機可執行檔，便於單機測試。
  - manifest 新增 `link_mode` 與 `default_link_mode`，對應 `dynamic/static` 追蹤。
- Phase 30：Linux ABI 最小骨架（完成）
  - userproc 增加 Linux x86_64 syscall 預覽子集合：`write(1/2)`、`getpid`、`exit`。
  - 新增 `run linux-abi` user app（教學用途，非完整 Linux userspace 相容）。
- Phase 31：Linux ABI syscall 擴展（完成）
  - userproc 擴充預覽子集合：`writev`、`brk`、`uname`、`gettid`、`set_tid_address`、`arch_prctl`、`exit_group`。
  - `run linux-abi` 在 user-mode 關閉時也會執行 kernel fallback probe，直接輸出 syscall 回傳值與 uname/fs-base 資訊。
- Phase 32：ELF 檢查與樣本更新流程（完成）
  - 新增 `run elf-inspect`，可檢查 embedded Linux user ELF 的 entry/program header/load segment 範圍。
  - 新增 `samples/linux-user/hello_linux_tiny.S` 與 `make refresh-elf-sample`，可重生 `kernel/core/elf_sample_blob.c`。
- Phase 33：ELF 樣本回歸測試（完成）
  - 新增 `make test-elf-sample`（`scripts/test_elf_sample.sh`），檢查 ELF magic、符號與 blob 大小下限。
  - 將 `refresh-elf-sample` 從「一次性工具」提升為可重複驗證的測試入口。
- Phase 34：可寫入 VFS overlay（完成）
  - 新增 `/tmp` 可寫入 overlay（固定容量 in-memory），保留 `/boot/init` 唯讀啟動檔。
  - shell 新增 `write`、`append`、`touch`、`rm` 指令，支援最小檔案建立/修改/刪除流程。
  - 新增 `make test-vfs-rw`（`scripts/test_vfs_rw.sh`）驗證 VFS 讀寫與刪除行為。
- Phase 35：scheduler 任務控制（完成）
  - shell 新增 `task list/start/stop/reset`，可依任務名稱或 index 控制執行狀態。
  - scheduler 新增 task active 狀態管理與 run counter 重置 API。
  - 新增 `make test-scheduler-ctl` 驗證任務啟停與統計重置路徑。

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
- Phase 16：GUI app 啟動路徑（可選）
  - 目的：正式化 `app` 命令入口，將啟動邏輯收斂到 `app launch <name>`
  - 實作重點：保留 `app` 預設行為向前相容，新增 `app launch app` / `app launch alt`
  - 預期成效：可見明確命令路徑且行為可重用、可擴充
- Phase 17：GUI app 查詢路徑（可選）
  - 目的：讓 CLI 可查到 app metadata，模擬最小應用管理介面
  - 實作重點：新增 `app info <name>`
  - 預期成效：可透過 `app info app`、`app info alt` 查看說明，未知 app 回傳 `unknown app: <name>`
- Phase 18：可見性增強（可選）
  - 目的：讓 scheduler 狀態可在 shell 直接觀測，用於任務切換概念驗證
  - 實作重點：新增 `tasks` 指令，輸出每個 task 名稱、狀態與運行次數
  - 預期成效：`tasks` 指令可看到 `task-a`、`task-b` 的輪詢資料
- Phase 20：使用者模式最小路徑
  - 目的：建立 `run hello` 路徑雛形，並預留 ring3 + `int 0x80` 設計。
  - 實作重點：建立 `userproc_enter_asm`、`isr_user_syscall`、IDT 0x80、最小 `user app` 載入/回傳機制。
  - 當前狀態：預設採用 kernel-mode fallback（避免目前測試環境因頁權限導致 `Page Fault`），同時保留 `MINIOS_PHASE20_USER_MODE=1` 切換點供後續逐步啟用 ring3。
  - 預期成效：`PHASE20_DEMO=1` 下仍維持核心開機保證輸出 `hello from kernel`，並在 shell 可呼叫 `run hello` 看到對應 fallback 行為訊息。

## 目前可驗證狀態

- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通過（serial 8 秒內出現 `hello from kernel`）
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
make host-programs
make refresh-elf-sample
make test-elf-sample
make test-vfs-rw
make test-scheduler-ctl
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
LIMINE_LOCAL_DIR=/tmp/limine-bin make run
```

### 重要測試命令

- `SKIP_SMOKE_RUN=1 make test-smoke`：只做建置階段驗證
- `QEMU_GUI=1 make run`：開啟 QEMU VGA 視窗並同步顯示 framebuffer 文字輸出
- `make host-programs`：只編譯主機端 C/C++ 範例（不影響 kernel）
- `make refresh-elf-sample`：重生內嵌 Linux ELF 樣本 blob（供 `run elf-inspect` 診斷）
- `make test-elf-sample`：驗證 ELF 樣本重生輸出契約（magic/符號/大小）
- `make test-vfs-rw`：驗證 `/tmp` 可寫 overlay 的 host 端回歸測試
- `make test-scheduler-ctl`：驗證 scheduler 任務啟停與 reset 控制
- `python3 scripts/dev_status.py --build`：環境 + 建置健康檢查（建議每次大改後）
- `python3 scripts/dev_status.py --build-programs`：主機端範例程式建置檢查
- `python3 scripts/dev_status.py`：只做環境檢查，不會重建

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
- `MHOST_VERBOSE=1 make host-programs`：顯示 host 編譯指令與摘要
- `MHOST_COMMON_FLAGS` / `MHOST_CFLAGS` / `MHOST_CXXFLAGS`：覆寫 host 編譯旗標
- `MHOST_STATIC=1 make host-programs`：以 `-static` 連結主機可執行檔（適合單機攜帶測試）

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

- `boot/limine.conf`、`boot/limine/`、`build/iso_root/boot/limine.conf`
- `kernel/`：核心、mm、裝置、arch
- `docs/`：架構與 road map
- `scripts/`：smoke/build/qemu 工具
- `tests/smoke/`：smoke 使用說明

## 常見問題排查

- `readelf -S build/mvos.elf | grep ".requests"` 不存在：檢查 Limine request block
- `hello from kernel` 未出現：確認 `boot/limine.conf` 有 `KERNEL_PATH=boot:///boot/mvos.bin`
- 無法載入 Limine：使用 `LIMINE_LOCAL_DIR` 指向已準備好的 bootloader 檔案
- `python3 scripts/dev_status.py`：快速確認 `make` / `gcc` / `nasm` / `qemu-system-x86_64` / `xorriso` 可用

重跑建議：

```bash
make clean
LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
```

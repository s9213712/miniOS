# miniOS Roadmap（教學用）

每一個 phase 都曾經有獨立 `phase-*` 分支，完成後以 commit 形式保留在主線歷史中，作為學習紀錄與回溯點。  
本文件提供每個階段的目的、預期成效與最小可驗證成果，方便學員對照 `main`、回頭閱讀歷史。

## 分支保留原則

- `main` 為可開機主線（最終目標），其階段歷史在每次 phase 提交中可回溯。
- 本教學保留變更節點與測試紀錄；在本分支也可直接沿用分支作為教學實作線，實務上可視情況再合併到 `main`。

## 最近進度摘要

- 2026-04-24：`Phase 22` 完成，已補齊 `run cpp` 演示命令，並同步更新 smoke/文件流程。
- 2026-04-24：`Phase 23` 完成，`run --help` 與 `run --status` 已上線。
- `Phase 21` 的 `ls`/`cat` 目前仍為唯讀 initfs 檢視能力。
- `Phase 20` `run hello` 路徑先走 kernel fallback，後續保留 ring3 切換開關。
- `Phase 23` `run` 子命令已上線，加入 user/kernel 模式資訊與說明輸出。
- 2026-04-24：`Phase 24` 完成，新增 `help run` 與 `run help` 快速查詢，並讓 `run --status` 顯示索引與摘要。
- 2026-04-24：`Phase 25` 完成，加入 `run list` 作為命令別名與幫助路徑一致化補強。
- 2026-04-24：`Phase 26` 啟動完成，新增主機端 `C/C++` 編譯流程，建立可重複 `make host-programs` 驗證路徑。
- 2026-04-24：`Phase 27` 加入 `cap` / `capabilities`，提供能力矩陣與目前不支援項目（Linux 應用、Python 內部 runtime）查詢。
- 2026-04-24：`Phase 28` 補齊 host-side 編譯可追蹤輸出（`status`、`flags`、`sha256`、`request`）與調整腳本可診斷性。
- 2026-04-24：`Phase 29` 加入主機端靜態連結模式（`MHOST_STATIC=1`）與 `link_mode` metadata，建立可重複建置報告。
- 2026-04-24：`Phase 30` 新增 Linux ABI 預覽路徑，支援最小 syscall 子集合（`write/getpid/exit`）與 `run linux-abi` 驗證命令。
- 2026-04-24：`Phase 31` 擴充 Linux ABI 預覽 syscall，新增 `writev/brk/uname/gettid/set_tid_address/arch_prctl/exit_group`，並讓 fallback 可直接跑 probe。
- 2026-04-24：`Phase 32` 新增 `run elf-inspect` 與可重生的 ELF 樣本更新流程（`make refresh-elf-sample`）。
- 2026-04-24：`Phase 33` 新增 `make test-elf-sample`，將 ELF 樣本重生流程納入回歸測試。
- 2026-04-24：`Phase 34` 新增 `/tmp` 可寫 overlay VFS 與 shell `write/append/touch/rm` 指令，並加入 `make test-vfs-rw`。
- 2026-04-24：`Phase 35` 新增 scheduler 任務控制命令（`task start/stop/reset/list`）與 `make test-scheduler-ctl`。
- 2026-04-24：`Phase 36` 新增 VMM 骨架（map/unmap + region metadata）與 `brk` 狀態接線，並加入 `make test-vmm-basic`。
- 2026-04-24：`Phase 37` 新增 user image loader 骨架（`run elf-load`）與 `make test-userimg-loader`，提供 ELF `PT_LOAD` 到 VMM metadata 的最小接線。
- 2026-04-24：`Phase 38` 擴充 user image 執行上下文（加入 `userimg-stack` 映射與 stack report），讓後續 user-mode handoff 有固定入口資料。
- 2026-04-24：`Phase 39` 完成 `userproc_handoff_dry_run`，把 `run elf-load` 的 entry/stack 規劃接入可測試的 handoff 前置驗證。
- 2026-04-24：`Phase 40` 新增 `execve` 風格 stack scaffold（`argc/argv/envp/auxv`），並將結果納入 `run elf-load` 與 `test-userimg-loader` 驗證。

## 檢查前提

每個階段都必須滿足：

1. `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通過
2. serial 8 秒內看到 `hello from kernel`
3. 測試失敗有可讀診斷（不只 timeout）

## Phase 0：基準穩定

目的：建立可回歸的啟動標準與錯誤診斷。  
預期成效：固定 boot 設定、request 區段與 smoke 的結構化輸出。

必做：

- 驗證 `boot/limine.conf`、`build/iso_root/boot/limine.conf` 的  
  `PROTOCOL=limine`、`KERNEL_PATH=boot:///boot/mvos.bin`
- 補齊 smoke 失敗訊息與重試流程
- 在 `README` 補上完整前置、建置與預期輸出

驗收：

- 8 秒內看到 `hello from kernel`
- 缺少 Limine/bootloader 時報錯有清楚原因

## Phase 1：早期可觀測性

目的：確保 kernel 進入 C 前後可追蹤。  
預期成效：早期 `serial` 輸出順序穩定，panic/fault 有一致標頭。

必做：

- 確認序列化啟動輸出（`serial_init` 早於其他輸出）
- 驗證 early entry 與 `limine` 請求段可被 `readelf` 讀取
- 將 panic/fault path 走向 `serial` 對齊

## Phase 2：記憶體管理

目的：建立 HHDM + memmap + PMM/heap 的最低可用鏈路。  
預期成效：可穩定列出實體頁資訊並完成一次基礎分配測試。

必做：

- `pmm_init` 前後輸出 memory map 與頁面統計
- `kmalloc`/`kfree` 行為在 boot 中可觀測
- `main` 記錄 `free/total/allocated` 統計

## Phase 3：中斷與節拍

目的：加入最小時鐘驅動，讓系統有 timer timebase。  
預期成效：`timer_ticks()` 持續上升，IRQ handler 可穩定進入。

必做：

- PIT 設定 timer 時間片
- 安裝 ISR（timer + 例外向量）
- timer 訊息可供 scheduler 與測試邏輯使用

## Phase 4：輸入與 shell（可選擴充）

目的：保留互動介面骨架以利後續實作。  
預期成效：鍵盤與 shell 程式可獨立編譯啟用，不阻塞 core boot path。

必做：

- 確認 `kernel/core/shell.c` 和輸入路徑可切換啟用
- serial I/O 為主輸出通道，不以 framebuffer 為依賴

## Phase 5：任務切換骨架

目的：加入最小 cooperative scheduler。  
預期成效：兩個測試任務可按 tick 輪詢輸出 trace，不改變 boot 基本流程。

必做：

- 任務註冊表（name/entry/runs/active）
- timer tick 觸發 `scheduler_run_once()` 切換邏輯
- `scheduler_ticks()` 提供測試可觀測時間軸

## Phase 6：最小 VFS / initramfs

目的：加入只讀檔案入口，作為 loader 與診斷前置。  
預期成效：`list/open/read/close/missing` 行為可在 boot 初段驗證。

必做：

- 內建 `/boot/init/*` 測試節點（固定字串）
- `vfs_open/vfs_read/vfs_close/vfs_list` 實作
- boot-path 診斷：`vfs_diagnostic_list/read_file/missing`

## 下一步階段建議

- Phase 8：FrameBuffer GUI（完成）
  - 啟用 VGA 文字緩衝鏡射輸出
  - 維持 serial 作為 smoke 主要通道
- Phase 9：Framebuffer request 探測（完成）
  - 補齊 `limine.h` 的 framebuffer request 協定定義
  - 以 `limine_framebuffer_request` 檢查 framebuffer 回應並記錄輸出
  - 保留既有 VGA 文字鏡射作為可視 fallback
- Phase 10：GUI boot window（完成）
  - 啟用 framebuffer 圖形輸出後繪製視窗 demo
  - 避免改動 smoke 的核心序列與文字序列輸出
- Phase 11：互動 shell（可選）
  - 以 `ENABLE_SHELL=1` 進入 `shell_run()`
  - 保留預設流程不阻塞，維持 smoke 時間線一致
- Phase 12：shell GUI 指令（可選）
  - 在 shell 中加入 `gui` 指令
  - 驗證已啟用 graphics backend 時可再次繪製視窗
  - 保持 `make smoke-offline` 不受互動指令干擾
- Phase 13：第一個視窗程式（可選）
  - 在 shell 中加入 `app` 指令
  - 透過 framebuffer 繪製可辨識的第二個視窗層次，模擬第一個可執行視窗程式
  - 保持預設 boot/smoke 流程不阻塞（僅 ENABLE_SHELL 路徑可觸發）
- Phase 14：GUI 應用樣式擴展（可選）
  - 在 `app` 指令新增 `alt` 子模式，繪製不同視窗外觀與佈局
  - 使用同樣不可阻塞流程與 serial 訊息回報：`GUI alt app launched.`
  - 驗證目標：`app` 與 `app alt` 均可在 shell 互動下各自啟動
- Phase 15：GUI app 管理介面（可選）
  - 在 `app` 指令加入 `list` 與 `status`
  - `app status` 需輸出 frame buffer 狀態（如 width/height/pitch/bpp）
  - 保持既有 `app` 與 `app alt` 行為不變，僅新增查詢能力，不新增阻塞邏輯
- Phase 16：GUI app 指令路徑（可選）
  - 為 `app` 新增明確的 `launch` 子指令，支援 `app launch app`、`app launch alt`
  - 保持 `app` 預設行為向前相容，保持非阻塞性質
- Phase 17：GUI app 資訊查詢（可選）
  - 新增 `app info <name>`，回報目標 app 的簡要描述與呼叫路徑
  - 對不存在 app 回報 `unknown app: <name>`
  - 保持既有 `list/status/launch` 行為與輸出格式穩定
- Phase 18：任務可見性（可選）
  - 為 `scheduler` 建立任務快照 API
  - 在 `shell` 新增 `tasks` 指令，輸出 `task-a`、`task-b` 等任務名稱與執行次數
  - 維持 `smoke` 時序，維持 serial 為主輸出
- Phase 20：使用者模式最小路徑（完成）
  - 以可切換旗標保留 `run` 指令入口，預設路徑落在 kernel-mode fallback
  - 為後續真正 ring3 移轉預留最小介面
- Phase 21：檔案可見性（可選）
  - shell 新增 `ls`、`cat`，讓訓練系統可查詢 `/boot/init` 內建節點
  - `cat` 仍走唯讀流程，保留 boot 時序穩定
- Phase 22：基本 C++ 示範（完成）
  - 在 `userapp` 管線加入 C++ 對應編譯路徑，提供 `run cpp` 範例 app
  - `Python` 仍停留在「未提供 runtime」狀態，現階段改以主機端 `python3 scripts/dev_status.py` 驗證環境
- Phase 23：`run` 觀測性提升（完成）
  - `run` 新增 `--help` 與 `--status`，可輸出 app 列表與執行模式標記（user/kernel）
  - 維持既有 `run <name>` 行為與 smoke 路徑不變
  - `run -h` 支援相同幫助行為，降低指令門檻
- Phase 24：help 路徑一致性（完成）
  - 讓 `help run` 進入 run 專屬說明，提升指令發現性
  - `run help` 視為 `run --help` 別名，降低操作摩擦
  - `run --status` 新增 app 索引與 user/kernel 總覽摘要（`summary: user=... kernel=...`）
- Phase 25：run 命令命名一致性（完成）
  - 新增 `run list`，與 `run` 無參數行為一致。
  - 保持 `help run` / `run help` / `run --help` 一致性，降低操作摩擦。
- Phase 26：主機端程式編譯（完成）
  - 維護 `samples/user-programs/*`，保留可直接複製擴充的 C/C++ 範例原始碼
  - 用 `make host-programs` 編譯主機可執行程式，輸出 `build/host-programs/manifest.json`
  - 下階段目標：將 manifest 導入 miniOS loader，讓 `run` 支援載入與執行外部程式
- Phase 27：能力邊界透明化（完成）
  - 在 shell 加入 `cap`/`capabilities`，直接輸出可執行 app、host build 能力與 Linux 可執行檔限制
  - 明確告知 `transmission/htop/nano` 之類 Linux 使用者空間軟體需完整使用者載入器與 libc 才能執行
- Phase 30：Linux ABI 預覽骨架（完成）
  - userproc 對齊第一批 Linux x86_64 syscall 編號（1, 39, 60）並保留教學輸出。
  - `run linux-abi` 提供最小 userspace 呼叫路徑驗證；目前僅示範 ABI，不含 ELF loader / libc 相容層。
- Phase 31：Linux ABI 預覽擴展（完成）
  - userproc 擴充到常見初始化 syscall 子集合（`writev/brk/uname/gettid/set_tid_address/arch_prctl/exit_group`）。
  - 在 `MINIOS_PHASE20_USER_MODE=0` 的預設情境下，`run linux-abi` 也會透過 fallback 顯示 syscall probe 結果，避免只能停留在「不可驗證」狀態。
- Phase 32：ELF64 檢查與樣本重生（完成）
  - 新增 `run elf-inspect`，直接在 miniOS 內核路徑檢查 embedded Linux ELF metadata。
  - 新增 `samples/linux-user/hello_linux_tiny.S` 與 `make refresh-elf-sample`，讓 `kernel/core/elf_sample_blob.c` 可重現更新。
- Phase 33：ELF 樣本回歸化（完成）
  - 新增 `scripts/test_elf_sample.sh` 與 `make test-elf-sample`，檢查 ELF magic、symbol 與 blob byte count 下限。
  - 讓 ELF 樣本刷新路徑可在每次修改後快速驗證，不需要手動比對 blob 內容。
- Phase 34：可寫入檔案系統最小路徑（完成）
  - VFS 在保留 `/boot/init` 唯讀節點外，新增 `/tmp` 可寫 overlay（建立、覆寫、附加、刪除）。
  - shell 新增 `write/append/touch/rm` 命令，讓使用者可直接做檔案內容變更。
  - 新增 `scripts/test_vfs_rw.sh` 與 `make test-vfs-rw`，補齊 host-side 行為回歸。
- Phase 35：任務控制可操作化（完成）
  - scheduler 新增 task active 控制與 run counter reset API。
  - shell 新增 `task start/stop/reset/list`，支援透過名稱或 index 操作任務。
  - 新增 `scripts/test_scheduler_ctl.sh` 與 `make test-scheduler-ctl`，補齊 host-side 行為回歸。
- Phase 36：虛擬記憶體骨架（完成）
  - 新增 `vmm` 模組，提供最小 `map/unmap/region list` 介面與 flags/tag metadata。
  - 新增 `vmm_user_heap_init` / `vmm_user_brk_set`，將 Linux ABI `brk` 路徑納入同一狀態模型。
  - 新增 `scripts/test_vmm_basic.sh` 與 `make test-vmm-basic`，驗證 VMM 介面與 brk 邊界行為。
- Phase 37：使用者映像載入骨架（完成）
  - 新增 `userimg` 模組，解析 embedded ELF 的 `PT_LOAD` 佈局並映射到固定 user image 區段（layout-only）。
  - 新增 `run elf-load` 命令路徑，輸出 mapped entry、mapped range、mapped regions/bytes。
  - 新增 `scripts/test_userimg_loader.sh` 與 `make test-userimg-loader`，驗證重複載入下 VMM 行為穩定。
- Phase 38：使用者映像執行上下文骨架（完成）
  - `userimg` 佈局新增 user stack 映射（`userimg-stack`），並輸出 `stack_base/stack_top/stack_size`。
  - `run elf-load` 可同時觀察 image 及 stack 佈局，提供後續切換到真實 userspace 前置資訊。
  - `make test-userimg-loader` 補驗 stack region tag、flags 與 report 一致性。
- Phase 39：handoff dry-run 驗證（完成）
  - 新增 `userproc_handoff_dry_run`，檢查 user entry 位於可執行 userimg 區域、stack top 對齊且位於可寫 user stack 區域。
  - `run elf-load` 直接輸出 handoff dry-run 狀態，讓載入結果可立即驗證是否可進一步 handoff。
  - `make test-userimg-loader` 新增 dry-run 成功與失敗案例，避免未來改動時退化。
- Phase 40：exec stack 佈局骨架（完成）
  - 新增 `userproc_prepare_exec_stack`，可建構 `argc/argv/envp/auxv` 的初始 userspace stack 內容（scaffold）。
  - `run elf-load` 新增 exec stack prep 結果與 `rsp/argv/envp` 報告輸出。
  - `make test-userimg-loader` 補驗 stack payload、auxv terminator 與失敗路徑。

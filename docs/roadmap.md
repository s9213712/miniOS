# miniOS Roadmap（教學用）

每一個 phase 都曾經有獨立 `phase-*` 分支，完成後已併入 `main`，保留主線歷史作為學習紀錄。  
本文件提供每個階段的目的、預期成效與最小可驗證成果，方便學員對照 `main`、回頭閱讀歷史。

## 分支保留原則

- `main` 保留可開機主線，所有已完成階段都以歷史提交存在
- 每個 stage 的差異可由 commit 記錄或標籤回顧，不再保留長期 phase 分支

## 檢查前提

每個階段都必須滿足：

1. `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 通過
2. serial 8 秒內看到 `hello from kernel`
3. 測試失敗有可讀診斷（不只 timeout）

## Phase 0：基準穩定

目的：建立可回歸的啟動標準與錯誤診斷。  
預期成效：固定 boot 設定、request 區段與 smoke 的結構化輸出。

必做：

- 驗證 `boot/limine.conf`、`boot/iso_root/boot/limine.conf` 的  
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
- Phase 9：Framebuffer request 探測（Current）
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

# miniOS Roadmap（教學用）

每一個 phase 都曾經有獨立 `phase-*` 分支，完成後以 commit 形式保留在主線歷史中，作為學習紀錄與回溯點。  
本文件提供每個階段的目的、預期成效與最小可驗證成果，方便學員對照 `main`、回頭閱讀歷史。

## 分支保留原則

- `main` 為可開機主線（最終目標），其階段歷史在每次 phase 提交中可回溯。
- 本教學保留變更節點與測試紀錄；在本分支也可直接沿用分支作為教學實作線，實務上可視情況再合併到 `main`。

## 最近進度摘要

- 2026-04-24：`Phase 22` 完成，已補齊 `run cpp` 演示命令，並同步更新 smoke/文件流程。
- `Phase 21` 的 `ls`/`cat` 目前仍為唯讀 initfs 檢視能力。
- `Phase 20` `run hello` 路徑先走 kernel fallback，後續保留 ring3 切換開關。

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

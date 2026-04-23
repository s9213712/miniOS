# miniOS Roadmap

This roadmap is organized by one branch per phase.  
每個階段完成後，先 merge 回主線再進入下一個階段，避免跨階段大改。

## Baseline and Scope
- Baseline branch: `main`
- 本文件列出的階段以 `phase-*` 開頭為獨立分支名稱
- 所有變更以目前可開機 + `make smoke-offline` 通過為前提

## Phase 0：基準穩定（`phase-0-baseline-hardening`）
- 目標：固定目前開機成功條件，讓 CI 與 smoke 可預測、錯誤可追蹤。
- 主要工作
  - 驗證 `boot/limine.conf` 與 `boot/iso_root/boot/limine.conf` 的 `PROTOCOL`、`KERNEL_PATH` 對應 `/boot/mvos.bin`。
  - 完整化 `make smoke-offline` timeout/error message 輸出。
  - 在 `README.md` 明確寫入：必要前置條件、建置步驟、預期 serial 字串、失敗診斷。
- 驗收標準
  - `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline` 8 秒內輸出 `hello from kernel`。
  - 未通過時，輸出清楚失敗階段與原因（非僅 `timeout`）。

### Phase 0 具體可執行清單（順序）
1. 建立階段分支：
`git checkout main && git checkout -b phase-0-baseline-hardening`
2. 檢查並更新 limine 設定（兩份檔）：
`boot/limine.conf`、`boot/iso_root/boot/limine.conf`
   - `PROTOCOL=limine`
   - `KERNEL_PATH=boot:///boot/mvos.bin`
3. 統一 kernel 輸出規則並確保與預期一致。
4. 跑完整 smoke：
`LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
5. 不通過時補診斷輸出：
   - 讀取 `scripts/test_smoke.sh` 與 `scripts/make_iso.sh` log 點位
   - 在失敗點列印 `[phase-0] failed at ...` 及下一步建議
6. 更新 `README.md`：
   - 將 `make smoke-offline`、必要依賴、預期 serial 訊息、問題排查整理成一節
7. 重新測：
`LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`（通過為止）
8. commit 與 merge 指引：
`git add ... && git commit -m "phase-0: baseline hardening and smoke diagnostics"`  
`git checkout main && git merge --no-ff phase-0-baseline-hardening`

## Phase 1：輸出與 early-boot 可觀測性（`phase-1-early-observability`）
- 目標：早期啟動可觀測性最小化但一致。
- 主要工作
  - 確認 `kernel/arch/x86_64/boot/entry.asm` 早期 COM1 serial 初始化與最前段輸出穩定。
  - 統一 `miniOS: kernel entry`、`hello from kernel` 的啟動訊息格式。
  - 補齊 panic/assert 日誌到 serial 的一致路徑與前綴。
- 驗收標準
  - serial 前段輸出順序固定且可 grep。
  - 觸發故障時輸出錯誤碼/節點與上下文。

## Phase 2：記憶體管理加固（`phase-2-memory-management`）
- 目標：PMM / heap 從測試用走向可持續使用。
- 主要工作
  - 完善 `kernel/mm/pmm.c` 取頁邊界檢查、對齊與失敗回報。
  - 完成最小可回收 `kfree` 流程，或明確標記目前不支持並阻止假成功回傳。
  - 設計簡單 allocator 統計輸出，避免無限擴張/重複釋放。
- 驗收標準
  - 重複分配/釋放壓測不崩潰，分配失敗有明確碼。
  - `main` 中的基本 heap 自測通過（分配後、釋放後狀態可核對）。

## Phase 3：中斷與時間基礎（`phase-3-interrupt-timer`）
- 目標：建立可預期的 event loop 前置條件。
- 主要工作
  - 檢視/固定 IDT/GDT 初始化順序與 handler 對應。
  - 加入 timer IRQ（先以最小可用 PIT，後續可替換為 APIC）。
  - 增加 `ticks` 計數器與 1ms/10ms 對外可見輸出接口。
- 驗收標準
  - 啟動後有穩定 tick 增長（可由 serial 查證）。
  - IRQ/fault handler 可辨識並持續運作，不會因未註冊 handler 直接 triple fault。

## Phase 4：輸入與 shell（`phase-4-input-shell`）
- 目標：建立最低層互動能力，驗證 runtime 邏輯。
- 主要工作
  - 啟用並測試 `kernel/dev/keyboard.c` 輸入流程。
  - 接上 `kernel/core/shell.c`，新增 `help`、`mem`、`ticks`、`reboot`、`halt`。
  - 讓 shell 走 serial I/O，避免 framebuffer 仍阻塞進度。
- 驗收標準
  - 可通過串列鍵入指令並回傳結果。
  - 非預期鍵位輸入不會卡死或造成 kernel panic。

## Phase 5：任務切換骨架（`phase-5-scheduler`）
- 目標：建立最小 task abstraction 及 context 切換能力。
- 主要工作
  - `Task` 結構、task list、context save/restore。
  - 建立 2 個空任務與 yield/test，使用 timer 觸發切換。
  - 加入簡單 stack 佈局保護與 `current_task` 檢查。
- 驗收標準
  - 啟動後能輪流執行兩個任務並持續輸出 trace。
  - 任務切換時不破壞 kernel 的 serial 輸出與堆管理狀態。

## Phase 6：暫存檔案系統（`phase-6-vfs-initramfs`）
- 目標：提供最小 I/O 與資源載入入口。
- 主要工作
  - 引入 initramfs 掛載流程（只讀）。
  - 實作極小 VFS 介面：`open/read/close/list`。
  - 用固定映像中的測試檔驅動 loader/diagnostic。
- 驗收標準
  - 能讀取至少 1 個內建測試檔並輸出檔案長度/校驗。
  - 找不到檔案時回報一致錯誤碼，不異常結束。

## Phase 7：發行與整合（`phase-7-release`）
- 目標：整合回主線並形成可維護節奏。
- 主要工作
  - 將各 phase 驗收結果整理進 `CHANGELOG`/`README`。
  - 更新 smoke 測試腳本，新增 regression checklist。
  - 清理過時文件，保留單一權威文件。
- 驗收標準
  - `make smoke-offline` 全量成功。
  - 主線具備「本地開發→測試→合併」一致流程。

## 里程碑節奏（建議）
1. 每次只做一個分支：完成 `phase-*` 後 merge 回 `main`。  
2. 每個階段保留最小必要改動，不做尚未納入該階段目標的重構。  
3. 當某階段失敗先修補同階段，必要時回退到該分支，不拖累下一個階段。

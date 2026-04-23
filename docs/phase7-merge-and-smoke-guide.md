# Phase 7 合併流程（文字教學）

此文件為純文字流程，目標是 **取消不合併分支限制**，將所有教學 phase 分支依序合併到 `main`，並確認 `main` 可啟動 smoke 測試再推送。

## 目標
- 不刪除任何分支，保留所有 `phase-*` 分支供教學追蹤。
- 先合併全部分支到 `main`，只要合併成功就立刻記錄結果。
- 合併後至少跑一次 boot smoke。
- 確認 `main` 可推到遠端（GitHub）。

## 階段 1：整理工作目錄
1. 確認狀態為乾淨：
   - `git status --short --branch`
2. 若有尚未提交變更，先 `git stash push -u -m "..."`
   - 這一步可避免未完成修正干擾合併流程。

## 階段 2：逐一合併分支
在 `main` 上依序執行：
- `git merge phase-0-baseline-hardening -m "merge phase-0-baseline-hardening into main for test"`
- `git merge phase-1-early-observability -m "merge phase-1-early-observability into main for test"`
- `git merge phase-2-memory-management -m "merge phase-2-memory-management into main for test"`
- `git merge phase-4-input-shell -m "merge phase-4-input-shell into main for test"`
- `git merge phase-5-scheduler -m "merge phase-5-scheduler into main for test"`
- `git merge phase-6-vfs-initramfs -m "merge phase-6-vfs-initramfs into main for test"`

實作結果：全部為 `Fast-forward`，未出現衝突。

## 階段 3：驗證 smoke
1. `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
2. 觀察輸出需包含：
   - `test_smoke Boot text found.`
   - `Phase 3 memory map verified.`
   - `PASS`

實作結果：`smoke-offline` 成功通過。

補測：
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make test-smoke`
- 需顯示 `PASS`

## 階段 4：推上主線
- `git push -u origin main`

實作結果：推送成功，`origin/main` 由 `6b56a0a` 前進到 `7522cf4`。

## 本次重點觀察
- 合併過程只做「主線整併」測試，不做大型重構。
- 所有 phase 分支仍保留在本地，可用於後續教學對照與回滾。
- 若日後再次需要，請先還原暫存後再重新套用（`git stash list` 可檢查）。

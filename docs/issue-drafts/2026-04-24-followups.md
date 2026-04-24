# 2026-04-24 Follow-up Issues

本文件整理自 2026-04-24 狀態檢查，可直接拆成 GitHub issues。

## Issue 1

Title: `Finalize Phase 29 into a clean commit`

Problem:

- `README.md`、`docs/commands.md`、`docs/roadmap.md`、`kernel/core/shell.c`、`scripts/build_user_programs.py` 已反映 `Phase 29`
- 但主線歷史仍停在 `Phase 28`
- 版本敘事與 git 歷史暫時不一致

Why it matters:

- 後續 phase 疊加時，容易混淆哪一批變更屬於 `Phase 29`
- 教學型 repo 的回溯價值會下降

Proposed fix:

- 將 `Phase 29` 的 code/doc 變更整理為單一提交
- commit message 直接表明 host static build 與 manifest metadata 的變更
- 補一段簡短 release note，說明 `MHOST_STATIC=1` 的支援範圍

Acceptance criteria:

- `git log --oneline` 可明確看到 `Phase 29` 提交
- `README`、`roadmap`、shell `version` 指令一致回報 `Phase 29`
- `MHOST_STATIC=1 make host-programs` 成功，manifest 含 `default_link_mode` 與每個 program 的 `link_mode`

Priority: `P1`

## Issue 2

Title: `Move generated ISO staging artifacts out of the tracked boot tree`

Problem:

- `boot/iso_root/` 會在建置/測試後出現在工作樹
- `.gitignore` 沒有吸收這個 generated path

Why it matters:

- 工作樹持續變髒
- 容易誤把 generated files 提交進 repo

Proposed fix:

- 將 `boot/iso_root/` 加入 `.gitignore`
- 更進一步把 staging path 移到 `build/iso_root/`
- 更新 `make iso` / `smoke` 腳本的路徑引用

Acceptance criteria:

- 執行 `make iso` 或 `make smoke-offline` 後，`git status --short` 不再出現 `boot/iso_root/`
- 建置流程與 smoke 流程仍通過

Priority: `P1`

## Issue 3

Title: `Add regression coverage for host-program link-mode metadata`

Problem:

- `Phase 29` 新增 `MHOST_STATIC=1` 與 manifest `link_mode`
- 目前缺少明確自動化檢查保護這些欄位

Why it matters:

- 後續 refactor 可能讓 manifest 欄位消失或回退，但不會立刻被發現

Proposed fix:

- 增加一個 host-program regression test
- 至少覆蓋 dynamic / static 兩種建置模式
- 驗證 manifest 中的 `default_link_mode`、program `link_mode`、`flags`

Acceptance criteria:

- dynamic / static 兩種模式皆有自動化檢查
- 缺少 `link_mode` 或 `default_link_mode` 時測試失敗
- 文件示例與測試契約一致

Priority: `P2`

## Issue 4

Title: `Align dev_status.py wording with Make targets`

Problem:

- `scripts/dev_status.py --build-programs` 的 help 文字與輸出訊息仍提到 `make build-host-programs`
- 實際 target 名稱是 `make host-programs`

Why it matters:

- CLI 指示與 Makefile 不一致
- 使用者容易誤以為 target 缺失

Proposed fix:

- 統一 help text、stdout 訊息與 README 指令名稱
- 若需要保留舊稱，可提供 alias target

Acceptance criteria:

- `--help`、執行輸出、README 全部一致使用 `host-programs`
- 執行 `python3 scripts/dev_status.py --build-programs` 時不再出現舊名稱

Priority: `P2`

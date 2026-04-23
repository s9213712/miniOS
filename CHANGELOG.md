# Changelog

## Phase 7 – Release Preparation

### Added
- 新增 `docs/phase7-merge-and-smoke-guide.md`：記錄所有 phase 合併流程、驗證命令與推送結果。
- 更新 `README.md` 與 `docs/roadmap.md`，同步 Phase 7 狀態（主線為主、分支已合併清理後以提交歷史追蹤）。

### Fixed
- 文件層級一致性修正：將分支管理與里程碑描述統一到實際 git 歷程（已移除「分支保留」表述，改為以 `main` 歷史回顧）。
- 為後續教學與維運保留清楚的「下一階段收斂條件」。

### Validation
- `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
- `make test-smoke`
- 兩者皆需通過且包含 `Boot text found`、`Phase 3 memory map verified`。

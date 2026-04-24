# miniOS 可用指令手冊

本文件記錄在目前開發環境中，miniOS 專案可直接使用的指令。

> 先決條件：
> ```bash
> cd /home/s92137/miniOS
> ```

> 目前進度摘要：`smoke` 主線穩定後，支援 `run`、`ls`、`cat`、`app`、`tasks`、`run cpp`，且 `run hello` 僅以 kernel fallback 執行。`run python` 已提供「未支援/需主機執行」提示。Python 尚未提供 miniOS 內部 runtime。

---

## 一、前置檢查（建議先做）

- 檢查工作目錄
  ```bash
  pwd
  git status --short
  ```
- 檢查可用工具
  ```bash
  command -v git
  command -v make
  command -v gcc
  command -v nasm
  command -v qemu-system-x86_64
  command -v xorriso
  ```
- 檢查項目檔案是否齊全
  ```bash
  rg --files
  ```

---

## 二、核心建置指令

- 完整建置（只建 ELF/bin）：
  ```bash
  make
  ```
- 清除 `build/` 再建置：
  ```bash
  make clean
  make
  ```
- 建 ISO：
  ```bash
  make iso
  ```
- 預先取得 Limine 相關 artifacts（可選）：
  ```bash
  make prefetch-limine
  ```
- 僅做健康檢查（不重建）：
  ```bash
  python3 scripts/dev_status.py
  ```
- 完整檢查並重建：
  ```bash
  python3 scripts/dev_status.py --build
  ```

---

## 三、執行與 Smoke 測試

- 一般啟動 QEMU：
  ```bash
  make run
  ```
- 只做 smoke 指令流程（含完整建置）：
  ```bash
  make test-smoke
  ```
- 完整 smoke：
  ```bash
  make smoke
  ```
- 離線 smoke（推薦，搭配本機 Limine）：
  ```bash
  LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
  ```
- 僅建置 smoke，不跑 QEMU：
  ```bash
  SKIP_SMOKE_RUN=1 make smoke-offline
  ```

### 參數/環境說明

- `LIMINE_LOCAL_DIR=/tmp/limine-bin`：離線使用本機 Limine。
- `QEMU_TIMEOUT=8`：smoke timeout（`scripts/test_smoke.sh`）。
- `TEST_SMOKE_LOG_DIR=/tmp`：測試日誌存放目錄。
- `TEST_SMOKE_BASENAME=xxx`：測試 log 名稱。

---

## 四、故障測試與驗證

- 觸發 panic：
  ```bash
  PANIC_TEST=1 make run
  PANIC_TEST=1 LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
  ```
- 觸發 fault：
  ```bash
  FAULT_TEST=div0 make run
  FAULT_TEST=opcode make run
  FAULT_TEST=gpf make run
  FAULT_TEST=pf make run
  ```

---

## 五、Shell/互動開關（啟動後）

- 進入互動 shell（僅在內核啟用時）
  ```bash
  ENABLE_SHELL=1 make run
  ENABLE_SHELL=1 LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
  ```

- shell 常用命令（互動模式）：
  - `help`
  - `mem`
  - `ticks`
  - `tasks`
  - `ls`
  - `cat`
  - `app`
  - `app launch <name>`
  - `app info <name>`
  - `app alt`
- `run`
- `run hello`
- `run ticks`
- `run scheduler`
- `run cpp`
- `run python`（目前僅回報 Python 尚未支援於 miniOS）
- `gui`
- `reboot`
- `halt`

- miniOS 本體目前沒有 Python 直譯器（不能直接執行 `.py`），如需 Python 檢查請使用 host 的 `scripts/dev_status.py`。
- C 應用是直接編譯進 miniOS 核心，透過 `run <name>` 在 shell 內叫用（例如 `run hello`、`run cpp`）。
- `run python` 目前僅回傳：
  `Python not available in miniOS runtime yet. Use host-side execution, e.g.:
  python3 scripts/dev_status.py`
- `ls` 與 `cat` 目前只支援 initfs 內建唯讀節點，無法建立或修改檔案。
- `run cpp` 為目前 C++ 使用者應用示範，仍以 kernel-mode fallback 路徑實作。

---

## 六、Git 常用流程

- 查看目前分支與提交歷史：
  ```bash
  git status --short
  git branch --show-current
  git log --oneline -n 20
  ```
- 檢查差異：
  ```bash
  git diff
  git diff --stat
  ```
- 提交：
  ```bash
  git add <file...>
  git commit -m "Your message"
  ```
- 發佈（需可達 GitHub）：
  ```bash
  git push -u origin <branch>
  ```

---

## 七、文件與清單

- 查看文件：
  ```bash
  sed -n '1,200p' README.md
  sed -n '1,220p' docs/roadmap.md
  sed -n '1,220p' docs/architecture.md
  sed -n '1,200p' CHANGELOG.md
  ```
- 列出專案文件：
  ```bash
  rg --files docs
  ```

---

## 八、常用排查

- 若 `make smoke-offline` 失敗：
  - 確認 `boot/limine.conf` 與 `boot/iso_root/boot/limine.conf` 的 `PROTOCOL=limine` 與 `KERNEL_PATH=boot:///boot/mvos.bin`
  - 確認 `LIMINE_LOCAL_DIR` 有指到正確目錄
  - 檢查 `build/mvos.elf` 是否有 `.requests`（`readelf -S build/mvos.elf | grep .requests`）
- serial 8 秒內未見 `hello from kernel` 時，先確認 `serial_init` 與 Limine handoff log 是否都存在，並改看 `/tmp` 下 smoke 日誌最後 80 行
- 清空並重試：
  ```bash
  make clean
  LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
  ```

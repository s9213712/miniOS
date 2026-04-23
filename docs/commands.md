# miniOS 可用指令手冊

本文件記錄在目前開發環境中，miniOS 專案可直接使用的指令。

> 先決條件：
> ```bash
> cd /home/s92137/miniOS
> ```

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
  - `gui`
  - `reboot`
  - `halt`

- 目前沒有提供 Python 直譯器（不能直接執行 `.py`）。
- C 應用是直接編譯進 miniOS 核心，透過 `run <name>` 在 shell 內叫用（例如 `run hello`）。
- `ls` 與 `cat` 目前只支援內建 initfs（唯讀）路徑，無法進行實際檔案建立/修改。
- C++ 目前可當作 `run cpp` 的最小示範 app（目前仍是 kernel-mode fallback，非一般使用者程式環境）。

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
- 清空並重試：
  ```bash
  make clean
  LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline
  ```

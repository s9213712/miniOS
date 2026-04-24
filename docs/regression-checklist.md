# Phase 7 Regression Checklist

## 啟動前提
- `LIMINE_LOCAL_DIR` 指向有效的 local Limine 目錄（例如 `/tmp/limine-bin`）或已準備 `prefetch` 快取。
- 使用乾淨樹（`git clean` 前提下）重跑，確保不受舊建構檔干擾。
- 每次變更結束都執行：
  - `make clean`
  - `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-offline`
  - `LIMINE_LOCAL_DIR=/tmp/limine-bin make smoke-execve-demo`

## 必要輸出檢查
- serial 在 8 秒內出現 `MiniOS Phase 3 bootstrap`
- serial 在 8 秒內出現 `hello from kernel`
- 驗證段輸出包含：
  - `Boot text found.`
  - `Phase 3 memory map verified.`
  - `kernel entering C`（kernel 進入 C 入口）
  - `PASS`

建議同時確認：
- `readelf -S build/mvos.elf | grep ".requests"` 存在
- `boot/limine.conf` 與 `build/iso_root/boot/limine.conf` 皆有 `PROTOCOL=limine` 與 `KERNEL_PATH=boot:///boot/mvos.bin`
- `make test-host-programs` 通過（manifest `default_link_mode` / `link_mode` 回歸）
- `make smoke-offline` 輸出 `Execve demo verified.`

## 失敗診斷要點
- 若 timeout：  
  - 檢查 ISO 內 `boot/limine.conf` 的 `PROTOCOL=limine` 與 `KERNEL_PATH=boot:///boot/mvos.bin`
  - 檢查 `scripts/make_iso.sh` 是否更新 `boot/limine.conf` 到 `build/iso_root/boot/`
- 若 serial 無內容：  
  - 確認 Limine serial 有被啟用且 kernel entry 早期已進入 `serial_init`
  - 確認 kernel 檔案有正確載入（`build/mvos.bin`）
  - 建議加上 `SMOKE_KEEP_LOGS=1 SMOKE_BASENAME=smoke-$(date +%s)` 保留每次失敗日誌
- 若 memory checks 失敗：  
  - 檢查 `limine` bootloader request 回應非空
  - 檢查 PMM 初始化是否在 `gdt/idt` 後順序執行

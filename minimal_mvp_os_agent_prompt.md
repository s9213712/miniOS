# 最小可用作業系統（MVP OS）建議與 Agent 提示詞

## 1. 目標先定義清楚：你要的「最小可用」是什麼？

對個人 OS 專案來說，「最小可用」不要一開始就碰：
- 完整驅動框架
- 多工排程器完整版
- 虛擬記憶體大系統
- 檔案系統全實作
- 網路堆疊
- GUI

建議把 MVP 定義成：

### MVP-0：能開機的核心
- 透過 bootloader 載入 kernel
- 進入 64-bit long mode
- 成功輸出 `hello from kernel`
- 能在 QEMU 穩定啟動與重開

### MVP-1：可互動的小系統
- 螢幕輸出（framebuffer 或文字輸出）
- 鍵盤輸入
- 簡單 shell
- 幾個內建指令：`help`, `clear`, `echo`, `reboot`, `meminfo`

### MVP-2：勉強算 OS
- 基本中斷
- 簡單 page allocator / heap
- timer tick
- 單工或非常簡單的 cooperative task
- initramfs 或簡單記憶體內建檔案表

這樣你能很快做出「像 OS 的東西」，而不是陷入半年都在修 boot / linker / paging 的黑洞。

---

## 2. 最建議的技術路線

### 架構
- **x86_64**
- 先只支援 **QEMU**
- 不要先碰真機

### 開機方式
- **Limine boot protocol**
- 理由：比自己從 BIOS/UEFI 手搓切模式輕鬆很多，能更快進入 kernel 主體

### 語言
- **C 為主**
- 少量 assembly（entry / context / low-level glue）
- 暫時不要一開始就全 Rust、全 C++、全自製編譯鏈

### 工具鏈
- `clang` 或 `gcc`
- `ld.lld` 或 `ld`
- `nasm`
- `make` 或 `just`
- `qemu-system-x86_64`
- `gdb`

### 核心風格
- **monolithic kernel**
- 不要一開始做 microkernel
- 不要一開始追求可插拔模組系統

---

## 3. 為什麼我不建議你一開始碰的東西

### 先不要自己寫 bootloader
因為你真正想做的是 OS，不是先花大量時間在：
- BIOS int 13h
- UEFI boot services
- PE/ELF loading 細節
- page table handoff
- framebuffer negotiation 細節

### 先不要做完整 POSIX
會直接爆量：
- syscall ABI
- process model
- signals
- file descriptor
- fork/exec
- userspace C library

### 先不要做真硬碟檔案系統
建議先用：
- initramfs
- memory-backed pseudo FS
- 或內建 command table

---

## 4. 建議的 MVP 功能切分

## Phase 1：Boot 到 kernel
目標：
- QEMU 啟動 ISO / image
- kernel entry 正常執行
- framebuffer 或 serial 輸出 hello

必做：
- Limine 設定
- linker script
- kernel entry
- panic / halt
- serial logger

完成標準：
- `make run` 後能穩定看到 kernel 啟動訊息
- 錯誤時有 panic 訊息，不是直接黑屏

---

## Phase 2：基礎硬體抽象
目標：
- 有最基本 console 與 debug 能力

必做：
- serial console
- framebuffer text renderer 或 VGA text
- GDT / IDT
- interrupt stubs
- panic dump

完成標準：
- 能同時把 log 打到 serial
- 遇到 fault 至少知道是什麼 fault

---

## Phase 3：記憶體基建
目標：
- kernel 內部可安全配置記憶體

必做：
- 讀取 bootloader memory map
- page frame allocator
- bump allocator / kernel heap
- `kmalloc/kfree` 最小版（可先簡化）

完成標準：
- 能配置與釋放小塊記憶體
- 有 allocator 自測
- 不因 allocator 立刻炸掉

---

## Phase 4：輸入與 shell
目標：
- 做出「看得見可互動」的 OS 感

必做：
- PS/2 keyboard 或簡單輸入路徑
- line editor
- command parser
- built-in commands

建議指令：
- `help`
- `echo`
- `clear`
- `reboot`
- `halt`
- `ticks`
- `meminfo`

完成標準：
- 可鍵入指令並回應
- 系統可持續互動，不是只印一次字

---

## Phase 5：時間與簡單任務
目標：
- 讓系統開始有「活著」的感覺

必做：
- PIT / APIC timer（先從最簡單可行的開始）
- tick counter
- sleep/busy wait 抽象
- cooperative task 或簡單 work loop

完成標準：
- `ticks` 會跳動
- shell 不會因 timer 整個掛掉

---

## 5. 最小專案目錄建議

```text
mvos/
├─ boot/
│  ├─ limine.conf
│  └─ iso/
├─ kernel/
│  ├─ arch/x86_64/
│  │  ├─ boot/
│  │  ├─ gdt/
│  │  ├─ idt/
│  │  ├─ interrupt/
│  │  └─ mmu/
│  ├─ core/
│  │  ├─ main.c
│  │  ├─ panic.c
│  │  ├─ log.c
│  │  └─ shell.c
│  ├─ mm/
│  │  ├─ pmm.c
│  │  ├─ heap.c
│  │  └─ alloc.c
│  ├─ dev/
│  │  ├─ serial.c
│  │  ├─ fb_console.c
│  │  ├─ keyboard.c
│  │  └─ timer.c
│  └─ include/
├─ libc/
│  ├─ memset.c
│  ├─ memcpy.c
│  ├─ strlen.c
│  └─ printf_min.c
├─ scripts/
│  ├─ run_qemu.sh
│  ├─ debug_qemu.sh
│  └─ make_iso.sh
├─ tests/
│  └─ kernel_unit/
├─ linker/
│  └─ x86_64.ld
├─ Makefile
└─ README.md
```

---

## 6. 你應該優先保留的工程品質

### 一開始就要有
- `panic()` 機制
- assertion
- serial log
- `make run`
- `make debug`
- 固定可重現的 QEMU 啟動命令

### 一開始先不要過度設計
- plugin architecture
- driver manager 大系統
- VFS 大統一層
- 完整 syscall table
- user/kernel mode 全隔離
- SMP

---

## 7. 建議的 QEMU 開發方式

建議固定以 serial 為第一除錯出口，畫面為第二：
- 螢幕可能黑
- serial 通常還活著
- 你會比較快知道死在哪

建議：
- 預設 `-serial stdio`
- 保留 `-s -S` 給 GDB
- 所有 early boot log 都走 serial

---

## 8. 建議的 MVP 指令集

```text
help
echo hello
clear
ticks
meminfo
reboot
halt
panic-test
```

每個指令都很小，但能迫使你建立：
- parser
- dispatch table
- console output
- timer
- allocator
- error handling

這比單純印 hello world 更像真正的 OS 核心骨架。

---

## 9. 成功定義：做到哪裡算第一版完成？

### 建議你的 v0.1 完成線
1. 在 QEMU 可穩定開機
2. 有 serial 與 framebuffer/console 輸出
3. 有 IDT 與基本 fault handler
4. 有 page allocator / heap 最小版
5. 有 keyboard input
6. 有 shell 與 6 個以上 built-in commands
7. 有 timer ticks
8. 有 `panic`、`assert`、`make debug`

做到這裡，就已經不是玩具 hello kernel，而是可展示、可擴充的最小 OS 原型。

---

## 10. 不同野心對應的後續分支

### 路線 A：教育型 / 展示型
強化：
- shell
- framebuffer UI
- 小型記憶體工具
- 更漂亮的 log / monitor

### 路線 B：工程型
強化：
- paging
- virtual memory
- slab allocator
- VFS
- ramfs
- ELF loader

### 路線 C：研究型
強化：
- scheduler
- userspace
- syscall ABI
- process isolation
- copy-on-write
- IPC

---

## 11. 我最推薦你的 agent 執行策略

讓 agent 不要一次暴衝把「完整 OS」全刻出來。  
要要求它：

1. **以 Phase 為單位提交**
2. 每個 Phase 都要能編譯、能跑、能 debug
3. 每次只加少量功能
4. 先保證 boot/debug/path 正確
5. 任何複雜功能都要先提交設計說明，再實作

---

## 12. 可直接餵給 agent 的提示詞（精簡版）

```text
你現在是資深 systems programmer / kernel engineer。
請幫我建立一個「最小可用作業系統」專案，目標是：

- 架構：x86_64
- 執行環境：QEMU
- boot protocol：Limine
- 語言：C 為主，必要處少量 x86_64 assembly
- 風格：monolithic kernel
- 目標：先做出可開機、可互動、可 debug 的最小 OS
- 不要一開始實作完整 userspace / VFS / network / SMP
- 先專注於 MVP

請遵守以下原則：
1. 先做最小可跑通版本，再逐步擴充。
2. 每個 phase 都必須能 `make run` 成功。
3. 先建立穩定的 serial logging、panic、assert、debug 流程。
4. 任何新增模組都要保持最小可測。
5. 不要過度抽象化，不要做華而不實的大架構。

請按以下 phases 實作：

Phase 1:
- 建立 Limine 開機流程
- 建立 linker script
- 建立 kernel entry
- 啟動後輸出 hello from kernel
- 建立 serial logger
- 提供 `make run` 與 `make debug`

Phase 2:
- 建立 GDT / IDT
- 建立基本 interrupt / exception stubs
- 建立 panic handler
- page fault / general protection fault 至少能印出錯誤訊息

Phase 3:
- 解析 bootloader memory map
- 實作 page frame allocator
- 實作最小 kernel heap（可先 bump allocator）
- 提供 allocator smoke tests

Phase 4:
- 實作 console abstraction
- 實作 keyboard input（先求簡單可用）
- 實作 shell
- 內建 commands: help, echo, clear, reboot, halt, ticks, meminfo, panic-test

Phase 5:
- 實作 timer/tick
- 提供 uptime/ticks 查詢
- 視情況加入 cooperative task 或 deferred work loop

對每個 phase 的輸出要求：
- 說明設計決策
- 列出新增檔案
- 列出修改檔案
- 提供測試方法
- 提供風險與後續待辦
- 產出後專案必須可編譯、可執行、可 debug

額外要求：
- 所有 early boot log 必須優先走 serial
- 所有 magic number 盡量集中定義
- 所有平台相關邏輯放入 arch/x86_64
- 所有 public headers 放在 include
- 先不要導入 C++ RTTI / exceptions / STL
- 先不要導入第三方大型函式庫
- 保持專案乾淨、可讀、可教學、可擴充

請先輸出：
1. 專案目錄樹
2. Phase 1 詳細任務清單
3. 建置與執行命令
4. 之後再開始產生實作檔案
```

---

## 13. 可直接餵給 agent 的提示詞（鐵血工程版）

```text
你要扮演嚴格的 kernel engineer，不是玩具生成器。
你的任務不是一次生成一個幻想中的完整 OS，而是建立一個真正能在 QEMU 啟動、能 debug、能逐步擴充的最小可用 x86_64 kernel 專案。

【總目標】
建立 MVP OS：
- x86_64
- QEMU
- Limine boot protocol
- C + 少量 x86_64 asm
- monolithic kernel
- serial-first debugging
- 能穩定開機、接受輸入、執行基本 shell 指令

【禁止事項】
- 不要直接生成龐大但不可驗證的完整 OS
- 不要偷渡尚未整合完成的 userspace / VFS / scheduler 大系統
- 不要假設某些功能「之後自然可用」
- 不要省略 linker script、build scripts、run scripts、debug scripts
- 不要寫一堆未接線 dead code
- 不要把架構相關程式散落全專案

【硬性要求】
1. 每個 phase 完成後都必須可編譯。
2. 每個 phase 完成後都必須可由 `make run` 在 QEMU 啟動。
3. 每個 phase 都要有最少一個 smoke test 或手動驗證流程。
4. 每次只加入必要複雜度。
5. 若某功能會大幅增加風險，先用最小可行替代方案。

【Phase 切分】
Phase 1: boot + hello + serial + panic + Makefile + QEMU scripts
Phase 2: GDT/IDT + exception handlers + readable fault logs
Phase 3: memory map + PMM + simple heap
Phase 4: console + keyboard + shell + built-in commands
Phase 5: timer + ticks + simple task/work loop

【輸出格式】
每一階段先輸出：
- 設計摘要
- 檔案樹變更
- 實作步驟
- 驗證方法
- 已知風險

只有在說明清楚後，才開始產生實際檔案內容。

【程式風格】
- 清楚命名
- 單一職責
- 不做過度抽象
- 保持教學可讀性
- 所有錯誤走 panic/assert/log
- 先 correctness，再談 elegance

【第一步現在就做】
請先輸出：
1. 建議的完整專案目錄樹
2. Phase 1 的檔案清單
3. 每個檔案的責任
4. make run / make debug / make iso 的預計命令
5. Phase 1 完成標準
不要跳過規劃，先把骨架定乾淨。
```

---

## 14. 我給你的實戰建議

如果你是第一次做 OS，請這樣做：

### 最佳切入順序
1. serial hello
2. panic/assert
3. framebuffer text
4. IDT/fault
5. memory map
6. allocator
7. keyboard
8. shell
9. timer

### 最容易卡死的點
- linker script
- boot protocol 對接
- early memory assumptions
- interrupt frame / ABI
- keyboard 掃碼處理
- `printf` 太早寫太大

### 最值得先偷懶的點
- 用最小 printf
- 先不用 `kfree`
- shell 先只做 line-based
- 任務系統先 cooperative，不要 preemptive

---

## 15. 你之後若要升級，我建議的里程碑

### v0.2
- ramfs
- 更好的 heap
- command history
- 更多 debug commands

### v0.3
- 虛擬記憶體
- user mode groundwork
- syscall skeleton

### v0.4
- ELF userspace loader
- init process
- scheduler

---

## 16. 一句總結

如果你的目標是「真的做出最小可用 OS」：
**不要先做大而全，先做一個能在 QEMU 穩定開機、能輸入指令、能回應、能 debug 的小核心。**
那個版本，才是你真正的起跑線。

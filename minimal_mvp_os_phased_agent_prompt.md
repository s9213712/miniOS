# 最小可用作業系統：分階段工程任務書（可直接餵給 Agent）

## 文件目的

本文件用來指揮 agent 逐階段建立一個 **最小可用作業系統（MVP OS）**，要求：

- 架構：`x86_64`
- 執行環境：`QEMU`
- Boot protocol / bootloader：`Limine`
- 語言：`C` 為主，少量 `x86_64 assembly`
- 核心形態：`monolithic kernel`
- 開發策略：**serial-first、可編譯、可開機、可驗證、可除錯**
- 產出策略：**按階段提交，不可一次暴衝生成不可驗證的大量代碼**

---

# 一、總體原則（Agent 必須遵守）

## 1. 開發哲學

你不是在生成一個幻想中的完整作業系統。  
你要建立的是一個：

- 真正能編譯
- 真正能在 QEMU 啟動
- 真正能用 serial log 除錯
- 真正能逐步擴充

的最小核心系統。

## 2. 硬性約束

1. 每一階段結束後，專案都必須：
   - 能 `make`
   - 能 `make run`
   - 能在 QEMU 啟動
   - 至少有一個 smoke test 或手動驗證方式

2. 不允許：
   - 一次產生大量 dead code
   - 先放未接線模組佔位卻不整合
   - 先塞完整 userspace / VFS / network / SMP
   - 過度抽象化
   - 架構相關邏輯散落全專案

3. 所有 early boot log 必須優先走：
   - `serial`
   - framebuffer / 螢幕輸出只能當輔助，不可當唯一除錯出口

4. 所有錯誤路徑至少要有：
   - `panic()`
   - `assert()`
   - log 訊息
   - 明確 halt 行為

---

# 二、最終 MVP 定義

當以下條件成立時，視為 MVP 第一版完成：

- 能由 Limine 載入 kernel
- kernel 成功進入 64-bit long mode 執行主邏輯
- 有 serial logger
- 有 panic / assert
- 有基本 GDT / IDT / exception handling
- 能解析 bootloader 提供的 memory map
- 有最小可用 page frame allocator 或等效 PMM
- 有簡單 kernel heap（至少 bump allocator）
- 有 keyboard input
- 有 shell
- 有內建指令：
  - `help`
  - `echo`
  - `clear`
  - `ticks`
  - `meminfo`
  - `reboot`
  - `halt`
  - `panic-test`
- 有 timer tick
- 有 `make run` 與 `make debug`

---

# 三、建議專案目錄樹

```text
mvos/
├─ boot/
│  ├─ limine.conf
│  ├─ limine/
│  └─ iso_root/
├─ kernel/
│  ├─ arch/
│  │  └─ x86_64/
│  │     ├─ boot/
│  │     ├─ cpu/
│  │     ├─ gdt/
│  │     ├─ idt/
│  │     ├─ interrupt/
│  │     ├─ io/
│  │     ├─ timer/
│  │     └─ mm/
│  ├─ core/
│  │  ├─ main.c
│  │  ├─ panic.c
│  │  ├─ assert.c
│  │  ├─ log.c
│  │  ├─ shell.c
│  │  └─ command.c
│  ├─ dev/
│  │  ├─ serial.c
│  │  ├─ fb_console.c
│  │  ├─ keyboard.c
│  │  └─ timer.c
│  ├─ mm/
│  │  ├─ pmm.c
│  │  ├─ heap.c
│  │  └─ alloc.c
│  └─ include/
│     └─ mvos/
├─ libc/
│  ├─ memset.c
│  ├─ memcpy.c
│  ├─ memmove.c
│  ├─ strlen.c
│  ├─ strcmp.c
│  └─ printf_min.c
├─ linker/
│  └─ x86_64.ld
├─ scripts/
│  ├─ make_iso.sh
│  ├─ run_qemu.sh
│  ├─ debug_qemu.sh
│  └─ test_smoke.sh
├─ docs/
│  ├─ architecture.md
│  ├─ debugging.md
│  └─ roadmap.md
├─ tests/
│  └─ smoke/
├─ Makefile
└─ README.md
```

---

# 四、階段總覽

| Phase | 名稱 | 目標 |
|---|---|---|
| Phase 0 | 專案骨架與工具鏈 | 建立可重現的 build / run / debug 基礎 |
| Phase 1 | 開機到核心 | 成功進入 kernel 並輸出 hello |
| Phase 2 | 例外與故障可見性 | 建立 GDT / IDT / fault logs |
| Phase 3 | 記憶體基建 | 建立 memory map + PMM + heap |
| Phase 4 | 控制台與互動 | keyboard + console + shell |
| Phase 5 | 計時與系統活性 | timer tick + uptime/ticks |
| Phase 6 | 穩定化與整理 | 重構、文件、smoke test、驗收 |

---

# 五、Phase 0：專案骨架與工具鏈

## 目標
建立一個乾淨、可重現、可開發的最小工程環境。

## 必做項目
- 建立專案目錄
- 建立 `Makefile`
- 建立：
  - `make`
  - `make run`
  - `make debug`
  - `make iso`
  - `make clean`
- 建立 QEMU 執行腳本
- 建立 GDB/除錯入口
- 建立 README 初稿
- 建立基本 coding conventions

## 建議產出檔案
- `Makefile`
- `scripts/run_qemu.sh`
- `scripts/debug_qemu.sh`
- `scripts/make_iso.sh`
- `README.md`
- `docs/architecture.md`
- `docs/roadmap.md`

## 驗收標準
- `make` 不報錯
- `make run` 至少能跑到 bootloader / 嘗試啟動映像
- `make debug` 能讓 QEMU 進入可附加 GDB 的狀態
- 專案結構清楚，沒有明顯混亂命名

## Agent 回報格式
- 新增檔案清單
- 執行命令
- 目前已知依賴工具
- 風險說明

---

# 六、Phase 1：開機到核心

## 目標
成功由 Limine 將 kernel 載入，並執行 kernel entry。

## 必做項目
- 整合 Limine
- 撰寫 linker script
- 建立 kernel entry
- 建立最小 startup / low-level glue
- serial logger 最小版
- 在 serial 輸出：
  - boot banner
  - hello from kernel
- 建立 panic / halt 最小版

## 建議產出檔案
- `boot/limine.conf`
- `linker/x86_64.ld`
- `kernel/arch/x86_64/boot/entry.asm`
- `kernel/core/main.c`
- `kernel/dev/serial.c`
- `kernel/core/log.c`
- `kernel/core/panic.c`
- `kernel/include/mvos/*.h`

## 驗收標準
- `make run` 可穩定啟動
- serial 可看到 kernel 啟動訊息
- panic 測試時不是黑屏，而是印出錯誤後 halt

## 手動驗證
- 啟動後觀察 serial
- 暫時在 `kmain()` 中插入 `panic("test panic")`
- 確認畫面/serial 有可讀訊息

## 本階段禁止
- 不要先加鍵盤
- 不要先加複雜記憶體系統
- 不要先加完整 framebuffer text engine

## Agent 回報格式
1. 設計摘要  
2. 新增/修改檔案  
3. 啟動命令  
4. 預期輸出  
5. 已知限制

---

# 七、Phase 2：例外與故障可見性

## 目標
建立最低限度 CPU 例外處理能力，避免一 fault 就無聲死亡。

## 必做項目
- GDT 初始化
- IDT 初始化
- 例外處理 stubs
- fault handler
- 至少可辨識：
  - divide by zero
  - invalid opcode
  - general protection fault
  - page fault
- 在 serial 輸出 fault 訊息與必要暫存器資訊

## 建議產出檔案
- `kernel/arch/x86_64/gdt/*`
- `kernel/arch/x86_64/idt/*`
- `kernel/arch/x86_64/interrupt/*`
- `kernel/core/assert.c`

## 驗收標準
- 可以主動觸發至少 2 種 fault 並印出可讀訊息
- fault 後系統進入安全 halt
- 不再只是黑屏重啟

## 手動驗證
- divide by zero 測試
- invalid opcode 測試
- 人工 page fault 測試（可選）

## Agent 特別要求
- fault log 不求華麗，但要清楚
- 不要把 exception handling 做成巨大抽象框架
- 先 correctness，後 elegance

---

# 八、Phase 3：記憶體基建

## 目標
建立 kernel 可用的基礎記憶體管理能力。

## 必做項目
- 解析 bootloader memory map
- 建立 page frame allocator（PMM）
- 建立簡單 heap：
  - 第一版可用 bump allocator
- 提供基本 allocation API
- 提供 allocator smoke tests

## 建議產出檔案
- `kernel/mm/pmm.c`
- `kernel/mm/heap.c`
- `kernel/mm/alloc.c`
- `kernel/arch/x86_64/mm/*`

## 驗收標準
- 開機後能列出記憶體區域摘要
- 能成功分配多塊記憶體
- allocator 自測不直接 crash
- `meminfo` 後續可用這批資訊

## 手動驗證
- 開機時印出 memory map 摘要
- 執行 allocator 自測
- 確認分配結果合理

## 本階段可接受的簡化
- 先不做完整 `kfree`
- 先不做 slab
- 先不做 virtual memory 大系統
- 先不做 userspace 隔離

---

# 九、Phase 4：控制台與互動

## 目標
讓系統從「會開機」變成「可互動」。

## 必做項目
- console abstraction
- framebuffer text 或最小畫面輸出層
- keyboard input
- line input buffer
- command parser
- command dispatch table
- 內建 shell 指令

## 必備指令
- `help`
- `echo`
- `clear`
- `reboot`
- `halt`
- `ticks`
- `meminfo`
- `panic-test`

## 建議產出檔案
- `kernel/dev/fb_console.c`
- `kernel/dev/keyboard.c`
- `kernel/core/shell.c`
- `kernel/core/command.c`

## 驗收標準
- 能輸入一行文字
- 能執行至少 6 個內建指令
- shell 可以連續使用，不是一次性 demo
- 無輸入時系統穩定，不亂跳

## 手動驗證
- 輸入 `help`
- 輸入 `echo hello`
- 輸入 `meminfo`
- 輸入 `panic-test`

## Agent 特別要求
- keyboard 先求可用，不求完美 keymap
- shell 先 line-based，不要過早做 job control
- command table 要可擴充，但不要過度設計

---

# 十、Phase 5：計時與系統活性

## 目標
讓系統有時間感與基本內部節奏。

## 必做項目
- timer 初始化
- tick counter
- 基本 sleep / delay 抽象（可簡化）
- `ticks` 指令接線
- `uptime` 或等效資訊（可選）
- 視情況加入 cooperative task/work loop

## 建議產出檔案
- `kernel/dev/timer.c`
- `kernel/arch/x86_64/timer/*`

## 驗收標準
- tick 會持續增加
- `ticks` 可查詢
- 啟用 timer 後 shell 仍穩定

## 手動驗證
- 進 shell 輸入 `ticks`
- 間隔幾秒再輸入 `ticks`
- 確認值有增加

## 本階段可接受的簡化
- 可先用最簡單 timer 來源
- 可先不做 preemptive scheduler
- 可先不做多工

---

# 十一、Phase 6：穩定化、整理、文件、驗收

## 目標
把前面完成的功能整理成可展示、可維護、可交接的工程狀態。

## 必做項目
- 清理命名與 header
- 拆分平台相關與通用邏輯
- README 補全
- 文件補全：
  - 架構圖
  - boot flow
  - 記憶體流程
  - 除錯方式
- 建立 smoke test 腳本
- 建立已知限制列表
- 建立 roadmap

## 驗收標準
- 新人可按 README 成功 build/run
- 文件能說明專案做了什麼、沒做什麼
- 專案可被繼續擴充，而非只能勉強跑一次

---

# 十二、每階段統一輸出模板（要求 Agent 使用）

每完成一個 phase，請嚴格用以下格式回報：

```text
[Phase X 回報]

1. 本階段目標
- ...

2. 設計決策
- ...
- ...

3. 新增檔案
- path/to/file1
- path/to/file2

4. 修改檔案
- path/to/file3
- path/to/file4

5. 實作摘要
- ...
- ...

6. 建置/執行命令
- make
- make run
- make debug

7. 驗證方法
- ...
- ...

8. 預期輸出
- ...

9. 已知限制
- ...

10. 下一階段建議
- ...
```

---

# 十三、Agent 執行提示詞（完整分階段版）

```text
你現在是資深 kernel engineer / systems programmer。
請幫我建立一個最小可用作業系統（MVP OS）專案，但必須嚴格按階段實作與回報。

【專案目標】
- 架構：x86_64
- 執行環境：QEMU
- boot protocol：Limine
- 語言：C 為主，必要處少量 x86_64 assembly
- 核心：monolithic kernel
- 開發哲學：serial-first、可編譯、可啟動、可驗證、可除錯

【總原則】
1. 不要一次生成龐大不可驗證的完整 OS。
2. 每個 phase 完成後都必須可以：
   - make
   - make run
   - 在 QEMU 開機
3. 所有 early boot log 優先走 serial。
4. 每階段都要提供：
   - 設計摘要
   - 檔案樹變更
   - 實作內容
   - 驗證方法
   - 已知限制
5. 不允許過度抽象化與 dead code。
6. 不要先做 userspace / VFS / network / SMP / 完整 scheduler。
7. 所有錯誤路徑都要能走 panic/assert/log。

【Phase 規劃】
Phase 0：專案骨架與工具鏈
Phase 1：Limine 開機到 kernel + hello + serial + panic
Phase 2：GDT / IDT / exception handling / fault logs
Phase 3：memory map + PMM + simple heap
Phase 4：console + keyboard + shell + built-in commands
Phase 5：timer tick + ticks/uptime
Phase 6：穩定化、README、debug docs、smoke tests

【每個 Phase 的硬要求】
- 先說明設計，再產出程式
- 每次只增加必要複雜度
- 先 correctness，再 elegance
- 若某功能高風險，先用最小可行替代方案
- 平台相關程式碼集中在 arch/x86_64
- 通用 header 放 include
- 不導入第三方大型 runtime / STL / exceptions / RTTI

【你現在的第一步】
請先輸出：
1. 完整專案目錄樹
2. Phase 0 與 Phase 1 的詳細任務清單
3. 每個檔案的職責
4. 建置/執行/除錯命令
5. Phase 1 的驗收標準

完成規劃後，再開始輸出實作檔案內容。
每次完成一個 phase，都要用以下格式回報：

[Phase X 回報]
1. 本階段目標
2. 設計決策
3. 新增檔案
4. 修改檔案
5. 實作摘要
6. 建置/執行命令
7. 驗證方法
8. 預期輸出
9. 已知限制
10. 下一階段建議
```

---

# 十四、Agent 執行提示詞（鐵血版，嚴格控工程）

```text
你不是在寫玩具 hello kernel。
你要建立一個真正可啟動、可除錯、可擴充的最小作業系統專案。

【身份】
你是嚴格的 kernel engineer。
你必須優先確保：
- build 正確
- boot 正確
- serial log 正確
- fault visibility 正確
- 之後才是功能擴充

【禁止】
- 禁止一次丟出整包不可驗證代碼
- 禁止先放巨大架構空殼
- 禁止提前塞 userspace / VFS / network / SMP
- 禁止把平台細節散落專案各處
- 禁止略過 Makefile / linker / run scripts / debug scripts

【必須】
- 每階段都要可 make
- 每階段都要可 make run
- 每階段都要給手動驗證步驟
- 早期輸出一定走 serial
- 錯誤一定能 panic/log
- 命名清楚、責任分明、可教學可維護

【固定階段】
Phase 0：骨架與工具鏈
Phase 1：boot 到 kernel
Phase 2：fault visibility
Phase 3：memory foundation
Phase 4：interactive shell
Phase 5：timer/ticks
Phase 6：stabilization/docs/tests

【回報要求】
每個 phase 都先說明：
- 設計摘要
- 檔案樹變更
- 實作步驟
- 驗證方式
- 風險與限制

說明完，才開始產生實際代碼。

【第一步現在執行】
先輸出：
1. 專案目錄樹
2. Phase 0~1 任務清單
3. 每個主要檔案的責任
4. make / make run / make debug / make iso 預計命令
5. Phase 1 完成標準
不要跳步，不要偷做後面 phase。
```

---

# 十五、實戰建議（給你本人，不是給 agent）

## 最佳開發順序
1. serial hello
2. panic/assert
3. linker / boot 穩定
4. GDT / IDT / fault log
5. memory map
6. PMM / bump allocator
7. keyboard
8. shell
9. timer

## 最容易爆炸的地方
- linker script
- Limine 對接
- early boot 的記憶體假設
- interrupt ABI / stack frame
- keyboard 掃碼
- 過早實作大型 `printf`

## 最值得先偷懶的地方
- `printf` 先做最小版
- `kfree` 可延後
- shell 先 line-based
- scheduler 可延後
- 檔案系統可延後
- userspace 可延後

---

# 十六、後續升級路線（未來版本）

## v0.2
- 更好的 heap
- 命令歷史
- 更完整 console
- ramfs

## v0.3
- virtual memory
- syscall skeleton
- ring3 groundwork

## v0.4
- ELF loader
- userspace init
- scheduler

## v0.5
- VFS
- block device 抽象
- 真正的檔案系統

---

# 十七、總結

真正的最小可用 OS，不是只會印一行 hello world。  
真正的起跑線是：

- 能開機
- 能 log
- 能 panic
- 能看 fault
- 能分配記憶體
- 能接受輸入
- 能執行 shell 指令
- 能用 timer 驗證系統還活著

請讓 agent 依照本文件，**逐階段施工、逐階段驗收、逐階段回報**，不要暴衝。

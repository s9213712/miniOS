# miniOS 測試檔案實戰教學

本文件只描述「實際可行」流程：  
先在 miniOS 用 `nano` 寫完 `test.py`，再在主機端編譯 `test.c` 並跑起來。

> miniOS 目前只有 mini python 子集（不能執行完整 CPython），C 則只支援在主機端編譯與測試可執行檔。

## 先決條件

- 已在 repo 根目錄：`/home/s92137/miniOS`
- miniOS 能夠正常進入 shell（`ENABLE_SHELL=1`）
- 主機已安裝 `gcc`/`make`

## 一、在 miniOS 用 nano 寫 `test.py`，並執行

1. 進入 miniOS
```bash
ENABLE_SHELL=1 make run
```

2. 在 miniOS shell 裡建立並編輯 Python 檔案
```text
touch /tmp/test.py
nano /tmp/test.py
```

3. 在 `nano` 內輸入內容（可直接貼上）
```python
print("hello miniOS")
x = 1234
print(x / 2)
```

4. 存檔與離開
- 儲存：`Ctrl+S`
- 離開：`Ctrl+Q`

5. 回到 miniOS shell 執行
```text
run python /tmp/test.py
```

6. 成功範例輸出（重點）
- 檔案名稱行為 `mini python executing: /tmp/test.py`
- 每個 `print` 會輸出一行
- 不支援完整 Python API（例如 import、大型套件）

## 二、`test.c` 的編譯與執行（主機端）

### 方式 A：直接用 `gcc` 編譯任意 C 檔

在主機上執行（非 miniOS 內）：
```bash
cd /home/s92137/miniOS
cat > /tmp/test.c <<'EOF'
#include <stdio.h>

int main(void) {
    puts("hello from test.c");
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -O2 -g /tmp/test.c -o /tmp/test
/tmp/test
```

### 方式 B：用專案的 host program pipeline

1. 把測試檔放到 `samples/user-programs/`
```bash
cd /home/s92137/miniOS
cp /tmp/test.c samples/user-programs/test.c
```

2. 編譯全部 host 程式（含 `test.c`）
```bash
make host-programs
```

3. 直接執行 `test.c` 產生的主機可執行檔（輸出檔名為 `test_c`）
```bash
./build/host-programs/test_c
```

4. 清理測試來源（可選）
```bash
rm samples/user-programs/test.c
```

## 常見流程結構

- `test.py`：在 miniOS 內編輯並用 `run python /tmp/test.py` 執行  
- `test.c`：在主機端編譯成 `/tmp/test` 或 `./build/host-programs/test_c` 再執行  

如果你要做 miniOS 內測試腳本與主機程式對照，建議把兩者都保留輸出版本，先比對 `run python ...` 的輸出和 `./test` 的輸出是否一致。

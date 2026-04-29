# 06 — 測試完整指南

整個 repo 的測試分**三套**互不重疊的 env，每套有自己的角色。理解這個結構是寫好測試的前提。

## 0. 大圖

| Env | 路徑 | 跑什麼 | 何時用 |
|---|---|---|---|
| `native_unit` | `AIO_Firmware_PIO/test/native/test_*` | Unity unit tests，host C++ | 純邏輯模組（parser、state machine、no-Arduino-dep code） |
| `native_ftp` | `AIO_Firmware_PIO/test/native/test_ftp_basic_commands/` | Unity FTP harness | 驗證 FtpServer 的 protocol state machine（PR-3.0a 加的） |
| `native_test` | `lv_simulater_platformio/` 跑 SDL2 binary，靠 `test/scenarios/` | GUI scenario harness | App UI render + IMU action 流程 |
| `pytest`(AIO_Tool) | `AIO_Tool/tests/` | Python pytest | 上位機的 util 模組 |

每套各有一個對應的 CI job — 詳見章節 07。

## 1. 韌體 Unity unit tests — `native_unit`

### 1a. 跑既有的

```bash
cd AIO_Firmware_PIO
pio test -e native_unit
```

預期看到 6 個 test directories 各自 build + 跑：
```
test_app_controller       PASSED  3 tests
test_config               PASSED  4 tests
test_game_2048            PASSED  9 tests
test_heartbeat_config     PASSED  6 tests
test_imu_action           PASSED  5 tests
test_ftp_basic_commands   SKIPPED (拉到 native_ftp env 去)
```

### 1b. 寫一個新的

範本：[`test/native/test_heartbeat_config/test_main.cpp`](../../AIO_Firmware_PIO/test/native/test_heartbeat_config/test_main.cpp)。

**前置條件**：你要測的程式碼要能在 host 編譯，意思是：
- 不能 `#include <Arduino.h>`（除了測試 stub 的最小版本）
- 不能呼叫 `Serial.print()`、`millis()` 等 Arduino-only API（除非你的 stub 有 mock）
- 不能 link LVGL / TFT_eSPI / WiFi 等 vendored library

如果現有程式跟 Arduino 綁很死，**抽出來**：把純邏輯部分搬到獨立的 `*_parse.cpp` / `*_logic.cpp` 模組，只 include `<string.h>` `<stdint.h>` 之類的 host-safe header。範本：[`heartbeat_config_parse.cpp`](../../AIO_Firmware_PIO/src/app/heartbeat/heartbeat_config_parse.cpp) 是從 `heartbeat.cpp` 的 `read_config` 抽出來的。

### 1c. test_main.cpp 寫法

```cpp
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"   // host stub from test/stubs_unit/Arduino.h
#include "Wire.h"      // host stub
#include "driver/flash_fs.h"
#include "app/myapp/my_parse.h"   // ← 你要測的模組

void setUp(void) {}      // 每個 test 之前跑（state reset 在這）
void tearDown(void) {}   // 每個 test 之後跑

void test_well_formed_input_parses_correctly() {
    char input[256];
    snprintf(input, sizeof(input), "value1\nvalue2\n");

    MyConfig out{};
    bool ok = my_parse_config(input, strlen(input), &out);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("value1", out.field1);
    TEST_ASSERT_EQUAL_STRING("value2", out.field2);
}

void test_empty_buffer_returns_false() {
    MyConfig out{};
    bool ok = my_parse_config("", 0, &out);
    TEST_ASSERT_FALSE(ok);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_well_formed_input_parses_correctly);
    RUN_TEST(test_empty_buffer_returns_false);
    return UNITY_END();
}
```

放到 `AIO_Firmware_PIO/test/native/test_my_module/test_main.cpp`。

### 1d. 加進 build_src_filter

[`platformio.ini`](../../AIO_Firmware_PIO/platformio.ini) `[env:native_unit]` 的 `build_src_filter` 列出每個編進測試的 src/。新加的模組要加進去：

```ini
build_src_filter =
  -<*>
  +<driver/imu.cpp>
  +<driver/analyse_param.cpp>
  +<app/heartbeat/heartbeat_config_parse.cpp>
  +<app/myapp/my_parse.cpp>           ; ← 新增這行
  +<sys/send_to_dispatch.cpp>
  +<../test/stubs_unit/test_globals.cpp>
```

跑 `pio test -e native_unit` 看你的新測試出現。

### 1e. Unity 常用 assert

| Macro | 用途 |
|---|---|
| `TEST_ASSERT_TRUE(cond)` / `_FALSE(cond)` | bool |
| `TEST_ASSERT_EQUAL_INT(expect, actual)` | 整數 |
| `TEST_ASSERT_EQUAL_STRING(expect, actual)` | C 字串 |
| `TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, n)` | 二進位 buffer |
| `TEST_ASSERT_FLOAT_WITHIN(delta, expect, actual)` | float 容差 |
| `TEST_ASSERT_TRUE_MESSAGE(cond, "msg")` | 失敗顯示自訂訊息 |

## 2. FTP harness — `native_ftp`

PR-3.0a (#68) 為了 PR-3.3 (#69) FtpServer 拆檔的安全網而建。把整個 FtpServer class 跟 fake WiFiServer/WiFiClient/SD stub 接起來，可以從 host 端 inject FTP command + 看回應。

### 2a. 跑既有的

```bash
cd AIO_Firmware_PIO
pio test -e native_ftp
```

預期 6 個 test pass：
```
test_auth_flow_succeeds              PASSED
test_unknown_command_returns_500     PASSED
test_pwd_echoes_root_after_login     PASSED
test_feat_lists_extensions           PASSED
test_noop_returns_200                PASSED
test_quit_disconnects_client         PASSED
```

### 2b. 寫新 FTP test

範本 [`test/native/test_ftp_basic_commands/test_main.cpp`](../../AIO_Firmware_PIO/test/native/test_ftp_basic_commands/test_main.cpp)：

```cpp
#include <unity.h>
#include "Arduino.h"
#include "WiFiClient.h"
#include "SD.h"
#include "app/file_manager/ESP32FtpServer.h"
#include "ftp_test_helpers.h"

static FtpServer *srv = nullptr;

void setUp() {
    g_fake_millis = 0;
    SD.clear();
    srv = new FtpServer();
    srv->begin("user", "pass");
}

void tearDown() {
    delete srv;
    srv = nullptr;
    while (ftpServer.hasClient()) (void)ftpServer.available();
}

void test_my_new_command() {
    // 1. 連線 + 認證（helper 處理掉 welcome → USER → PASS dance）
    WiFiClient c = ftp_connect_and_auth(*srv);

    // 2. 送 command + 收 response
    std::string resp = ftp_send_command(*srv, c, "MYCMD foo\r\n");

    // 3. 斷言 response 有預期的 code
    TEST_ASSERT_TRUE_MESSAGE(ftp_tx_contains(resp, "200"),
                             "expected 200 response");
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_my_new_command);
    return UNITY_END();
}
```

### 2c. helper API 一覽

[`test/stubs_ftp/ftp_test_helpers.h`](../../AIO_Firmware_PIO/test/stubs_ftp/ftp_test_helpers.h)：

| Helper | 做什麼 |
|---|---|
| `ftp_pump(srv, client, max_iters)` | 重複 `srv.handleFTP()` 直到 client.rx 清空 |
| `ftp_connect_and_auth(srv, user, pass)` | 跑完 welcome→USER→PASS→230 流程，回 connected client |
| `ftp_send_command(srv, client, line)` | inject 一行 command + pump + 回 tx 內容 |
| `ftp_tx_contains(tx, needle)` | substring 搜尋 tx |

### 2d. State machine 注意事項

FtpServer 一開機 cmdStatus = 0 → 1（reset） → 2（idle waiting connect） → 3（waiting USER）...

**重點**：你不能在 cmdStatus < 2 就 push 一個 client，否則 cmdStatus 0 那段會把它 disconnectClient() 直接幹掉。`ftp_connect_and_auth()` 已經處理：先 pump 兩次再 push。如果手動寫測試忘了這步，會看到「welcome banner 沒進 tx」的鬼故事。

## 3. GUI scenario harness — host SDL2

這是最強大也最複雜的一套。把**真的 firmware app code** + LVGL + 一堆 mocked driver / network 編成 SDL2 桌面 binary，可以「跑」一支 app 並截圖比對 golden。

### 3a. 跑既有的（local）

```bash
cd lv_simulater_platformio
pio run -e native_test       # build SDL2 binary
./.pio/build/native_test/program --scenario ../test/scenarios/server/smoke.scn --headless
```

或一次跑全部：
```bash
for scn in ../test/scenarios/*/*.scn; do
  ./.pio/build/native_test/program --scenario "$scn" --headless
done
```

### 3b. .scn 檔語法

[`test/scenarios/`](../../test/scenarios/) 每個子資料夾對應一個 app。範例 [`test/scenarios/server/smoke.scn`](../../test/scenarios/server/smoke.scn)：

```
app server                                    # 啟動哪個 app
threshold 5.0                                 # 容許的 pixel diff %（預設 0.5）

flash_seed /weather_accu.cfg "TEST_KEY\nBeijing\n..."   # 預先寫 flash config
http_fixture relation/stat bilibili_negative/empty.json # 換掉預設 HTTP mock

wait_ms 1200                                  # 跑 LVGL 1200ms（讓動畫穩定）
screenshot 01_initial                         # 截圖到 golden/server/01_initial.png
assert_no_crash                               # 沒 segfault 就 pass

action UP                                     # 模擬 IMU 往上
wait_ms 400
screenshot 02_after_up
assert_no_crash

action RETURN                                 # 退出 app
wait_ms 200
assert_no_crash
```

### 3c. 寫新 scenario

```bash
mkdir -p test/scenarios/myapp
cat > test/scenarios/myapp/smoke.scn <<EOF
app myapp
wait_ms 800
screenshot 01_initial
assert_no_crash

action GO_FORWARD
wait_ms 400
screenshot 02_after_action
assert_no_crash

action RETURN
wait_ms 200
assert_no_crash
EOF
```

第一次跑要 generate goldens：
```bash
cd lv_simulater_platformio
./.pio/build/native_test/program --scenario ../test/scenarios/myapp/smoke.scn --update-golden
```

`--update-golden` 把現在 render 出來的東西寫到 `test/golden/myapp/01_initial.png` / `02_after_action.png`。**目視確認這幾張看起來是對的**，再 commit。後續 CI 跑 `--scenario` 就會跟這些 golden 比對。

### 3d. 容差調整

預設 `--diff-threshold 0.5`（0.5%）。某些 app 的 render 有 subpixel non-determinism（例如 FontAwesome glyphs），會 flaky。兩個解法：

1. **scenario 裡 `threshold N`** — 只放寬這支 scenario：
   ```
   threshold 5.0
   ```
2. **預先 wait_ms 拉長** — 讓動畫完全 settle 再截圖：
   ```
   action UP
   wait_ms 400        # 比預設 50ms 久很多
   screenshot 02_with_data
   ```

`pc_resource/smoke.scn` 兩招都用了 — 看裡面 comment 是教科書級的 flaky-test 處理範例。

### 3e. HTTP fixtures — 餵 fake API response

[`test/fixtures/http/<domain>/<path>.json`](../../test/fixtures/http/)。Host 端的 `HTTPClient` stub 看到 `http_fetch_json("https://api.bilibili.com/x/relation/stat?...")` 會去找 `test/fixtures/http/api.bilibili.com/x/relation/stat.json` 餵回去。

scenario 裡 `http_fixture <substring> <relpath>` 可以單次覆蓋：
```
http_fixture relation/stat bilibili_negative/empty.json
```
意思是「這支 scenario 內，URL 含 `relation/stat` 的全部用 `test/fixtures/http/bilibili_negative/empty.json`」。

寫負路徑測試（500、空 body、畸形 JSON）就靠這個。範本：`test/fixtures/http/bilibili_negative/`。

### 3f. Socket fixtures（pc_resource 特殊用法）

`pc_resource` 用 raw `WiFiClient.print("GET /sse")` 拉 SSE stream，host stub 接到 `connect("0.0.0.0", ...)` 會去找 `test/fixtures/socket/0.0.0.0.txt` 拿整段 fake response。

寫類似 streaming app 的測試時參考 [`test/scenarios/pc_resource/smoke.scn`](../../test/scenarios/pc_resource/smoke.scn)。

### 3g. Harness flag 一覽

```bash
./program \
  --scenario PATH        # 跑指定的 .scn
  --headless             # SDL_VIDEODRIVER=dummy（CI 一定要加）
  --update-golden        # 重新生 golden（不比對）
  --diff-threshold 1.0   # 全域 default tolerance（per-scenario `threshold` 會覆蓋）
```

## 4. AIO_Tool pytest

```bash
cd AIO_Tool
make test     # 等於 uv run pytest
```

範本 + 詳細寫法見 [05 — AIO_Tool](./05-aio-tool.md) 第 10 段。

## 5. 何時寫哪種測試

決策樹：

```
要測什麼？
├─ 純邏輯 / parser / state machine
│   └─ → native_unit
│       例：heartbeat config parse、game_2048 board logic、imu action 識別
│
├─ 一個 stateful protocol 的整體流程
│   └─ → native_ftp（建新的話照同樣 pattern 開新 env）
│       例：FtpServer command dispatch
│
├─ App 進去後畫面 render 對不對 / IMU 操作有沒有 crash
│   └─ → GUI scenario (.scn)
│       例：weather 切換頁籤、bilibili 抓不到資料的 fallback
│
└─ AIO_Tool 的 Python 邏輯
    └─ → AIO_Tool/tests/ pytest
        例：i18n loading、序列化 frame
```

不確定就**先寫 GUI scenario** — coverage 最廣，能抓到大多數整合錯誤。

## 6. CI 上看到 fail 怎麼辦

### 6a. unit_test fail
```bash
gh run view <run-id> --log-failed | grep TEST
```
通常顯示 `test_xxx FAIL: TEST_ASSERT_EQUAL_INT expected 5, was 7` 之類的。本機重現 `pio test -e native_unit --filter test_xxx`。

### 6b. firmware-build fail
通常是真正的 compile error。`gh run view` log 找 `error:` 行。本機 `pio run -e HoloCubic_AIO_Releases` 重現。

### 6c. GUI regression fail（pixel diff 超過 tolerance）
```bash
gh run download <run-id> -n regression-results
# 解壓得到 actual.png + expected.png + diff.png
```
打開 diff.png 目視比對。如果是預期的視覺改動，跑 `--update-golden` 重生 baseline、commit 新 PNG。如果是不該動的，bug 在你最近改的地方。

### 6d. AIO_Tool fail
通常是 `ruff check` 或 `pytest`。本機 `make lint` / `make test` 重現。

## 下一步

- [07 — CI + Release](./07-ci-and-release.md) — workflow 設定、tag 怎麼觸發 release。

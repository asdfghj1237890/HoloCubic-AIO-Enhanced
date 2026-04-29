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

## 7. 完整 TDD walkthrough — 從緊耦合 code refactor 出測試

理論講完了，做一次。情境：你要改 `heartbeat.cpp` 的 config 解析邏輯，想先寫測試確認既有行為，再改 code。但 `heartbeat.cpp` 把 parse 邏輯跟 Arduino call、global state 混在一起 → **不能直接 include 進 host 測試**。

### 7.1 認清緊耦合

打開 [`heartbeat.cpp` (commit 26fbf0c~1)](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/commit/26fbf0c) 看 read_config 原貌：

```cpp
static void read_config(HeartbeatAppForeverData *cfg)
{
    char info[256] = {0};
    uint16_t size = g_flashCfg.readFile(HEARTBEAT_CONFIG_PATH, (uint8_t *)info);
    Serial.printf("size %d\n", size);
    info[size] = 0;
    if (size == 0) {
        snprintf(cfg->mqtt_server, sizeof(cfg->mqtt_server), "%s", "");
        // ... 預設值 ...
        write_config(cfg);
    } else {
        char *param[6] = {0};
        analyseParam(info, 6, param);
        snprintf(cfg->mqtt_server, sizeof(cfg->mqtt_server), "%s", param[0]);
        // ... 6 個欄位 ...
    }
}
```

這函式做了**三件事**：
1. **I/O**（`g_flashCfg.readFile`、`Serial.printf`、`write_config`）
2. **Parsing**（`analyseParam` + 6 個 `snprintf`）
3. **Default 提供**（`size == 0` 那 branch）

要在 host 測試 #2 + #3，必須**把它從 #1 抽出來**。

### 7.2 抽出 pure function

新檔 `heartbeat_config_parse.h`：

```cpp
#ifndef HEARTBEAT_CONFIG_PARSE_H
#define HEARTBEAT_CONFIG_PARSE_H

#include <stdint.h>

struct HeartbeatRawFields {
    char mqtt_server[32];
    char mqtt_port[8];
    char mqtt_user[16];
    char mqtt_password[16];
    char role[4];
    char qq_num[20];
};

// Pure function: parse a 6-line config buffer into raw string fields.
// Caller is responsible for I/O (reading from flash, writing defaults
// back if size==0). Returns true if all 6 fields parsed.
bool heartbeat_parse_config(const char *buf, uint16_t size, HeartbeatRawFields *out);

#endif
```

新檔 `heartbeat_config_parse.cpp`（**只 include host-safe header**）：

```cpp
#include "heartbeat_config_parse.h"
#include <stdio.h>
#include <string.h>
#include "driver/analyse_param.h"  // analyseParam 是 host-safe (純 C)

bool heartbeat_parse_config(const char *buf, uint16_t size, HeartbeatRawFields *out)
{
    if (size == 0 || buf == nullptr || out == nullptr) return false;

    char working[256];   // analyseParam 會 mutate buffer，先 copy
    if (size >= sizeof(working)) return false;
    memcpy(working, buf, size);
    working[size] = 0;

    char *param[6] = {0};
    analyseParam(working, 6, param);
    if (!param[0] || !param[1] || !param[2] || !param[3] || !param[4] || !param[5])
        return false;

    snprintf(out->mqtt_server, sizeof(out->mqtt_server), "%s", param[0]);
    snprintf(out->mqtt_port,   sizeof(out->mqtt_port),   "%s", param[1]);
    snprintf(out->mqtt_user,   sizeof(out->mqtt_user),   "%s", param[2]);
    snprintf(out->mqtt_password, sizeof(out->mqtt_password), "%s", param[3]);
    snprintf(out->role,        sizeof(out->role),        "%s", param[4]);
    snprintf(out->qq_num,      sizeof(out->qq_num),      "%s", param[5]);
    return true;
}
```

關鍵設計：
- **不**碰 `g_flashCfg`、`Serial.printf`、`HeartbeatAppForeverData` — 這些 caller 處理
- **不**碰 default 值 — caller 看回傳 false 自己決定要塞什麼預設
- **不**在裡面 malloc — 寫進 caller 給的 buffer
- **mutate-in-place 的 input 先 copy** — analyseParam 會把 buffer 切碎，這個函式對外應該 read-only

### 7.3 改原本 heartbeat.cpp 用新模組

```cpp
#include "heartbeat_config_parse.h"

static void read_config(HeartbeatAppForeverData *cfg)
{
    char info[256] = {0};
    uint16_t size = g_flashCfg.readFile(HEARTBEAT_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;

    HeartbeatRawFields raw{};
    if (heartbeat_parse_config(info, size, &raw)) {
        snprintf(cfg->mqtt_server, sizeof(cfg->mqtt_server), "%s", raw.mqtt_server);
        snprintf(cfg->mqtt_port,   sizeof(cfg->mqtt_port),   "%s", raw.mqtt_port);
        // ... copy 6 fields ...
    } else {
        // Default 值
        snprintf(cfg->mqtt_server, sizeof(cfg->mqtt_server), "%s", "");
        // ...
        write_config(cfg);
    }
}
```

行為應該等價（這是 refactor，不是 feature change）。Build + 燒進去測一次確認沒退化。

### 7.4 寫 unit test

`AIO_Firmware_PIO/test/native/test_heartbeat_config/test_main.cpp`：

```cpp
#include <unity.h>
#include "Arduino.h"
#include "Wire.h"
#include "driver/flash_fs.h"
#include "app/heartbeat/heartbeat_config_parse.h"

void setUp() {}
void tearDown() {}

static void make_cfg(char *dst, size_t cap, ...) { /* ...拼 6 行 buffer... */ }

void test_well_formed_config_parses_all_six_fields() {
    char buf[256];
    make_cfg(buf, sizeof(buf), "broker.example.com", "1883", "alice", "s3cret", "0", "12345678");
    HeartbeatRawFields out{};
    bool ok = heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("broker.example.com", out.mqtt_server);
}

void test_empty_buffer_returns_false() {
    HeartbeatRawFields out{};
    bool ok = heartbeat_parse_config("", 0, &out);
    TEST_ASSERT_FALSE(ok);
}

void test_truncated_buffer_returns_false() {
    HeartbeatRawFields out{};
    bool ok = heartbeat_parse_config("only\ntwo\n", 9, &out);
    TEST_ASSERT_FALSE(ok);    // 只 2 行，少 4 個欄位
}

void test_oversized_field_truncated_to_buffer_size() {
    char buf[256];
    char giant[64];
    memset(giant, 'A', 50);
    giant[50] = 0;
    make_cfg(buf, sizeof(buf), giant, "1883", "u", "p", "0", "1");
    HeartbeatRawFields out{};
    bool ok = heartbeat_parse_config(buf, strlen(buf), &out);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(31, strlen(out.mqtt_server));   // truncated to 31 + NUL
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_well_formed_config_parses_all_six_fields);
    RUN_TEST(test_empty_buffer_returns_false);
    RUN_TEST(test_truncated_buffer_returns_false);
    RUN_TEST(test_oversized_field_truncated_to_buffer_size);
    return UNITY_END();
}
```

### 7.5 加進 build_src_filter + 跑

`platformio.ini`：

```ini
build_src_filter =
  -<*>
  +<driver/imu.cpp>
  +<driver/analyse_param.cpp>
  +<app/heartbeat/heartbeat_config_parse.cpp>   ; ← 加這行
  +<sys/send_to_dispatch.cpp>
  +<../test/stubs_unit/test_globals.cpp>
```

```bash
pio test -e native_unit
```

預期 4 個 test 通過。如果有失敗，**測試告訴你哪個 case 出問題**，比起燒韌體 + 觀察 log debug 快 100 倍。

### 7.6 反向：先有測試，再改 code（真 TDD）

7.1-7.5 是「已有 code，補測試」(retrofit)。**真 TDD** 是反過來：

1. 先寫測試（紅）
2. 寫最小 code 讓它過（綠）
3. Refactor（保持綠）

範例：你要新增「parse 第 7 個欄位 `subtopic`」。流程：

1. 加新欄位到 struct + 寫個 `test_parses_seven_fields` test → run → 紅（function 還沒支援）
2. 修 `heartbeat_parse_config` 讓 test 過 → run → 綠
3. 看 code 是否還清楚 / 是否要拆 helper → refactor → run → 依然綠

每個循環 30 秒以內。Embedded code 用 TDD 最大障礙是 build 時間，但 native_unit env build 通常 < 5 秒，**比真的燒板子快得多**。

---

## 8. 三套 stub layer 為什麼是三套

`AIO_Firmware_PIO/test/` 下面有 `stubs_unit/`、`stubs_ftp/`，加上 `lv_simulater_platformio/` 用 `test/stubs/`，**共三套 stub**。為什麼不統一？

### 8.1 各自的設計目標

| Stub set | 路徑 | 給誰用 | 設計目標 |
|---|---|---|---|
| `test/stubs_unit/` | host Unity | 邏輯模組單元測試 | **最小**。只 cover 被測函式 transitively 用到的 type/macro。Build < 5 秒 |
| `test/stubs_ftp/` | host Unity | FtpServer 整體 protocol test | **scriptable**。WiFiServer 可以 push pending client、WiFiClient 雙向 buffer 可以 inject + inspect、SD 是 in-memory FakeSD |
| `test/stubs/` | host SDL2 GUI | 整個 firmware 跑起來看 UI | **vendored libraries 的 mock**。LVGL 真的接 SDL2 視窗。HTTPClient 接 fixture file。WiFiServer/WiFiClient 接 socket fixture |

### 8.2 為什麼不能合併

**衝突點**：

- `Arduino.h`：unit 測試只要 `String` 跟 `boolean`、`F()` macro；FTP 測試要 `Print` interface、`millis()` mock 控制時間；SDL2 harness 要完整 LVGL-aware String 跟 sleep helper
- `WiFiClient.h`：unit 測試根本不需要；FTP 測試要 scriptable rx/tx queue；SDL2 harness 要從 `test/fixtures/socket/` 讀逐字 fixture
- `Serial`：unit 測試丟 devnull；FTP 測試也丟 devnull；SDL2 harness 要 print 到 stdout 給開發者看
- 編譯時間：合併 stub 會把 SDL2 harness 用的 LVGL-aware String 也拉進 unit test build，慢 5-10 倍

**結論**：每套用途差太多，**抽象層級不同**，硬合併會讓最常跑的 unit test 變慢。三套並存、各有 README 說明，反而清楚。

### 8.3 何時用哪套

寫測試**前**問自己：

```
我要驗證什麼？
├─ 一個 function 的 input/output 對不對 (parser, state machine)
│   └─ stubs_unit + native_unit env
│
├─ 一個 long-lived class instance 收到一連串 event 的反應對不對
│   └─ stubs_ftp + native_ftp env (or 開新的 native_<name> env)
│
└─ 整個 app 的 UI render 對不對 / 跑流程不會 crash
    └─ stubs/ + lv_simulater_platformio 的 native_test env
```

### 8.4 開新 stub set 的時機

如果你要寫一個全新類型的 integration test（例如 MQTT protocol harness），**不要硬塞到 stubs_unit 或 stubs_ftp** — 開新的 `stubs_mqtt/` + 新的 `[env:native_mqtt]`。理由：

- 你的 stub 需求跟既有的不同（你需要 mock PubSubClient、不需要 mock SD）
- 加進 stubs_unit 會拖慢 unit test build 時間
- 獨立 env 失敗時不影響其他 test job
- CI workflow 多加一個 step 跑你的 env（範本：[`regression.yml`](../../.github/workflows/regression.yml) 的「Run FTP harness tests」step）

`stubs_ftp/` 的歷史就是這樣：寫 FTP harness 時不想塞 stubs_unit，開了新 dir + 新 env。完全分開、互不影響。

---

## 9. Goldens workflow — 從零產生到 commit

GUI scenario 的 `screenshot LABEL` 指令會把當下螢幕存到 `test/golden/<app>/<label>.png`（如果不存在）或 `test/results/<app>/actual_<label>.png`（如果存在 — 然後跟 golden 比對）。

### 9.1 第一次寫 scenario：產 golden

```bash
cd lv_simulater_platformio
pio run -e native_test
./.pio/build/native_test/program \
    --scenario ../test/scenarios/myapp/smoke.scn \
    --update-golden
```

`--update-golden` 強制把 actual 寫成 golden（不比對）。預期看到 `test/golden/myapp/01_initial.png` 等檔案出現。

### 9.2 目視 review

**這步不能跳**。打開新生的 PNG 用眼睛看：

- 字有沒有 render 正確？是否被截斷？
- 顏色對不對？（dark theme 不該出現大塊白色）
- 圖示位置合理嗎？
- 預期內容（例如 "天氣 25°C"）有沒有出現？

如果看起來不對 → **scenario 寫錯**或**程式碼 bug**，不是 golden 要 commit 的時機。修對了再產一次。

### 9.3 Commit goldens 跟 scenario 一起進 git

```bash
git add test/scenarios/myapp/smoke.scn test/golden/myapp/*.png
git commit -m "test(myapp): add smoke scenario"
```

**Golden PNG 要進 git** — 它們是 expected behavior 的 snapshot。reviewer 在 PR 看到 PNG 改動就知道你動了 UI。

### 9.4 後續跑：比對 vs 重生

平常 CI / 本機 dev：
```bash
./program --scenario .../smoke.scn --headless    # 比對 golden
```
失敗時 actual 會留在 `test/results/myapp/actual_01_initial.png` + `diff_01_initial.png`（diff 是 pixel-level 紅色標出差異點）。

你**故意改了 UI**（例如改了字、調了 layout）：
```bash
./program --scenario .../smoke.scn --update-golden    # 重生 golden
```
然後 review 新 PNG → commit。

### 9.5 What counts as "real regression"

不是每次 pixel diff 都是 bug。常見的 false positive：

| 症狀 | 原因 | 處理 |
|---|---|---|
| 0.1-0.5% 紅點散在字邊緣 | font sub-pixel anti-aliasing 不確定性 | 預設 0.5% threshold 已 cover |
| 1-3% 集中在某 icon | FontAwesome glyph render 對 LVGL render order 敏感 | scenario 加 `wait_ms 400` 或 `threshold 5.0` |
| diff 是整塊區域顏色變掉 | **真的 bug** — 你動的 code 改了 render | 修 code 或更新 golden（如果 intentional） |
| diff 是時鐘數字 | 時間相關 — golden 凍結在某個時刻 | scenario 用 `flash_seed` 固定時間，或 mask 掉時鐘區域（目前還沒實作 mask） |

### 9.6 Per-scenario threshold

[`test/scenarios/pc_resource/smoke.scn`](../../test/scenarios/pc_resource/smoke.scn) 是 textbook 範例，看頂部 comment 解釋為什麼 threshold 從 3% 一路調到 5%（兩次 flaky run 後逐步放寬）。**threshold 改動時把 commit 訊息寫清楚原因**，讓未來 reviewer 知道是 known flakiness 而不是隨便鬆綁。

### 9.7 Goldens 的視覺工具

VS Code 有 PNG diff 擴充，或用 `compare` (ImageMagick)：

```bash
compare test/golden/myapp/01_initial.png test/results/myapp/actual_01_initial.png diff.png
```

對 1-2 KB 小 PNG 的 diff 用眼睛看通常更快。

---

## 10. Scenario harness 內部到底在幹嘛

[`test/harness/scenario_runner.cpp`](../../test/harness/scenario_runner.cpp) + [`main.cpp`](../../test/harness/main.cpp) 是整個 GUI 測試的引擎。理解它能讓你寫 scenario 時不會誤解 timing。

### 10.1 啟動序列

```
1. SDL2 init (--headless 時 SDL_VIDEODRIVER=dummy，不開視窗)
2. LVGL init + framebuffer 接到 SDL surface
3. 整個 firmware 的 setup() 跑一遍：
   - app_controller 初始化
   - 每個 *_app 透過 app_install() 註冊
   - mpu / tft / RGB stub 接上
4. Open scenario file，開始逐行 parse
```

### 10.2 Per-line 處理

每一個 `.scn` 指令都對應一個 handler：

```cpp
if (line == "app NAME")          → app_controller->launch_by_name(NAME)
if (line == "wait_ms N")         → tick_for_ms(N)  // 重複呼叫 lv_task_handler() 直到時間到
if (line == "action TURN_LEFT")  → set fake imu action → tick_for(50)
if (line == "screenshot LABEL")  → SDL surface → PNG → 存或比對
if (line == "assert_no_crash")   → check 沒抓到 SIGSEGV/SIGABRT
if (line == "http_fixture URL F")→ register override (per-scenario）
if (line == "flash_seed PATH C") → write to in-memory flash mock
if (line == "threshold N")       → override default diff_threshold_pct
```

### 10.3 `tick_for(ms)` 在做什麼

```cpp
void tick_for_ms(int ms) {
    auto end = millis() + ms;
    while (millis() < end) {
        lv_task_handler();   // 跑 LVGL animation / event
        // 也戳 app_controller main loop 一次（如果有 IMU action queued）
        SDL_Delay(5);        // 5ms 真實休息（讓 host CPU 不爆）
    }
}
```

意思：scenario 裡的 `wait_ms 400` ≈ 80 次 `lv_task_handler()` 呼叫，跟真實 device 上 400ms 過程**很接近但不完全一樣**（host CPU 比 ESP32 快得多，render 一次 frame 較快）。這就是為什麼某些 anim 在 host 跑起來看起來太快 — 但比對 golden PNG 還是準的，只是動畫過程的 frame-by-frame timing 不一樣。

### 10.4 Action 的 fake injection

`action UP` 不是真的戳 IMU — 是把 fake IMU stub 的下個 `getAction()` 回傳值設成 UP，然後 `tick_for(50)` 讓 main_process 看到一次。所以**每個 action 後預設只跑 50ms**，如果你的 app 處理 action 需要更久（例如載圖），記得加 `wait_ms 400` 在 screenshot 之前讓畫面穩定。

### 10.5 Crash 偵測

`assert_no_crash` 不是真的 try/catch — 是 host 端 `SIGSEGV` / `SIGABRT` handler 設了 sentinel flag，scenario 跑完看 flag 有沒有被設。

如果 scenario 跑到一半 crash，process 會直接退掉，CI workflow 用 `addr2line` 把 stack trace 從 `+0xN` offset decode 成 `file:line`（[`regression.yml`](../../.github/workflows/regression.yml) 那個 `if [ $rc -eq 139 ]` block）。**所以 SDL2 harness 的 crash 是看得到 stack trace 的**，比真機方便。

---

## 11. CI fail 真實案例 — v2.6.1 release fail（2026-04-29）

PR-3.3 (#69) 把 `ESP32FtpServer.cpp` 拆成 5 個檔。所有 CI job 全綠 → merge → tag v2.6.1 → release workflow 跑韌體 build → **fail**：

```
src/app/file_manager/ESP32FtpServer_internal.h:21:8: error: 'WiFiServer' does not name a type
```

### 11.1 為什麼 CI 沒抓到

regression workflow 當時跑：
- `gui-regression` (host SDL2 build) ✅
- `unit-tests` (`pio test -e native_unit`) ✅
- `unit-tests` (`pio test -e native_ftp`) ✅

**但**`pio run -e HoloCubic_AIO_Releases`（真實 ESP32 firmware build）**只在 release workflow 跑，從不在 PR CI 跑**。所以 PR 過、merge 之後才在 release time 才發現。

### 11.2 Bug 本身

`ESP32FtpServer_internal.h` `#include <WiFiClient.h>` 但**沒** `#include <WiFi.h>`。
- 真實 ESP32 Arduino-core：`WiFiServer` class 在 `WiFi.h`，`WiFiClient` 在 `WiFiClient.h`，獨立兩個 header
- host stub `test/stubs_ftp/WiFiClient.h`：把兩個都塞同一個檔（為了測試方便）

→ host build 過、firmware build 炸。

### 11.3 修法

PR-#76：
1. 加 `#include <WiFi.h>` 到 internal header → firmware build 通過
2. PR #72：**把 `pio run -e HoloCubic_AIO_Releases` 加進 regression workflow**（新 `firmware-build` job），這類 host-stub vs Arduino-core 不一致下次 PR 時就抓得到，不會等到 release

### 11.4 從這個 fail 學到

1. **CI 沒跑到的 build target 永遠會在最不方便的時候炸** — Coverage gap 比沒測試還危險，你以為有測但其實沒
2. **Stub 跟 production library 不一致是設計常見陷阱** — 兩邊靜態檢查方式不同 (host stub include 順序鬆散、ESP32 Arduino-core 嚴格)，需要兩邊都 build 過才安全
3. **Recovery 流程**：tag 已 push 但 release 未發布 → 砍 tag、修 code、重 push tag。正常做。如果 release 已 publish 就要升版號（v2.6.1 → v2.6.2）不要 force-update tag。詳見章節 07 §7

---

## 12. 寫好測試的設計原則

讀完 1-11 段，回頭整理幾條心法：

1. **Pure function 比 stateful class 好測**。先 refactor 成 pure（章節 7.2），測試立刻變簡單
2. **Test name 是文件**。`test_well_formed_config_parses_all_six_fields` 比 `test_config_1` 好十倍
3. **Test 要 isolated**。`setUp` reset state、不要相依執行順序
4. **每個 test 一個 assertion 概念**（multiple `TEST_ASSERT_*` 都驗同一個概念 OK）
5. **失敗訊息要可讀**。`TEST_ASSERT_TRUE_MESSAGE(cond, "explanation")` 比 raw `TEST_ASSERT_TRUE(cond)` 好
6. **Negative path 跟 happy path 一樣重要**。空 input、超大 input、畸形 input 都要有 case
7. **Flaky 測試比沒測試還糟**。發現 flaky 立刻處理（拉長 wait_ms、放寬 threshold、固定 input），不要 ignore
8. **CI 失敗訊息**永遠先比對 local 重現。本機跟 CI fail 了同一件事 → 容易 debug；只有 CI fail → 通常是環境差異（cache 殘留、平台差異），先看 CI log 找線索

## 下一步

- [07 — CI + Release](./07-ci-and-release.md) — workflow 設定、tag 怎麼觸發 release
- [08 — 重構與優化案例集](./08-refactoring-case-studies.md) — 10 個真實 PR 的「舊→新」對照，含為什麼改

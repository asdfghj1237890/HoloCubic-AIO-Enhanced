# 04 — 工具函式 + 常用模式

幾個你寫 app 一定會用到的東西。

## 1. `http_util` — 統一的 HTTP fetch

[`AIO_Firmware_PIO/src/http_util.h`](../../AIO_Firmware_PIO/src/http_util.h) — 包掉 `HTTPClient` 的 boilerplate。

```cpp
#include "http_util.h"
#include "ArduinoJson.h"

void my_fetch_weather(void)
{
    DynamicJsonDocument doc(2048);
    int httpCode = 0;

    // GET https://api.example.com/weather → 解析成 JSON
    bool ok = http_fetch_json(
        "https://api.example.com/weather",
        doc,
        3000,        // timeout_ms
        &httpCode
    );

    if (!ok) {
        Serial.printf("[Weather] fetch failed (code=%d)\n", httpCode);
        return;
    }

    // 用 ArduinoJson v6 的 `| fallback` 安全取值
    int temp = doc["temperature"] | 0;
    const char *city = doc["city"] | "Unknown";
    Serial.printf("[Weather] %s %d°C\n", city, temp);
}
```

兩個變體：
- `http_fetch_json(url, doc, timeout_ms, http_code_out, header_name?, header_value?)` — GET + JSON parse
- `http_fetch_string(url, out, timeout_ms, header_name?, header_value?)` — GET + 原始字串

第 5、6 個參數是可選 header（PR-2.4 加的，weather 為了 AccuWeather 要送 User-Agent）。

**為什麼用這個**：
- 自動處理 begin/setTimeout/GET/getStream/end 的順序
- 自動 retry on transient failure
- 統一 timeout 行為
- WiFi 沒連、HTTP 非 2xx、JSON parse fail 都回 false 而不會 crash

歷史上 weather/bilibili/anniversary/settings/stockmarket 各自寫一份 HTTPClient 樣板，後來 Phase 2 (PR #54-58) 全部遷移到這個 helper。

## 2. JSON 解析 — 防止 crash

[`AIO_Firmware_PIO/src/json_util.h`](../../AIO_Firmware_PIO/src/json_util.h)（雖然 PR-1.5 後大部分地方直接用 ArduinoJson v6 的 `| fallback` 就夠了）。

ArduinoJson v6 的安全 idiom：

```cpp
// ❌ 危險 — 如果 "name" 不存在或型別不對，crash
const char *name = doc["name"].as<const char*>();
int age = doc["age"].as<int>();

// ✅ 安全 — 用 | 提供 fallback 值
const char *name = doc["name"] | "Unknown";
int age = doc["age"] | 0;
```

巢狀 key 也安全 — 中間任何一層 missing 都會傳到最尾的 `|`：
```cpp
float temp = doc["weather"]["current"]["temp"] | 0.0f;
```

**這是 PR-1.5 標準化的做法**。新程式碼**不要**裸用 `.as<T>()`。

## 3. `String` vs `char[]` — 何時用哪個

| 情境 | 用什麼 | 為什麼 |
|---|---|---|
| 暫時拼接 HTML / JSON 字串 | `String` | 自動成長、好讀；`web_setting_forms.cpp` 全用這個 |
| 韌體 config 結構欄位 | `char[N]` | 大小固定、好預測 SRAM；`heartbeat_config_parse.h` 是範本 |
| Snprintf 目的地 | `char[N]` (`snprintf(buf, sizeof(buf), ...)`) | snprintf 不會吃 String |
| F() 字面值 | `F("...")` 不是 String | `F()` 把字串塞 PROGMEM (flash)，不吃 SRAM |

**重要**：`snprintf(dst, sizeof(dst), "%s\n", src)` 永遠比 `strcpy(dst, src)` + `strcat(dst, "\n")` 安全。PR-1.2 + PR #71 全部把 `strcpy` 換掉了。**新程式碼禁用 `strcpy` / `sprintf` (沒 n 的版本)。**

## 4. PROGMEM / `F()` — 把字串塞 flash 不吃 SRAM

ESP32 SRAM 只有 ~320KB，大字串字面值預設會載到 SRAM 占空間。`F()` macro 把字串標記為 PROGMEM，從 flash on-demand 讀：

```cpp
// ❌ 占 SRAM
Serial.println("This long debug message wastes 50 bytes of RAM");

// ✅ 留在 flash
Serial.println(F("This long debug message wastes 50 bytes of RAM"));
```

`String webpage += F("...")` 是常見模式。整個 `web_setting.cpp` 都這樣做，把 ~30KB 的 Glass CSS/JS 字面值留在 flash。

> Host 端 stub 把 `F(x)` 定義成 identity，所以同一份程式碼在 host 測試時也能編譯。

## 5. `g_flashCfg` — 設定持久化

[`driver/flash_fs.h`](../../AIO_Firmware_PIO/src/driver/flash_fs.h) — 用 LittleFS 包出來的簡單 read/write API。

```cpp
#define MY_CONFIG_PATH "/myapp.cfg"

// 寫
g_flashCfg.writeFile(MY_CONFIG_PATH, "line1\nline2\n");

// 讀
char buf[256] = {0};
uint16_t size = g_flashCfg.readFile(MY_CONFIG_PATH, (uint8_t*)buf);
buf[size] = 0;  // 別忘了 null-terminate
```

慣例：
- 路徑用 app name 開頭，避免衝突
- 內容用 `\n` 分隔欄位（簡單）
- 解析用 [`driver/analyse_param.cpp`](../../AIO_Firmware_PIO/src/driver/analyse_param.cpp) 的 `analyseParam(buf, n_fields, char *out[])`
- 第一次讀（size==0）就寫預設值

範本：[`app/heartbeat/heartbeat.cpp` 的 `read_config`/`write_config`](../../AIO_Firmware_PIO/src/app/heartbeat/heartbeat.cpp)。

## 6. Web Settings — 怎麼加新欄位

要讓使用者透過瀏覽器改設定，需要動三個地方：

### 6a. 加 form-render handler（顯示欄位）

[`AIO_Firmware_PIO/src/app/server/web_setting_forms.cpp`](../../AIO_Firmware_PIO/src/app/server/web_setting_forms.cpp) — 每個 `*_setting()` 函式 build 一張 form。新增 app 就加新函式：

```cpp
void hello_setting()
{
    char username[32];
    char display_speed[32];
    app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_GET_PARAM,
                            (void*)"username", username);
    app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_GET_PARAM,
                            (void*)"display_speed", display_speed);

    String form;
    emit_form_open (form, "saveHelloConf", "form_hello_title");
    emit_text_field(form, "fld_hello_username", "username", username);
    emit_text_field(form, "fld_hello_speed", "display_speed", display_speed);
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}
```

helper 函式（`emit_form_open` / `emit_text_field` / `emit_pwd_field` / `emit_radio2_field` / `emit_form_close`）都在同一個檔案頂端的匿名 namespace 裡。

### 6b. 加 save handler（接收 POST）

[`web_setting_handlers.cpp`](../../AIO_Firmware_PIO/src/app/server/web_setting_handlers.cpp) — 對應 form 的 `action="/saveHelloConf"`：

```cpp
void saveHelloConf()
{
    if (server.hasArg("username"))
        app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_SET_PARAM,
                                (void*)"username", (void*)server.arg("username").c_str());
    if (server.hasArg("display_speed"))
        app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_SET_PARAM,
                                (void*)"display_speed", (void*)server.arg("display_speed").c_str());

    app_controller->send_to(SERVER_APP_NAME, "Hello", APP_MESSAGE_WRITE_CFG, NULL, NULL);
    post_save_redirect("/hello_setting");
}
```

### 6c. 註冊 routes

[`server.cpp`](../../AIO_Firmware_PIO/src/app/server/server.cpp) `start_web_config()`：
```cpp
#if APP_HELLO_USE
    server.on("/hello_setting", hello_setting);
    server.on("/saveHelloConf", saveHelloConf);
#endif
```

### 6d. 加 sidebar nav 連結

[`web_setting.cpp`](../../AIO_Firmware_PIO/src/app/server/web_setting.cpp) `init_page_header()` 裡找 sidebar 那段，加一個 nav-item：
```cpp
#if APP_HELLO_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/hello_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/hello_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("hello");
    webpage_header += F("</span></a>");
#endif
```

### 6e. 加 i18n keys

`web_setting.cpp` 的 `getText()` cascade（找 `if (strcmp(key, "fld_xxx") == 0)` 那一堆，照 pattern 加）：
```cpp
if (strcmp(key, "form_hello_title") == 0) {
    if (current_lang == LANG_ZH_CN) return "Hello 应用";
    if (current_lang == LANG_ZH_TW) return "Hello 應用";
    return "Hello App";
}
if (strcmp(key, "fld_hello_username") == 0) { ... }
if (strcmp(key, "fld_hello_speed") == 0) { ... }
if (strcmp(key, "hello") == 0) { ... }    // sidebar nav label
```

### 6f. 在 app message_handle 裡接 GET_PARAM / SET_PARAM

回到 `hello.cpp`：
```cpp
static void hello_message_handle(const char *from, const char *to,
                                 APP_MESSAGE_TYPE type, void *message, void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_GET_PARAM: {
        const char *key = (const char *)message;
        if (!strcmp(key, "username")) {
            snprintf((char*)ext_info, 32, "%s", cfg_data.username);
        } else if (!strcmp(key, "display_speed")) {
            snprintf((char*)ext_info, 32, "%d", cfg_data.display_speed);
        }
    } break;
    case APP_MESSAGE_SET_PARAM: {
        const char *key = (const char *)message;
        const char *val = (const char *)ext_info;
        if (!strcmp(key, "username")) {
            snprintf(cfg_data.username, sizeof(cfg_data.username), "%s", val);
        } else if (!strcmp(key, "display_speed")) {
            cfg_data.display_speed = atoi(val);
        }
    } break;
    case APP_MESSAGE_READ_CFG:  hello_read_config(&cfg_data);  break;
    case APP_MESSAGE_WRITE_CFG: hello_write_config(&cfg_data); break;
    default: break;
    }
}
```

完成。重 build 燒進去 → 在瀏覽器到 `http://192.168.4.1/hello_setting` → 改值 → Save → 你的 app 下次啟動就讀到新設定。

## 7. PSRAM — 可以用嗎？

ESP32 (HoloCubic 用的版本) 有 4MB PSRAM，用 `ps_malloc()` / `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` 配置。圖片解碼 / 大 buffer 用 PSRAM；常駐小資料用普通 SRAM。

`media_player`、`picture` app 都用 PSRAM 存 frame buffer，可以參考。

## 8. 不要做的事 — 常見地雷

- `delay()` 在 main_process 裡 — 卡住 LVGL，整個 UI 凍結。**用 `if (millis() - last < N) return;` 早退**。PR-1.8 整個 audit 過了。
- `String += ` 一大堆短 chunk — 會頻繁 realloc，造成 heap fragmentation。一個函式裡如果 `+=` 超過 ~30 次，考慮用 `char[N]` + `snprintf` 一次寫完。
- `serial.print()` 在每個 tick — 串口很慢 (115200 bps ≈ 12KB/s)，太多 print 也會卡 main loop。Debug 訊息用 `#ifdef FTP_DEBUG` 之類的條件編譯。
- 在 `background_task` 裡碰 `run_data` — 它可能已經被 `exit_callback` 釋放了。只能碰 `forever_data`（你自己定義的常駐結構）。
- 直接 `WiFi.localIP()` 然後 assume 連得上 — 開機後 WiFi 是 AP 模式，要看 `WiFi.softAPIP()`。詳見 PR #80 的修正。

## 下一步

- AIO_Tool 加按鈕 → [05 — AIO_Tool 開發](./05-aio-tool.md)
- 寫測試 → [06 — 測試完整指南](./06-testing.md)

# 08 — 重構與優化案例集

這份文件列舉 v2.6.x 系列做過的真實重構，每一個都記下「**舊的長什麼樣 → 為什麼那樣不好 → 新的長什麼樣 → 為什麼這樣解**」。目的不是讓你複製 commit hash，是讓你看到**思考過程**，下次自己遇到類似情境能比較快推理。

每一節獨立，可以跳著讀。但案例的順序大致從「明顯錯誤」到「設計取捨」排列。

---

## 1. `strcpy` → `snprintf` 有界拷貝（PR #26、#41）

### 舊的

[`heartbeat.cpp` (pre-PR-1.2)](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/blob/26fbf0c~1/AIO_Firmware_PIO/src/app/heartbeat/heartbeat.cpp)：

```cpp
strcpy(cfg->mqtt_server, param[0]);   // dst is char[32]
strcpy(cfg->mqtt_user,   param[2]);   // dst is char[16]
strcpy(cfg->mqtt_password, param[3]); // dst is char[16]
```

### 為什麼不好

`param[0]` 來自使用者透過 web 設定填入的字串。沒人保證使用者填的長度小於 dst buffer。如果 user 填了 50 個字元的 mqtt_server，`strcpy` 會**乖乖寫滿 50 個 byte 加 NUL**，把 dst 後面 18 個 byte 連同**相鄰的 cfg 結構欄位整個踩爛**。在 ESP32 上踩到結構邊界外的 byte = stack 上某個變數被改 = 後面任何時間點 random crash，trace 完全對不上原因。

這是**最古典的 buffer overflow** — 不是駭客攻擊路徑（cube 沒對外網），但是**「使用者誤填欄位 → 一週後隨機 crash」**這種炸彈。

### 新的

```cpp
snprintf(cfg->mqtt_server, sizeof(cfg->mqtt_server), "%s", param[0]);
snprintf(cfg->mqtt_user,   sizeof(cfg->mqtt_user),   "%s", param[2]);
snprintf(cfg->mqtt_password, sizeof(cfg->mqtt_password), "%s", param[3]);
```

### 為什麼這樣解

- `snprintf` 第二個參數是 dst 容量，超過就**安靜截斷**，永遠寫不出去
- `sizeof(cfg->mqtt_server)` 自動跟著 struct 欄位大小走 — struct 改了不用回頭找 strcpy 改數字
- `"%s"` 是固定 format string，沒有 user-data-as-format-string 的雷
- 結尾 NUL 由 snprintf 保證

**rule of thumb**：新 code **禁用 `strcpy` / `sprintf`**（沒 n 的版本）。用 `snprintf(dst, sizeof(dst), ...)` 一律安全。`strncpy` 也不要 — 它**不保證 NUL 結尾**（很多人忘記），自己去看 man page。

---

## 2. JSON `.as<T>()` → `| fallback`（PR #34）

### 舊的

[`weather.cpp` (pre-PR-1.5)](../../AIO_Firmware_PIO/src/app/weather/weather.cpp) 解析 AccuWeather 回應：

```cpp
JsonObject current = doc[0];
run_data->wea.temperature = current["Temperature"]["Metric"]["Value"].as<int>();
run_data->wea.humidity    = current["RelativeHumidity"].as<int>();
const char *weatherText   = current["WeatherText"].as<const char*>();
strcpy(run_data->wea.weather, weatherText);
```

### 為什麼不好

如果 AccuWeather 哪天 schema 改了（移掉 "RelativeHumidity"、或免費 tier 不再回傳 "Metric.Value" 子物件），`.as<int>()` 對一個不存在 / 型別不對的 JsonVariant 會回 0、`.as<const char*>()` 會回 `nullptr`。

回 0 還好；**回 nullptr 接到 `strcpy(dst, weatherText)` 直接 segfault**。

更慘的是這種 schema-change crash 是**間歇性的** — 平常 API 都好好的，某天 AccuWeather 部署新版 → 你裝置開機就 panic → reboot → panic → reboot → 你抓不到原因。

### 新的

```cpp
run_data->wea.temperature = current["Temperature"]["Metric"]["Value"] | 0;
run_data->wea.humidity    = current["RelativeHumidity"] | 0;
const char *weatherText   = current["WeatherText"] | "";
snprintf(run_data->wea.weather, sizeof(run_data->wea.weather), "%s", weatherText);
```

### 為什麼這樣解

- ArduinoJson v6 的 `| fallback` operator：欄位 missing / 型別不對 → 走 fallback；否則回實際值
- 巢狀路徑（`["Temperature"]["Metric"]["Value"]`）中**任何**一層 missing 都會 propagate 到最尾的 `|`
- fallback 給 `0` / `""` 之類的 sentinel，UI 顯示「0°C」「unknown」遠遠比 crash 好
- 配合 `snprintf` 確保 `weatherText` 即使是 `nullptr` 也不會炸（雖然這個版本回的是 `""` 不是 `nullptr`，但雙保險）

**rule of thumb**：新 code 碰 ArduinoJson **永遠用 `| fallback`**，不要裸用 `.as<T>()`。

---

## 3. MQTT callback 寫到 `payload[length]` 越界（PR #38）

### 舊的

[`heartbeat.cpp` (pre-PR-1.4)](../../AIO_Firmware_PIO/src/app/heartbeat/heartbeat.cpp)：

```cpp
void HeartbeatAppForeverData::callback(char *topic, byte *payload, unsigned int length)
{
    payload[length] = 0;   // ← 這行
    char *msg = (char *)payload;
    if (strcmp(msg, "BEAT") == 0) {
        // ...
    }
}
```

### 為什麼不好

`payload` 是 `PubSubClient` library **借**給我們的 buffer。我們**不擁有它**。`payload[length] = 0` 寫在 `length` 那個 index — 那個 byte 是 buffer 的下一格，**屬於 PubSubClient 內部結構**。

PubSubClient 的 source 看下去，`payload[length]` 那格通常是下一個 packet header 的開頭。我們把它寫成 0 → 下一個 MQTT message 處理時 PubSubClient 看到 header 是 0 → 整個 client crash 或行為錯亂。

實際症狀：MQTT 連線間歇性斷掉、收幾個 message 後再也收不到。

### 新的

```cpp
void HeartbeatAppForeverData::callback(char *topic, byte *payload, unsigned int length)
{
    char local[64];
    size_t copy = (length < sizeof(local) - 1) ? length : sizeof(local) - 1;
    memcpy(local, payload, copy);
    local[copy] = 0;   // ← 寫進自己的 buffer，不踩別人的記憶體

    if (strcmp(local, "BEAT") == 0) {
        // ...
    }
}
```

### 為什麼這樣解

- 借來的 buffer 一律當 read-only
- 自己 stack 上開個 char[64]，把 payload 複製進來再 NUL terminate
- 64 byte 對 MQTT control message（"BEAT"、"PING" 之類短字串）綽綽有餘；超過就截斷
- 不再碰 `payload[length]` 那格

**rule of thumb**：別人傳給你的 buffer，**寫之前先抄一份到自己的 buffer**。Library API doc 沒明寫「你可以寫到 length 位置」就是不行。

---

## 4. `delay(N)` 在 main_process → millis-gated early return（PR #36）

### 舊的

[`weather.cpp` (pre-PR-1.8)](../../AIO_Firmware_PIO/src/app/weather/weather.cpp) `weather_process`：

```cpp
if (run_data->clock_page == 0) {
    display_weather(run_data->wea, anim_type);
    // ... send_to(...) for updates ...
    display_space();
    delay(30);    // ← 「節流」
} else if (run_data->clock_page == 1) {
    display_curve(run_data->wea.daily_max, run_data->wea.daily_min, anim_type);
    delay(300);   // ← 「動畫播完」
}
```

### 為什麼不好

`main_process` 在 main thread 跑，跟 LVGL render、IMU 讀取、其他 app `background_task`、`message_handle` 共用同一個 loop。`delay(300)` = **整個系統凍 300ms** — 螢幕不更新、IMU 動作收不到、WiFi 心跳沒送、所有 app 同時凍。

更糟的是 main_process 大概每 50ms 被戳一次。`delay(300)` 在每個 tick 都跑 → 實際 frame rate 從 ~20 FPS 掉到 ~3 FPS，操作明顯卡頓。

歷史上這種 `delay()` 是「我想限制這個動作頻率」的偷懶寫法。但 **`delay()` 不只是限制這個動作，是把整個系統一起綁住**。

### 新的

```cpp
if (run_data->clock_page == 0) {
    display_weather(run_data->wea, anim_type);
    // ... send_to(...) for updates ...
    display_space();
    // (was: delay(30) — pure throttle removed; AppController already
    //  rate-limits main_process via its 200ms loop timer.)
} else if (run_data->clock_page == 1) {
    display_curve(run_data->wea.daily_max, run_data->wea.daily_min, anim_type);
    // (was: delay(300) — pure throttle removed; same reasoning.)
}
```

如果**真的**需要節流，用 millis-gated early return：

```cpp
static unsigned long last_render = 0;
if (millis() - last_render < 100) return;   // 早退，不卡別人
last_render = millis();
display_weather(...);
```

### 為什麼這樣解

- main_process 已經由 AppController 的 timer 控制 ~200ms 戳一次。再 delay 是雙重節流，沒意義
- 如果 transition 動畫需要時間，正解是**用 `lv_anim_*` API 開非阻塞動畫**，不是 delay 阻塞 main thread
- 早退 (`return`) 把 CPU 還給其他 callback，整個系統 responsive

**rule of thumb**：`main_process` / `message_handle` / LVGL event callback **裡面禁用 `delay()`**。要等時間就 `if (millis() - last < N) return;`。要做動畫就 `lv_anim`。要在 FreeRTOS task 裡等用 `vTaskDelay`（task 內阻塞 OK，main thread 不行）。

---

## 5. HTTPClient 6 份重複 → `http_util` helper（PR #54-58）

### 舊的

[`weather.cpp` (pre-PR-2.2a)](../../AIO_Firmware_PIO/src/app/weather/weather.cpp) 抓天氣：

```cpp
HTTPClient http;
http.setTimeout(3000);
http.addHeader("User-Agent", "ESP32-Weather-Station");
http.begin(api);
int httpCode = http.GET();
if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, payload);
        if (err) {
            Serial.println("[JSON] parse error");
            http.end();
            return;
        }
        // ...用 doc 做事...
    } else {
        Serial.printf("[HTTP] unexpected status %d\n", httpCode);
    }
} else {
    Serial.printf("[HTTP] GET failed: %s\n", http.errorToString(httpCode).c_str());
}
http.end();
```

**這 25 行樣板**在 weather/bilibili/anniversary/settings/stockmarket/heartbeat 各複製一份。每份微妙地不一樣（timeout 不同、有沒有 header、handle redirect 的方式、JSON parse error 處理）。

### 為什麼不好

- **6 份 boilerplate** = 6 個 bug 機會。改一個地方（例如多 retry transient failures），要改 6 處
- 每個 app 自己決定 timeout、retry 策略 → 行為不一致
- 容易忘記 `http.end()` → 連線洩漏 → ESP32 socket pool 用完 → 後面整個 device 連不上
- 各種 corner case 的 error handling 散在 6 個檔案，沒有 single source of truth

### 新的

[`http_util.h`](../../AIO_Firmware_PIO/src/http_util.h)：

```cpp
bool http_fetch_json(const char *url, JsonDocument &out,
                     uint32_t timeout_ms = 5000, int *http_code_out = nullptr,
                     const char *header_name = nullptr,
                     const char *header_value = nullptr);
int  http_fetch_string(const char *url, String &out,
                       uint32_t timeout_ms = 5000,
                       const char *header_name = nullptr,
                       const char *header_value = nullptr);
```

呼叫端從 25 行降到 3 行：

```cpp
DynamicJsonDocument doc(2048);
int httpCode = 0;
if (!http_fetch_json(api, doc, 3000, &httpCode,
                     "User-Agent", "ESP32-Weather-Station")) {
    Serial.printf("[Weather] fetch failed (code=%d)\n", httpCode);
    return;
}
// ...用 doc 做事...
```

### 為什麼這樣解

- 一個函式，所有呼叫端共用 → fix 一次，6 處受益
- 內建 `http.end()` 在 RAII-ish 的位置（function 出去前一定 cleanup），洩漏不再可能
- timeout 由 caller 傳，default 安全值 5000ms
- `http_code_out` optional，caller 想看細節就傳指標
- HTTP header 也參數化（PR-2.4 加的，weather AccuWeather 那站會 ban 沒 User-Agent 的）
- error handling 集中：WiFi 沒連、HTTP code 非 2xx、JSON parse 失敗 → 全部回 false，caller 不需 nested if

### 為什麼當時沒順便改成 retry / circuit breaker？

刻意**不**加。不是不需要，是 scope 控制：retry 邏輯加進來會涉及策略決策（重試幾次？間隔多久？要不要 backoff？）。**第一個版本只做 extract + dedupe，行為跟舊的逐位元一致**，這樣 review 容易看出「真的等價、我沒偷塞 behavior change」。retry 留給後續 PR。

**rule of thumb**：**每次只做一件事**。Helper 抽取 + 行為變更分兩個 PR。

---

## 6. CTkButton `widget["text"]` 兩個雷（PR #74）

### 舊的

[`download_debug.py` (pre-PR-74)](../../AIO_Tool/page/download_debug.py)：

```python
def com_connect(self):
    if self.m_connect_button["text"] == self.i18n.t("open_serial"):  # ← 雷 1
        # ... 開啟 port ...
        self.m_connect_button["text"] = self.i18n.t("close_serial")   # ← 雷 2
        self.m_reboot_button["state"] = tk.DISABLED                    # ← 雷 2
    else:
        self.m_connect_button["text"] = self.i18n.t("open_serial")     # ← 雷 2
```

### 為什麼不好（兩個獨立的雷疊在一起）

**雷 1：`widget["text"]` 讀** —
`tkinter.Misc.__getitem__` 的實作是這樣：

```python
def cget(self, key):
    return self.tk.call(self._w, 'cget', '-' + key)
```

直接呼叫 Tk 的 cget，**不會走** CTkButton 的 cget override。Tk 的 underlying widget 是 Frame（CTkButton 用 Frame + 內嵌 Label 偽造 button），Frame 沒有 `-text` option → **TclError: unknown option "-text"**。

但 Tk 的 callback exception handling 預設是 print stack trace 到 stderr → PyInstaller `--noconsole` build 沒有 stderr → **錯誤完全消失，使用者看到「按了沒反應」**。

**雷 2：`widget["text"] = X` 寫** —
`__setitem__` 的實作：

```python
def __setitem__(self, key, value):
    self.configure({key: value})
```

把 dict 當第一個 positional arg 丟給 configure。但 CTkButton.configure 簽名是：

```python
def configure(self, require_redraw=False, **kwargs):
```

dict 被當成 `require_redraw`（被 absorb 成一個 truthy 值），`kwargs` 是空的。**none of the special-case branches for "text"/"state" 被執行，文字永遠不會更新，silent no-op**。

兩個雷疊起來的症狀：**按鈕看起來有反應（disable/enable 動作 visibly 沒生效但小到看不出來），按一次後文字「應該」要從「開啟」變「關閉」實際沒變，再按一次又進了「開啟」分支重複開 port → "Receive_thread start" log 出現兩次**。

### 新的

```python
def com_connect(self):
    if self.m_connect_button.cget("text") == self.i18n.t("open_serial"):  # ← 用 cget()
        # ...
        self.m_connect_button.configure(text=self.i18n.t("close_serial"))  # ← 用 configure()
        self.m_reboot_button.configure(state=tk.DISABLED)
    else:
        self.m_connect_button.configure(text=self.i18n.t("open_serial"))
```

### 為什麼這樣解

- `cget("text")` 是直接 method call，走 MRO，會打到 CTkButton 的 cget override
- `configure(text=...)` 是 keyword argument 形式，`require_redraw` 留 default False，`kwargs={"text": ...}` 進去走 special-case branch
- 整個 file 全部換掉，rule of thumb 統一

**這個修法的學習**：library 用法的 footgun 不會自動寫在 doc 上，**靠看 source 才會發現**。當 production 行為跟 documentation 對不上，**先去看 library 的 setitem/getitem 實作**，多半問題在那。

順帶一提這個 bug 是怎麼被**找到**的：使用者說「按了沒反應」→ 我加了 `report_callback_exception` 全域 handler 把 Tk 吞掉的 exception 抓出來顯示 → 看到 `TclError: unknown option "-text"` → 才往 CTkButton source 挖。**做對的「surface error」永遠是 debug 起點**，比加更多 print 有效。

---

## 7. PyInstaller `--noconsole` 的兩個 bundle 雷（PR #74、#76）

### 雷 7a：`sys.stdout = None`

#### 舊的

`CubicAIO_Tool.spec` 設定 `console=False`（windowed app，沒 console window）。esptool 跑下去：

```python
esptool.main(["--port", "COM5", "--baud", "115200", "write_flash", ...])
```

→ esptool 內部 `print("Connecting...")` 然後 `sys.stdout.flush()` → **AttributeError: 'NoneType' object has no attribute 'flush'**。

#### 為什麼不好

PyInstaller windowed build **把 sys.stdout 跟 sys.stderr 設成 None**（沒 console 可寫）。任何 third-party library 假設 stdout 一定存在的就會炸。esptool 是其中一個。

#### 新的

App startup 加：

```python
def _ensure_std_streams() -> None:
    import sys
    if sys.stdout is None:
        sys.stdout = open(os.devnull, "w", encoding="utf-8")
    if sys.stderr is None:
        sys.stderr = open(os.devnull, "w", encoding="utf-8")

if __name__ == "__main__":
    _ensure_std_streams()  # ← 一定要在 import 任何 library 之前
    ...
```

#### 為什麼這樣解

- 替換 None 成 devnull writer → esptool 可以 `.flush()` 不炸
- 它 print 的內容沒人看到（沒 console）但**那本來就不是 windowed app 的責任**
- 如果你想讓 esptool 的 progress 進到操作記錄，要寫 custom writer 把 .write() 轉發到 print_log，是另一個 scope

### 雷 7b：missing data files

#### 舊的

flash 進到下一步又炸：

```
Stub flasher JSON file for ESP32 not found
```

esptool 的 `targets/stub_flasher/<v>/esp32.json` 是執行期才載的 package data。

#### 為什麼不好

PyInstaller 的靜態分析是看 `import` 語句決定要 bundle 什麼。**runtime 才用 `importlib.resources` / `pkgutil.get_data` 載的 data file 它看不到** → 沒 bundle 進 .exe → runtime FileNotFoundError。

#### 新的

`CubicAIO_Tool.spec` 加：

```python
from PyInstaller.utils.hooks import collect_data_files
esptool_datas = collect_data_files('esptool')

a = Analysis(
    ...
    datas=[
        ('cubictool.json', '.'),
        ('image', 'image'),
        ('i18n', 'i18n'),
        *esptool_datas,    # ← 把整個 esptool 的 data 拉進來
    ],
    ...
)
```

#### 為什麼這樣解

- `collect_data_files('esptool')` 走整個 package install 目錄，把所有 non-`.py` 檔案列出
- `*` unpack 進 datas list — esptool 不管以後加多少 stub_flasher JSON 都自動 cover
- 同樣的 pattern 適用於任何「運行期才載 data」的 library（jinja2 templates、locale 檔...）

**這兩個 bug 的學習**：PyInstaller 跟 windowed app 的組合**有一整類隱性陷阱**，不是 library 本身的 bug，是 packaging 假設不成立。下次包 windowed Python tool 要警惕：(a) sys.std{out,err} 可能是 None；(b) runtime 載的 data 要手動列。

---

## 8. `WiFi.status() == WL_CONNECTED` 看不到 AP mode（PR #80）

### 舊的

[`web_api.cpp` (pre-PR-80)](../../AIO_Firmware_PIO/src/app/server/web_api.cpp)：

```cpp
bool wifi_up = (WiFi.status() == WL_CONNECTED);
char ip_buf[32] = {0};
if (wifi_up) {
    IPAddress ip = WiFi.localIP();
    snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
// JSON 回傳 wifi.connected = wifi_up, wifi.ip = ip_buf
```

### 為什麼不好

ESP32 的 WiFi 有三種模式：**STA**（client，連到 router）、**AP**（hosting hotspot）、**AP_STA**（同時兩個）。

- `WiFi.status()` 只回 **STA** 的狀態。AP-only 模式時永遠回 `WL_DISCONNECTED`
- `WiFi.localIP()` 只是 STA mode 的 IP
- AP mode 的 IP 在 `WiFi.softAPIP()`，獨立的 interface

我們的 cube 預設**就是 AP mode**（hosting `HoloCubic_AIO` 給使用者的 PC join）。所以使用者透過 cube 的 AP 連到 web UI 後：

- API 報 `wifi.connected = false`
- IP 顯示 `--`
- ...但是使用者**明明就連在裝置上**才看得到這個畫面，這個 false 完全違反直覺

更糟的是這 bug 完全靜默 — 不會 crash、不會 error、就是顯示錯。typically 要使用者主動回報才會發現。

### 新的

```cpp
bool sta_up = (WiFi.status() == WL_CONNECTED);
IPAddress ap_ip = WiFi.softAPIP();
bool ap_up = (ap_ip[0] | ap_ip[1] | ap_ip[2] | ap_ip[3]) != 0;
bool wifi_up = sta_up || ap_up;

char ip_buf[32] = {0};
if (sta_up) {
    IPAddress ip = WiFi.localIP();
    snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
} else if (ap_up) {
    snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
}

int rssi = sta_up ? WiFi.RSSI() : 0;
String ssid = sta_up ? WiFi.SSID() : (ap_up ? String("AP mode") : String());
```

### 為什麼這樣解

- 真實狀態 = STA-up OR AP-up
- IP 優先用 STA（router-routable，給 mobile 之類用），fallback 用 AP IP
- SSID 在 AP-only 時顯示 "AP mode"，不要空字串看起來像 broken
- AP IP 用「四個 byte 都 0 → 沒 AP」的方式判斷（`softAPIP()` 沒 active 時回 0.0.0.0）

**這個 bug 的學習**：「wifi.status() == WL_CONNECTED」是 ESP32 教學裡常見的 idiom，看起來合理但**只 cover 了一半的場景**。ESP32 的 WiFi 有 3 種 mode 共存，**任何「我在用 WiFi 嗎？」的判斷都要分別看 STA + AP 兩個 interface**。

---

## 9. 靜態資源 cache busting（PR #76、#81）

### 舊的

[`web_setting.cpp` (pre-PR-81)](../../AIO_Firmware_PIO/src/app/server/web_setting.cpp)：

```cpp
webpage_header += F("<link rel=\"stylesheet\" href=\"/static/glass.css\">");
webpage_footer += F("<script src=\"/static/glass.js\"></script>");
```

`/static/glass.{css,js}` 由 [PR-Glass-FU-2 (#65)](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/pull/65) 從 inline 抽出來，serve 時加 `Cache-Control: max-age=86400`。

### 為什麼不好

24 小時 browser cache → 韌體升新版、有 CSS / JS 改動 → **使用者瀏覽器繼續用舊的**，新效果看不到，要等快取過期或手動 hard-refresh。

實際發生過：v2.6.5 修了 uptime 顯示「h」單位錯的 bug。使用者**燒了新韌體**，重新打開 web UI，看到還是舊行為。我以為是修法沒生效，準備重 debug，後來才意識到是 cache 問題。

### 新的

```cpp
webpage_header += F("<link rel=\"stylesheet\" href=\"/static/glass.css?v=" AIO_VERSION "\">");
webpage_footer += F("<script src=\"/static/glass.js?v=" AIO_VERSION "\"></script>");
```

`AIO_VERSION` 是 `"2.6.x"` 字串字面值，C 預處理器在編譯時就把整串拼好。Browser 看到 `?v=2.6.7` → 跟 cache 裡 `?v=2.6.5` 不一樣 → 重抓。

### 為什麼這樣解

- query string 不影響 server route matching（`server.on("/static/glass.js", ...)` 還是會 match `/static/glass.js?v=anything`）
- 但 browser 的 cache key **包含 query string**，version 改了就 invalidate
- 需要記得每次 release 升 `AIO_VERSION`（章節 07 第 5 段）— 沒升就沒效果

### 為什麼不直接縮短 max-age？

- 完全砍 cache（max-age=0）→ 每次 page load 都重抓 30KB，wifi 慢的話頁面慢 1-2 秒打開
- max-age=300（5 分鐘）→ 平常的「升版立即生效」需求還是達不到
- **保留 max-age=86400 + 版本 bust** = 兩全：**沒升版時 cache 24h 飛快**，**升版時新版本立即生效**

**這個的學習**：cache 是 "should be invisible" 的優化，但**版本切換的時候會變成 visible bug**。任何上線靜態資源 cache 都要配版本化 URL 才不會踩這個雷。

---

## 10. FtpServer cmdStatus 0 清掉測試 client（PR #68）

### 舊的（測試端的錯誤假設）

寫 PR-3.0a FTP harness 時，第一版 `ftp_connect_and_auth` 是這樣：

```cpp
WiFiClient ftp_connect_and_auth(FtpServer &srv, const char *user, const char *pass) {
    WiFiClient c;
    c.mark_connected(true);
    ftpServer.push_pending_client(c);  // ← 立刻 push

    srv.handleFTP();   // 應該會送 220 banner？
    std::string welcome = c.take_tx_as_string();
    // welcome 預期含 "220" — 結果空字串，所有 6 個 test 失敗
}
```

### 為什麼不好

`FtpServer::handleFTP()` 是顯式 state machine（章節 02 §9.4）。**它的 cmdStatus 一開始是 0**，0 那個 state 的 job 是「**清掉殘留 client**」：

```cpp
if (cmdStatus == 0) {
    if (client.connected())
        disconnectClient();   // ← 把 client.stop()
    cmdStatus = 1;
}
```

我推一個 client 進去，第一次 `handleFTP()` 看到 `client.connected() == true`、cmdStatus==0 → `disconnectClient()` → `client.stop()` → **rx/tx queue 整個清空**。

cmdStatus 1 → reset state → cmdStatus 2。state 2 才是「等 client」，但這時候我的 client 已經 stop 過了。

實機跑沒這個問題，因為**真實的 TCP client 不會在 server 還沒進到 state 2 之前連上來**（physical world 有時間 ordering）。但測試環境一切都瞬間發生，順序錯就完蛋。

### 新的

```cpp
WiFiClient ftp_connect_and_auth(FtpServer &srv, const char *user, const char *pass) {
    // Walk the server through cmdStatus 0 → 1 → 2 (waiting-for-connect)
    // BEFORE pushing a client. Pushing earlier means the cmdStatus 0
    // branch (`if (client.connected()) disconnectClient();`) calls
    // client.stop() on our queues, wiping the test's injected data
    // before clientConnected() ever runs.
    srv.handleFTP();  // 0 → 1
    srv.handleFTP();  // 1 → 2

    WiFiClient c;
    c.mark_connected(true);
    ftpServer.push_pending_client(c);

    srv.handleFTP();  // 2 → 3, 同時送 220 banner
    // 現在 c.tx 裡有 banner 了
}
```

### 為什麼這樣解

- 模擬真實順序：server **先進入 ready 狀態**，client **才連上來**
- helper 把這個邏輯包好，每個 test case 都不用再煩惱 ordering
- 也用 comment 詳細解釋為什麼要先 pump 兩次，下次有人 review 不會以為這兩行多餘

### 這個 case 的多重學習

1. **顯式 state machine 比隱式好 debug**。FtpServer cmdStatus 用 0/1/2/3 數字，可以直接 reason 順序。如果是隱式 boolean `is_idle && !has_client && ...` 我可能要 debug 半天才找到問題在 "state 0 清 client" 這個前置邏輯
2. **測試環境跟 production 的順序不一樣**。本機 / 真實 ESP32 因為時間是真實流逝，event 自然有間隔。模擬環境一切瞬時，**unstated time-ordering assumption 會炸**
3. **測試 helper 應該封裝 "正確使用順序"**。不要讓每個 test 自己對抗 ordering — helper 把它變成 atomic operation

---

## 結語 — 共通的 pattern

讀完 10 個案例，幾個重複出現的教訓：

1. **「看似 work」不等於「真的 work」**。CTkButton 的 silent no-op、Cache 過期、AP mode 看不到... 都是行為錯了但沒 error。**Surface error first**（PR #74 加 `report_callback_exception` 是模板）
2. **每次只做一件事**。HTTP helper 抽取 PR 不偷塞 retry 邏輯。Refactor 跟 feature 分開
3. **Library 假設不會自己冒出來**。snprintf 比 strcpy 好、cget 比 [...]好、`| fallback` 比 `.as<T>()` 好 — 都是踩過才知道
4. **Test 環境跟 production 不一樣**。Time ordering、buffer sharing、stdout 存在性...都不能假設
5. **`delay()` 是萬惡之源**（在 cooperative event loop 裡）

下次寫類似情境的 code 時，記得回來翻這份。

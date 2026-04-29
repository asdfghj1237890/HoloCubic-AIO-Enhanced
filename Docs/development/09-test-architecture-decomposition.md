# 09 — 測試架構解構（從上到下）

Chapter 06 是「**怎麼寫**測試」，這份是「**測試系統長什麼樣**」。同一套東西，視角不同。

如果你想知道「我寫的這行 code 出 bug，會由哪一層測試抓到？」、「為什麼某個 bug 沒被任何測試攔到（v2.6.1 那次 release fail）？」、「整個 CI 跑的時候在驗什麼？」— 這份是給你看的。

---

## 1. 從 functional view 開始

先不管實作，從**使用者角度**看 cube 該怎樣才算「對」：

```
使用者拿起 cube → 開機 → 看到 launcher menu →
  傾斜選 weather app → 進入 → 看到溫度數字 →
  傾斜切換到 7 日預報 → 切回主頁 →
  按 RETURN 退出 → 回到 launcher

       同時間 →
        透過 PC 連到 cube AP → 開瀏覽器 → 設定頁改 city name → save →
        cube 上 weather app 下次刷新看到新城市
```

**這個 functional flow 涉及的所有層**（從 silicon 到 user input）：

```
                  使用者
                 ↑     ↓
                IMU 動作 / 視覺輸出
                 ↑     ↓
       ┌─────────────────────────────┐
   L7  │  使用者體驗：UI render、互動回饋  │
   L6  │  App 業務邏輯（page 切換、抓資料） │
   L5  │  Web 設定 / API endpoint         │
   L4  │  AppController + 訊息派送         │
   L3  │  http_util / json_util / 共用 util│
   L2  │  Driver layer (mpu/tft/RGB/SD/wifi) │
   L1  │  ESP32 silicon + Arduino-core      │
       └─────────────────────────────┘
```

這 7 層每一層都可能出 bug。**測試系統的任務就是**：每一層都要有方法驗證它對。

---

## 2. 測試系統長什麼樣

我們有 **4 個獨立的測試 env**，每個負責不同層、有自己的 stub 跟 harness：

```
┌─────────────────────────────────────────────────────────────────┐
│  CI: regression.yml                                              │
│  ┌────────────────┐  ┌────────────────┐  ┌──────────────────┐  │
│  │  gui-regression │  │  unit-tests    │  │ firmware-build   │  │
│  │                │  │                │  │                  │  │
│  │  native_test   │  │  native_unit + │  │ HoloCubic_AIO_   │  │
│  │  (SDL2)        │  │  native_ftp    │  │ Releases         │  │
│  │                │  │  (Unity)       │  │ (ESP32 compile)  │  │
│  │  Cover: L7-L4  │  │ Cover: L4-L3   │  │ Cover: L1 only   │  │
│  │  for full app  │  │ for pure logic │  │ (link/compile)   │  │
│  │  + L5 Web UI   │  │ + L5 protocol  │  │                  │  │
│  └────────────────┘  └────────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  CI: aio-tool.yml                                                │
│  ┌────────────────┐                                              │
│  │  pytest+ruff   │   Cover: AIO_Tool 的 util 模組                │
│  └────────────────┘                                              │
└─────────────────────────────────────────────────────────────────┘
```

每個 env 設計目的不同 → 模擬什麼、放什麼進來、stub 什麼，都不一樣。下一節展開。

---

## 3. 每一層 — 模擬什麼、stub 什麼、抓什麼 bug

### 3.1 L1 - ESP32 silicon + Arduino-core（不模擬，靠 compile）

**測試方法**：`pio run -e HoloCubic_AIO_Releases`（CI: `firmware-build` job）

**做什麼**：用真實的 espressif32 platform、真實的 Arduino-core header、cross-compile 韌體 binary。**不執行**，只 link。

**抓哪類 bug**：
- 程式碼語法錯
- Header include 順序問題（PR-3.3 那次 missing `<WiFi.h>`）
- ESP32-only API 用錯（host stub 寬鬆、真實 Arduino-core 嚴格）
- Linker symbol 找不到

**抓不到**：執行時行為、跨層交互、UI 對不對 — 沒跑就抓不到。

**為什麼這層獨立 job**：跟 logic test 跑同一個 env 不對等 — Arduino-core compile 慢，會拖垮所有 PR feedback 時間。獨立 firmware-build job 保留快速失敗的好處又給 ESP32 真編譯。

### 3.2 L2 - Driver layer（基本不測，靠真機驗）

`AIO_Firmware_PIO/src/driver/`：mpu、tft、RGB、flash_fs、analyse_param。

**測試覆蓋率**：~20%。只有 `analyse_param`（純字串操作，無 hw dep）有 unit test。

**為什麼這麼少**：
- mpu driver 跟真實 MPU6050 I²C 訊號綁死，host mock 沒意義（mock 一個假 I²C 流回去不能驗 real bug）
- tft driver 跟 SPI/DMA timing 強相關，真機才看得出
- flash_fs 在 host 端有 in-memory mock（`stubs/` 提供），但 mock 的 flash 不會驗 LittleFS layout 損壞之類的真問題

**結論**：driver layer **本來就需要真機驗**。host 測試不該嘗試 cover 它。

**例外**：`analyse_param` 是純字串 split，跟 hardware 無關 → 該有 unit test。實際上 [`test_config/`](../../AIO_Firmware_PIO/test/native/test_config/) cover 它。

### 3.3 L3 - 共用 util（unit test 主戰場）

`http_util` / `json_util` / 各 app 的 `*_config_parse.cpp` / `game2048_controller.cpp` 等等 — **不依賴 LVGL/WiFi/SD 的純邏輯**。

**測試方法**：`pio test -e native_unit`（Unity）

**Stub 用 `test/stubs_unit/`** — 最小集合，只 cover 純邏輯需要的 type（`String`、`boolean`、`F()` macro、`Wire`、`I2Cdev`、`MPU6050` 假類別）。

**抓哪類 bug**：
- Parser 邊界條件（empty input、超長 input、畸形 input）
- State machine transition 順序
- Number-formatting / config serialization
- Data structure 一致性

**這層的測試最便宜**：build 5 秒、跑 0.3 秒、本機 5 秒內 redo cycle。**TDD 應該全部發生在這層**。

**範例**：[`test_heartbeat_config/`](../../AIO_Firmware_PIO/test/native/test_heartbeat_config/) — 6 個 test cover heartbeat config 的 6 個 corner case（chapter 06 §7 walkthrough 過）。

### 3.4 L4 - System layer（AppController 派送 + cross-app 訊息）

`src/sys/app_controller.cpp` / `send_to_dispatch.cpp`。

**測試方法**：
- 部分 unit test：[`test_app_controller/`](../../AIO_Firmware_PIO/test/native/test_app_controller/) cover send_to_dispatch 的 routing logic
- 整合驗證：所有 GUI scenario 跑起來都暗示這層還活著（不然 app 進不去）

**Stub**：`test/stubs_unit/test_globals.cpp` 提供 fake Wire/Serial 等等。

**抓哪類 bug**：
- 訊息送錯目標 app
- Event queue 漏處理 / 順序錯
- app_install / app_exit 的 lifecycle 邏輯

**抓不到**：跨多個 tick 的時序問題（unit test 沒模擬 hardware timer）。

### 3.5 L5 - Web 設定 / API endpoint

兩條 path 走不同測試：

#### 5a. Web setting form / HTTP handlers
`src/app/server/web_setting*.cpp`、`web_api.cpp`。

**測試方法**：GUI scenario（[`test/scenarios/server/smoke.scn`](../../test/scenarios/server/smoke.scn)）

**為什麼用 GUI scenario 而不是 unit test**：handler 直接呼叫 `Send_HTML` / `server.send` — 強耦合 ESP32 WebServer。沒有便宜的方式 unit-test 這些 handler 的 HTML 輸出，但 GUI scenario harness 已經把 server.cpp link 進來了 → 順手測。

**抓哪類 bug**：
- Form HTML 缺 field、CSS 沒 load
- Save endpoint 不接收 POST data
- Page 渲染 crash

**Coverage gap**：HTML output 的細節 diff（例如 i18n 翻譯漏一個字）GUI scenario 的 screenshot diff 抓不到 — 那是 web 內容、不是 device GUI。**這層其實有點空隙**。

#### 5b. FTP server protocol
`src/app/file_manager/ESP32FtpServer*.cpp`。

**測試方法**：`pio test -e native_ftp`

**Stub**：`test/stubs_ftp/` — scriptable WiFiServer/WiFiClient（雙向 buffer）+ in-memory FakeSD。

**抓哪類 bug**：
- USER/PASS state machine 錯
- Command dispatch 路由錯（PR-3.3 file split 全靠這個 net）
- Response code 錯
- Disconnect timing 錯

**這套是 PR-3.0a (#68) 為了 PR-3.3 拆 FtpServer 而專門建的**。設計時就是「這個 stateful class 我要拆，先有 net 才能拆」。

### 3.6 L6 - App 業務邏輯（state machine + IMU 互動）

每個 `src/app/<name>/<name>.cpp` 的 `*_process` / `*_message_handle`。

**測試方法**：GUI scenario（`test/scenarios/<app_name>/*.scn`）

**Stub**：`test/stubs/`（heaviest stub set — LVGL 接 SDL2、HTTPClient 接 fixture file、IMU 接 fake action injector）。

**抓哪類 bug**：
- Page 切換邏輯錯（按 TURN_LEFT 沒切到下一頁）
- 抓資料失敗時不 graceful（crash 而不是顯示「載入中」）
- Update timer 邏輯錯（永遠不刷新）
- Run_data 跟 cfg_data 用混

**範例**：[`test/scenarios/weather/`](../../test/scenarios/weather/) — flash_seed 寫 fake AccuWeather key，http_fixture 餵 fake JSON response，scenario 走 init → 切頁 → 強制刷新 → screenshot 對比 golden。

### 3.7 L7 - GUI render（最末層，使用者眼睛看到的東西）

LVGL widget tree 變成 framebuffer 變成 PNG。

**測試方法**：GUI scenario 的 `screenshot LABEL` 指令 + golden PNG diff

**抓哪類 bug**：
- 字 truncated / 重疊
- 顏色錯
- 圖示位置漂移
- 因為前一個 PR 改了某個 LVGL container 的 padding 不小心影響到鄰居 widget

**Coverage 限制**：
- Subpixel anti-aliasing 不確定性 → 預設 0.5% threshold，特定 scenario 放寬到 5%（chapter 06 §9.5 講過）
- 動畫過程的 frame-by-frame 不測（只測 wait_ms 後 stable 的 snapshot）
- 顏色 calibration 在 host 跟 ESP32 上**可能略有差異**（同樣的 `lv_color_hex(0xFFFFFF)`，host SDL2 跟真實 TFT 顯示出來的「白」沒到 byte-identical），但實務上 0.5% threshold 涵蓋了

---

## 4. 一個使用者動作貫穿整個 stack — 測試在每一層做什麼

情境：使用者在 weather app 主頁傾斜往前（GO_FORWORD）→ 觸發強制刷新天氣。

```
    ┌──────────────────────────────────────────────────────────┐
    │ User: 拿起 cube 往前傾                                    │
    └────────────────────┬─────────────────────────────────────┘
                         ↓
[L1 silicon] MPU6050 經 I²C 回 raw accelerometer reading
    ↓
    [native_test] ❌ 不 cover (driver 跳過)
    [真機] ✅ 唯一驗法
                         ↓
[L2 driver] mpu.getAction() 把 raw → ImuAction.active = GO_FORWORD
    ↓
    [native_unit test_imu_action] ✅ cover 識別邏輯（用 fake raw input）
                         ↓
[L4 system] AppController.main_process(act_info) 看 active app
    ↓
    [native_unit test_app_controller] ✅ cover dispatch routing
                         ↓
[L6 app] weather_process(sys, act_info)
        看到 GO_FORWORD → coactusUpdateFlag = 1
        send_to(WEATHER_APP, CTRL_NAME, APP_MESSAGE_WIFI_CONN, UPDATE_NOW, NULL)
    ↓
    [GUI scenario test/scenarios/weather/smoke.scn] ✅ 整段流程 cover
        action GO_FORWORD
        wait_ms 400
        screenshot 02_after_forward
                         ↓
[L4 system] req_event_deal() 把 message dispatch 回 weather.message_handle
        weather.message_handle(WIFI_CONN, UPDATE_NOW)
        → 呼叫 get_weather() （在 weather_api.cpp）
                         ↓
[L3 util] http_fetch_json("https://accuweather.com/...", doc, 2000, ...)
    ↓
    [GUI scenario httpfixture] ✅ 餵 fake fixture 不打真網路
    [native_unit test_http_util] ✅ 也 unit-cover http_util 本身
                         ↓
[L6 app] weather_api.cpp parse JSON → run_data->wea.temperature = ...
    ↓
    [GUI scenario weather/empty_response.scn] ✅ cover JSON parse 邊界
                         ↓
[L7 GUI] display_weather() 呼叫 LVGL 把溫度寫到 label
        LVGL render to framebuffer
    ↓
    [GUI scenario screenshot diff] ✅ 對比 golden PNG
```

**每一層都被某種測試 cover** — 除了 L1（silicon I²C 訊號，靠真機）跟 L2 driver（MPU6050 讀數，靠真機）。

換另一個情境（user 透過 web 設定改 city name → 看下次 weather refresh 顯示新城市）的 trace 會經過 L5（web 那條）+ L4 訊息 + L3 util + L6 app + L7 GUI，每段也都各有對應的測試層。

---

## 5. Bug class → 應該由哪一層抓

| Bug 類型 | 應該被誰抓 | 範例 |
|---|---|---|
| Buffer overflow / strcpy | native_unit (config parse test) | PR #26 strcpy → snprintf |
| JSON 欄位 missing 導致 crash | GUI scenario (negative-path fixture) | PR #34 weather schema-change |
| MQTT callback 越界 | native_unit (mqtt callback test) | PR #38 |
| `delay()` 卡 main thread | golden screenshot 看「動畫沒跑出來」+ flake | PR #36 |
| HTTPClient cleanup 漏掉 | (沒有 unit test 直接抓；靠靜態審查) | PR #54 整個 dedupe |
| AppController dispatch 錯 | native_unit (test_app_controller) | (尚未發生過) |
| Web form HTML 不對 | GUI scenario (server smoke) | (有限 cover) |
| FTP command 路由錯 | native_ftp (test_ftp_basic_commands) | PR-3.3 split 用這個保 |
| ESP32 link error | firmware-build job | **PR-3.3 missing `<WiFi.h>`** ← v2.6.1 fail |
| Web UI 字翻譯漏 | (沒測試 cover；視覺檢查) | PR #82 12 forms i18n |
| LVGL widget render 異常 | GUI scenario (screenshot diff) | (常見) |
| AIO_Tool button silent fail | (沒測試 cover；使用者回報 → PR #74) | CTkButton dict-access |

**注意右欄的最後一筆**：CTkButton silent fail 是「對應沒有測試 cover」。AIO_Tool 沒有 GUI 測試（pytest 只 cover util module），button 互動行為只能靠人工或使用者回報。**這是 known coverage gap**。

---

## 6. 測試 vs 真機檢查的明確分工

什麼**只能**真機驗、host test 跳過不嘗試：

| 項目 | 為什麼 host 測不到 |
|---|---|
| MPU6050 校正 / 溫漂 | 真實 sensor 才有真實 noise |
| TFT panel SPI/DMA timing | host 沒 SPI bus |
| RGB LED 顏色 / 亮度曲線 | 真實 LED 才能看 |
| WiFi reconnect / RSSI | 真實 router 環境才有 |
| Flash partition / OTA | 真機 NVS / OTA partition |
| 電池 / 電源切換 | 真機才有 BMS |
| 散熱 / 長時間穩定性 | 真實熱源才能顯現 |
| AIO_Tool .exe 在不同 Windows 版本相容性 | 各 Windows 版本不同 |

什麼 host test **應該** cover、不應該等真機才發現：

| 項目 | 在哪個 env |
|---|---|
| 任何純邏輯 bug | native_unit |
| 任何 stateful protocol bug | native_unit / native_ftp |
| 任何 UI render regression | native_test (GUI scenario) |
| 任何 firmware compile / link issue | firmware-build (ESP32 真編譯) |
| 任何 Python utility module bug | aio-tool pytest |

**rule of thumb**：「在 host 上能驗的就在 host 驗，去真機只驗 host 驗不到的」。每個 PR 跑 ~3 分鐘 host CI 比燒板子 ~10 分鐘 + 觀察行為快得多。

---

## 7. Coverage map — 已測 vs 沒測

把整個 codebase 的測試覆蓋率逐 layer 列出（粗估）：

```
                    ┌──────────────────────────────────────────┐
                    │                                          │
   L7 GUI render    │  ▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░  ~70%                 │
                    │  GUI scenario 對 18 個 app 都有 smoke      │
                    │  缺 deep interaction scenario             │
                    │                                          │
   L6 App logic     │  ▓▓▓▓▓▓▓▓▓▓▓░░░░░░  ~60%                 │
                    │  GUI scenario covers happy path          │
                    │  缺各 app 的 negative path / edge case    │
                    │                                          │
   L5 Web/API/FTP   │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░  ~80%                 │
                    │  FTP 100% (PR-3.0a)                      │
                    │  Web Setting form HTML diff 沒測          │
                    │                                          │
   L4 System        │  ▓▓▓▓▓▓░░░░░░░░░░░  ~30%                 │
                    │  test_app_controller 只 cover send_to    │
                    │  Lifecycle / event queue ordering 沒測   │
                    │                                          │
   L3 Util          │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  ~95%                 │
                    │  http_util / json_util / parsers 全 cover│
                    │                                          │
   L2 Driver        │  ▓░░░░░░░░░░░░░░░░  ~5%                  │
                    │  只 analyse_param 有 unit test            │
                    │  其他 driver 真機驗                        │
                    │                                          │
   L1 Silicon       │  -                  N/A                  │
                    │  靠 firmware-build job 確保 compile       │
                    │                                          │
                    └──────────────────────────────────────────┘
                    AIO_Tool: ▓▓▓▓░░░░ ~25% (util 模組 only)
```

最弱 cover 的地方：
- **L4 system layer**：lifecycle bug 沒測。範例：app_init 失敗時 app_exit 沒對稱清理 → 之後重進該 app crash。**這類 bug 在 GUI scenario 跑單一 app 不會看到（沒 enter-exit-enter cycle）**
- **L6 app negative path**：每個 app 大多只有 happy-path scenario。網路炸了、JSON 缺 field、SD 滿了... 這些常常沒 scenario
- **AIO_Tool GUI**：pytest 完全沒碰 GUI 互動，button silent fail（PR #74）就是這樣 escape 的

最強 cover：
- **L3 util**：純邏輯，TDD 寫起來快又準。新加的 util 都該寫測試
- **FTP（L5 一支）**：PR-3.0a 為了 PR-3.3 拆檔特別投資 harness → 完整 cover

---

## 8. 長時間 leak detection — coverage gap deep dive

§7 列了三大弱項，但其中**最隱蔽的一類**值得獨立展開：**長時間連續操作才會炸的 memory leak**。

### 8.1 真實案例：[stockmarket leak (commit 7e7b742)](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/commit/7e7b742)

兩個獨立的 leak 疊在一起：

**Leak A：每次 refresh 重建 LVGL widget tree**
```c
// 舊的
void display_stockmarket_init(void) {
    lv_obj_t *act_obj = lv_scr_act();
    if (act_obj == stockmarket_gui) return;   // ← 這個 guard 不可靠
    stockmarket_gui_del();
    lv_obj_clean(act_obj);
    stockmarket_gui = lv_obj_create(NULL);    // 每秒 refresh → 每秒重建
    // ...
}
```

**Leak B：`lv_obj_add_style` 累加**
```c
lv_obj_add_style(nowQuoLabel, &numberBigRed_style, LV_STATE_DEFAULT);
```
LVGL 的 `add_style` 是 **append**，不是 replace。同一個 label 每 refresh 一次，style list 多一筆。

修法：
```c
// Leak A: 早退 — 只 init 一次
if (stockmarket_gui != NULL) return;

// Leak B: 用 set_style_*（覆蓋語意），不用 add_style
lv_obj_set_style_text_color(nowQuoLabel, lv_color_hex(0xff0000), LV_PART_MAIN);
lv_obj_set_style_text_font(nowQuoLabel, &lv_font_montserrat_48, LV_PART_MAIN);
```

**這 bug 為什麼很容易 escape**：
- 不是 logic bug — code 讀起來很 reasonable
- Code review 看不出來 — 要熟 LVGL 內部才知道 `add_style` 語意
- compile pass、scenario screenshot 對 golden、unit test 過 → CI **全綠**
- 真機開機後**頭幾分鐘也運作正常**

只有跑了**夠久之後**才會 OOM crash → 「不知道為什麼跑一陣子就重開機」。**Production-only bug class**。

### 8.2 為什麼 4 個 env 都抓不到

| Env | 為什麼漏 |
|---|---|
| `native_unit` | 純邏輯，根本沒 link LVGL，看不到 widget tree 怎麼成長 |
| `native_ftp` | 同上，只 link FtpServer + fake SD |
| `native_test` (GUI scenario) | **link 了 LVGL**，本來有機會抓 — 但 scenario 預設只跑單次（init → 幾個 action → screenshot → exit）。沒有「重複 1000 次看 heap 漲不漲」的 pattern。Screenshot diff 只看畫面像不像，不看記憶體 |
| `firmware-build` | 只編譯不執行 |
| 真機 | 理論上會 OOM crash，但要等 30 分鐘到 2 小時 — 正常 PR review 跟 release 流程沒人在燒板子放 1 小時 |

### 8.3 還有哪些 app 可能有同類 leak

任何「**主畫面會週期性 refresh**」的 app 都是嫌疑犯：

- `weather` — 每 weatherUpdataInterval ms 重畫
- `pc_resource` — 每秒 SSE stream 進來都重畫
- `bilibili_fans` — 每 updateInterval ms 重畫
- `anniversary` — 每秒倒數重畫
- `picture` — 自動切換 image
- `media_player` — 影格更新
- `idea_anim` — 持續動畫

我們**沒測試任何一個**對 long-run heap 走勢有沒有問題。

### 8.4 補 cover 的 4 個方向（從便宜到貴）

#### 選項 A：scenario harness 加 `loop N` + 記憶體 baseline assertion

**新 directive**：
```
mem_baseline                       # 記下當前 heap free
loop 100
  http_fixture sina_data ...
  wait_ms 100                      # 觸發 display_stockmarket
end_loop
mem_assert_delta_lt 4096           # 跑完後 heap 不該漲超過 4KB
```

**需要改的東西**：
1. `scenario_runner.cpp` 加 3 個新 directive（`mem_baseline` / `loop N`/`end_loop` / `mem_assert_delta_lt N`）
2. host 端 `esp_get_free_heap_size` mock 接到 instrumented allocator counter（包 `malloc` / `free`）
3. 對 `lv_mem_alloc` / `lv_mem_free` 也要包

**ROI**：高。可重用 — 任何「重複 N 次預期 stable」的 leak 都能抓。

**風險**：
- LVGL 內部本身有 cache / pool，跑 100 次 + 4KB tolerance 可能 false positive。要先跑乾淨韌體 calibrate baseline
- Host 上的 malloc 跟 ESP32 的 malloc 行為不完全一樣（fragmentation pattern 差），數字非 1:1 對應 — 但**趨勢**對得上

#### 選項 B：AddressSanitizer (ASAN) build

PlatformIO 加 `build_flags = -fsanitize=address` 跑 host build。ASAN 會抓 leak、use-after-free、buffer overflow。

**需要改的東西**：
1. 新 `[env:native_test_asan]` env
2. CI 多一個 job 跑 ASAN build

**ROI**：中。對 stock leak A（widget tree 沒釋放，純粹的 alloc-without-free）有效；對 leak B（`add_style` 內部 list 累加）**無效** — ASAN 看到 LVGL 該配置的 style entry 都被 module 內的 list **持有**，所有指標都還在 → 不算 leak，只是「持續成長的合法資料結構」。

**額外問題**：
- LVGL / SDL2 內部本來就有一些 leak-on-purpose（global cache 永遠不釋放）→ 大量 false positive，要寫 suppressions file
- 跑速度約慢 2-3 倍

#### 選項 C：LVGL `lv_mem_monitor()` 整合進 scenario assert

LVGL 自己提供 [`lv_mem_monitor(lv_mem_monitor_t *mon)`](https://docs.lvgl.io/8.3/overview/memory.html)，回傳 total / free / used / max_used / frag_pct。

**新 directive**：
```
mem_lv_assert_used_lt 50000     # LVGL 用掉的記憶體不超過 50KB
mem_lv_assert_max_used_lt 60000 # 過程中峰值不超過 60KB
```

**需要改的東西**：
1. `scenario_runner.cpp` 加 directive
2. 直接呼叫 LVGL API，不用自己 instrument

**ROI**：很高。**這就是專為 stock leak B 設計的工具** — `lv_obj_add_style` 累加會直接讓 LVGL `used` 持續上升。

**搭配選項 A 的 loop directive 一起用 = 完整解**。leak A 用「整體 heap delta」抓、leak B 用「LVGL used delta」抓。

#### 選項 D：真機 soak test（最遠期，不在 CI）

設一台 cube 接 USB 一直跑，定期 ping `/api/stats` 紀錄 heap 走勢。發現**長期下降**就 alert。

**需要的東西**：硬體 + monitoring server / cron job

**ROI**：最真實，但**貴 + 慢**。發現問題到通知到開 ticket → 24 hr 起跳。Release CI 不能用，只能定期 release 後跑（例如每月一次）。

### 8.5 推薦實作順序

| 階段 | 選項 | 規模 | ROI |
|---|---|---|---|
| 1 | C：`lv_mem_monitor` 整合 | ~150 LOC harness 改動 + 5-6 個 mem-leak scenarios | 立即 cover stock-class leak |
| 2 | A：通用 loop + heap-delta directive | ~300 LOC harness + 對所有可疑 app 加 mem scenario | cover 任何「重複 → 預期 stable」的 leak |
| 3 (跳過) | B：ASAN | — | 對最常見的 leak class 無效 + suppressions 維護成本高 |
| 4 (長期) | D：真機 soak | infra | release 後 monthly 跑，補 host 抓不到的物理現象 |

### 8.6 「如果現在重做這個 bug 會被抓到嗎？」

| 環境 | 結果 |
|---|---|
| 選項 C 已實作 | ✅ `mem_lv_assert_used_lt` 在 100 次 refresh 後 fail |
| 選項 A 已實作 | ✅ heap delta > 4KB tolerance |
| **目前狀態** | ❌ escape 到 production，跟當年一樣 |

### 8.7 為什麼這個 gap 還沒補

純粹是優先順序問題 — leak 出現得不頻繁（過去一年只一次明顯的 stock leak），相比 i18n / Glass UI 之類使用者看得到的東西優先順序低。但**作為 architectural gap**它應該被明寫，下一個發生 leak 時不要假裝沒料到。

如果你（或未來的 maintainer）讀到這裡並且：
1. 剛踩到一個 long-run leak
2. 或者要 refactor 跟 LVGL widget lifecycle 有關的 code

→ **回來實作選項 C**。150 LOC 投資換掉一整類「production-only bug」。

---

## 9. 兩個典型 fail 的 trace — 從 bug 到 root cause

### 9.1 「按鈕沒反應」class（CTkButton dict-access）

User report：「我按開啟序列埠沒反應，按關閉也沒反應」

```
Layer 7 (GUI)        ✓ Tk render 出來，button 在
Layer 6 (App logic)  ✗ ← bug 在這
Layer 5 (Web/API)    -  N/A (不在 web)
Layer 4 (System)     -  N/A (不在 system)
```

但**沒測試 cover Layer 6 的 CTkButton 互動**。所以：

```
1. User 回報 → 我加 surface error handler (PR #74 第一階段)
2. Surface 後看到 TclError: unknown option "-text"
3. Trace 到 CTkButton.__getitem__ 不走 cget override
4. 修法：用 cget() / configure()
5. 教訓：「沒被 cover 的 layer」永遠是下個 bug 的住處
```

**怎麼預防**：要寫 GUI 測試很麻煩（CTk 跑不了 headless？要再研究），目前還在「使用者回報」模式。**known coverage gap**。

### 9.2 「release tag fail」class（PR-3.3 missing `<WiFi.h>`）

CI report：v2.6.1 release workflow 的 build_firmware step fail with `'WiFiServer' does not name a type`

```
Layer 1 (compile)    ✗ ← 這層 fail
Layer 2 (driver)     ↑   header include 順序問題
Layer 3-7            -   後續沒跑到
```

**為什麼 PR CI 沒抓到**：當時 regression workflow 沒有 firmware-build job，只測 host 端 build。**Coverage map 缺一塊**。

```
1. Release fail → 我修 missing include + 補 firmware-build job (PR #72)
2. 從此每個 PR 真的編譯 ESP32 firmware
3. 教訓：「coverage gap 永遠在最不方便的時候炸」
```

兩個 case 共通：**bug 都在「沒被測試 cover 的 layer」**。每次新 fail 都該問「這層該不該補 cover」。

---

## 10. 測試系統的演化路徑（怎麼長成現在這樣）

時間軸：

```
v1.x 早期  -- 只有真機測試
v2.5 之前 -- ClimbSnail 加 PlatformIO env，零 host 測試
            CI 只有 firmware compile，無功能測試

v2.5      -- 引入 native_unit env (Phase 1 第一個 unit test)
            heartbeat config parse 是第一個 module
            目的：給 strcpy → snprintf 重構安全網

v2.6.0    -- 引入 GUI scenario harness (test/scenarios/)
            18 個 app 都有 smoke scenario
            目的：refactor weather/server/etc 的安全網

v2.6.1    -- 引入 native_ftp env (PR-3.0a)
            為了 PR-3.3 拆 FtpServer 特別建
            ←── 這版 release fail，發現 firmware-build job 缺 ──

v2.6.1+   -- 補 firmware-build job (PR #72)
            Coverage 多一層，不再有「PR 全綠但 release fail」

v2.6.x    -- AIO_Tool pytest（漸進加）
            i18n / robotsocket / massagehead 都有 cover
            GUI 互動還沒 cover ← known gap

現在     -- 4 個 host env + 1 個 ESP32 build job
           ~3 分鐘跑完整套
           Coverage estimated 60-70%
```

每次擴展都是「踩到痛 → 加新 env / job」的反應式設計。沒有預先規劃，但實際上 cover 到位的 layer 都是過去出過問題的地方。

---

## 11. 寫新 layer 測試的 checklist

如果你判斷某個 layer / module 沒被 cover、想補：

```
1. [ ] 目標 layer 的 module 有依賴 hardware / vendored library 嗎？
       是 → 抽出 pure function（章節 06 §7.2）；否則無法 host 測
       否 → 直接寫測試

2. [ ] 已有的 stub layer 夠用嗎？
       夠 → 用既有 env（native_unit / native_ftp / native_test）
       不夠 → 開新 stubs_xxx + 新 [env:native_xxx] + 新 CI step
              （參考 stubs_ftp 為範本，章節 06 §8）

3. [ ] 寫 happy-path test
4. [ ] 寫 negative-path test（empty input / oversize input / malformed）
5. [ ] 跑本機 — 全綠
6. [ ] 加 PR — CI 跑 — 全綠
7. [ ] 故意 break 你被測的 code 看 test 會不會 fail
       會 → 真的 cover 到了
       不會 → 你的 test 沒 cover 到關鍵路徑，重寫
8. [ ] 把改動還原，PR 進去
```

第 7 步**很多人跳過**，但這是唯一驗證「我的測試真的 cover 到」的方法。沒做這步，你的測試可能根本沒在驗你想驗的東西。

---

## 12. 最常被誤解的事

### 12.1 「Coverage 100% = bug-free」
錯。Coverage 高代表「這條路徑跑過至少一次」，不代表「這條路徑的所有 input 組合都驗過」。100% line coverage 的 code 一樣可能 fail edge case。

### 12.2 「GUI scenario 過了 = UI 沒問題」
錯。GUI scenario 只驗你**寫到的那幾個 screenshot 時刻**像 golden。沒 screenshot 的 frame、沒 trigger 的互動、沒測的螢幕大小... 都沒 cover。

### 12.3 「Unit test 越多越好」
錯。Unit test 對「容易隔離的純邏輯」回報率最高，對「強耦合的 IO」回報率低（要建大量 mock，mock 維護成本高、又不會抓到 mock 跟 production 行為不一致的 bug — chapter 08 §10 那種）。**該寫的地方寫，不該寫的地方寫了反而拖累**。

### 12.4 「Mock 越真實越好」
錯。Mock 越真實 → 越靠近 production behavior → 越像在跑 production code → 越失去 mock 的快速 + 隔離優勢。Mock 該**只 cover 你被測模組需要的最小 surface**。

### 12.5 「測試應該驗實作」
錯。測試應該驗**對外行為 / 公開 API**，不該驗實作細節。實作細節改了測試也得改 = 你在重寫測試而不是真的測東西。**只測 public surface，refactor 才不會帶大量 test 改動**。

---

## 結語

從上到下看，整個測試系統其實是**幾個獨立 env 的疊加**，每個 env 對應的 layer / 用的 stub / 抓的 bug 都不同。理解這個結構之後，下次：

- 看到 bug → 想「這 bug 屬於哪 layer？該由哪個 env 抓？」
- 寫新 feature → 想「我要加哪 layer 的測試？該開新 env 還是擴 existing？」
- 看到 coverage gap → 評估「補這層的 ROI 多少？stub 多難寫？」

測試不是 binary（有 / 沒有），是**架構決策**。

## 下一步

- [06 — 測試完整指南](./06-testing.md) — 怎麼**寫**每一種測試（操作層）
- [08 — 重構與優化案例集](./08-refactoring-case-studies.md) — 真實 bug 跟修法（橫切視角）

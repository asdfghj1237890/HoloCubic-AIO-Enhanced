# 02 — 韌體架構

韌體的核心抽象只有兩個：
1. **`APP_OBJ`** — 一個應用要實作的 7 個 callback
2. **`AppController`** — 主迴圈，負責切換 app + 派送訊息

懂這兩個就懂整個架構了。

## 1. main loop 在做什麼

`AIO_Firmware_PIO/src/HoloCubic_AIO.cpp` 的 `loop()` 約莫是這樣：

```cpp
void loop()
{
    // ... 一些 ambient light / RGB 抽風的雜事 ...

    if (isCheckAction)              // ~每 50ms 由 hardware timer 戳一次
    {
        isCheckAction = false;
        act_info = mpu.getAction(); // 讀 IMU，產生 ImuAction
    }

    // (PR #77 後) 從 serial 收 ~U / ~L / ~R / ~F / ~H 命令
    // 覆蓋 act_info，當作虛擬 IMU 動作

    app_controller->main_process(act_info);  // ← 把動作丟給控制器
}
```

`act_info->active` 是 `ACTIVE_TYPE` enum，可能是 `TURN_LEFT`、`TURN_RIGHT`、`UP`、`DOWN`、`GO_FORWORD`、`RETURN`、`SHAKE` 或 `UNKNOWN`。

## 2. AppController 在 main loop 裡做什麼

[`AIO_Firmware_PIO/src/sys/app_controller.cpp`](../../AIO_Firmware_PIO/src/sys/app_controller.cpp) 的 `main_process()`：

```
+-- main_process(act_info) ----------------------------+
|                                                      |
|   1. 處理 event queue（其他 app 排隊送來的訊息）       |
|        → req_event_deal()                            |
|                                                      |
|   2. 如果現在沒 app 在跑（app_exit_flag == 0）：       |
|       a. TURN_LEFT/RIGHT  → 切換 app icon            |
|       b. GO_FORWORD       → 啟動目前選中的 app         |
|       c. 顯示 launcher menu UI                       |
|                                                      |
|   3. 如果有 app 在跑：                                 |
|       直接呼叫 active app 的 main_process(this, act) |
|                                                      |
+------------------------------------------------------+
```

關鍵那一行在 [`app_controller.cpp:203`](../../AIO_Firmware_PIO/src/sys/app_controller.cpp#L203)：
```cpp
(*(appList[cur_app_index]->main_process))(this, act_info);
```

這就是把控制權交給 active app 的瞬間。`this` 是 `AppController*`（給 app 用來呼叫 `send_to`、`app_exit` 等等），`act_info` 是 IMU 事件。

## 3. APP_OBJ 結構

每個 app 都是一個 `APP_OBJ` 實例。定義在 [`sys/interface.h:31-60`](../../AIO_Firmware_PIO/src/sys/interface.h#L31)：

```cpp
struct APP_OBJ
{
    const char *app_name;           // "Weather"、"Picture" 等
    const void *app_image;          // 128×128 icon (lv_img_dsc_t*)
    const char *app_info;           // 「Author HQ\nVersion 2.0.0\n」之類
    int  (*app_init)        (AppController *sys);
    void (*main_process)    (AppController *sys, const ImuAction *act_info);
    void (*background_task) (AppController *sys, const ImuAction *act_info);
    int  (*exit_callback)   (void *param);
    void (*message_handle)  (const char *from, const char *to,
                             APP_MESSAGE_TYPE type, void *message, void *ext_info);
};
```

七個 callback 的時機：

| Callback | 何時被呼叫 | 你要做什麼 |
|---|---|---|
| `app_init` | 使用者按下啟動 → `AppController` 載入 app 時呼叫一次 | 配置 LVGL UI、`malloc`/`calloc` runtime data、讀設定 |
| `main_process` | 每個 main-loop tick (~50ms 一次) | 處理 IMU 動作、刷新 UI、決定何時呼叫 `sys->send_to(...)` 拉資料 |
| `background_task` | App **未運行**時，~每分鐘呼叫一次 | 用 `forever_data`（不會被釋放的常駐資料）做背景刷新；**不可以**碰 `run_data` |
| `exit_callback` | 使用者按 RETURN → `app_exit()` 觸發 | 釋放 `run_data`、刪 LVGL 物件、停背景 task |
| `message_handle` | 別人對你 `send_to(this_app, ...)` 時 | 處理 IPC 事件（WiFi 來了、設定要存了等等） |

## 4. APP_MESSAGE_TYPE — 跨 app 訊息

[`sys/interface.h:4-18`](../../AIO_Firmware_PIO/src/sys/interface.h#L4)：

```cpp
enum APP_MESSAGE_TYPE
{
    APP_MESSAGE_WIFI_CONN,    // 我要連 WiFi
    APP_MESSAGE_WIFI_AP,      // 我要開 AP
    APP_MESSAGE_WIFI_ALIVE,   // WiFi keep-alive 心跳
    APP_MESSAGE_WIFI_DISCONN, // 斷 WiFi
    APP_MESSAGE_UPDATE_TIME,  // 同步時間
    APP_MESSAGE_MQTT_DATA,    // MQTT payload 收到
    APP_MESSAGE_GET_PARAM,    // 抓我儲存的設定值
    APP_MESSAGE_SET_PARAM,    // 寫入新設定
    APP_MESSAGE_READ_CFG,     // 從 flash 讀完整 config
    APP_MESSAGE_WRITE_CFG,    // 寫整份 config 到 flash
    APP_MESSAGE_NONE
};
```

你的 app 在 `message_handle` 裡用 `switch (type)` 分支處理。`from` / `to` 是字串 app name，`message` / `ext_info` 是泛型 `void*`，意義依 type 而定（例如 `GET_PARAM` 時 `message` 是 key name、`ext_info` 是給你寫值進去的 buffer）。

## 5. send_to — 怎麼跨 app 通訊

從 app A 戳 app B：
```cpp
sys->send_to("WeatherApp", "ServerApp", APP_MESSAGE_WIFI_CONN, NULL, NULL);
```

實作 ([`app_controller.cpp:237-268`](../../AIO_Firmware_PIO/src/sys/app_controller.cpp#L237)) 把這個訊息**排進 event queue**，timer 每 ~300ms 戳一下 `req_event_deal()` 把 queue dispatch 到目標 app 的 `message_handle`。

> **不是**直接 function call — 是異步的。所以你 `send_to` 完不要馬上預期看到結果。

特例：呼叫自己的 `message_handle` 拿/設參數通常是同步的（透過 `send_to_dispatch` 的 fast path），所以 web setting 表單可以呼叫 `send_to(MY_APP, MY_APP, GET_PARAM, ...)` 同步抓值。

## 6. 完整生命週期實例

User 開機 → 在 launcher 選 Weather → 進去 → 退出：

```
[setup()]
  ├─ app_controller->app_install(&weather_app)   ← weather 已註冊但未跑
  └─ app_controller->main_loop()  ← 進主迴圈

[loop() x N]
  └─ launcher 顯示 weather icon (cur_app_index 指向 weather)
     使用者 GO_FORWORD →
       app_controller 呼叫 weather_app.app_init(this)
         ├─ malloc run_data
         ├─ weather_gui_init()  (建 LVGL 物件)
         ├─ read_config(&cfg_data)
         └─ return 0
       app_exit_flag = 1   ← 表示有 app 正在跑

[loop() x N]
  └─ active app = weather → 呼叫 weather_app.main_process(this, act_info)
     ├─ 處理 IMU: TURN_LEFT/RIGHT 切頁
     ├─ 每隔 weatherUpdataInterval 戳 send_to(WEATHER, CTRL, WIFI_CONN, ...)
     ├─ 呼叫 display_weather() 刷 UI
     └─ 如果 act_info->active == RETURN：呼叫 sys->app_exit(); return;

[使用者 RETURN]
  └─ sys->app_exit()
       └─ weather_app.exit_callback(NULL)
            ├─ weather_gui_del()
            ├─ free(run_data)
            └─ run_data = NULL
       app_exit_flag = 0   ← launcher 又顯示出來

[loop() x N（背景任務間歇觸發）]
  └─ weather_app.background_task(this, act_info)
       └─ 用 forever_data 偷偷刷新天氣（不開 LVGL UI）
```

## 7. AppType — 一般 app vs 背景 app

`app_install()` 接受第二個 optional 參數 `APP_TYPE`：
- `APP_TYPE_REAL`（預設）— 一般 app，會出現在 launcher icon list
- `APP_TYPE_BACKGROUND` — 背景 app，**不會**出現在 launcher，只跑 `background_task`

`heartbeat` app 就用 `APP_TYPE_BACKGROUND`，因為它沒 UI、只在背景監聽 MQTT。

## 8. 真實案例：去看 example app

最簡單的完整 app 在 [`AIO_Firmware_PIO/src/app/example/`](../../AIO_Firmware_PIO/src/app/example/)：

```
example/
├── example.h           # extern APP_OBJ example_app;
├── example.cpp         # 主邏輯 + 5 個 callback
├── example_gui.h       # LVGL UI 函式宣告
├── example_gui.c       # LVGL UI 實作
└── example_ico.c       # 128x128 icon (lv_img_dsc_t)
```

打開 `example.cpp` 對照前面的 callback 表 — 每一個都齊全，5 個函式總共約 130 行。**這就是你下一章複製出來的範本**。

## 9. 把它當 event-driven FSM 看

讀完前面 8 節你會發現一個 pattern — 整個 framework 其實是**三層巢狀的 finite state machine**，每層各自有 state、transition、event。把它畫出來會比一直唸 callback 名字好理解：

### 9.1 三層 FSM

```
[第一層：硬體 timer]
    每 ~50ms 戳一次 → isCheckAction = true
              ↓
[第二層：AppController outer FSM]
    States: LAUNCHER_IDLE (app_exit_flag==0) | APP_RUNNING (app_exit_flag==1)
    Transitions:
      LAUNCHER_IDLE  --GO_FORWORD-->   呼叫 active app 的 app_init() → APP_RUNNING
      APP_RUNNING    --(app 主動 sys->app_exit())--> 呼叫 exit_callback() → LAUNCHER_IDLE
      LAUNCHER_IDLE  --TURN_LEFT/RIGHT--> 切 cur_app_index（換 launcher icon）
              ↓
[第三層：每個 app 的 internal FSM]
    States 存在 run_data 結構裡（page index、bitmask 旗標、上次 update 時間）
    Transitions 由三類事件觸發：
      a. IMU action (act_info->active 是 RETURN/TURN_LEFT/...)
      b. 時間經過 (millis() - last > interval)
      c. 異步訊息回來 (message_handle 收到 send_to 的 event)
```

### 9.2 weather 是教科書範例

[`weather.cpp`](../../AIO_Firmware_PIO/src/app/weather/weather.cpp) 的 state 全在 `WeatherAppRunData` 裡：

```cpp
struct WeatherAppRunData {
    int  clock_page;                 // ← 0 = 主頁、1 = 7日預報，這是 state
    unsigned int coactusUpdateFlag;  // ← 1 = 強制更新中
    unsigned int update_type;        // ← bitmask: WEATHER | TIME | DAILY
    unsigned long preWeatherMillis;  // ← 上次刷新時間（time-based transition 用）
    unsigned long preTimeMillis;
    Weather wea;                     // ← payload data，不是 state
};
```

State diagram：

```
        ┌─── TURN_LEFT/RIGHT ────────┐
        ↓                              ↑
[clock_page=0 主頁]  ←──────────→  [clock_page=1 預報]
        │                              │
        ├── 每 weatherUpdataInterval ms 觸發 send_to(CTRL, WIFI_CONN, UPDATE_NOW)
        ├── 每 timeUpdataInterval ms   觸發 send_to(CTRL, WIFI_CONN, UPDATE_NTP)
        ├── GO_FORWORD → coactusUpdateFlag = 1 (強制刷新)
        └── RETURN → sys->app_exit() → outer FSM 跳回 LAUNCHER_IDLE
```

注意 transition 不只來自 IMU — **時間經過**跟**異步訊息回來**也是 transition trigger。這是純 event-driven model，沒有 polling loop 在裡面打轉。

### 9.3 兩個 entry point 進你的 app

```
   main loop tick (每 ~50ms)
        │
        ├──→ app_main_process(act_info)     ← IMU + time-based transitions
        │
        └──→ (異步, 由 req_event_deal 排隊處理)
             app_message_handle(...)         ← send_to 來的事件
                                                例：WIFI_CONN 連上了、SET_PARAM 要存設定
```

兩條 path 都會 mutate 同一份 `run_data`。**兩個都跑在 main thread**（`message_handle` 由 timer-driven 的 `req_event_deal()` dispatch），所以**不需要 mutex**。

但是！必須是 **cooperative**：

> ⚠️ 任何一個 callback 裡 `delay()` 就卡死整個 device。LVGL 不會 render、IMU 讀不到、其他 app 的 background_task 也不會跑。這是 PR-1.8 整個 audit 過 40+ 個 `delay()` 的原因。**用 `if (millis() - last < N) return;` 早退**，不要 `delay()`。

### 9.4 顯式 vs 隱式 state

兩種寫法都常見：

**隱式**（weather 風格）— state 是 page index + boolean 旗標：
```cpp
if (run_data->clock_page == 0) {
    display_weather(...);   // 主頁的事
} else if (run_data->clock_page == 1) {
    display_curve(...);     // 預報頁的事
}
```

**顯式**（FtpServer 風格，state 寫成 0/1/2/3 數字）— [`ESP32FtpServer.cpp` `handleFTP()`](../../AIO_Firmware_PIO/src/app/file_manager/ESP32FtpServer.cpp)：

```cpp
0 → disconnect any leftover client; goto 1
1 → reset state; iniVariables(); goto 2
2 → wait for client connect; if connected: clientConnected() (送 220 banner); goto 3
3 → wait for USER command; if userIdentity() OK: goto 4 else goto 0
4 → wait for PASS command; if userPassword() OK: goto 5 else goto 0
5 → main command loop: processCommand() dispatches RETR/STOR/LIST/...
```

寫測試（PR-3.0a #68）的時候踩過一個雷：我以為「push 一個 client 進去然後 `handleFTP()` 就會跑 welcome」，結果第一個 tick 在 cmdStatus 0 把我的 client `disconnectClient()` 掉了 — 因為 0 的 job 就是清掉殘留 client。要先 pump 兩次推進到 cmdStatus 2，才能 push client。**這就是顯式 state 的好處：你看著 0/1/2 數字直接 reason 順序**；隱式 boolean flag 那種就比較容易不小心搞錯前置條件。

新 app 寫小一點（< 3 個 state）用隱式 OK；如果開始覺得 if/else 一堆 boolean 讀不懂，**該換成顯式 state enum**。

### 9.5 跟 LVGL event 的關係

LVGL 自己也是 event-driven framework — 你 `lv_obj_add_event_cb()` 註冊的 callback 也跑在 main thread（透過 `lv_task_handler()`）。意思是如果你 LVGL event handler 裡碰 `run_data`，跟 `main_process` / `message_handle` 是 race-free 的，不需要鎖。**前提還是不能 `delay()`**。

## 下一步

[03 — 寫你的第一個 App](./03-firmware-write-your-first-app.md) — 從 example 複製出來，加進主 loop，跑起來。

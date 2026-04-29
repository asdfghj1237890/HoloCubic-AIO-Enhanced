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

## 下一步

[03 — 寫你的第一個 App](./03-firmware-write-your-first-app.md) — 從 example 複製出來，加進主 loop，跑起來。

# 03 — 寫你的第一個 App

我們來實際做一個。目標：一個叫 `Hello` 的 app，畫面顯示 "Hello, HoloCubic!"，按 `RETURN` 退出回 launcher。會用到 章節 02 講的全部抽象。

## 0. 準備

```bash
cd AIO_Firmware_PIO/src/app
cp -r example hello
```

進到 `hello/` 把每個檔的 `example` 字樣換成 `hello`：

```bash
cd hello
mv example.cpp hello.cpp
mv example.h   hello.h
mv example_gui.c hello_gui.c
mv example_gui.h hello_gui.h
mv example_ico.c hello_ico.c
```

然後 sed 一次：
```bash
sed -i 's/example/hello/g; s/EXAMPLE/HELLO/g; s/Example/Hello/g' *.h *.c *.cpp
```

> Windows + Git Bash 也行；MSYS2 / WSL 都 OK。沒有 sed 就用 IDE 的 Find&Replace。

## 1. 編輯 `hello.h`

```cpp
#ifndef APP_HELLO_H
#define APP_HELLO_H

#include "sys/interface.h"

extern APP_OBJ hello_app;   // ← AppController 會用這個 extern

#endif
```

## 2. 編輯 `hello.cpp`

最小可運作版本：

```cpp
#include "hello.h"
#include "hello_gui.h"
#include "sys/app_controller.h"
#include "common.h"

#define HELLO_APP_NAME "Hello"

struct HelloRunData
{
    int dummy;  // 留白等之後擴充
};

static HelloRunData *run_data = NULL;

// ---- callbacks ----

static int hello_init(AppController *sys)
{
    tft->setSwapBytes(true);
    hello_gui_init();    // 建 LVGL UI

    run_data = (HelloRunData *)calloc(1, sizeof(HelloRunData));
    return 0;
}

static void hello_process(AppController *sys, const ImuAction *act_info)
{
    if (RETURN == act_info->active)
    {
        sys->app_exit();   // 觸發 exit_callback、返回 launcher
        return;
    }
    // 沒事就 idle — 畫面已經由 hello_gui_init() 畫好了
}

static void hello_background_task(AppController *sys, const ImuAction *act_info)
{
    // 沒背景任務 — 留空
}

static int hello_exit_callback(void *param)
{
    hello_gui_del();   // 釋放 LVGL 物件

    if (run_data != NULL)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void hello_message_handle(const char *from, const char *to,
                                 APP_MESSAGE_TYPE type, void *message,
                                 void *ext_info)
{
    // 沒人會 send_to 我們，留空
}

// ---- 註冊 ----
APP_OBJ hello_app = {
    HELLO_APP_NAME, &app_hello, "Author You\nVersion 1.0.0\n",
    hello_init, hello_process, hello_background_task,
    hello_exit_callback, hello_message_handle
};
```

`app_hello` 是 icon — 在 `hello_ico.c` 裡（從 `example_ico.c` 複製，型別是 `lv_img_dsc_t`）。

## 3. 編輯 `hello_gui.h` / `hello_gui.c`

最簡 LVGL UI：螢幕上一行字。

`hello_gui.h`：
```c
#ifndef APP_HELLO_GUI_H
#define APP_HELLO_GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
extern const lv_img_dsc_t app_hello;  // 來自 hello_ico.c
void hello_gui_init(void);
void hello_gui_del(void);

#ifdef __cplusplus
}
#endif

#endif
```

`hello_gui.c`：
```c
#include "hello_gui.h"

static lv_obj_t *scr = NULL;

void hello_gui_init(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello, HoloCubic!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scr);
}

void hello_gui_del(void)
{
    if (scr) {
        lv_obj_del(scr);
        scr = NULL;
    }
}
```

## 4. 註冊到 AppController

打開 [`AIO_Firmware_PIO/src/HoloCubic_AIO.cpp`](../../AIO_Firmware_PIO/src/HoloCubic_AIO.cpp)，找 `setup()` 裡有一串 `app_controller->app_install(&xxx_app);`，加你的：

```cpp
app_controller->app_install(&server_app);
app_controller->app_install(&hello_app);   // ← 加這行
#if APP_WEATHER_USE
    app_controller->app_install(&weather_app);
#endif
// ...
```

順序決定 launcher icon 排列順序。

頂部加 `#include`：
```cpp
#include "app/hello/hello.h"
```

## 5. 加 feature flag（optional but recommended）

[`AIO_Firmware_PIO/src/app/app_conf.h`](../../AIO_Firmware_PIO/src/app/app_conf.h) 集中管理 `APP_*_USE` 開關。加一個：
```cpp
#define APP_HELLO_USE 1
```

然後 install 改成條件式：
```cpp
#if APP_HELLO_USE
    app_controller->app_install(&hello_app);
#endif
```

這樣別人可以一行關掉你的 app。

## 6. Build + 燒進去

```bash
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases -t upload --upload-port COM5
```

Build 失敗最常見的：
- `app_hello` undefined → `hello_ico.c` 裡型別/名字寫錯
- `hello_gui_init` undefined → `extern "C"` 的 wrapper 漏了，或 .c/.cpp 副檔名給錯
- LVGL 函式 unrecognised → `#include "lvgl.h"` 在 .c 檔頂端漏了

開機後在 launcher 找 Hello icon，前傾選擇進入，看到 "Hello, HoloCubic!" → 按後傾退出。完成。

## 7. 加進階功能 — 用 IMU 互動

把 `hello_process` 改成讓字會動：

```cpp
static int x_offset = 0;

static void hello_process(AppController *sys, const ImuAction *act_info)
{
    if (RETURN == act_info->active) {
        sys->app_exit();
        return;
    }
    if (TURN_LEFT == act_info->active)  x_offset -= 10;
    if (TURN_RIGHT == act_info->active) x_offset += 10;

    extern lv_obj_t *hello_label;  // export 到 gui.c
    lv_obj_set_x(hello_label, x_offset);
}
```

(配合改 `hello_gui.c` 的 `label` 換成全局 `hello_label`)

## 8. 加設定持久化（讀寫 flash config）

需要存設定的話，用 `g_flashCfg.writeFile` / `readFile` API。標準 pattern 在每個 `*_setting()` handler 都看得到，但精簡版：

```cpp
#define HELLO_CFG_PATH "/hello.cfg"

struct HelloConfig {
    char username[32];
    int  display_speed;
};

static HelloConfig cfg_data;

static void hello_write_config(HelloConfig *cfg)
{
    String s;
    s = s + cfg->username + "\n";
    s = s + String(cfg->display_speed) + "\n";
    g_flashCfg.writeFile(HELLO_CFG_PATH, s.c_str());
}

static void hello_read_config(HelloConfig *cfg)
{
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(HELLO_CFG_PATH, (uint8_t*)info);
    info[size] = 0;
    if (size == 0) {
        // 預設值
        snprintf(cfg->username, sizeof(cfg->username), "%s", "World");
        cfg->display_speed = 100;
        hello_write_config(cfg);
    } else {
        char *param[2] = {0};
        analyseParam(info, 2, param);
        snprintf(cfg->username, sizeof(cfg->username), "%s", param[0]);
        cfg->display_speed = atoi(param[1]);
    }
}
```

`hello_init` 開頭加 `hello_read_config(&cfg_data);` 就行。

## 9. 加 web 設定頁（讓使用者透過瀏覽器改 username）

詳見 [04 — 工具函式 + 常用模式](./04-firmware-utilities.md) 的「Web Settings 怎麼加新欄位」一節。會帶你接到 `*_setting()` handler + `save*Conf()` + i18n keys + 加進 sidebar nav。

## 10. 寫 unit test 驗證 config parser

詳見 [06 — 測試完整指南](./06-testing.md) 的「寫 Unity unit test」一節。基本上：

```cpp
// AIO_Firmware_PIO/test/native/test_hello_config/test_main.cpp
#include <unity.h>
// ... include 你抽出來的 hello_config_parse 模組

void test_default_when_empty() {
    HelloConfig cfg{};
    hello_parse_config("", 0, &cfg);
    TEST_ASSERT_EQUAL_STRING("World", cfg.username);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_when_empty);
    return UNITY_END();
}
```

把 parser 邏輯抽成獨立 `hello_config_parse.cpp` 是必要的（不能 include `Arduino.h` 才能在 host 跑）— 章節 06 會詳細講這個重構流程，看 `app/heartbeat/heartbeat_config_parse.cpp` 是現成範本。

## 下一步

- 想加 web 設定頁、用 HTTP fetch、學 PROGMEM → [04 — 工具函式](./04-firmware-utilities.md)
- 想寫測試 → [06 — 測試完整指南](./06-testing.md)

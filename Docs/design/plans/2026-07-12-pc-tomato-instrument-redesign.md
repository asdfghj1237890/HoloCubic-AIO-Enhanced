# PC Monitor + Tomato Instrument-Family Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the `pc_resource` and `tomato` firmware screens onto the stock screen's design language (black / gold header / big mono numerals / hairline dividers), per the approved spec `Docs/design/specs/2026-07-12-pc-tomato-instrument-redesign.md`.

**Architecture:** Two GUI files are rewritten in place (visual layer only); derived-display math lives in two new header-only helpers so `native_unit` can TDD them; app logic, data sources, and scenario files are untouched; 4 goldens regenerate; README hero becomes a 3-up golden row.

**Tech Stack:** LVGL 9.5 (with `aio_lvgl_compat.h` shim, style/idiom reference: `AIO_Firmware_PIO/src/app/stockmarket/stockmarket_gui.c`), PlatformIO (`uvx platformio`), Unity via `native_unit`, SDL2 scenario harness (`lv_simulater_platformio`, goldens via `--update-golden`; CI fallback `regression.yml` `mode=update-golden`).

**Branch:** `pc-tomato-instrument-redesign` (already exists; spec committed as `14f3571`).

> **Historical note (post-execution):** the code blocks below are the pre-amendment versions. The shipped code differs where spec §9's golden-eyeball amendments applied (freq label montserrat_14 at y=12, pc_resource footer rows montserrat_20, tomato release-first screen rebuild, named BG tokens). The spec is the source of truth; this plan is a point-in-time execution record.

**Read first:** the spec (§3 tokens, §4/§5 element maps are the layout source of truth), `stockmarket_gui.c` (the idioms every widget here copies), CLAUDE.md firmware conventions (no `strcpy`/`sprintf`, no `delay()` in GUI paths).

**Pre-existing dirty files (NOT ours — never `git add` them):** `AGENTS.md`, `CLAUDE.md`, `AIO_Tool/Cargo.lock`, `AIO_Tool/studio/Cargo.toml`. Always stage by explicit path.

---

### Task 0: Preflight checks and baseline

**Files:** none modified.

- [ ] **Step 0.1: Confirm branch and clean intent**

Run: `git branch --show-current && git log --oneline -1`
Expected: `pc-tomato-instrument-redesign`, top commit `docs: add PC monitor + tomato instrument-family redesign spec`.

- [ ] **Step 0.2: Record baseline firmware size**

```powershell
cd AIO_Firmware_PIO
uvx platformio run -e HoloCubic_AIO_Releases
(Get-Item .pio/build/HoloCubic_AIO_Releases/firmware.bin).Length
```
Expected: build exits 0 (~65 s). Write the byte count into the final PR notes (acceptance §8.5 wants a before/after).

- [ ] **Step 0.3: Guard greps (all from repo root)**

```powershell
git grep -n "display_tomato" -- AIO_Firmware_PIO/src
git grep -n "ANIEND" -- AIO_Firmware_PIO/src/app/tomato
git grep -n "APP_WEATHER_GUI_H" -- AIO_Firmware_PIO/src
git grep -nE "lv_font_ibmplex_(16|18|24)\b" -- AIO_Firmware_PIO
git grep -n "lv_font_ibmplex_200" -- AIO_Firmware_PIO
git grep -n "tomato_chFont_20" -- AIO_Firmware_PIO
```
Expected:
- `display_tomato` → only `tomato_gui.{h,c}` and `tomato.cpp:363` (one call site).
- `ANIEND` in tomato → only the `#define` in `tomato_gui.h` (safe to drop).
- `APP_WEATHER_GUI_H` → both `weather` and `pc_resource` gui headers (the guard collision the rewrite fixes).
- ibmplex 16/18/24 → only `pc_resource` files; ibmplex_200 and `tomato_chFont_20` → only `tomato` files.

If any grep shows an extra consumer, STOP and re-check the spec's deletion list before proceeding.

---

### Task 1: `pc_resource_fmt.h` — net-speed formatter (TDD)

**Files:**
- Create: `AIO_Firmware_PIO/test/native/test_pc_resource_fmt/test_main.cpp`
- Create: `AIO_Firmware_PIO/src/app/pc_resource/pc_resource_fmt.h`

No `platformio.ini` change needed: the helper is header-only and `test_filter = native/*` auto-discovers the new test dir.

- [ ] **Step 1.1: Write the failing test**

`AIO_Firmware_PIO/test/native/test_pc_resource_fmt/test_main.cpp`:

```cpp
#include <unity.h>

#include "app/pc_resource/pc_resource_fmt.h"

void setUp() {}
void tearDown() {}

void test_speed_zero_is_kilobytes()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_STRING("0.0K", buf);
}

void test_speed_sub_megabyte_keeps_tenths()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 1005); // 100.5 KB/s
    TEST_ASSERT_EQUAL_STRING("100.5K", buf);
}

void test_speed_top_of_k_range()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 9999); // 999.9 KB/s
    TEST_ASSERT_EQUAL_STRING("999.9K", buf);
}

void test_speed_promotes_to_megabytes_at_1000k()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 10000); // 1000.0 KB/s
    TEST_ASSERT_EQUAL_STRING("1.0M", buf);
}

void test_speed_megabytes_keep_one_decimal()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), 123465); // 12346.5 KB/s
    TEST_ASSERT_EQUAL_STRING("12.3M", buf);
}

void test_speed_negative_clamps_to_zero()
{
    char buf[16];
    pc_resource_format_speed(buf, sizeof(buf), -5);
    TEST_ASSERT_EQUAL_STRING("0.0K", buf);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_speed_zero_is_kilobytes);
    RUN_TEST(test_speed_sub_megabyte_keeps_tenths);
    RUN_TEST(test_speed_top_of_k_range);
    RUN_TEST(test_speed_promotes_to_megabytes_at_1000k);
    RUN_TEST(test_speed_megabytes_keep_one_decimal);
    RUN_TEST(test_speed_negative_clamps_to_zero);
    return UNITY_END();
}
```

(If the build errors on a missing `main`-vs-runner convention, open `test/native/test_stockmarket_color_rule/test_main.cpp` and mirror its bottom-of-file runner exactly — that file is the repo's canonical Unity shape.)

- [ ] **Step 1.2: Run it, verify it fails on the missing header**

```powershell
cd AIO_Firmware_PIO
uvx platformio test -e native_unit -f "native/test_pc_resource_fmt*"
```
Expected: FAIL — `app/pc_resource/pc_resource_fmt.h: No such file or directory`.

- [ ] **Step 1.3: Implement the header**

`AIO_Firmware_PIO/src/app/pc_resource/pc_resource_fmt.h`:

```c
#ifndef APP_PC_RESOURCE_FMT_H
#define APP_PC_RESOURCE_FMT_H

#include <stdio.h>
#include <stddef.h>

// Net speeds arrive as KB/s scaled x10 (struct PC_Resource). The redesigned
// footer cells are fixed-width, so values >= 1000 KB/s promote to "M" instead
// of relying on the old scrolling labels.
static inline void pc_resource_format_speed(char *buf, size_t len, int raw_x10)
{
    if (raw_x10 < 0)
        raw_x10 = 0;
    int kbps = raw_x10 / 10;
    if (kbps < 1000)
        snprintf(buf, len, "%d.%dK", kbps, raw_x10 % 10);
    else
        snprintf(buf, len, "%d.%dM", kbps / 1000, (kbps % 1000) / 100);
}

#endif
```

- [ ] **Step 1.4: Run the test env, verify green**

Run: `uvx platformio test -e native_unit -f "native/test_pc_resource_fmt*"`
Expected: 6 PASSED, exit 0. Then run the full env once: `uvx platformio test -e native_unit` — all suites PASS (no collateral).

- [ ] **Step 1.5: Commit**

```powershell
git add AIO_Firmware_PIO/src/app/pc_resource/pc_resource_fmt.h AIO_Firmware_PIO/test/native/test_pc_resource_fmt/test_main.cpp
git commit -m "pc_resource: add fixed-width net speed formatter (TDD)"
```

---

### Task 2: `tomato_calc.h` — countdown display math (TDD)

**Files:**
- Create: `AIO_Firmware_PIO/test/native/test_tomato_calc/test_main.cpp`
- Create: `AIO_Firmware_PIO/src/app/tomato/tomato_calc.h`

- [ ] **Step 2.1: Write the failing test**

`AIO_Firmware_PIO/test/native/test_tomato_calc/test_main.cpp`:

```cpp
#include <unity.h>

#include "app/tomato/tomato_calc.h"

void setUp() {}
void tearDown() {}

void test_total_seconds()
{
    TEST_ASSERT_EQUAL_INT(1500, tomato_total_seconds(25, 0));
    TEST_ASSERT_EQUAL_INT(59, tomato_total_seconds(0, 59));
}

void test_progress_starts_at_zero()
{
    TEST_ASSERT_EQUAL_INT(0, tomato_progress_pct(1500, 1500));
}

void test_progress_midway()
{
    TEST_ASSERT_EQUAL_INT(50, tomato_progress_pct(1500, 750));
}

void test_progress_complete()
{
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(1500, 0));
}

void test_progress_zero_total_is_full()
{
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(0, 0));
}

void test_progress_clamps_out_of_range_remain()
{
    TEST_ASSERT_EQUAL_INT(0, tomato_progress_pct(1500, 2000)); // remain > total
    TEST_ASSERT_EQUAL_INT(100, tomato_progress_pct(1500, -5)); // negative remain
}

void test_focus_modes()
{
    TEST_ASSERT_TRUE(tomato_is_focus(0));
    TEST_ASSERT_TRUE(tomato_is_focus(1));
    TEST_ASSERT_FALSE(tomato_is_focus(-1));
    TEST_ASSERT_FALSE(tomato_is_focus(2));
}

void test_next_segment_minutes()
{
    TEST_ASSERT_EQUAL_INT(5, tomato_next_minutes(0));   // focus 25 -> break 5
    TEST_ASSERT_EQUAL_INT(15, tomato_next_minutes(1));  // focus 45 -> break 15
    TEST_ASSERT_EQUAL_INT(25, tomato_next_minutes(-1)); // break 5 -> focus 25
    TEST_ASSERT_EQUAL_INT(45, tomato_next_minutes(2));  // break 15 -> focus 45
    TEST_ASSERT_EQUAL_INT(5, tomato_next_minutes(7));   // out-of-range fallback
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_total_seconds);
    RUN_TEST(test_progress_starts_at_zero);
    RUN_TEST(test_progress_midway);
    RUN_TEST(test_progress_complete);
    RUN_TEST(test_progress_zero_total_is_full);
    RUN_TEST(test_progress_clamps_out_of_range_remain);
    RUN_TEST(test_focus_modes);
    RUN_TEST(test_next_segment_minutes);
    return UNITY_END();
}
```

- [ ] **Step 2.2: Run it, verify it fails on the missing header**

Run: `uvx platformio test -e native_unit -f "native/test_tomato_calc*"`
Expected: FAIL — `app/tomato/tomato_calc.h: No such file or directory`.

- [ ] **Step 2.3: Implement the header**

`AIO_Firmware_PIO/src/app/tomato/tomato_calc.h`:

```c
#ifndef APP_TOMATO_CALC_H
#define APP_TOMATO_CALC_H

// Pure countdown-display helpers, kept LVGL-free so native_unit can test them.
// time_mode values (tomato.cpp time_switch): 0=focus25, 1=focus45,
// -1=break5, 2=break15.

static inline int tomato_total_seconds(int minute, int second)
{
    return minute * 60 + second;
}

// Elapsed percentage of the countdown, clamped to [0,100].
static inline int tomato_progress_pct(int total_sec, int remain_sec)
{
    if (total_sec <= 0)
        return 100;
    if (remain_sec < 0)
        remain_sec = 0;
    if (remain_sec > total_sec)
        remain_sec = total_sec;
    return (int)(100L * (total_sec - remain_sec) / total_sec);
}

static inline int tomato_is_focus(int time_mode)
{
    return time_mode == 0 || time_mode == 1;
}

static inline int tomato_next_minutes(int time_mode)
{
    switch (time_mode)
    {
    case 0:
        return 5;
    case 1:
        return 15;
    case -1:
        return 25;
    case 2:
        return 45;
    default:
        return 5;
    }
}

#endif
```

- [ ] **Step 2.4: Run tests, verify green**

Run: `uvx platformio test -e native_unit -f "native/test_tomato_calc*"`
Expected: 8 PASSED. Then full `uvx platformio test -e native_unit` — all PASS.

- [ ] **Step 2.5: Commit**

```powershell
git add AIO_Firmware_PIO/src/app/tomato/tomato_calc.h AIO_Firmware_PIO/test/native/test_tomato_calc/test_main.cpp
git commit -m "tomato: add pure countdown display helpers (TDD)"
```

---

### Task 3: Rewrite the pc_resource GUI

**Files:**
- Modify: `AIO_Firmware_PIO/src/app/pc_resource/pc_resource_gui.h` (full replace)
- Modify: `AIO_Firmware_PIO/src/app/pc_resource/pc_resource_gui.c` (full replace)

Spec reference: §3 tokens, §4 element map. Idiom reference: `stockmarket_gui.c` (lv_line dividers, recolor markers, clean-not-delete release).

- [ ] **Step 3.1: Replace `pc_resource_gui.h`** (drops `sensor_module`; fixes the copy-pasted `APP_WEATHER_GUI_H` guard; public API unchanged)

```c
#ifndef APP_PC_RESOURCE_GUI_H
#define APP_PC_RESOURCE_GUI_H

// 遥感器数据，带一位小数的数据均为扩大10倍后的整数部分
struct PC_Resource
{
    int cpu_usage; // CPU利用率(%)
    int cpu_temp;  // CPU温度(℃)，扩大10倍
    int cpu_freq;  // CPU主频(MHz)
    int cpu_power; // CPU功耗(W)，扩大10倍

    int gpu_usage; // GPU利用率(%)
    int gpu_temp;  // GPU温度(℃)，扩大10倍
    int gpu_power; // GPU功耗(W)，扩大10倍

    int ram_usage; // 内存RAM使用率(%)
    int ram_use;   // 内存RAM使用量(MB)

    int net_upload_speed;   // 网络上行速率(KB/s)，扩大10倍
    int net_download_speed; // 网络下行速率(KB/s)，扩大10倍
};

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
    extern const lv_img_dsc_t app_pc_resource;

    void display_pc_resource_gui_init(void);
    void display_pc_resource_init(void);
    void display_pc_resource(struct PC_Resource sensorInfo);
    void pc_resource_gui_release(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
```

- [ ] **Step 3.2: Replace `pc_resource_gui.c`**

```c
#include "pc_resource_gui.h"
#include "pc_resource_fmt.h"
#include <string.h>

// Instrument-family tokens — mirrors stockmarket_gui.c
#define PCR_COLOR_GOLD 0xFFD000
#define PCR_COLOR_GOLD_DIM 0xC89030
#define PCR_COLOR_GRAY_DIM 0x666666
#define PCR_COLOR_GRAY_LABEL 0x888888
#define PCR_COLOR_WHITE 0xFFFFFF
#define PCR_COLOR_TRACK 0x222222
#define PCR_COLOR_GREEN 0x00FF44

LV_FONT_DECLARE(ch_font20);
LV_FONT_DECLARE(lv_font_ibmplex_bold_30);

static lv_obj_t *scr = NULL;

static lv_obj_t *header_label = NULL;
static lv_obj_t *freq_label = NULL;
static lv_obj_t *divider_top = NULL;
static lv_obj_t *name_label[3] = {NULL, NULL, NULL};
static lv_obj_t *value_label[3] = {NULL, NULL, NULL};
static lv_obj_t *pct_label[3] = {NULL, NULL, NULL};
static lv_obj_t *meta_label[3] = {NULL, NULL, NULL};
static lv_obj_t *usage_bar[3] = {NULL, NULL, NULL};
static lv_obj_t *divider_bot = NULL;
static lv_obj_t *net_up_label = NULL;
static lv_obj_t *rail_net = NULL;
static lv_obj_t *net_down_label = NULL;
static lv_obj_t *cpu_power_label = NULL;
static lv_obj_t *rail_power = NULL;
static lv_obj_t *gpu_power_label = NULL;

static lv_style_t default_style;
static lv_style_t header_style;     // ch_font20 gold
static lv_style_t small_gray_style; // montserrat_20 gray labels
static lv_style_t value_style;      // ibmplex_bold_30 white digits
static lv_style_t meta_gold_style;  // montserrat_20 gold (temperatures)
static lv_style_t footer_style;     // montserrat_24 white (recolor markers inline)

static const char *row_names[3] = {"CPU", "GPU", "RAM"};
static const lv_coord_t row_y[3] = {46, 86, 126};

static const lv_point_precise_t divider_points[] = {{0, 0}, {239, 0}};
static const lv_point_precise_t rail_points[] = {{0, 0}, {0, 24}};

void display_pc_resource_gui_init(void)
{
    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));

    lv_style_init(&header_style);
    lv_style_set_text_opa(&header_style, LV_OPA_COVER);
    lv_style_set_text_color(&header_style, lv_color_hex(PCR_COLOR_GOLD));
    lv_style_set_text_font(&header_style, &ch_font20);

    lv_style_init(&small_gray_style);
    lv_style_set_text_opa(&small_gray_style, LV_OPA_COVER);
    lv_style_set_text_color(&small_gray_style, lv_color_hex(PCR_COLOR_GRAY_LABEL));
    lv_style_set_text_font(&small_gray_style, &lv_font_montserrat_20);

    lv_style_init(&value_style);
    lv_style_set_text_opa(&value_style, LV_OPA_COVER);
    lv_style_set_text_color(&value_style, lv_color_hex(PCR_COLOR_WHITE));
    lv_style_set_text_font(&value_style, &lv_font_ibmplex_bold_30);

    lv_style_init(&meta_gold_style);
    lv_style_set_text_opa(&meta_gold_style, LV_OPA_COVER);
    lv_style_set_text_color(&meta_gold_style, lv_color_hex(PCR_COLOR_GOLD));
    lv_style_set_text_font(&meta_gold_style, &lv_font_montserrat_20);

    lv_style_init(&footer_style);
    lv_style_set_text_opa(&footer_style, LV_OPA_COVER);
    lv_style_set_text_color(&footer_style, lv_color_hex(PCR_COLOR_WHITE));
    lv_style_set_text_font(&footer_style, &lv_font_montserrat_24);
}

static lv_obj_t *pcr_make_hline(lv_coord_t y, uint32_t color_hex)
{
    lv_obj_t *line = lv_line_create(scr);
    lv_line_set_points(line, divider_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(color_hex), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, y);
    return line;
}

static lv_obj_t *pcr_make_rail(lv_coord_t y)
{
    lv_obj_t *line = lv_line_create(scr);
    lv_line_set_points(line, rail_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(PCR_COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 128, y);
    return line;
}

void display_pc_resource_init(void)
{
    lv_obj_t *act_obj = lv_scr_act();
    if (act_obj == scr)
        return;

    pc_resource_gui_release();
    lv_obj_clean(act_obj);

    scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &default_style, LV_STATE_DEFAULT);

    header_label = lv_label_create(scr);
    lv_obj_add_style(header_label, &header_style, LV_STATE_DEFAULT);
    lv_label_set_text(header_label, "PC MONITOR");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, 12, 8);

    freq_label = lv_label_create(scr);
    lv_obj_add_style(freq_label, &small_gray_style, LV_STATE_DEFAULT);
    lv_label_set_text(freq_label, "0MHz");
    lv_obj_align(freq_label, LV_ALIGN_TOP_RIGHT, -12, 10);

    divider_top = pcr_make_hline(40, PCR_COLOR_GOLD_DIM);

    for (int i = 0; i < 3; i++)
    {
        name_label[i] = lv_label_create(scr);
        lv_obj_add_style(name_label[i], &small_gray_style, LV_STATE_DEFAULT);
        lv_label_set_text(name_label[i], row_names[i]);
        lv_obj_align(name_label[i], LV_ALIGN_TOP_LEFT, 12, row_y[i] + 6);

        value_label[i] = lv_label_create(scr);
        lv_obj_add_style(value_label[i], &value_style, LV_STATE_DEFAULT);
        lv_label_set_text(value_label[i], "0");
        lv_obj_align(value_label[i], LV_ALIGN_TOP_LEFT, 72, row_y[i]);

        pct_label[i] = lv_label_create(scr);
        lv_obj_add_style(pct_label[i], &small_gray_style, LV_STATE_DEFAULT);
        lv_label_set_text(pct_label[i], "%");
        lv_obj_align_to(pct_label[i], value_label[i], LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -2);

        meta_label[i] = lv_label_create(scr);
        lv_obj_add_style(meta_label[i], (i == 2) ? &small_gray_style : &meta_gold_style, LV_STATE_DEFAULT);
        lv_label_set_text(meta_label[i], (i == 2) ? "0MB" : "0.0°C");
        lv_obj_align(meta_label[i], LV_ALIGN_TOP_RIGHT, -12, row_y[i] + 6);

        usage_bar[i] = lv_bar_create(scr);
        lv_obj_set_size(usage_bar[i], 216, 4);
        lv_obj_align(usage_bar[i], LV_ALIGN_TOP_LEFT, 12, row_y[i] + 32);
        lv_bar_set_range(usage_bar[i], 0, 100);
        lv_bar_set_value(usage_bar[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(usage_bar[i], lv_color_hex(PCR_COLOR_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(usage_bar[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(usage_bar[i], 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(usage_bar[i], lv_color_hex(PCR_COLOR_GREEN), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(usage_bar[i], LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(usage_bar[i], 2, LV_PART_INDICATOR);
    }

    divider_bot = pcr_make_hline(166, PCR_COLOR_GRAY_DIM);

    net_up_label = lv_label_create(scr);
    lv_obj_add_style(net_up_label, &footer_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(net_up_label, true);
    lv_label_set_text(net_up_label, "#00ff44 " LV_SYMBOL_UPLOAD "# 0.0K");
    lv_obj_align(net_up_label, LV_ALIGN_TOP_LEFT, 12, 172);

    rail_net = pcr_make_rail(172);

    net_down_label = lv_label_create(scr);
    lv_obj_add_style(net_down_label, &footer_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(net_down_label, true);
    lv_label_set_text(net_down_label, "#ff2020 " LV_SYMBOL_DOWNLOAD "# 0.0K");
    lv_obj_align(net_down_label, LV_ALIGN_TOP_LEFT, 140, 172);

    cpu_power_label = lv_label_create(scr);
    lv_obj_add_style(cpu_power_label, &footer_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(cpu_power_label, true);
    lv_label_set_text(cpu_power_label, "#ffd000 C# 0.0W");
    lv_obj_align(cpu_power_label, LV_ALIGN_TOP_LEFT, 12, 200);

    rail_power = pcr_make_rail(200);

    gpu_power_label = lv_label_create(scr);
    lv_obj_add_style(gpu_power_label, &footer_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(gpu_power_label, true);
    lv_label_set_text(gpu_power_label, "#ffd000 G# 0.0W");
    lv_obj_align(gpu_power_label, LV_ALIGN_TOP_LEFT, 140, 200);
}

void display_pc_resource(struct PC_Resource sensorInfo)
{
    display_pc_resource_init();

    char speed_buf[16];

    lv_label_set_text_fmt(freq_label, "%dMHz", sensorInfo.cpu_freq);

    const int usage[3] = {sensorInfo.cpu_usage, sensorInfo.gpu_usage, sensorInfo.ram_usage};
    for (int i = 0; i < 3; i++)
    {
        lv_label_set_text_fmt(value_label[i], "%d", usage[i]);
        lv_bar_set_value(usage_bar[i], usage[i], LV_ANIM_OFF);
        lv_obj_align_to(pct_label[i], value_label[i], LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -2);
    }

    lv_label_set_text_fmt(meta_label[0], "%d.%d°C", sensorInfo.cpu_temp / 10, sensorInfo.cpu_temp % 10);
    lv_label_set_text_fmt(meta_label[1], "%d.%d°C", sensorInfo.gpu_temp / 10, sensorInfo.gpu_temp % 10);
    lv_label_set_text_fmt(meta_label[2], "%dMB", sensorInfo.ram_use);

    pc_resource_format_speed(speed_buf, sizeof(speed_buf), sensorInfo.net_upload_speed);
    lv_label_set_text_fmt(net_up_label, "#00ff44 " LV_SYMBOL_UPLOAD "# %s", speed_buf);
    pc_resource_format_speed(speed_buf, sizeof(speed_buf), sensorInfo.net_download_speed);
    lv_label_set_text_fmt(net_down_label, "#ff2020 " LV_SYMBOL_DOWNLOAD "# %s", speed_buf);

    lv_label_set_text_fmt(cpu_power_label, "#ffd000 C# %d.%dW", sensorInfo.cpu_power / 10, sensorInfo.cpu_power % 10);
    lv_label_set_text_fmt(gpu_power_label, "#ffd000 G# %d.%dW", sensorInfo.gpu_power / 10, sensorInfo.gpu_power % 10);

    lv_scr_load(scr);
}

void pc_resource_gui_release(void)
{
    if (scr != NULL)
    {
        lv_obj_clean(scr);
        scr = NULL;
        header_label = NULL;
        freq_label = NULL;
        divider_top = NULL;
        divider_bot = NULL;
        net_up_label = NULL;
        rail_net = NULL;
        net_down_label = NULL;
        cpu_power_label = NULL;
        rail_power = NULL;
        gpu_power_label = NULL;
        memset(name_label, 0, sizeof(name_label));
        memset(value_label, 0, sizeof(value_label));
        memset(pct_label, 0, sizeof(pct_label));
        memset(meta_label, 0, sizeof(meta_label));
        memset(usage_bar, 0, sizeof(usage_bar));
    }
}
```

Notes for the implementer:
- `°C` is a two-byte UTF-8 literal; montserrat built-ins include U+00B0 (same precedent as the old `℃` usage, and stock's files are UTF-8).
- `LV_SYMBOL_UPLOAD`/`LV_SYMBOL_DOWNLOAD` are the same U+F093/U+F019 codepoints the deleted FontAwesome-bearing app fonts provided.
- Keep the release semantics `lv_obj_clean`-not-`lv_obj_del` (see comment in `stockmarket_gui.c:307-316` for why).

- [ ] **Step 3.3: Build firmware**

Run: `cd AIO_Firmware_PIO && uvx platformio run -e HoloCubic_AIO_Releases`
Expected: exit 0, no warnings about implicit declarations in pc_resource files.

- [ ] **Step 3.4: Commit**

```powershell
git add AIO_Firmware_PIO/src/app/pc_resource/pc_resource_gui.h AIO_Firmware_PIO/src/app/pc_resource/pc_resource_gui.c
git commit -m "pc_resource: restyle onto stock instrument family"
```

---

### Task 4: Rewrite the tomato GUI

**Files:**
- Modify: `AIO_Firmware_PIO/src/app/tomato/tomato_gui.h` (full replace)
- Modify: `AIO_Firmware_PIO/src/app/tomato/tomato_gui.c` (full replace)
- Modify: `AIO_Firmware_PIO/src/app/tomato/tomato.cpp:363` (one line)

- [ ] **Step 4.1: Replace `tomato_gui.h`** (new `display_tomato` signature; drops the unused `ANIEND` macro and duplicate extern block)

```c
#ifndef APP_TOMATO_GUI_H
#define APP_TOMATO_GUI_H

struct TimeStr
{
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int weekday;
};

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
    extern const lv_img_dsc_t app_tomato_icon;

    void tomato_gui_init(void);
    void tomato_gui_del(void);
    void display_tomato(struct TimeStr t, struct TimeStr t_start, int mode);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
```

- [ ] **Step 4.2: Replace `tomato_gui.c`**

```c
#include "tomato_gui.h"
#include "tomato_calc.h"
#include <stdio.h>
#include "lvgl.h"

// Instrument-family tokens — mirrors stockmarket_gui.c
#define TMT_COLOR_GOLD 0xFFD000
#define TMT_COLOR_GOLD_DIM 0xC89030
#define TMT_COLOR_GRAY_DIM 0x666666
#define TMT_COLOR_GRAY_LABEL 0x888888
#define TMT_COLOR_WHITE 0xFFFFFF
#define TMT_COLOR_TRACK 0x222222
#define TMT_COLOR_GREEN 0x00FF44
#define TMT_COLOR_RED 0xFF2020

LV_FONT_DECLARE(ch_font20);
LV_FONT_DECLARE(lv_font_ibmplex_bold_64);

static lv_obj_t *tomato_scr = NULL;
static lv_obj_t *header_label = NULL;
static lv_obj_t *target_label = NULL;
static lv_obj_t *divider_top = NULL;
static lv_obj_t *minute_label = NULL;
static lv_obj_t *colon_dot_1 = NULL;
static lv_obj_t *colon_dot_2 = NULL;
static lv_obj_t *second_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *divider_bot = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *footer_rail = NULL;
static lv_obj_t *next_label = NULL;
static lv_obj_t *hint_label = NULL;

static lv_style_t default_style;
static lv_style_t header_style;     // ch_font20 gold
static lv_style_t small_gray_style; // montserrat_20 gray (target)
static lv_style_t digit_style;      // ibmplex_bold_64 (color set per state)
static lv_style_t status_style;     // montserrat_24 (color set per state)
static lv_style_t next_style;       // montserrat_24 gray base, gold N via recolor
static lv_style_t hint_style;       // montserrat_14 dim gray

static const lv_point_precise_t divider_points[] = {{0, 0}, {239, 0}};
static const lv_point_precise_t rail_points[] = {{0, 0}, {0, 24}};

void tomato_gui_init(void)
{
    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));

    lv_style_init(&header_style);
    lv_style_set_text_opa(&header_style, LV_OPA_COVER);
    lv_style_set_text_color(&header_style, lv_color_hex(TMT_COLOR_GOLD));
    lv_style_set_text_font(&header_style, &ch_font20);

    lv_style_init(&small_gray_style);
    lv_style_set_text_opa(&small_gray_style, LV_OPA_COVER);
    lv_style_set_text_color(&small_gray_style, lv_color_hex(TMT_COLOR_GRAY_LABEL));
    lv_style_set_text_font(&small_gray_style, &lv_font_montserrat_20);

    lv_style_init(&digit_style);
    lv_style_set_text_opa(&digit_style, LV_OPA_COVER);
    lv_style_set_text_color(&digit_style, lv_color_hex(TMT_COLOR_WHITE));
    lv_style_set_text_font(&digit_style, &lv_font_ibmplex_bold_64);

    lv_style_init(&status_style);
    lv_style_set_text_opa(&status_style, LV_OPA_COVER);
    lv_style_set_text_color(&status_style, lv_color_hex(TMT_COLOR_RED));
    lv_style_set_text_font(&status_style, &lv_font_montserrat_24);

    lv_style_init(&next_style);
    lv_style_set_text_opa(&next_style, LV_OPA_COVER);
    lv_style_set_text_color(&next_style, lv_color_hex(TMT_COLOR_GRAY_LABEL));
    lv_style_set_text_font(&next_style, &lv_font_montserrat_24);

    lv_style_init(&hint_style);
    lv_style_set_text_opa(&hint_style, LV_OPA_COVER);
    lv_style_set_text_color(&hint_style, lv_color_hex(TMT_COLOR_GRAY_DIM));
    lv_style_set_text_font(&hint_style, &lv_font_montserrat_14);
}

static lv_obj_t *tmt_make_hline(lv_coord_t y, uint32_t color_hex)
{
    lv_obj_t *line = lv_line_create(tomato_scr);
    lv_line_set_points(line, divider_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(color_hex), LV_PART_MAIN);
    lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, y);
    return line;
}

static lv_obj_t *tmt_make_dot(lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *dot = lv_obj_create(tomato_scr);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(TMT_COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_pos(dot, x, y);
    return dot;
}

static void tomato_ui_create(void)
{
    if (tomato_scr == lv_scr_act())
        return;
    lv_obj_clean(lv_scr_act());
    if (tomato_scr == NULL)
        tomato_scr = lv_obj_create(NULL);
    lv_obj_add_style(tomato_scr, &default_style, LV_STATE_DEFAULT);

    header_label = lv_label_create(tomato_scr);
    lv_obj_add_style(header_label, &header_style, LV_STATE_DEFAULT);
    lv_label_set_text(header_label, "TOMATO");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, 12, 8);

    target_label = lv_label_create(tomato_scr);
    lv_obj_add_style(target_label, &small_gray_style, LV_STATE_DEFAULT);
    lv_label_set_text(target_label, "25min");
    lv_obj_align(target_label, LV_ALIGN_TOP_RIGHT, -12, 10);

    divider_top = tmt_make_hline(40, TMT_COLOR_GOLD_DIM);

    minute_label = lv_label_create(tomato_scr);
    lv_obj_add_style(minute_label, &digit_style, LV_STATE_DEFAULT);
    lv_label_set_text(minute_label, "25");
    lv_obj_align(minute_label, LV_ALIGN_TOP_LEFT, 32, 56);

    colon_dot_1 = tmt_make_dot(116, 76);
    colon_dot_2 = tmt_make_dot(116, 100);

    second_label = lv_label_create(tomato_scr);
    lv_obj_add_style(second_label, &digit_style, LV_STATE_DEFAULT);
    lv_label_set_text(second_label, "00");
    lv_obj_align(second_label, LV_ALIGN_TOP_LEFT, 132, 56);

    progress_bar = lv_bar_create(tomato_scr);
    lv_obj_set_size(progress_bar, 216, 6);
    lv_obj_align(progress_bar, LV_ALIGN_TOP_LEFT, 12, 144);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(TMT_COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(TMT_COLOR_RED), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress_bar, 3, LV_PART_INDICATOR);

    divider_bot = tmt_make_hline(166, TMT_COLOR_GRAY_DIM);

    status_label = lv_label_create(tomato_scr);
    lv_obj_add_style(status_label, &status_style, LV_STATE_DEFAULT);
    lv_label_set_text(status_label, "FOCUS .");
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 12, 172);

    footer_rail = lv_line_create(tomato_scr);
    lv_line_set_points(footer_rail, rail_points, 2);
    lv_obj_set_style_line_color(footer_rail, lv_color_hex(TMT_COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_line_width(footer_rail, 2, LV_PART_MAIN);
    lv_obj_align(footer_rail, LV_ALIGN_TOP_LEFT, 128, 172);

    next_label = lv_label_create(tomato_scr);
    lv_obj_add_style(next_label, &next_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(next_label, true);
    lv_label_set_text(next_label, "#ffd000 N# 5min");
    lv_obj_align(next_label, LV_ALIGN_TOP_LEFT, 140, 172);

    hint_label = lv_label_create(tomato_scr);
    lv_obj_add_style(hint_label, &hint_style, LV_STATE_DEFAULT);
    lv_label_set_text(hint_label, "HOLD +1min | TILT reset");
    lv_obj_align(hint_label, LV_ALIGN_TOP_LEFT, 12, 206);

    lv_scr_load(tomato_scr);
}

void display_tomato(struct TimeStr t, struct TimeStr t_start, int mode)
{
    tomato_ui_create();

    int minute = t.minute;
    int second = t.second;
    if (second == 60) // preserve the old 60s normalization (x min 60 s -> x+1 min 00 s)
    {
        second = 0;
        minute = t.minute + 1;
    }

    lv_label_set_text_fmt(minute_label, "%02d", minute);
    lv_label_set_text_fmt(second_label, "%02d", second);
    lv_label_set_text_fmt(target_label, "%dmin", t_start.minute);
    lv_label_set_text_fmt(next_label, "#ffd000 N# %dmin", tomato_next_minutes(mode));

    int total = tomato_total_seconds(t_start.minute, t_start.second);
    int remain = tomato_total_seconds(minute, second);
    lv_bar_set_value(progress_bar, tomato_progress_pct(total, remain), LV_ANIM_OFF);

    int is_focus = tomato_is_focus(mode);
    uint32_t accent = is_focus ? TMT_COLOR_RED : TMT_COLOR_GREEN;
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(accent), LV_PART_INDICATOR);

    uint32_t digit_color = (minute == 0) ? TMT_COLOR_RED : TMT_COLOR_WHITE;
    lv_obj_set_style_text_color(minute_label, lv_color_hex(digit_color), LV_PART_MAIN);
    lv_obj_set_style_text_color(second_label, lv_color_hex(digit_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(colon_dot_1, lv_color_hex(digit_color), LV_PART_MAIN);
    lv_obj_set_style_bg_color(colon_dot_2, lv_color_hex(digit_color), LV_PART_MAIN);

    char status_buf[16];
    if (minute == 0 && second == 0)
    {
        snprintf(status_buf, sizeof(status_buf), "TIME UP!");
        lv_obj_set_style_text_color(status_label, lv_color_hex(TMT_COLOR_WHITE), LV_PART_MAIN);
    }
    else
    {
        int dots = (60 - second - 1) % 5 + 1; // same cadence the old GUI used
        snprintf(status_buf, sizeof(status_buf), "%s %.*s", is_focus ? "FOCUS" : "BREAK", dots, ".....");
        lv_obj_set_style_text_color(status_label, lv_color_hex(accent), LV_PART_MAIN);
    }
    lv_label_set_text(status_label, status_buf);
}

void tomato_gui_del(void)
{
    if (NULL != tomato_scr)
    {
        lv_obj_clean(tomato_scr);
        tomato_scr = NULL;
        header_label = NULL;
        target_label = NULL;
        divider_top = NULL;
        minute_label = NULL;
        colon_dot_1 = NULL;
        colon_dot_2 = NULL;
        second_label = NULL;
        progress_bar = NULL;
        divider_bot = NULL;
        status_label = NULL;
        footer_rail = NULL;
        next_label = NULL;
        hint_label = NULL;
    }
}
```

- [ ] **Step 4.3: Update the single call site** — `AIO_Firmware_PIO/src/app/tomato/tomato.cpp:363`

Old:
```cpp
    display_tomato(run_data->t, run_data->time_mode);
```
New:
```cpp
    display_tomato(run_data->t, run_data->t_start, run_data->time_mode);
```

- [ ] **Step 4.4: Build firmware**

Run: `cd AIO_Firmware_PIO && uvx platformio run -e HoloCubic_AIO_Releases`
Expected: exit 0. A failure mentioning `lv_font_ibmplex_115`/`_64`/`_200` here means a stale declare was left in `tomato_gui.c` — the new file must not declare them.

- [ ] **Step 4.5: Commit**

```powershell
git add AIO_Firmware_PIO/src/app/tomato/tomato_gui.h AIO_Firmware_PIO/src/app/tomato/tomato_gui.c AIO_Firmware_PIO/src/app/tomato/tomato.cpp
git commit -m "tomato: restyle onto stock instrument family, English status words"
```

---

### Task 5: Delete orphaned fonts, measure flash win

**Files:**
- Delete: `AIO_Firmware_PIO/src/app/pc_resource/lv_font_ibmplex_16.c`, `lv_font_ibmplex_18.c`, `lv_font_ibmplex_24.c`
- Delete: `AIO_Firmware_PIO/src/app/tomato/lv_font_ibmplex_200.c`, `tomato_chFont_20.c`

- [ ] **Step 5.1: Re-run the orphan greps** (post-rewrite they must show zero source references)

```powershell
git grep -nE "lv_font_ibmplex_(16|18|24)\b|lv_font_ibmplex_200|tomato_chFont_20" -- AIO_Firmware_PIO/src
```
Expected: no matches outside the five font files themselves. Any other hit = STOP, do not delete that file.

- [ ] **Step 5.2: Delete the five files**

```powershell
git rm AIO_Firmware_PIO/src/app/pc_resource/lv_font_ibmplex_16.c AIO_Firmware_PIO/src/app/pc_resource/lv_font_ibmplex_18.c AIO_Firmware_PIO/src/app/pc_resource/lv_font_ibmplex_24.c AIO_Firmware_PIO/src/app/tomato/lv_font_ibmplex_200.c AIO_Firmware_PIO/src/app/tomato/tomato_chFont_20.c
```

- [ ] **Step 5.3: Build + record new size**

```powershell
cd AIO_Firmware_PIO
uvx platformio run -e HoloCubic_AIO_Releases
(Get-Item .pio/build/HoloCubic_AIO_Releases/firmware.bin).Length
```
Expected: exit 0 (a link error naming a deleted font symbol means step 5.1 was wrong — restore and re-grep). Size must be smaller than the Task 0 baseline; note both numbers for the PR body.

- [ ] **Step 5.4: Run all host tests once**

Run: `uvx platformio test -e native_unit && uvx platformio test -e native_ftp`
Expected: all PASS (fonts are not compiled into these envs; this is a regression tripwire).

- [ ] **Step 5.5: Commit**

```powershell
git commit -m "pc_resource/tomato: drop orphaned app fonts (flash savings)"
```
(`git rm` already staged the deletions; verify with `git status --porcelain` that ONLY the five deletions are staged.)

---

### Task 6: Regenerate the four goldens

**Files:**
- Modify (binary): `test/golden/pc_resource/smoke/01_initial.png`, `02_with_data.png`, `test/golden/tomato/smoke/01_initial.png`, `02_after_go_forward.png`

Scenario files are NOT edited (spec §8).

- [ ] **Step 6.1: Try the local SDL2 harness first**

```powershell
cd lv_simulater_platformio
uvx platformio run -e native_test
```
If this builds on the Windows host, run (from `lv_simulater_platformio`):

```powershell
./.pio/build/native_test/program --scenario ../test/scenarios/pc_resource/smoke.scn --update-golden --headless
./.pio/build/native_test/program --scenario ../test/scenarios/tomato/smoke.scn --update-golden --headless
```
then re-run both WITHOUT `--update-golden` and expect exit 0 (compare mode passes against the fresh baselines). Skip to Step 6.4.

- [ ] **Step 6.2 (fallback): regenerate via CI** — if the SDL2 build is unavailable locally (expected on this machine; see memory note "host-harness envs via CI").

**CHECKPOINT — pushing the branch is an outward-facing action: confirm with the user before the first push.** Then:

```powershell
git push -u origin pc-tomato-instrument-redesign
gh workflow run regression.yml --ref pc-tomato-instrument-redesign -f mode=update-golden
gh run list --workflow=regression.yml --branch pc-tomato-instrument-redesign --limit 1 --json databaseId,status
gh run watch <databaseId> --exit-status --interval 30
```

- [ ] **Step 6.3 (fallback cont.): download and place the baselines**

```powershell
gh run download <databaseId> --dir golden-artifact
```
Copy ONLY the four PNGs listed above from the artifact into `test/golden/...` (artifact layout mirrors `test/golden/`; the workflow uploads regenerated baselines when `mode=update-golden` — see `.github/workflows/regression.yml:138-141`). Then `Remove-Item -Recurse -Force golden-artifact`.

- [ ] **Step 6.4: Eyeball the four PNGs against the spec**

Open each regenerated golden and check against spec §4/§5: gold header top-left, divider at y=40/166, PC shows three rows + green bars + footer `↑/↓` and `C/G` rows; tomato shows big white `MM:SS` with square colon dots, red progress bar, `FOCUS ...` status, `N 5min`, gesture hint. Confirm `git status` shows ONLY the four PNGs changed (no other app's golden moved).

- [ ] **Step 6.5: Commit**

```powershell
git add test/golden/pc_resource/smoke/01_initial.png test/golden/pc_resource/smoke/02_with_data.png test/golden/tomato/smoke/01_initial.png test/golden/tomato/smoke/02_after_go_forward.png
git commit -m "test: regenerate pc_resource + tomato goldens for instrument redesign"
```

---

### Task 7: README hero 3-up

**Files:**
- Modify: `README.md:7`
- Maybe modify: `README_zh-CN.md` (only if it embeds the stock golden)

- [ ] **Step 7.1: Replace the single stock image with a 3-column table**

`README.md` line 7 currently:
```markdown
![Stock Market simulator preview](test/golden/stockmarket/smoke/01_initial.png)
```
Replace with:
```markdown
| Stock Market | PC Monitor | Tomato Timer |
|---|---|---|
| ![Stock Market simulator preview](test/golden/stockmarket/smoke/01_initial.png) | ![PC resource monitor simulator preview](test/golden/pc_resource/smoke/02_with_data.png) | ![Tomato pomodoro simulator preview](test/golden/tomato/smoke/01_initial.png) |
```

- [ ] **Step 7.2: Check the zh-CN README**

Run: `git grep -n "test/golden" README_zh-CN.md`
If it embeds the stock golden the same way, apply the same table (translated column headers: 股票行情 / PC 性能监控 / 番茄钟). If no match, do nothing.

- [ ] **Step 7.3: Commit**

```powershell
git add README.md README_zh-CN.md
git commit -m "docs: README hero shows stock + pc_resource + tomato goldens"
```
(Drop `README_zh-CN.md` from the add if unmodified.)

---

### Task 8: Final gates and PR

- [ ] **Step 8.1: Full local gate run** (from `AIO_Firmware_PIO`)

```powershell
uvx platformio run -e HoloCubic_AIO_Releases
uvx platformio test -e native_unit
uvx platformio test -e native_ftp
```
Expected: all exit 0. Paste the tails into the PR body.

- [ ] **Step 8.2: Diff hygiene**

Run: `git status --porcelain && git log --oneline main..HEAD`
Expected: working tree shows only the four pre-existing dirty files (untouched); history shows the spec + plan + ~7 implementation commits.

- [ ] **Step 8.3: Push + PR — CHECKPOINT: confirm with the user before pushing (if not already pushed in Task 6)**

```powershell
git push -u origin pc-tomato-instrument-redesign
gh pr create --title "Restyle pc_resource + tomato screens onto the stock instrument family" --body "<summary, spec link, before/after golden images, flash size delta, gate outputs>

🤖 Generated with [Claude Code](https://claude.com/claude-code)"
gh run list --branch pc-tomato-instrument-redesign --limit 1 --json databaseId,status
gh run watch <databaseId> --exit-status --interval 30
```
Expected: CI green (firmware-build, unit-tests, gui-regression against the new goldens). Report PR URL to the user; merge only on user instruction (`gh pr merge <#> --squash --delete-branch`).

---

## Verification summary (maps to spec §8 acceptance)

| Spec acceptance | Covered by |
|---|---|
| 1. Releases build exits 0 | Tasks 3.3, 4.4, 5.3, 8.1 |
| 2. Scenarios pass vs 4 new goldens, no others change | Task 6 (compare re-run / CI gui-regression), 6.4 |
| 3. Zero refs to deleted font symbols | Tasks 0.3, 5.1 |
| 4. Goldens eyeballed vs §4/§5 | Task 6.4 |
| 5. `.bin` size before/after reported | Tasks 0.2, 5.3 → PR body |
| 6. Only intended files in diff | Tasks 5.5, 8.2 |

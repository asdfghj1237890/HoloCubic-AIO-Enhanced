#include "weather_gui.h"
#include "weather_image.h"

#include <string.h>

#include "lvgl.h"

LV_FONT_DECLARE(lv_font_ibmplex_115);
LV_FONT_DECLARE(lv_font_ibmplex_64);
LV_FONT_DECLARE(ch_font20);
LV_FONT_DECLARE(ch_font20_tc);
static lv_style_t default_style;
static lv_style_t chFont_style;
static lv_style_t numberSmall_style;
static lv_style_t numberBig_style;
static lv_style_t btn_style;
static lv_style_t bar_style;

static lv_obj_t *scr_1 = NULL;
static lv_obj_t *scr_2 = NULL;
static lv_obj_t *chart, *titleLabel;

static lv_obj_t *weatherImg = NULL;
static lv_obj_t *cityLabel = NULL;
static lv_obj_t *airQualityBtn = NULL, *airQualityLabel = NULL;
static lv_obj_t *txtLabel = NULL;
static lv_obj_t *clockLabel_1 = NULL, *clockLabel_2 = NULL;
static lv_obj_t *dateLabel = NULL;
static lv_obj_t *tempImg = NULL, *tempBar = NULL, *tempLabel = NULL;
static lv_obj_t *humiImg = NULL, *humiBar = NULL, *humiLabel = NULL;
static lv_obj_t *spaceImg = NULL;

static lv_chart_series_t *ser1, *ser2;

// 天气图标路径的映射关系
const void *weaImage_map[] = {&weather_0, &weather_9, &weather_14, &weather_5, &weather_25,
                              &weather_30, &weather_26, &weather_11, &weather_23};
// 太空人图标路径的映射关系
const void *manImage_map[] = {&man_0, &man_1, &man_2, &man_3, &man_4, &man_5, &man_6, &man_7, &man_8, &man_9};

// Current language setting (default: Simplified Chinese)
static WeatherLanguage current_language = LANG_SIMPLIFIED_CHINESE;

// Dual language support: Simplified Chinese [0] and Traditional Chinese [1]
static const char weekDayCh[2][7][4] = {
    {"日", "一", "二", "三", "四", "五", "六"},  // Simplified Chinese
    {"日", "一", "二", "三", "四", "五", "六"}   // Traditional Chinese (same)
};

static const char airQualityCh[2][6][10] = {
    {"优", "良", "轻度", "中度", "重度", "严重"},      // Simplified Chinese
    {"優", "良", "輕度", "中度", "重度", "嚴重"}       // Traditional Chinese
};

// UI text strings for different languages
typedef struct {
    const char *view_more_weather;
    const char *wind_power;
    const char *today_weather_format;
} UIStrings;

static const UIStrings ui_strings[2] = {
    // Simplified Chinese
    {
        .view_more_weather = "查看更多天气",
        .wind_power = "风力",
        .today_weather_format = "   今日天气:%s, %s风%s级.              "
    },
    // Traditional Chinese
    {
        .view_more_weather = "查看更多天氣",
        .wind_power = "風力",
        .today_weather_format = "   今日天氣:%s, %s風%s級.              "
    }
};

void weather_gui_init(void)
{
    // app_init re-runs this GUI init on every app entry; static styles must
    // only be initialised once - lv_style_init() on an already-inited style
    // leaks its property array (LVGL "Potential memory leak").
    static bool style_inited = false;
    if (style_inited)
    {
        return;
    }
    style_inited = true;

    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));

    lv_style_init(&chFont_style);
    lv_style_set_text_opa(&chFont_style, LV_OPA_COVER);
    lv_style_set_text_color(&chFont_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&chFont_style, &ch_font20);

    lv_style_init(&numberSmall_style);
    lv_style_set_text_opa(&numberSmall_style, LV_OPA_COVER);
    lv_style_set_text_color(&numberSmall_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&numberSmall_style, &lv_font_ibmplex_64);

    lv_style_init(&numberBig_style);
    lv_style_set_text_opa(&numberBig_style, LV_OPA_COVER);
    lv_style_set_text_color(&numberBig_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&numberBig_style, &lv_font_ibmplex_115);

    lv_style_init(&btn_style);
    lv_style_set_border_width(&btn_style, 0);
    lv_style_init(&bar_style);
    lv_style_set_bg_color(&bar_style, lv_color_hex(0x000000));
    lv_style_set_border_width(&bar_style, 2);
    lv_style_set_border_color(&bar_style, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_top(&bar_style, 1); // 指示器到背景四周的距离
    lv_style_set_pad_bottom(&bar_style, 1);
    lv_style_set_pad_left(&bar_style, 1);
    lv_style_set_pad_right(&bar_style, 1);
}

void display_curve_init(lv_scr_load_anim_t anim_type)
{
    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == scr_2)
        return;

    weather_gui_release();
    lv_obj_clean(act_obj); // 清空此前页面

    scr_2 = lv_obj_create(NULL);
    lv_obj_add_style(scr_2, &default_style, LV_STATE_DEFAULT);

    titleLabel = lv_label_create(scr_2);
    lv_obj_add_style(titleLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(titleLabel, true);
    lv_label_set_text(titleLabel, ui_strings[current_language].view_more_weather);

    chart = lv_chart_create(scr_2);
    lv_obj_set_size(chart, 220, 180);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -50, 50); // 设置进度条表示的温度为-50~50
    lv_chart_set_point_count(chart, FORECAST_DAYS);
    lv_chart_set_div_line_count(chart, 5, FORECAST_DAYS);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); /*Show lines and points too*/

    ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_SECONDARY_Y);
    ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_SECONDARY_Y);

    // lv_obj_set_style_pad_left(chart, 40, LV_STATE_DEFAULT);

    // 设置Y轴上刻度线的数量
    // lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y,
    //                        10, 5, 10, 2, true, 20);
    // lv_chart_set_zoom_y();

    // 绘制
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 10);

    if (LV_SCR_LOAD_ANIM_NONE != anim_type)
    {
        lv_scr_load_anim(scr_2, anim_type, 300, 300, false);
    }
    else
    {
        lv_scr_load(scr_2);
    }
}

void display_curve(short maxT[], short minT[], lv_scr_load_anim_t anim_type)
{
    display_curve_init(anim_type);
    for (int Ti = 0; Ti < FORECAST_DAYS; ++Ti)
    {
        lv_chart_set_series_value_by_id(chart, ser1, Ti, maxT[Ti] + 50); // 补偿50度
    }
    for (int Ti = 0; Ti < FORECAST_DAYS; ++Ti)
    {
        lv_chart_set_series_value_by_id(chart, ser2, Ti, minT[Ti] + 50); // 补偿50度
    }
    lv_chart_refresh(chart);
}

void display_weather_init(lv_scr_load_anim_t anim_type)
{
    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == scr_1)
        return;

    weather_gui_release();
    lv_obj_clean(act_obj); // 清空此前页面

    scr_1 = lv_obj_create(NULL);
    lv_obj_add_style(scr_1, &default_style, LV_STATE_DEFAULT);

    weatherImg = lv_img_create(scr_1);
    lv_img_set_src(weatherImg, weaImage_map[0]);

    cityLabel = lv_label_create(scr_1);
    lv_obj_add_style(cityLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(cityLabel, true);
    lv_label_set_text(cityLabel, "上海");

    airQualityBtn = lv_btn_create(scr_1);
    lv_obj_add_style(airQualityBtn, &btn_style, LV_STATE_DEFAULT);
    lv_obj_set_pos(airQualityBtn, 90, 15);
    lv_obj_set_size(airQualityBtn, 50, 25);
    lv_obj_set_style_bg_color(airQualityBtn, lv_palette_main(LV_PALETTE_ORANGE), LV_STATE_DEFAULT);

    airQualityLabel = lv_label_create(airQualityBtn);
    lv_obj_add_style(airQualityLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_obj_align(airQualityLabel, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(airQualityLabel, airQualityCh[current_language][0]);

    txtLabel = lv_label_create(scr_1);
    lv_obj_add_style(txtLabel, &chFont_style, LV_STATE_DEFAULT);
    // lvgl8之前版本，模式一旦设置 LV_LABEL_LONG_SCROLL_CIRCULAR
    // 宽度恒定等于当前文本的长度，所以下面先设置以下长度
    lv_label_set_text(txtLabel, current_language == LANG_SIMPLIFIED_CHINESE ? "最低气温12°C, " : "最低氣溫12°C, ");
    lv_obj_set_size(txtLabel, 120, 30);
    lv_label_set_long_mode(txtLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text_fmt(txtLabel, current_language == LANG_SIMPLIFIED_CHINESE ? "晴天, %s 0 级.   " : "晴天, %s 0 級.   ", 
                          ui_strings[current_language].wind_power);
    // lv_label_set_text_fmt(txtLabel, "最低氣溫%d°C, 最高氣溫%d°C, %s%d 級.   ", 15, 20, "西北風", 0);

    clockLabel_1 = lv_label_create(scr_1);
    lv_obj_add_style(clockLabel_1, &numberBig_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(clockLabel_1, true);
    lv_label_set_text_fmt(clockLabel_1, "%02d#ffa500 %02d#", 10, 52);

    clockLabel_2 = lv_label_create(scr_1);
    lv_obj_add_style(clockLabel_2, &numberSmall_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(clockLabel_2, true);
    lv_label_set_text_fmt(clockLabel_2, "%02d", 00);

    dateLabel = lv_label_create(scr_1);
    lv_obj_add_style(dateLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_label_set_text_fmt(dateLabel, "%2d月%2d日   周%s", 11, 23, weekDayCh[current_language][1]);

    tempImg = lv_img_create(scr_1);
    lv_img_set_src(tempImg, &temp);
    lv_img_set_zoom(tempImg, 180);
    tempBar = lv_bar_create(scr_1);
    lv_obj_add_style(tempBar, &bar_style, LV_STATE_DEFAULT);
    lv_bar_set_range(tempBar, -50, 50); // 设置进度条表示的温度为-50~50
    lv_obj_set_size(tempBar, 60, 12);
    lv_obj_set_style_bg_color(tempBar, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_bar_set_value(tempBar, 10, LV_ANIM_ON);
    tempLabel = lv_label_create(scr_1);
    lv_obj_add_style(tempLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_label_set_text_fmt(tempLabel, "%2d°C", 18);

    humiImg = lv_img_create(scr_1);
    lv_img_set_src(humiImg, &humi);
    lv_img_set_zoom(humiImg, 180);
    humiBar = lv_bar_create(scr_1);
    lv_obj_add_style(humiBar, &bar_style, LV_STATE_DEFAULT);
    lv_bar_set_range(humiBar, 0, 100);
    lv_obj_set_size(humiBar, 60, 12);
    lv_obj_set_style_bg_color(humiBar, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_bar_set_value(humiBar, 49, LV_ANIM_ON);
    humiLabel = lv_label_create(scr_1);
    lv_obj_add_style(humiLabel, &chFont_style, LV_STATE_DEFAULT);
    lv_label_set_text(humiLabel, "50%");

    // 太空人图标
    spaceImg = lv_img_create(scr_1);
    lv_img_set_src(spaceImg, manImage_map[0]);

    // 绘制图形
    lv_obj_align(weatherImg, LV_ALIGN_TOP_RIGHT, -10, 10);
    // lv_obj_align(cityLabel, LV_ALIGN_TOP_LEFT, 20, 15);
    lv_obj_align(cityLabel, LV_ALIGN_TOP_LEFT, 20, 25);
    lv_obj_align(txtLabel, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_align(tempImg, LV_ALIGN_LEFT_MID, 10, 70);
    lv_obj_align(tempBar, LV_ALIGN_LEFT_MID, 35, 70);
    lv_obj_align(tempLabel, LV_ALIGN_LEFT_MID, 103, 70);
    lv_obj_align(humiImg, LV_ALIGN_LEFT_MID, 0, 100);
    lv_obj_align(humiBar, LV_ALIGN_LEFT_MID, 35, 100);
    lv_obj_align(humiLabel, LV_ALIGN_LEFT_MID, 103, 100);
    lv_obj_align(spaceImg, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    lv_obj_align(clockLabel_1, LV_ALIGN_LEFT_MID, 0, 10);
    lv_obj_align(clockLabel_2, LV_ALIGN_LEFT_MID, 165, 9);
    // lv_obj_align(dateLabel, LV_ALIGN_LEFT_MID, 10, 32);
    lv_obj_align(dateLabel, LV_ALIGN_LEFT_MID, 10, 38);

    // if (LV_SCR_LOAD_ANIM_NONE != anim_type)
    // {
    //     lv_scr_load_anim(scr_1, anim_type, 300, 300, false);
    // }
    // else
    // {
    // lv_scr_load(scr_1);
    // }
}

void display_weather(struct Weather weaInfo, lv_scr_load_anim_t anim_type)
{
    display_weather_init(anim_type);

    lv_label_set_text(cityLabel, weaInfo.cityname);
    if (strlen(weaInfo.cityname) > 6)
    {
        lv_obj_align(cityLabel, LV_ALIGN_TOP_LEFT, 5, 15);
    }
    else
    {
        lv_obj_align(cityLabel, LV_ALIGN_TOP_LEFT, 20, 15);
    }
    lv_label_set_text(airQualityLabel, airQualityCh[current_language][weaInfo.airQulity]);
    lv_img_set_src(weatherImg, weaImage_map[weaInfo.weather_code]);
    // 下面这行代码可能会出错
    // lv_label_set_text_fmt(txtLabel, "最低氣溫%d°C, 最高氣溫%d°C, %s%d 級.   ",
    //                       weaInfo.minTemp, weaInfo.maxTemp, weaInfo.windDir, weaInfo.windLevel);
    lv_label_set_text_fmt(txtLabel, ui_strings[current_language].today_weather_format,
                          weaInfo.weather, weaInfo.windDir, weaInfo.windpower);

    lv_bar_set_value(tempBar, weaInfo.temperature, LV_ANIM_ON);
    lv_label_set_text_fmt(tempLabel, "%2d°C", weaInfo.temperature);
    lv_bar_set_value(humiBar, weaInfo.humidity, LV_ANIM_ON);
    lv_label_set_text_fmt(humiLabel, "%d%%", weaInfo.humidity);

    // // 绘制图形
    // lv_obj_align(weatherImg, NULL, LV_ALIGN_TOP_RIGHT, -10, 10);
    // lv_obj_align(cityLabel, NULL, LV_ALIGN_TOP_LEFT, 20, 15);
    // lv_obj_align(txtLabel, NULL, LV_ALIGN_TOP_LEFT, 0, 50);
    // lv_obj_align(tempImg, NULL, LV_ALIGN_LEFT_MID, 10, 70);
    // lv_obj_align(tempBar, NULL, LV_ALIGN_LEFT_MID, 35, 70);
    // lv_obj_align(tempLabel, NULL, LV_ALIGN_LEFT_MID, 100, 70);
    // lv_obj_align(humiImg, NULL, LV_ALIGN_LEFT_MID, 0, 100);
    // lv_obj_align(humiBar, NULL, LV_ALIGN_LEFT_MID, 35, 100);
    // lv_obj_align(humiLabel, NULL, LV_ALIGN_LEFT_MID, 100, 100);
    // lv_obj_align(spaceImg, NULL, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    if (LV_SCR_LOAD_ANIM_NONE != anim_type)
    {
        lv_scr_load_anim(scr_1, anim_type, 300, 300, false);
    }
    else
    {
        lv_scr_load(scr_1);
    }
}

void display_time(struct TimeStr timeInfo, lv_scr_load_anim_t anim_type)
{
    display_weather_init(anim_type);
    lv_label_set_text_fmt(clockLabel_1, "%02d#ffa500 %02d#", timeInfo.hour, timeInfo.minute);
    lv_label_set_text_fmt(clockLabel_2, "%02d", timeInfo.second);
    lv_label_set_text_fmt(dateLabel, "%2d月%2d日   周%s", timeInfo.month, timeInfo.day,
                          weekDayCh[current_language][timeInfo.weekday]);

    // lv_obj_align(clockLabel_1, NULL, LV_ALIGN_LEFT_MID, 0, 10);
    // lv_obj_align(clockLabel_2, NULL, LV_ALIGN_LEFT_MID, 165, 9);
    // lv_obj_align(dateLabel, NULL, LV_ALIGN_LEFT_MID, 10, 32);

    // if (LV_SCR_LOAD_ANIM_NONE != anim_type)
    // {
    //     lv_scr_load_anim(scr_1, anim_type, 300, 300, false);
    // }
    // else
    // {
    //     lv_scr_load(scr_1);
    // }
}

void weather_gui_release(void)
{
    if (scr_1 != NULL)
    {
        lv_obj_clean(scr_1);
        scr_1 = NULL;
        weatherImg = NULL;
        cityLabel = NULL;
        airQualityBtn = NULL;
        airQualityLabel = NULL;
        txtLabel = NULL;
        clockLabel_1 = NULL;
        clockLabel_2 = NULL;
        dateLabel = NULL;
        tempImg = NULL;
        tempBar = NULL;
        tempLabel = NULL;
        humiImg = NULL;
        humiBar = NULL;
        humiLabel = NULL;
        spaceImg = NULL;
    }

    if (scr_2 != NULL)
    {
        lv_obj_clean(scr_2);
        scr_2 = NULL;
        chart = NULL;
        titleLabel = NULL;
        ser1 = NULL;
        ser2 = NULL;
    }
}

void weather_gui_del(void)
{
    weather_gui_release();

    // 手动清除样式，防止内存泄漏
    // lv_style_reset(&default_style);
    // lv_style_reset(&chFont_style);
    // lv_style_reset(&numberSmall_style);
    // lv_style_reset(&numberBig_style);
    // lv_style_reset(&btn_style);
    // lv_style_reset(&bar_style);
}

void display_space(void)
{
    static int _spaceIndex = 0;
    if (NULL != scr_1 && lv_scr_act() == scr_1)
    {
        lv_img_set_src(spaceImg, manImage_map[_spaceIndex]);
        _spaceIndex = (_spaceIndex + 1) % 10;
    }
}

int my_isdigit(char c)
{
    return (c >= '0' && c <= '9');
}

int extractNumbers(const char *str)
{
    int result = 0;
    int i;
    int len = strlen(str);
    for (i = 0; i < len; i++)
    {
        if (my_isdigit(str[i]))
        {
            result = result * 10 + (str[i] - '0');
        }
    }
    return result;
}

// int airQulityLevel(int q)
// {
//     if (q < 50)
//     {
//         return 0;
//     }
//     else if (q < 100)
//     {
//         return 1;
//     }
//     else if (q < 150)
//     {
//         return 2;
//     }
//     else if (q < 200)
//     {
//         return 3;
//     }
//     else if (q < 300)
//     {
//         return 4;
//     }
//     return 5;
// }

int airQulityLevel(char *q)
{
    int number = extractNumbers(q);
    if (number <= 3)
    {
        return 0;
    }
    else if (number <= 4)
    {
        return 1;
    }
    else if (number <= 6)
    {
        return 2;
    }
    else if (number <= 8)
    {
        return 3;
    }
    else if (number <= 9)
    {
        return 4;
    }
    return 5;
}

// Language management functions
void weather_set_language(WeatherLanguage lang)
{
    if (lang >= LANG_SIMPLIFIED_CHINESE && lang <= LANG_TRADITIONAL_CHINESE)
    {
        current_language = lang;
    }
}

WeatherLanguage weather_get_language(void)
{
    return current_language;
}

void weather_toggle_language(void)
{
    if (current_language == LANG_SIMPLIFIED_CHINESE)
    {
        current_language = LANG_TRADITIONAL_CHINESE;
    }
    else
    {
        current_language = LANG_SIMPLIFIED_CHINESE;
    }
}

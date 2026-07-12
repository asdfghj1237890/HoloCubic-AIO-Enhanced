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

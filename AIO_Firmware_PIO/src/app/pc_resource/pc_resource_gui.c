#include "pc_resource_gui.h"
#include "pc_resource_fmt.h"
#include <string.h>

// Instrument-family tokens — mirrors stockmarket_gui.c
#define PCR_COLOR_BG 0x000000
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
    lv_style_set_bg_color(&default_style, lv_color_hex(PCR_COLOR_BG));

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

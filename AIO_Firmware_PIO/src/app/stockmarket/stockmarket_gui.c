#include "stockmarket_gui.h"

#include "lvgl.h"

LV_FONT_DECLARE(ch_font20);

static lv_obj_t *stockmarket_gui = NULL;

static lv_obj_t *header_label   = NULL;  // "AAPL - Apple Inc."
static lv_obj_t *divider_top    = NULL;  // lv_line, y=40
static lv_obj_t *price_label    = NULL;  // big price, e.g. "175.50"
static lv_obj_t *arrow_img      = NULL;  // up.png / down.png
static lv_obj_t *chg_pct_label  = NULL;  // "+1.33%"
static lv_obj_t *chg_value_label = NULL; // "+2.30"
static lv_obj_t *divider_bot    = NULL;  // lv_line, y=168
static lv_obj_t *hi_lo_label    = NULL;  // "H 176.20  L 173.42"
static lv_obj_t *close_label    = NULL;  // "C 174.18"

static lv_style_t default_style;
static lv_style_t header_style;
static lv_style_t price_style;
static lv_style_t change_style;
static lv_style_t secondary_style;

void stockmarket_gui_init(void)
{
    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));

    lv_style_init(&header_style);
    lv_style_set_text_opa(&header_style, LV_OPA_COVER);
    lv_style_set_text_color(&header_style, lv_color_hex(0xffb84d));
    lv_style_set_text_font(&header_style, &ch_font20);

    lv_style_init(&price_style);
    lv_style_set_text_opa(&price_style, LV_OPA_COVER);
    lv_style_set_text_font(&price_style, &lv_font_montserrat_48);
    // color set per-call in display_stockmarket

    lv_style_init(&change_style);
    lv_style_set_text_opa(&change_style, LV_OPA_COVER);
    lv_style_set_text_font(&change_style, &lv_font_montserrat_20);
    // color set per-call in display_stockmarket

    lv_style_init(&secondary_style);
    lv_style_set_text_opa(&secondary_style, LV_OPA_COVER);
    lv_style_set_text_color(&secondary_style, lv_color_hex(0xaaaaaa));
    lv_style_set_text_font(&secondary_style, &lv_font_montserrat_20);
}

static lv_point_t divider_points[] = {{0, 0}, {240, 0}};

void display_stockmarket_init(void)
{
    lv_obj_t *act_obj = lv_scr_act();

    if (stockmarket_gui != NULL)
    {
        return;
    }

    stockmarket_gui_del();
    lv_obj_clean(act_obj);

    stockmarket_gui = lv_obj_create(NULL);
    lv_obj_add_style(stockmarket_gui, &default_style, LV_STATE_DEFAULT);

    // Header (gold, top-left)
    header_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(header_label, &header_style, LV_STATE_DEFAULT);
    lv_label_set_long_mode(header_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(header_label, 224);
    lv_label_set_text(header_label, "--");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, 8, 8);

    // Top divider (gold-dim, y=40, full width)
    divider_top = lv_line_create(stockmarket_gui);
    lv_line_set_points(divider_top, divider_points, 2);
    lv_obj_set_style_line_color(divider_top, lv_color_hex(0x806124), LV_PART_MAIN);
    lv_obj_set_style_line_width(divider_top, 1, LV_PART_MAIN);
    lv_obj_align(divider_top, LV_ALIGN_TOP_LEFT, 0, 40);

    // Price (big, color set per-call)
    price_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(price_label, &price_style, LV_STATE_DEFAULT);
    lv_label_set_text(price_label, "0.00");
    lv_obj_align(price_label, LV_ALIGN_TOP_LEFT, 12, 60);

    // Arrow image
    arrow_img = lv_img_create(stockmarket_gui);
    lv_obj_align(arrow_img, LV_ALIGN_TOP_LEFT, 168, 72);

    // Change % (color set per-call) — to the right of arrow, upper
    chg_pct_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(chg_pct_label, &change_style, LV_STATE_DEFAULT);
    lv_label_set_text(chg_pct_label, "+0.00%");
    lv_obj_align(chg_pct_label, LV_ALIGN_TOP_LEFT, 200, 70);

    // Change absolute value — below change %
    chg_value_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(chg_value_label, &change_style, LV_STATE_DEFAULT);
    lv_label_set_text(chg_value_label, "+0.00");
    lv_obj_align(chg_value_label, LV_ALIGN_TOP_LEFT, 200, 102);

    // Bottom divider (grey, y=168)
    divider_bot = lv_line_create(stockmarket_gui);
    lv_line_set_points(divider_bot, divider_points, 2);
    lv_obj_set_style_line_color(divider_bot, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_line_width(divider_bot, 1, LV_PART_MAIN);
    lv_obj_align(divider_bot, LV_ALIGN_TOP_LEFT, 0, 168);

    // High / Low row
    hi_lo_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(hi_lo_label, &secondary_style, LV_STATE_DEFAULT);
    lv_label_set_text(hi_lo_label, "H 0.00  L 0.00");
    lv_obj_align(hi_lo_label, LV_ALIGN_TOP_LEFT, 12, 184);

    // Previous Close row
    close_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(close_label, &secondary_style, LV_STATE_DEFAULT);
    lv_label_set_text(close_label, "C 0.00");
    lv_obj_align(close_label, LV_ALIGN_TOP_LEFT, 12, 210);

    lv_scr_load(stockmarket_gui);
}

/*
 * Add other functions as needed
 */

void display_stockmarket(struct StockMarket stockInfo, lv_scr_load_anim_t anim_type)
{
    if (stockmarket_gui == NULL)
    {
        display_stockmarket_init();
    }

    // Header
    if (stockInfo.symbol[0] == '\0')
    {
        lv_label_set_text(header_label, "--");
    }
    else if (stockInfo.company[0] == '\0')
    {
        lv_label_set_text(header_label, stockInfo.symbol);
    }
    else
    {
        lv_label_set_text_fmt(header_label, "%s - %s",
                              stockInfo.symbol, stockInfo.company);
    }

    // Direction color (Taiwan/HK convention: green = up, red = down)
    lv_color_t dir_color = (stockInfo.updownflag == 1)
        ? lv_color_hex(0x22c55e)
        : lv_color_hex(0xef4444);
    lv_obj_set_style_text_color(price_label,     dir_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(chg_pct_label,   dir_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(chg_value_label, dir_color, LV_PART_MAIN);
    lv_img_set_src(arrow_img, stockInfo.updownflag == 1 ? &up : &down);

    // Numeric values
    lv_label_set_text_fmt(price_label,     "%.2f",    stockInfo.NowQuo);
    lv_label_set_text_fmt(chg_pct_label,   "%+.2f%%", stockInfo.ChgPercent);
    lv_label_set_text_fmt(chg_value_label, "%+.2f",   stockInfo.ChgValue);
    lv_label_set_text_fmt(hi_lo_label, "H %.2f  L %.2f",
                          stockInfo.MaxQuo, stockInfo.MinQuo);
    lv_label_set_text_fmt(close_label, "C %.2f", stockInfo.CloseQuo);
}

void stockmarket_gui_del(void)
{
    if (NULL != stockmarket_gui)
    {
        // lv_obj_clean (not lv_obj_del) matches every other app's _gui_del:
        // app_exit calls this before app_control_display_scr loads the next
        // screen, so deleting the active screen here would null out
        // disp->act_scr and the next refresh tick segfaults in
        // lv_obj_update_layout. Cleaning leaves the screen object alive.
        lv_obj_clean(stockmarket_gui);
        stockmarket_gui   = NULL;
        header_label      = NULL;
        divider_top       = NULL;
        price_label       = NULL;
        arrow_img         = NULL;
        chg_pct_label     = NULL;
        chg_value_label   = NULL;
        divider_bot       = NULL;
        hi_lo_label       = NULL;
        close_label       = NULL;
    }
}

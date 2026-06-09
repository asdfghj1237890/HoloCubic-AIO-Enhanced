#include "stockmarket_gui.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(ch_font20);
LV_FONT_DECLARE(lv_font_ibmplex_64);

static lv_obj_t *stockmarket_gui = NULL;

static lv_obj_t *header_label   = NULL;  // "AAPL - Apple Inc."
static lv_obj_t *divider_top    = NULL;  // lv_line, y=40
static lv_obj_t *arrow_head_label = NULL; // large LV_SYMBOL_UP/DOWN
static lv_obj_t *arrow_shaft      = NULL; // filled shaft rectangle
static lv_obj_t *price_int_label = NULL; // big price integer, e.g. "175"
static lv_obj_t *price_dec_label = NULL; // smaller price decimal, e.g. ".50"
static lv_obj_t *chg_pct_label  = NULL;  // "+1.33%", emphasized
static lv_obj_t *chg_value_label = NULL; // "+2.30", visually paired with %
static lv_obj_t *divider_bot    = NULL;  // lv_line, y=166
static lv_obj_t *hi_label       = NULL;  // "H 176.50" — left half of H/L row
static lv_obj_t *lo_label       = NULL;  // "L 173.00" — right half of H/L row
static lv_obj_t *hi_lo_divider  = NULL;  // vertical line between H and L
static lv_obj_t *close_label    = NULL;  // "C 174.18"
static lv_obj_t *datetime_label = NULL;  // "06-09 15:30"
static lv_obj_t *col_divider    = NULL;  // vertical line between C and datetime
                                          // (column-aligned with hi_lo_divider)

static lv_style_t default_style;
static lv_style_t header_style;
static lv_style_t arrow_style;
static lv_style_t price_int_style;
static lv_style_t price_dec_style;
static lv_style_t change_style;
static lv_style_t change_value_style;
static lv_style_t secondary_style;
static lv_style_t datetime_style;

#if LV_FONT_MONTSERRAT_48
#define STOCKMARKET_ARROW_FONT (&lv_font_montserrat_48)
#elif LV_FONT_MONTSERRAT_40
#define STOCKMARKET_ARROW_FONT (&lv_font_montserrat_40)
#else
#define STOCKMARKET_ARROW_FONT LV_FONT_DEFAULT
#endif

void stockmarket_gui_init(void)
{
    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));

    lv_style_init(&header_style);
    lv_style_set_text_opa(&header_style, LV_OPA_COVER);
    lv_style_set_text_color(&header_style, lv_color_hex(0xffd000));
    lv_style_set_text_font(&header_style, &ch_font20);

    lv_style_init(&arrow_style);
    lv_style_set_text_opa(&arrow_style, LV_OPA_COVER);
    lv_style_set_text_font(&arrow_style, STOCKMARKET_ARROW_FONT);
    // color set per-call in display_stockmarket

    lv_style_init(&price_int_style);
    lv_style_set_text_opa(&price_int_style, LV_OPA_COVER);
    lv_style_set_text_font(&price_int_style, &lv_font_ibmplex_64);
    // color set per-call in display_stockmarket

    lv_style_init(&price_dec_style);
    lv_style_set_text_opa(&price_dec_style, LV_OPA_COVER);
    lv_style_set_text_font(&price_dec_style, &lv_font_montserrat_30);
    // color set per-call in display_stockmarket

    lv_style_init(&change_style);
    lv_style_set_text_opa(&change_style, LV_OPA_COVER);
    lv_style_set_text_font(&change_style, &lv_font_montserrat_30);
    // color set per-call in display_stockmarket

    lv_style_init(&change_value_style);
    lv_style_set_text_opa(&change_value_style, LV_OPA_COVER);
    lv_style_set_text_font(&change_value_style, &lv_font_montserrat_24);
    // color set per-call in display_stockmarket

    lv_style_init(&secondary_style);
    lv_style_set_text_opa(&secondary_style, LV_OPA_COVER);
    lv_style_set_text_color(&secondary_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&secondary_style, &lv_font_montserrat_24);

    lv_style_init(&datetime_style);
    lv_style_set_text_opa(&datetime_style, LV_OPA_COVER);
    lv_style_set_text_color(&datetime_style, lv_color_hex(0xffffff));
    lv_style_set_text_font(&datetime_style, &lv_font_montserrat_14);
}

static const lv_point_t divider_points[] = {{0, 0}, {239, 0}};

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
    lv_obj_set_width(header_label, 216);
    lv_label_set_text(header_label, "--");
    lv_obj_align(header_label, LV_ALIGN_TOP_LEFT, 12, 8);

    // Top divider (gold-dim, y=40, full width)
    divider_top = lv_line_create(stockmarket_gui);
    lv_line_set_points(divider_top, divider_points, 2);
    lv_obj_set_style_line_color(divider_top, lv_color_hex(0xc89030), LV_PART_MAIN);
    lv_obj_set_style_line_width(divider_top, 2, LV_PART_MAIN);
    lv_obj_align(divider_top, LV_ALIGN_TOP_LEFT, 0, 40);

    // Direction arrow (big, color set per-call)
    arrow_head_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(arrow_head_label, &arrow_style, LV_STATE_DEFAULT);
    lv_label_set_text(arrow_head_label, LV_SYMBOL_UP);
    lv_obj_align(arrow_head_label, LV_ALIGN_TOP_LEFT, 12, 54);

    arrow_shaft = lv_obj_create(stockmarket_gui);
    lv_obj_set_size(arrow_shaft, 6, 44);
    lv_obj_set_style_radius(arrow_shaft, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(arrow_shaft, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arrow_shaft, 0, LV_PART_MAIN);

    // Price (big integer + smaller decimal, color set per-call)
    price_int_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(price_int_label, &price_int_style, LV_STATE_DEFAULT);
    lv_label_set_text(price_int_label, "0");
    lv_obj_align(price_int_label, LV_ALIGN_TOP_LEFT, 72, 54);

    price_dec_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(price_dec_label, &price_dec_style, LV_STATE_DEFAULT);
    lv_label_set_text(price_dec_label, ".00");
    // ibmplex_64's bounding box has noticeable descender padding below the
    // glyph baseline; LV_ALIGN_OUT_RIGHT_BOTTOM aligns boxes, not baselines,
    // so dec ends up visually below the integer's bottom. Negative y_offset
    // lifts dec up so its glyph bottom matches the integer's glyph bottom.
    lv_obj_align_to(price_dec_label, price_int_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -28);

    // Change % (color set per-call) — left-aligned on the change row
    chg_pct_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(chg_pct_label, &change_style, LV_STATE_DEFAULT);
    lv_label_set_text(chg_pct_label, "+0.00%");
    lv_obj_align(chg_pct_label, LV_ALIGN_TOP_LEFT, 52, 128);

    // Change absolute value — right-aligned, bottom-aligned with chg_pct.
    // chg_pct is mont_30 (~32px tall) at y=128 -> bottom y=160; chg_value is mont_24 (~26px tall) so its top sits at y=134 to share that bottom edge.
    // X nudged left so it visually sits closer to chg_pct.
    chg_value_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(chg_value_label, &change_value_style, LV_STATE_DEFAULT);
    lv_label_set_text(chg_value_label, "+0.00");
    lv_obj_align(chg_value_label, LV_ALIGN_TOP_RIGHT, -12, 134);

    // Bottom divider (grey, y=166)
    divider_bot = lv_line_create(stockmarket_gui);
    lv_line_set_points(divider_bot, divider_points, 2);
    lv_obj_set_style_line_color(divider_bot, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_line_width(divider_bot, 2, LV_PART_MAIN);
    lv_obj_align(divider_bot, LV_ALIGN_TOP_LEFT, 0, 166);

    // Column divider geometry — 2x24 line, same for both H/L row and C row
    // so the two vertical lines stack into one continuous column rail at
    // x=128. White to match the inline `|` users expected from the prior
    // single-label H/L row.
    static const lv_point_t col_divider_points[] = {{0, 0}, {0, 24}};

    // High row — left half of H/L row
    hi_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(hi_label, &secondary_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(hi_label, true);
    lv_label_set_text(hi_label, "#ffd000 H# 0.00");
    lv_obj_align(hi_label, LV_ALIGN_TOP_LEFT, 12, 172);

    // H/L vertical divider
    hi_lo_divider = lv_line_create(stockmarket_gui);
    lv_line_set_points(hi_lo_divider, col_divider_points, 2);
    lv_obj_set_style_line_color(hi_lo_divider, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_line_width(hi_lo_divider, 2, LV_PART_MAIN);
    lv_obj_align(hi_lo_divider, LV_ALIGN_TOP_LEFT, 128, 172);

    // Low row — right half of H/L row, starts just right of the divider
    lo_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(lo_label, &secondary_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(lo_label, true);
    lv_label_set_text(lo_label, "#ffd000 L# 0.00");
    lv_obj_align(lo_label, LV_ALIGN_TOP_LEFT, 140, 172);

    // Previous Close row — left half
    close_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(close_label, &secondary_style, LV_STATE_DEFAULT);
    lv_label_set_recolor(close_label, true);
    lv_label_set_text(close_label, "#ffd000 C# 0.00");
    lv_obj_align(close_label, LV_ALIGN_TOP_LEFT, 12, 200);

    // Vertical divider between C value (left) and datetime (right) — same
    // x=128 as hi_lo_divider so the two stack into one continuous rail.
    col_divider = lv_line_create(stockmarket_gui);
    lv_line_set_points(col_divider, col_divider_points, 2);
    lv_obj_set_style_line_color(col_divider, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_line_width(col_divider, 2, LV_PART_MAIN);
    lv_obj_align(col_divider, LV_ALIGN_TOP_LEFT, 128, 200);

    // Last update datetime — left-aligned at x=140 (just right of col_divider).
    // Format is the compact MM-DD HH:MM (11 chars) populated by
    // update_stock_data via getTime("%m-%d %H:%M") after NTP bootstrap.
    datetime_label = lv_label_create(stockmarket_gui);
    lv_obj_add_style(datetime_label, &datetime_style, LV_STATE_DEFAULT);
    lv_label_set_text(datetime_label, "--");
    // mont_14 (~16px) bottom-aligned with mont_24 close (bottom ~y=226):
    // datetime top = 226 - 16 = 210.
    lv_obj_align(datetime_label, LV_ALIGN_TOP_LEFT, 140, 210);

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
        ? lv_color_hex(0x00ff44)
        : lv_color_hex(0xff2020);
    const char *dir_symbol = (stockInfo.updownflag == 1) ? LV_SYMBOL_UP : LV_SYMBOL_DOWN;
    lv_label_set_text(arrow_head_label, dir_symbol);
    lv_obj_set_style_text_color(arrow_head_label, dir_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(arrow_shaft, dir_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arrow_shaft, LV_OPA_COVER, LV_PART_MAIN);

    if (stockInfo.updownflag == 1) {
        // Up: head on top, shaft auto-centered below.
        lv_obj_align(arrow_head_label, LV_ALIGN_TOP_LEFT, 12, 54);
        lv_obj_update_layout(arrow_head_label);
        lv_obj_align_to(arrow_shaft, arrow_head_label, LV_ALIGN_OUT_BOTTOM_MID, 0, -32);
    } else {
        // Down: shaft on top, head auto-centered below it.
        lv_obj_align(arrow_shaft, LV_ALIGN_TOP_LEFT, 0, 54);
        lv_obj_align(arrow_head_label, LV_ALIGN_TOP_LEFT, 12, 92);
        lv_obj_update_layout(arrow_head_label);
        lv_obj_align_to(arrow_shaft, arrow_head_label, LV_ALIGN_OUT_TOP_MID, 0, 32);
    }

    lv_obj_set_style_text_color(price_int_label, dir_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(price_dec_label, dir_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(chg_pct_label,   dir_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(chg_value_label, dir_color, LV_PART_MAIN);

    // Numeric values
    char price_buf[16];
    snprintf(price_buf, sizeof(price_buf), "%.2f", stockInfo.NowQuo);
    char *dot = strchr(price_buf, '.');
    if (dot) {
        *dot = '\0';
        lv_label_set_text(price_int_label, price_buf);
        char dec_buf[8];
        snprintf(dec_buf, sizeof(dec_buf), ".%s", dot + 1);
        lv_label_set_text(price_dec_label, dec_buf);
    } else {
        lv_label_set_text(price_int_label, price_buf);
        lv_label_set_text(price_dec_label, ".00");
    }
    lv_obj_update_layout(price_int_label);
    // ibmplex_64's bounding box has noticeable descender padding below the
    // glyph baseline; LV_ALIGN_OUT_RIGHT_BOTTOM aligns boxes, not baselines,
    // so dec ends up visually below the integer's bottom. Negative y_offset
    // lifts dec up so its glyph bottom matches the integer's glyph bottom.
    lv_obj_align_to(price_dec_label, price_int_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -28);
    lv_label_set_text_fmt(chg_pct_label,   "%+.2f%%", stockInfo.ChgPercent);
    lv_label_set_text_fmt(chg_value_label, "%+.2f",   stockInfo.ChgValue);
    lv_label_set_text_fmt(hi_label, "#ffd000 H# %.2f", stockInfo.MaxQuo);
    lv_label_set_text_fmt(lo_label, "#ffd000 L# %.2f", stockInfo.MinQuo);
    lv_label_set_text_fmt(close_label, "#ffd000 C# %.2f", stockInfo.CloseQuo);
    lv_label_set_text(datetime_label,
                      stockInfo.datetime_str[0] != '\0' ? stockInfo.datetime_str : "--");
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
        arrow_head_label  = NULL;
        arrow_shaft       = NULL;
        price_int_label   = NULL;
        price_dec_label   = NULL;
        chg_pct_label     = NULL;
        chg_value_label   = NULL;
        divider_bot       = NULL;
        hi_label          = NULL;
        lo_label          = NULL;
        hi_lo_divider     = NULL;
        close_label       = NULL;
        col_divider       = NULL;
        datetime_label    = NULL;
    }
}

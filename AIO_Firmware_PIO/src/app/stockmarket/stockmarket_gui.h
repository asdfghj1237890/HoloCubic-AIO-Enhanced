#ifndef APP_STOCKMARKET_GUI_H
#define APP_STOCKMARKET_GUI_H


struct StockMarket
{
    float OpenQuo;              //今日开盘价
    float CloseQuo;             //昨日收盘价
    float NowQuo;               //当前价
    float MaxQuo;               //今日最高价
    float MinQuo;               //今日最低价
    float ChgValue;             //涨跌幅
    float ChgPercent;           //涨跌率
    unsigned int updownflag;    //升降标志 1：上涨  0:下跌
    unsigned int color_rule;    //StockmarketColorRule: green-up or red-up
    char symbol[10];            //股票代号 (e.g. AAPL, 601126, 0700)
    char company[24];           //公司名 (CJK or Latin)
    char datetime_str[20];      //Last update time (YYYY-MM-DD HH:MM[:SS])
    float tradvolume;           //成交量 (kept; not currently displayed)
    float turnover;             //成交额 (kept; not currently displayed)
};

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#define ANIEND                      \
    while (lv_anim_count_running()) \
        lv_task_handler(); //等待动画完成

    void stockmarket_gui_init(void);
    void display_stockmarket(struct StockMarket stockInfo, lv_scr_load_anim_t anim_type);
    void stockmarket_obj_del(void);
    void stockmarket_gui_del(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
    extern const lv_img_dsc_t app_stockmarket;
    extern const lv_img_dsc_t stockmarket_logo_ico;
    extern const lv_img_dsc_t down;
    extern const lv_img_dsc_t up;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

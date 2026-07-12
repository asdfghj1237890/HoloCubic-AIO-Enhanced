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

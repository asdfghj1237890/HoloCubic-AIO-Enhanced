#ifndef APP_CONTROLLER_GUI_H
#define APP_CONTROLLER_GUI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

// ANIEND_WAIT — spin the LVGL pipeline until the in-flight launcher animations
// finish. Previously this was a bare `while (...) lv_task_handler();` macro,
// which did NOT take the global lvgl_mutex (added in ce6d0c1) while the
// background Display task's lv_timer_handler took it on every iteration.
// That race caused the launcher's slide-between-apps to leave the *whole*
// previous icon + label visible on screen — the lv_obj_del / lv_obj_set_x
// invalidations got eaten between the two tasks. Function lives in the .c
// so the lock + FreeRTOS includes don't leak through this header.
void aio_lvgl_aniend_wait(void);
#define ANIEND_WAIT aio_lvgl_aniend_wait();

    void app_control_gui_init(void);
    void app_control_gui_release(void);
    void display_app_scr_release(void);
    void display_app_scr_init(const void *src_img, const char *app_name);
    void app_control_display_scr(const void *src_img, const char *app_name,
                                 lv_scr_load_anim_t anim_type, bool force);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
    extern const lv_img_dsc_t app_loading;
    extern const lv_img_dsc_t app_loading1;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
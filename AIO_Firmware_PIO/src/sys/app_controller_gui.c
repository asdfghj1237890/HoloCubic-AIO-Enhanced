#include "app_controller_gui.h"
// #include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// The global LVGL mutex defined in common.h (extern there) and protected
// every other lv_task_handler / lv_timer_handler caller. We only re-declare
// it here so this C TU doesn't have to drag in common.h (which has C++
// types like `String` further down).
extern SemaphoreHandle_t lvgl_mutex;

#define LVGL_LOCK()   xSemaphoreTake(lvgl_mutex, portMAX_DELAY)
#define LVGL_UNLOCK() xSemaphoreGive(lvgl_mutex)

void aio_lvgl_aniend_wait(void)
{
    // Each iteration: take the lvgl mutex, advance one tick of the LVGL
    // pipeline (which drains pending invalidations + flushes via flush_cb),
    // release the mutex so the Display task can take its turn. vTaskDelay(1)
    // yields to lower-priority tasks instead of starving them via tight
    // mutex re-acquisition.
    while (lv_anim_count_running())
    {
        if (pdTRUE == LVGL_LOCK())
        {
            lv_task_handler();
            LVGL_UNLOCK();
        }
        vTaskDelay(1);
    }
}

// 必须定义为全局或者静态
static lv_obj_t *app_scr = NULL;
static lv_obj_t *app_scr_t = NULL;
static lv_obj_t *pre_app_image = NULL;
static lv_obj_t *pre_app_name = NULL;
static lv_obj_t *now_app_image = NULL;
static lv_obj_t *now_app_name = NULL;
const void *pre_img_path = NULL;

static lv_style_t default_style;
static lv_style_t app_name_style;

LV_FONT_DECLARE(lv_font_montserrat_24);

static void clear_launcher_widget_refs(void)
{
    pre_app_image = NULL;
    pre_app_name  = NULL;
    now_app_image = NULL;
    now_app_name  = NULL;
}

static void delete_launcher_obj_if_valid(lv_obj_t **obj)
{
    if (NULL == obj || NULL == *obj)
    {
        return;
    }

    if (lv_obj_is_valid(*obj))
    {
        lv_obj_del(*obj);
    }
    *obj = NULL;
}

void app_control_gui_init(void)
{
    // All LVGL state mutation here happens under the global mutex — the
    // Display task may already be running lv_timer_handler() in parallel
    // by this point in setup() (see HoloCubic_AIO.cpp). Without the lock
    // the style+object setup can race with a flush in flight and the
    // launcher screen comes up with stale framebuffer content.
    LVGL_LOCK();
    if (NULL != app_scr)
    {
        lv_obj_clean(app_scr);
        app_scr = NULL;
        app_scr_t = NULL;
        clear_launcher_widget_refs();
    }

    lv_style_init(&default_style);
    lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));
    lv_style_set_radius(&default_style, 0); // 设置控件圆角半径
    // 设置边框宽度
    lv_style_set_border_width(&default_style, 0);

    lv_style_init(&app_name_style);
    lv_style_set_text_opa(&app_name_style, LV_OPA_COVER);
    lv_style_set_text_color(&app_name_style, lv_color_white());
    lv_style_set_text_font(&app_name_style, &lv_font_montserrat_24);

    // APP图标页
    app_scr = lv_obj_create(NULL);
    lv_obj_add_style(app_scr, &default_style, LV_STATE_DEFAULT);
    // 设置不显示滚动条
    lv_obj_set_style_bg_opa(app_scr, LV_OPA_0,
                            LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    // lv_obj_set_size(app_scr, 240, 240);
    // lv_obj_align(app_scr, LV_ALIGN_CENTER, 0, 0);
    // lv_scr_load(app_scr);

    // 为消除开机的局部白屏问题 增加如下一层（可考虑删除改进）
    app_scr_t = lv_obj_create(app_scr);
    lv_obj_add_style(app_scr_t, &default_style, LV_STATE_DEFAULT);
    lv_obj_set_size(app_scr_t, 240, 240);
    lv_obj_align(app_scr_t, LV_ALIGN_CENTER, 0, 0);
    lv_scr_load(app_scr_t);
    LVGL_UNLOCK();
}

void app_control_gui_release(void)
{
    LVGL_LOCK();
    if (NULL != app_scr)
    {
        lv_obj_clean(app_scr);
        app_scr = NULL;
        app_scr_t = NULL;
        clear_launcher_widget_refs();
    }
    LVGL_UNLOCK();
}

void display_app_scr_init(const void *src_img_path, const char *app_name)
{
    // Whole body under the mutex — touches lv_scr_act(), lv_obj_clean,
    // lv_img_create / lv_label_create / *_set_src / *_set_text / align /
    // lv_scr_load_anim. Any of those can corrupt the dirty-region list
    // or the active-screen pointer if the Display task is flushing.
    LVGL_LOCK();
    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == app_scr)
    {
        // 防止一些不适用lvgl的APP退出 造成画面在无其他动作情况下无法绘制更新
        lv_scr_load_anim(app_scr, LV_SCR_LOAD_ANIM_NONE, 300, 300, false);
        LVGL_UNLOCK();
        return;
    }

    lv_obj_clean(act_obj); // 清空此前页面 (the exited app's screen)

    // CRITICAL LEAK FIX (ghosting root cause): delete the previously-tracked
    // launcher widgets on app_scr before creating new ones.
    //
    // AppController::app_exit() calls app_control_display_scr with force=true,
    // which lands here. The original code overwrote pre_app_image /
    // pre_app_name with freshly-created widgets without freeing the previous
    // ones — one icon + label leaked per app entry/exit cycle, all stacked
    // at LV_ALIGN_CENTER on app_scr. The slide animation only deleted the
    // most recently tracked widget, so leaked ones accumulated forever as
    // an immortal background ghost (typically Stockmarket — the default
    // auto_start_app for fresh installs).
    //
    // Do NOT use lv_obj_clean(app_scr) — that would also delete app_scr_t
    // (the workaround child layer created in app_control_gui_init), which
    // is currently the lv_scr_load'd active screen on first boot; deleting
    // it crashes LVGL on the next render tick.
    //
    // After a slide, pre_app_image and now_app_image alias the same widget
    // (the slide animation does `pre_app_image = now_app_image`), so we
    // only delete via the pre_* pointers and NULL all four to clear any
    // dangling now_* alias.
    delete_launcher_obj_if_valid(&pre_app_image);
    delete_launcher_obj_if_valid(&pre_app_name);
    clear_launcher_widget_refs();

    pre_app_image = lv_img_create(app_scr);
    pre_img_path = src_img_path; // 保存历史
    lv_img_set_src(pre_app_image, src_img_path);
    lv_obj_align(pre_app_image, LV_ALIGN_CENTER, 0, -20);

    // 添加APP的名字
    pre_app_name = lv_label_create(app_scr);
    lv_obj_add_style(pre_app_name, &app_name_style, LV_STATE_DEFAULT);
    // lv_label_set_recolor(pre_app_name, true); //先得使能文本重绘色功能
    lv_label_set_text(pre_app_name, app_name);
    lv_obj_align_to(pre_app_name, pre_app_image, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_scr_load_anim(app_scr, LV_SCR_LOAD_ANIM_NONE, 300, 300, false);
    LVGL_UNLOCK();
}

void app_control_display_scr(const void *src_img, const char *app_name, lv_scr_load_anim_t anim_type, bool force)
{
    // force为是否强制刷新页面 true为强制刷新
    if (true == force)
    {
        display_app_scr_init(src_img, app_name);
        return;
    }

    if (src_img == pre_img_path)
    {
        return;
    }

    pre_img_path = src_img;
    int now_start_x;
    int now_end_x;
    int old_start_x;
    int old_end_x;

    if (LV_SCR_LOAD_ANIM_MOVE_LEFT == anim_type)
    {
        // 120为半个屏幕大小 应用图标规定是128，一半刚好是64
        now_start_x = -120 - 64;
        now_end_x = 0;
        old_start_x = 0;
        old_end_x = 120 + 64;
    }
    else
    {
        // 120为半个屏幕大小 应用图标规定是128，一半刚好是64
        now_start_x = 120 + 64;
        now_end_x = 0;
        old_start_x = 0;
        old_end_x = -120 - 64;
    }

    // Phase 1: stage the new icon + label and configure both animations
    // under the lock. Without this, the Display task can flush mid-create
    // and the dirty-region accounting goes inconsistent — the post-anim
    // lv_obj_del's invalidation never lands on the previous icon's bbox.
    static lv_anim_t now_app;
    static lv_anim_t pre_app;
    LVGL_LOCK();
    now_app_image = lv_img_create(app_scr);
    lv_img_set_src(now_app_image, src_img);
    lv_obj_align(now_app_image, LV_ALIGN_CENTER, 0, -20);
    // 添加APP的名字
    now_app_name = lv_label_create(app_scr);
    lv_obj_add_style(now_app_name, &app_name_style, LV_STATE_DEFAULT);
    // lv_label_set_recolor(now_app_name, true); //先得使能文本重绘色功能
    lv_label_set_text(now_app_name, app_name);
    // 删除原先的APP name
    delete_launcher_obj_if_valid(&pre_app_name);
    pre_app_name = now_app_name;
    lv_obj_align_to(now_app_name, now_app_image, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_anim_init(&now_app);
    lv_anim_set_exec_cb(&now_app, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_var(&now_app, now_app_image);
    lv_anim_set_values(&now_app, now_start_x, now_end_x);
    uint32_t duration = lv_anim_speed_to_time(400, now_start_x, now_end_x); // 计算时间
    lv_anim_set_time(&now_app, duration);
    lv_anim_set_path_cb(&now_app, lv_anim_path_linear); // 设置一个动画的路径

    lv_anim_init(&pre_app);
    lv_anim_set_exec_cb(&pre_app, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_var(&pre_app, pre_app_image);
    lv_anim_set_values(&pre_app, old_start_x, old_end_x);
    duration = lv_anim_speed_to_time(400, old_start_x, old_end_x); // 计算时间
    lv_anim_set_time(&pre_app, duration);
    lv_anim_set_path_cb(&pre_app, lv_anim_path_linear); // 设置一个动画的路径

    lv_anim_start(&now_app);
    lv_anim_start(&pre_app);
    LVGL_UNLOCK();

    // Phase 2: drive the LVGL pipeline forward while the slide animates.
    // aio_lvgl_aniend_wait takes/releases the mutex per iteration so the
    // Display task can still flush rendered chunks to the panel.
    ANIEND_WAIT

    // Phase 3: post-anim cleanup. The bare lv_task_handler + lv_obj_del
    // here used to run without the mutex — that's exactly the window
    // where the previous icon's "delete from app_scr" invalidation
    // overlapped a Display-task flush and got dropped. Wrap both under
    // the lock so the dirty rect for the old icon's bbox is fully
    // accounted for before the next iteration of the Display task.
    LVGL_LOCK();
    lv_task_handler(); // 消除 ANIEND_WAIT 执行完后依然"卡顿一下"的问题
    delete_launcher_obj_if_valid(&pre_app_image); // 删除原先的图像
    pre_app_image = now_app_image;
    LVGL_UNLOCK();
}

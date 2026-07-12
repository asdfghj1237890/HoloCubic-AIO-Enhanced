#include "game_2048_gui.h"
#include "element_images.h"

#include "lvgl.h"

#define SCALE_SIZE 4
#define TILE_SIZE 50
#define TILE_GAP 58
#define TILE_MARGIN 8

lv_obj_t *game_2048_gui = NULL;
lv_obj_t *img[SCALE_SIZE * SCALE_SIZE];

static lv_style_t default_style;

static int tile_x(int i)
{
    return TILE_MARGIN + i % SCALE_SIZE * TILE_GAP;
}

static int tile_y(int i)
{
    return TILE_MARGIN + i / SCALE_SIZE * TILE_GAP;
}

static void reset_tile_state(int i)
{
    if (NULL == img[i])
    {
        return;
    }

    lv_anim_del(img[i], NULL);
    lv_obj_set_size(img[i], TILE_SIZE, TILE_SIZE);
    lv_img_set_offset_x(img[i], 0);
    lv_img_set_offset_y(img[i], 0);
    lv_obj_set_pos(img[i], tile_x(i), tile_y(i));
}

void game_2048_gui_init(void)
{
    // app_init re-runs this GUI init on every app entry; static styles must
    // only be initialised once - lv_style_init() on an already-inited style
    // leaks its property array (LVGL "Potential memory leak").
    static bool style_inited = false;
    if (!style_inited)
    {
        style_inited = true;

        lv_style_init(&default_style);
        lv_style_set_bg_color(&default_style, lv_color_hex(0x000000));
    }

    lv_obj_t *act_obj = lv_scr_act(); // 获取当前活动页
    if (act_obj == game_2048_gui)
        return;
    lv_obj_clean(act_obj); // 清空此前页面

    //创建屏幕对象
    game_2048_gui = lv_obj_create(NULL);
    lv_obj_add_style(game_2048_gui, &default_style, LV_STATE_DEFAULT);

    for (int i = 0; i < SCALE_SIZE * SCALE_SIZE; i++)
    {
        img[i] = lv_img_create(game_2048_gui);
        lv_img_set_src(img[i], &N0);
        reset_tile_state(i);
    }
    lv_scr_load(game_2048_gui);
}

/*
 * 其他函数请根据需要添加
 */
void display_game_2048(const char *file_name, lv_scr_load_anim_t anim_type)
{
}

void game_2048_gui_del(void)
{
    if (NULL != game_2048_gui)
    {
        lv_obj_clean(game_2048_gui);
        game_2048_gui = NULL;
    }

    // 手动清除样式，防止内存泄漏
    // lv_style_reset(&default_style);
}

//用于宽高同时增加的lv_anim_exec_xcb_t动画参数
static void anim_size_cb(void *var, int32_t v)
{
    lv_obj_set_size(var, v, v);
}

//用于斜向移动的lv_anim_exec_xcb_t动画参数
static void anim_pos_cb(void *var, int32_t v)
{
    lv_obj_set_pos(var, v, v);
}

/*
 * 出生动画
 * i：出生的位置
 */
void born(int i)
{
    reset_tile_state(i);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_size_cb);
    lv_anim_set_var(&a, img[i]);
    lv_anim_set_time(&a, 300);

    /* 在动画中设置路径 */
    lv_anim_set_path_cb(&a, lv_anim_path_linear);

    lv_anim_set_values(&a, 0, TILE_SIZE);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, tile_x(i) + TILE_SIZE / 2, tile_x(i));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, tile_y(i) + TILE_SIZE / 2, tile_y(i));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_offset_x);
    lv_anim_set_values(&a, -TILE_SIZE / 2, 0);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_offset_y);
    lv_anim_set_values(&a, -TILE_SIZE / 2, 0);
    lv_anim_start(&a);
}

/*
 * 合并动画
 * i：合并的位置
 */
void zoom(int i)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_size_cb);
    lv_anim_set_var(&a, img[i]);
    lv_anim_set_delay(&a, 400);
    lv_anim_set_time(&a, 100);
    //播完后回放
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_set_playback_time(&a, 100);

    //线性动画
    lv_anim_set_path_cb(&a, lv_anim_path_linear);

    lv_anim_set_values(&a, TILE_SIZE, TILE_SIZE + 6);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(img[i]), lv_obj_get_x(img[i]) - 3);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, lv_obj_get_y(img[i]), lv_obj_get_y(img[i]) - 3);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_offset_x);
    lv_anim_set_values(&a, 0, 3);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_offset_y);
    lv_anim_set_values(&a, 0, 3);
    lv_anim_start(&a);
}

/*
 * 移动动画
 * i：移动的目标对象
 * direction：移动的方向，lv_obj_set_x或lv_obj_set_y
 * dist：移动的距离，如1、-1
 */
void move(int i, lv_anim_exec_xcb_t direction, int dist)
{
    lv_anim_t a;
    lv_anim_init(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)direction);
    lv_anim_set_var(&a, img[i]);
    lv_anim_set_time(&a, 500);
    if (direction == (lv_anim_exec_xcb_t)lv_obj_set_x)
    {
        lv_anim_set_values(&a, lv_obj_get_x(img[i]), lv_obj_get_x(img[i]) + dist * TILE_GAP);
    }
    else
    {
        lv_anim_set_values(&a, lv_obj_get_y(img[i]), lv_obj_get_y(img[i]) + dist * TILE_GAP);
    }

    // 在动画中设置路径
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);

    lv_anim_start(&a);
}

//获取图片内容对象
const lv_img_dsc_t *getN(int i)
{
    switch (i)
    {
    case 0:
        return &N0;
    case 2:
        return &N2;
    case 4:
        return &N4;
    case 8:
        return &N8;
    case 16:
        return &N16;
    case 32:
        return &N32;
    case 64:
        return &N64;
    case 128:
        return &N128;
    case 256:
        return &N256;
    case 512:
        return &N512;
    case 1024:
        return &N1024;
    case 2048:
        return &N2048;
    default:
        return &N0;
    }
}

//刷新棋盘
void showBoard(int *map)
{
    for (int i = 0; i < SCALE_SIZE * SCALE_SIZE; i++)
    {
        lv_img_set_src(img[i], getN(map[i]));
        reset_tile_state(i);
    }
}

/*
 * showAnim————用动画来更新棋盘
 * animMap：动画的轨迹数组
 * direction：移动的方向，1.上 2.下 3.左 4.右
 */
void showAnim(int *animMap, int direction)
{
    lv_anim_exec_xcb_t Normal;
    switch (direction)
    {
    case 1:
    case 2:
        Normal = (lv_anim_exec_xcb_t)lv_obj_set_y;
        break;
    case 3:
    case 4:
        Normal = (lv_anim_exec_xcb_t)lv_obj_set_x;
        break;
    }

    for (int i = 0; i < SCALE_SIZE * SCALE_SIZE; i++)
    {
        reset_tile_state(i);
    }

    //移动和合并
    for (int i = 0; i < 16; i++)
    {
        if (animMap[i] > 4)
        {
            zoom(i);
            move(i, Normal, animMap[i] - 8);
        }
        else if (animMap[i] != 0)
        {
            move(i, Normal, animMap[i]);
        }
    }
}

/*
 * newborn：新棋子的位置
 * map：新棋盘的地址
 */
void showNewBorn(int newborn, int *map)
{
    //展示新棋盘
    showBoard(map);

    //出现
    born(newborn);
}
